#include "code_generator.hpp"
#include <iostream>
#include <vector>
#include <sstream>

// ============================================================================
// Helper Functions & Constructor
// ============================================================================

CodeGenerator::CodeGenerator(output::CodeBuffer& buffer) : buffer(buffer), current_reg(""), current_type(ast::BuiltInType::VOID) {
    // Initialize with a global scope
    beginScope();
}

void CodeGenerator::beginScope() {
    symbol_table.push_back({});
}

void CodeGenerator::endScope() {
    if (!symbol_table.empty()) {
        symbol_table.pop_back();
    }
}

void CodeGenerator::declareVar(const std::string& name, const std::string& reg_ptr, ast::BuiltInType type) {
    if (symbol_table.empty()) return;
    // Insert into the current (innermost) scope
    symbol_table.back()[name] = {reg_ptr, type};
}

CodeGenerator::SymbolInfo* CodeGenerator::getVar(const std::string& name) {
    // Search from the innermost scope backwards
    for (auto it = symbol_table.rbegin(); it != symbol_table.rend(); ++it) {
        auto search = it->find(name);
        if (search != it->end()) {
            return &(search->second);
        }
    }
    return nullptr;
}

/**
 * @brief Emits runtime checks for division by zero.
 */
static void checkDivisionByZero(output::CodeBuffer& buffer, const std::string& divisor_reg) {
    std::string is_zero = buffer.freshVar();
    buffer.emit(is_zero + " = icmp eq i32 " + divisor_reg + ", 0");

    std::string label_error = buffer.freshLabel();
    std::string label_continue = buffer.freshLabel();

    buffer.emit("br i1 " + is_zero + ", label " + label_error + ", label " + label_continue);

    buffer.emitLabel(label_error);
    //  - Not needed, conceptual check
    buffer.emit("call void @print(i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str_div_err, i32 0, i32 0))");
    buffer.emit("call void @exit(i32 0)");
    buffer.emit("br label " + label_continue);

    buffer.emitLabel(label_continue);
}

// ============================================================================
// Visitor Implementations
// ============================================================================

void CodeGenerator::visit(ast::Funcs &node) {
    global_strings.clear();

    // Declare libc functions
    buffer.emit("declare i32 @printf(i8*, ...)");
    buffer.emit("declare void @exit(i32)");

    // Define global constants
    buffer.emit("@.int_specifier = constant [4 x i8] c\"%d\\0A\\00\"");
    buffer.emit("@.str_specifier = constant [4 x i8] c\"%s\\0A\\00\"");
    buffer.emit("@.str_div_err = constant [23 x i8] c\"Error division by zero\\00\"");

    // Helpers
    buffer.emit("define void @printi(i32) {");
    buffer.emit("    %spec_ptr = getelementptr [4 x i8], [4 x i8]* @.int_specifier, i32 0, i32 0");
    buffer.emit("    call i32 (i8*, ...) @printf(i8* %spec_ptr, i32 %0)");
    buffer.emit("    ret void");
    buffer.emit("}");

    buffer.emit("define void @print(i8*) {");
    buffer.emit("    %spec_ptr = getelementptr [4 x i8], [4 x i8]* @.str_specifier, i32 0, i32 0");
    buffer.emit("    call i32 (i8*, ...) @printf(i8* %spec_ptr, i8* %0)");
    buffer.emit("    ret void");
    buffer.emit("}");

    for (auto& func : node.funcs) {
        func->accept(*this);
    }

    // Emit global string definitions
    for (const auto& str : global_strings) {
        buffer.emit(str.var_name + " = constant [" + std::to_string(str.length) + 
                    " x i8] c\"" + str.value + "\\00\"");
    }
}

void CodeGenerator::visit(ast::FuncDecl &node) {
    buffer.emit("define " + output::toString(node.return_type->type) + " @" + node.id->value + "() {");
    buffer.emitLabel("%entry");

    // We do NOT call beginScope() here because visit(ast::Statements) handles it.
    // However, if we had function arguments, we would need a scope for them here.
    // For Phase 3, relying on Statements scope is sufficient.

    node.body->accept(*this);

    if (node.return_type->type == ast::BuiltInType::VOID) {
        buffer.emit("ret void");
    } else {
        buffer.emit("ret i32 0"); 
    }

    buffer.emit("}");
}

void CodeGenerator::visit(ast::Statements &node) {
    // "אחסון על מחסנית" - Scope management ensures variables are local to the block
    beginScope();
    for (auto& st : node.statements) {
        st->accept(*this);
    }
    endScope();
}

// Phase 3: Variable Declaration
void CodeGenerator::visit(ast::VarDecl &node) {
    // 1. Evaluate the initial value
    std::string init_val = "0";
    if (node.init_exp) {
        node.init_exp->accept(*this);
        init_val = current_reg;
    }

    // 2. Allocate stack memory (alloca)
    // "alloca instruction allocates memory on the stack"
    std::string ptr_reg = buffer.freshVar();
    buffer.emit(ptr_reg + " = alloca i32");

    // 3. Store the initial value
    // "store instruction writes to memory"
    buffer.emit("store i32 " + init_val + ", i32* " + ptr_reg);

    // 4. Register in Symbol Table
    declareVar(node.id->value, ptr_reg, node.type->type);
}

// Phase 3: Assignment
void CodeGenerator::visit(ast::Assign &node) {
    // 1. Find variable pointer
    SymbolInfo* info = getVar(node.id->value);
    if (!info) {
        // Should not happen in valid AST after Semantic Analysis
        return; 
    }

    // 2. Evaluate expression
    node.exp->accept(*this);
    std::string val_reg = current_reg;

    // 3. Store new value
    buffer.emit("store i32 " + val_reg + ", i32* " + info->reg_ptr);
}

// Phase 3: Variable Usage (ID)
void CodeGenerator::visit(ast::ID &node) {
    // 1. Find variable pointer
    SymbolInfo* info = getVar(node.value);
    if (info) {
        // 2. Load value from stack
        // "load instruction reads from memory"
        std::string val_reg = buffer.freshVar();
        buffer.emit(val_reg + " = load i32, i32* " + info->reg_ptr);
        
        current_reg = val_reg;
        current_type = info->type;
    } else {
        // Placeholder or Error (Phase 3 tests assume valid IDs)
        current_reg = "0";
        current_type = ast::BuiltInType::INT;
    }
}

// ============================================================================
// Existing Visitors (Phase 1 & 2)
// ============================================================================

void CodeGenerator::visit(ast::Num &node) {
    current_reg = buffer.freshVar();
    buffer.emit(current_reg + " = add i32 0, " + std::to_string(node.value));
    current_type = ast::BuiltInType::INT;
}

void CodeGenerator::visit(ast::NumB &node) {
    current_reg = buffer.freshVar();
    buffer.emit(current_reg + " = add i32 0, " + std::to_string(node.value));
    current_type = ast::BuiltInType::BYTE;
}

void CodeGenerator::visit(ast::String &node) {
    std::string var_name = "@.str." + std::to_string(global_strings.size());
    int length = node.value.length() + 1;
    global_strings.push_back({node.value, var_name, length});
    
    current_reg = "getelementptr inbounds ([" + std::to_string(length) + " x i8], [" + 
                  std::to_string(length) + " x i8]* " + var_name + ", i32 0, i32 0)";
    current_type = ast::BuiltInType::STRING;
}

void CodeGenerator::visit(ast::Bool &node) {
    current_reg = buffer.freshVar();
    buffer.emit(current_reg + " = add i1 0, " + (node.value ? "1" : "0"));
    current_type = ast::BuiltInType::BOOL;
}

void CodeGenerator::visit(ast::BinOp &node) {
    node.left->accept(*this);
    std::string left_reg = current_reg;
    ast::BuiltInType left_type = current_type;

    node.right->accept(*this);
    std::string right_reg = current_reg;
    ast::BuiltInType right_type = current_type;

    std::string op_cmd;
    bool is_byte_op = (left_type == ast::BuiltInType::BYTE && right_type == ast::BuiltInType::BYTE);

    if (node.op == ast::DIV) {
        checkDivisionByZero(buffer, right_reg);
        op_cmd = is_byte_op ? "udiv" : "sdiv";
    } else {
        switch (node.op) {
            case ast::ADD: op_cmd = "add"; break;
            case ast::SUB: op_cmd = "sub"; break;
            case ast::MUL: op_cmd = "mul"; break;
            default: break; 
        }
    }

    std::string res_reg = buffer.freshVar();
    buffer.emit(res_reg + " = " + op_cmd + " i32 " + left_reg + ", " + right_reg);

    if (is_byte_op) {
        std::string trunc_reg = buffer.freshVar();
        buffer.emit(trunc_reg + " = and i32 " + res_reg + ", 255");
        current_reg = trunc_reg;
        current_type = ast::BuiltInType::BYTE;
    } else {
        current_reg = res_reg;
        current_type = ast::BuiltInType::INT;
    }
}

void CodeGenerator::visit(ast::RelOp &node) {
    node.left->accept(*this);
    std::string left_reg = current_reg;
    node.right->accept(*this);
    std::string right_reg = current_reg;

    std::string op_cmp;
    switch (node.op) {
        case ast::EQ: op_cmp = "eq"; break;
        case ast::NE: op_cmp = "ne"; break;
        case ast::LT: op_cmp = "slt"; break;
        case ast::GT: op_cmp = "sgt"; break;
        case ast::LE: op_cmp = "sle"; break;
        case ast::GE: op_cmp = "sge"; break;
    }

    std::string res_reg = buffer.freshVar();
    buffer.emit(res_reg + " = icmp " + op_cmp + " i32 " + left_reg + ", " + right_reg);
    current_reg = res_reg;
    current_type = ast::BuiltInType::BOOL;
}

void CodeGenerator::visit(ast::Not &node) {
    node.exp->accept(*this);
    std::string res_reg = buffer.freshVar();
    buffer.emit(res_reg + " = xor i1 " + current_reg + ", 1");
    current_reg = res_reg;
    current_type = ast::BuiltInType::BOOL;
}

void CodeGenerator::visit(ast::And &node) {
    node.left->accept(*this);
    std::string left_reg = current_reg;

    std::string label_check_right = buffer.freshLabel();
    std::string label_end = buffer.freshLabel();
    std::string ptr_var = buffer.freshVar(); 

    buffer.emit(ptr_var + " = alloca i1");
    buffer.emit("store i1 " + left_reg + ", i1* " + ptr_var);
    buffer.emit("br i1 " + left_reg + ", label " + label_check_right + ", label " + label_end);

    buffer.emitLabel(label_check_right);
    node.right->accept(*this);
    std::string right_reg = current_reg;
    buffer.emit("store i1 " + right_reg + ", i1* " + ptr_var);
    buffer.emit("br label " + label_end);

    buffer.emitLabel(label_end);
    std::string res_reg = buffer.freshVar();
    buffer.emit(res_reg + " = load i1, i1* " + ptr_var);
    current_reg = res_reg;
    current_type = ast::BuiltInType::BOOL;
}

void CodeGenerator::visit(ast::Or &node) {
    node.left->accept(*this);
    std::string left_reg = current_reg;

    std::string label_check_right = buffer.freshLabel();
    std::string label_end = buffer.freshLabel();
    std::string ptr_var = buffer.freshVar();

    buffer.emit(ptr_var + " = alloca i1");
    buffer.emit("store i1 " + left_reg + ", i1* " + ptr_var);
    buffer.emit("br i1 " + left_reg + ", label " + label_end + ", label " + label_check_right);

    buffer.emitLabel(label_check_right);
    node.right->accept(*this);
    std::string right_reg = current_reg;
    buffer.emit("store i1 " + right_reg + ", i1* " + ptr_var);
    buffer.emit("br label " + label_end);

    buffer.emitLabel(label_end);
    std::string res_reg = buffer.freshVar();
    buffer.emit(res_reg + " = load i1, i1* " + ptr_var);
    current_reg = res_reg;
    current_type = ast::BuiltInType::BOOL;
}

void CodeGenerator::visit(ast::If &node) {
    std::string true_label = buffer.freshLabel();
    std::string false_label = buffer.freshLabel();
    std::string end_label = buffer.freshLabel();

    node.condition->accept(*this);
    buffer.emit("br i1 " + current_reg + ", label " + true_label + ", label " + false_label);

    buffer.emitLabel(true_label);
    node.then->accept(*this);
    buffer.emit("br label " + end_label);

    buffer.emitLabel(false_label);
    if (node.otherwise) {
        node.otherwise->accept(*this);
    }
    buffer.emit("br label " + end_label);

    buffer.emitLabel(end_label);
}

void CodeGenerator::visit(ast::While &node) {
    std::string check_label = buffer.freshLabel();
    std::string loop_label = buffer.freshLabel();
    std::string end_label = buffer.freshLabel();

    buffer.emit("br label " + check_label);
    buffer.emitLabel(check_label);
    
    node.condition->accept(*this);
    buffer.emit("br i1 " + current_reg + ", label " + loop_label + ", label " + end_label);

    buffer.emitLabel(loop_label);
    node.body->accept(*this);
    buffer.emit("br label " + check_label);

    buffer.emitLabel(end_label);
}

void CodeGenerator::visit(ast::Call &node) {
    std::string func_name = node.func_id->value;
    
    if (func_name == "print") {
        if (!node.args->exps.empty()) {
            node.args->exps[0]->accept(*this);
            buffer.emit("call void @print(i8* " + current_reg + ")");
        }
    } 
    else if (func_name == "printi") {
        if (!node.args->exps.empty()) {
            node.args->exps[0]->accept(*this);
            if (current_type == ast::BuiltInType::BOOL) {
                std::string zext_reg = buffer.freshVar();
                buffer.emit(zext_reg + " = zext i1 " + current_reg + " to i32");
                current_reg = zext_reg;
            }
            buffer.emit("call void @printi(i32 " + current_reg + ")");
        }
    }
    else {
        // Generic call handling
        std::string args_str = "";
        buffer.emit("call i32 @" + func_name + "(" + args_str + ")");
    }
}

void CodeGenerator::visit(ast::Return &node) {
    if (node.exp) {
        node.exp->accept(*this);
        buffer.emit("ret i32 " + current_reg);
    } else {
        buffer.emit("ret void");
    }
}

// Stubs
void CodeGenerator::visit(ast::Type &node) {}
void CodeGenerator::visit(ast::Cast &node) {}
void CodeGenerator::visit(ast::ExpList &node) {}
void CodeGenerator::visit(ast::Break &node) {}
void CodeGenerator::visit(ast::Continue &node) {}
void CodeGenerator::visit(ast::Formal &node) {}
void CodeGenerator::visit(ast::Formals &node) {}
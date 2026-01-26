#include "code_generator.hpp"
#include <vector>
#include <sstream>

CodeGenerator::CodeGenerator(output::CodeBuffer& buffer) 
    : buffer(buffer), current_reg(""), current_type(ast::BuiltInType::VOID) {
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
    symbol_table.back()[name] = {reg_ptr, type};
}

CodeGenerator::SymbolInfo* CodeGenerator::getVar(const std::string& name) {
    // Search for the variable starting from the innermost scope
    for (auto it = symbol_table.rbegin(); it != symbol_table.rend(); ++it) {
        auto search = it->find(name);
        if (search != it->end()) {
            return &(search->second);
        }
    }
    return nullptr;
}


//Converts AST built-in types to their LLVM IR string representation.
static std::string toLLVMType(ast::BuiltInType type) {
    switch (type) {
        case ast::BuiltInType::INT: return "i32";
        // Bytes are promoted to i32
        case ast::BuiltInType::BYTE: return "i32"; 
        // Booleans are stored as i32 (0 or 1) in args/return
        case ast::BuiltInType::BOOL: return "i32"; 
        case ast::BuiltInType::VOID: return "void";
        case ast::BuiltInType::STRING: return "i8*";
        default: return "i32";
    }
}

//Emits LLVM IR for checking division by zero at runtime.
static void checkDivisionByZero(output::CodeBuffer& buffer, const std::string& divisor_reg) {
    std::string is_zero = buffer.freshVar();
    buffer.emit(is_zero + " = icmp eq i32 " + divisor_reg + ", 0");

    std::string label_error = buffer.freshLabel();
    std::string label_continue = buffer.freshLabel();

    buffer.emit("br i1 " + is_zero + ", label " + label_error + ", label " + label_continue);

    // Error handling
    buffer.emitLabel(label_error);
    buffer.emit("call void @print(i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str_div_err, i32 0, i32 0))");
    buffer.emit("call void @exit(i32 0)");
    buffer.emit("br label " + label_continue);

    buffer.emitLabel(label_continue);
}


// ***VISITOR IMPLEMENTATIONS***

void CodeGenerator::visit(ast::Funcs &node) {
    global_strings.clear();
    functions_table.clear();

    // Emit standard library declarations and constants
    buffer.emit("declare i32 @printf(i8*, ...)");
    buffer.emit("declare void @exit(i32)");
    buffer.emit("@.int_specifier = constant [4 x i8] c\"%d\\0A\\00\"");
    buffer.emit("@.str_specifier = constant [4 x i8] c\"%s\\0A\\00\"");
    buffer.emit("@.str_div_err = constant [23 x i8] c\"Error division by zero\\00\"");

    // Emit helper functions: printi and print
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

    //Register all function signatures to support forward references
    for (auto& func : node.funcs) {
        functions_table[func->id->value] = func->return_type->type;
    }

    //Generate code for function bodies
    for (auto& func : node.funcs) {
        func->accept(*this);
    }

    // Emit global string literals
    for (const auto& str : global_strings) {
        buffer.emit(str.var_name + " = constant [" + std::to_string(str.length) + 
                    " x i8] c\"" + str.value + "\\00\"");
    }
}

void CodeGenerator::visit(ast::FuncDecl &node) {
    std::stringstream args_ss;
    if (node.formals) {
        for (size_t i = 0; i < node.formals->formals.size(); ++i) {
            args_ss << "i32";
            if (i < node.formals->formals.size() - 1) args_ss << ", ";
        }
    }
    
    std::string return_type_str = toLLVMType(node.return_type->type);
    buffer.emit("define " + return_type_str + " @" + node.id->value + "(" + args_ss.str() + ") {");
    buffer.emitLabel("%entry");

    beginScope();

    // Allocate stack space for arguments and store initial values
    if (node.formals) {
        for (size_t i = 0; i < node.formals->formals.size(); ++i) {
            auto formal = node.formals->formals[i];
            std::string ptr_reg = buffer.freshVar();
            buffer.emit(ptr_reg + " = alloca i32");
            buffer.emit("store i32 %" + std::to_string(i) + ", i32* " + ptr_reg);
            declareVar(formal->id->value, ptr_reg, formal->type->type);
        }
    }

    node.body->accept(*this);
    endScope();

    // Fallback return to ensure valid control flow
    std::string fallback_label = buffer.freshLabel();
    buffer.emit("br label " + fallback_label);
    buffer.emitLabel(fallback_label);

    if (node.return_type->type == ast::BuiltInType::VOID) {
        buffer.emit("ret void");
    } else {
        buffer.emit("ret i32 0"); 
    }

    buffer.emit("}");
}

void CodeGenerator::visit(ast::Call &node) {
    std::string func_name = node.func_id->value;
    
    // Built-in print function
    if (func_name == "print") {
        if (!node.args->exps.empty()) {
            node.args->exps[0]->accept(*this);
            buffer.emit("call void @print(i8* " + current_reg + ")");
        }
        return;
    } 
    
    // Built-in printi function
    if (func_name == "printi") {
        if (!node.args->exps.empty()) {
            node.args->exps[0]->accept(*this);
            if (current_type == ast::BuiltInType::BOOL) {
                std::string zext_reg = buffer.freshVar();
                buffer.emit(zext_reg + " = zext i1 " + current_reg + " to i32");
                current_reg = zext_reg;
            }
            buffer.emit("call void @printi(i32 " + current_reg + ")");
        }
        return;
    }

    // User-defined functions
    std::stringstream args_str;
    if (node.args) {
        for (size_t i = 0; i < node.args->exps.size(); ++i) {
            node.args->exps[i]->accept(*this);
            
            std::string arg_val = current_reg;
            if (current_type == ast::BuiltInType::BOOL) {
                std::string zext_reg = buffer.freshVar();
                buffer.emit(zext_reg + " = zext i1 " + current_reg + " to i32");
                arg_val = zext_reg;
            }
            
            args_str << "i32 " << arg_val;
            if (i < node.args->exps.size() - 1) {
                args_str << ", ";
            }
        }
    }
    
    // Determine the return type logic
    bool is_void = false;
    bool is_bool = false; 

    if (functions_table.find(func_name) != functions_table.end()) {
        ast::BuiltInType ret_type = functions_table[func_name];
        if (ret_type == ast::BuiltInType::VOID) {
            is_void = true;
        } else if (ret_type == ast::BuiltInType::BOOL) {
            is_bool = true;
        }
    }

    if (is_void) {
        buffer.emit("call void @" + func_name + "(" + args_str.str() + ")");
        current_reg = "0";
        current_type = ast::BuiltInType::VOID;
    } else {
        std::string res_reg = buffer.freshVar();
        buffer.emit(res_reg + " = call i32 @" + func_name + "(" + args_str.str() + ")");
        
        if (is_bool) {
            std::string trunc_reg = buffer.freshVar();
            buffer.emit(trunc_reg + " = trunc i32 " + res_reg + " to i1");
            current_reg = trunc_reg;
            current_type = ast::BuiltInType::BOOL;
        } else {
            current_reg = res_reg;
            current_type = ast::BuiltInType::INT; 
        }
    }
}

void CodeGenerator::visit(ast::Statements &node) {
    beginScope();
    for (auto& st : node.statements) {
        st->accept(*this);
    }
    endScope();
}

void CodeGenerator::visit(ast::VarDecl &node) {
    std::string init_val = "0";
    if (node.init_exp) {
        node.init_exp->accept(*this);
        init_val = current_reg;

        if (current_type == ast::BuiltInType::BOOL) {
            std::string zext_reg = buffer.freshVar();
            buffer.emit(zext_reg + " = zext i1 " + current_reg + " to i32");
            init_val = zext_reg;
        }
    }

    std::string ptr_reg = buffer.freshVar();
    buffer.emit(ptr_reg + " = alloca i32");
    buffer.emit("store i32 " + init_val + ", i32* " + ptr_reg);

    declareVar(node.id->value, ptr_reg, node.type->type);
}

void CodeGenerator::visit(ast::Assign &node) {
    SymbolInfo* info = getVar(node.id->value);
    if (!info) return; 

    node.exp->accept(*this);
    std::string val_to_store = current_reg;

    if (current_type == ast::BuiltInType::BOOL) {
        std::string zext_reg = buffer.freshVar();
        buffer.emit(zext_reg + " = zext i1 " + current_reg + " to i32");
        val_to_store = zext_reg;
    }

    buffer.emit("store i32 " + val_to_store + ", i32* " + info->reg_ptr);
}

void CodeGenerator::visit(ast::ID &node) {
    SymbolInfo* info = getVar(node.value);
    if (info) {
        std::string val_reg = buffer.freshVar();
        buffer.emit(val_reg + " = load i32, i32* " + info->reg_ptr);
        current_reg = val_reg;
        
        // Truncate i32 back to i1 for boolean logic usage
        if (info->type == ast::BuiltInType::BOOL) {
            std::string trunc_reg = buffer.freshVar();
            buffer.emit(trunc_reg + " = trunc i32 " + val_reg + " to i1");
            current_reg = trunc_reg;
        }
        
        current_type = info->type;
    } else {
        current_reg = "0";
        current_type = ast::BuiltInType::INT;
    }
}

void CodeGenerator::visit(ast::While &node) {
    std::string check_label = buffer.freshLabel();
    std::string loop_label = buffer.freshLabel();
    std::string end_label = buffer.freshLabel();

    loops_stack.push_back({check_label, end_label});

    buffer.emit("br label " + check_label);
    buffer.emitLabel(check_label);
    
    node.condition->accept(*this);
    buffer.emit("br i1 " + current_reg + ", label " + loop_label + ", label " + end_label);

    buffer.emitLabel(loop_label);
    node.body->accept(*this);
    buffer.emit("br label " + check_label);

    buffer.emitLabel(end_label);
    
    loops_stack.pop_back();
}

void CodeGenerator::visit(ast::Break &node) {
    if (!loops_stack.empty()) {
        buffer.emit("br label " + loops_stack.back().end_label);
        std::string unreachable = buffer.freshLabel();
        buffer.emitLabel(unreachable);
    }
}

void CodeGenerator::visit(ast::Continue &node) {
    if (!loops_stack.empty()) {
        buffer.emit("br label " + loops_stack.back().check_label);
        std::string unreachable = buffer.freshLabel();
        buffer.emitLabel(unreachable);
    }
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

void CodeGenerator::visit(ast::Return &node) {
    if (node.exp) {
        node.exp->accept(*this);
        std::string ret_val = current_reg;
        if (current_type == ast::BuiltInType::BOOL) {
            std::string zext_reg = buffer.freshVar();
            buffer.emit(zext_reg + " = zext i1 " + current_reg + " to i32");
            ret_val = zext_reg;
        }
        buffer.emit("ret i32 " + ret_val);
    } else {
        buffer.emit("ret void");
    }
}

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

void CodeGenerator::visit(ast::Type &node) {}
void CodeGenerator::visit(ast::Cast &node) {}
void CodeGenerator::visit(ast::ExpList &node) {}
void CodeGenerator::visit(ast::Formal &node) {}
void CodeGenerator::visit(ast::Formals &node) {}
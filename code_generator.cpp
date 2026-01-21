#include "code_generator.hpp"
#include <iostream>

/**
 * @brief Emits runtime checks for division by zero.
 * * If the divisor is zero, the program prints an error message and exits.
 * * @param buffer Reference to the CodeBuffer.
 * @param divisor_reg The register holding the divisor value.
 */
static void checkDivisionByZero(output::CodeBuffer& buffer, const std::string& divisor_reg) {
    std::string is_zero = buffer.freshVar();
    buffer.emit(is_zero + " = icmp eq i32 " + divisor_reg + ", 0");

    std::string label_error = buffer.freshLabel();
    std::string label_continue = buffer.freshLabel();

    buffer.emit("br i1 " + is_zero + ", label " + label_error + ", label " + label_continue);

    buffer.emitLabel(label_error);
    buffer.emit("call void @print(i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str_div_err, i32 0, i32 0))");
    buffer.emit("call void @exit(i32 0)");
    buffer.emit("br label " + label_continue); // Unreachable, needed for block structure validity

    buffer.emitLabel(label_continue);
}

void CodeGenerator::visit(ast::Funcs &node) {
    // Declare libc functions
    buffer.emit("declare i32 @printf(i8*, ...)");
    buffer.emit("declare void @exit(i32)");

    // Define global constants for printing
    buffer.emit("@.int_specifier = constant [4 x i8] c\"%d\\0A\\00\"");
    buffer.emit("@.str_specifier = constant [4 x i8] c\"%s\\0A\\00\"");
    buffer.emit("@.str_div_err = constant [23 x i8] c\"Error division by zero\\00\"");

    // Define helper function: printi
    buffer.emit("define void @printi(i32) {");
    buffer.emit("    %spec_ptr = getelementptr [4 x i8], [4 x i8]* @.int_specifier, i32 0, i32 0");
    buffer.emit("    call i32 (i8*, ...) @printf(i8* %spec_ptr, i32 %0)");
    buffer.emit("    ret void");
    buffer.emit("}");

    // Define helper function: print
    buffer.emit("define void @print(i8*) {");
    buffer.emit("    %spec_ptr = getelementptr [4 x i8], [4 x i8]* @.str_specifier, i32 0, i32 0");
    buffer.emit("    call i32 (i8*, ...) @printf(i8* %spec_ptr, i8* %0)");
    buffer.emit("    ret void");
    buffer.emit("}");

    for (auto& func : node.funcs) {
        func->accept(*this);
    }
}

void CodeGenerator::visit(ast::FuncDecl &node) {
    buffer.emit("define " + output::toString(node.return_type->type) + " @" + node.id->value + "() {");
    buffer.emitLabel("%entry");

    // TODO: Stack allocation for local variables will be implemented here.

    node.body->accept(*this);

    // Implicit return handling for void functions or fallthrough
    if (node.return_type->type == ast::BuiltInType::VOID) {
        buffer.emit("ret void");
    } else {
        // Fallback return 0 for non-void functions hitting end of control flow
        buffer.emit("ret i32 0"); 
    }

    buffer.emit("}");
}

void CodeGenerator::visit(ast::Statements &node) {
    for (auto& st : node.statements) {
        st->accept(*this);
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
        // Truncate result to 8 bits (0-255) to maintain Byte semantics
        std::string trunc_reg = buffer.freshVar();
        buffer.emit(trunc_reg + " = and i32 " + res_reg + ", 255");
        current_reg = trunc_reg;
        current_type = ast::BuiltInType::BYTE;
    } else {
        current_reg = res_reg;
        current_type = ast::BuiltInType::INT;
    }
}

void CodeGenerator::visit(ast::Call &node) {
    // Temporary implementation for testing arithmetic expressions using printi
    if (node.func_id->value == "printi" && !node.args->exps.empty()) {
        node.args->exps[0]->accept(*this);
        buffer.emit("call void @printi(i32 " + current_reg + ")");
    }
}

void CodeGenerator::visit(ast::ID &node) {
    // Placeholder: Variable loading will be implemented in future stages
}

void CodeGenerator::visit(ast::Return &node) {
    if (node.exp) {
        node.exp->accept(*this);
        buffer.emit("ret i32 " + current_reg);
    } else {
        buffer.emit("ret void");
    }
}
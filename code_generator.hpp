#ifndef CODE_GEN_HPP
#define CODE_GEN_HPP

#include "visitor.hpp"
#include "nodes.hpp"
#include "output.hpp"
#include <string>

/**
 * @class CodeGenerator
 * @brief Visitor implementation for generating LLVM IR code from the AST.
 *
 * This class traverses the AST and emits corresponding LLVM IR instructions
 * to the provided CodeBuffer. It handles register allocation and type tracking
 * for expression evaluation.
 */
class CodeGenerator : public Visitor {
public:
    /**
     * Reference to the output buffer for writing LLVM IR instructions.
     */
    output::CodeBuffer& buffer;

    /**
     * Holds the name of the LLVM register containing the result of the
     * last visited expression (e.g., "%t0").
     */
    std::string current_reg;

    /**
     * Tracks the semantic type (INT or BYTE) of the value currently stored
     * in `current_reg`. Used for determining specific arithmetic instructions
     * (sdiv vs udiv) and truncation requirements.
     */
    ast::BuiltInType current_type;

    /**
     * @brief Constructs a new Code Generator object.
     * @param buffer Reference to the CodeBuffer wrapper.
     */
    explicit CodeGenerator(output::CodeBuffer& buffer) 
        : buffer(buffer), current_type(ast::BuiltInType::VOID) {}

    // --- Expression Visits ---
    void visit(ast::Num &node) override;
    void visit(ast::NumB &node) override;
    void visit(ast::BinOp &node) override;
    void visit(ast::ID &node) override;
    void visit(ast::Call &node) override;

    // --- Statement/Structure Visits ---
    void visit(ast::Funcs &node) override;
    void visit(ast::FuncDecl &node) override;
    void visit(ast::Statements &node) override;
    void visit(ast::Return &node) override;
    
    // --- Unimplemented/Future Visits (Placeholders) ---
    void visit(ast::String &node) override {}
    void visit(ast::Bool &node) override {}
    void visit(ast::RelOp &node) override {}
    void visit(ast::Not &node) override {}
    void visit(ast::And &node) override {}
    void visit(ast::Or &node) override {}
    void visit(ast::Type &node) override {}
    void visit(ast::Cast &node) override {}
    void visit(ast::ExpList &node) override {}
    void visit(ast::Break &node) override {}
    void visit(ast::Continue &node) override {}
    void visit(ast::If &node) override {}
    void visit(ast::While &node) override {}
    void visit(ast::VarDecl &node) override {}
    void visit(ast::Assign &node) override {}
    void visit(ast::Formal &node) override {}
    void visit(ast::Formals &node) override {}
};

#endif // CODE_GEN_HPP
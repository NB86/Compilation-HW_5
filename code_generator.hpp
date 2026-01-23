#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

#include "nodes.hpp"
#include "visitor.hpp"
#include "output.hpp"
#include <string>
#include <vector>
#include <unordered_map>

/**
 * @class CodeGenerator
 * @brief Visitor implementation responsible for generating LLVM IR code from the AST.
 * * This class traverses the Abstract Syntax Tree (AST) and emits corresponding 
 * LLVM intermediate representation commands to the provided CodeBuffer.
 * It manages symbol tables for variables and functions, as well as control flow 
 * structures for loops.
 */
class CodeGenerator : public Visitor {
public:
    /**
     * @brief Constructs a new CodeGenerator object.
     * @param buffer Reference to the output buffer where LLVM IR commands will be written.
     */
    explicit CodeGenerator(output::CodeBuffer& buffer);

    // Visitor implementations for AST nodes
    virtual void visit(ast::Num& node) override;
    virtual void visit(ast::NumB& node) override;
    virtual void visit(ast::String& node) override;
    virtual void visit(ast::Bool& node) override;
    virtual void visit(ast::ID& node) override;
    virtual void visit(ast::BinOp& node) override;
    virtual void visit(ast::RelOp& node) override;
    virtual void visit(ast::Not& node) override;
    virtual void visit(ast::And& node) override;
    virtual void visit(ast::Or& node) override;
    virtual void visit(ast::Type& node) override;
    virtual void visit(ast::Cast& node) override;
    virtual void visit(ast::ExpList& node) override;
    virtual void visit(ast::Call& node) override;
    virtual void visit(ast::Statements& node) override;
    virtual void visit(ast::Break& node) override;
    virtual void visit(ast::Continue& node) override;
    virtual void visit(ast::Return& node) override;
    virtual void visit(ast::If& node) override;
    virtual void visit(ast::While& node) override;
    virtual void visit(ast::VarDecl& node) override;
    virtual void visit(ast::Assign& node) override;
    virtual void visit(ast::Formal& node) override;
    virtual void visit(ast::Formals& node) override;
    virtual void visit(ast::FuncDecl& node) override;
    virtual void visit(ast::Funcs& node) override;

private:
    output::CodeBuffer& buffer;
    
    // Tracks the register holding the result of the last visited expression
    std::string current_reg;
    // Tracks the type of the result of the last visited expression
    ast::BuiltInType current_type;

    /**
     * @struct SymbolInfo
     * @brief Holds information about a local variable in the scope.
     */
    struct SymbolInfo {
        std::string reg_ptr;     // LLVM register pointer to the variable's stack location
        ast::BuiltInType type;   // The variable's type
    };

    // Symbol table supporting nested scopes.
    // Each element in the vector represents a scope level.
    std::vector<std::unordered_map<std::string, SymbolInfo>> symbol_table;

    // Maps function names to their return types to allow forward references.
    std::unordered_map<std::string, ast::BuiltInType> functions_table;

    /**
     * @struct LoopLabels
     * @brief Stores labels for control flow within loops.
     */
    struct LoopLabels {
        std::string check_label; // Label for the condition check (used by 'continue')
        std::string end_label;   // Label for the end of the loop (used by 'break')
    };

    // Stack of active loops to handle nested 'break' and 'continue' statements.
    std::vector<LoopLabels> loops_stack;

    /**
     * @struct GlobalString
     * @brief Represents a string literal to be defined globally.
     */
    struct GlobalString {
        std::string value;
        std::string var_name;
        int length;
    };
    std::vector<GlobalString> global_strings;

    // Helper methods for scope management
    void beginScope();
    void endScope();
    void declareVar(const std::string& name, const std::string& reg_ptr, ast::BuiltInType type);
    SymbolInfo* getVar(const std::string& name);
};

#endif // CODE_GENERATOR_H
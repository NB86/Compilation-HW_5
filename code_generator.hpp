#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

#include "nodes.hpp"
#include "visitor.hpp"
#include "output.hpp" // Assuming you have this from previous steps
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>

class CodeGenerator : public Visitor {
public:
    CodeGenerator(output::CodeBuffer& buffer);    
    // Visitor implementations
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
    
    // State management
    std::string current_reg;
    ast::BuiltInType current_type;

    // Symbol Table Structures
    struct SymbolInfo {
        std::string reg_ptr;     // The LLVM register holding the pointer (e.g., %ptr_x)
        ast::BuiltInType type;   // The type of the variable
    };

    // A vector of maps to handle nested scopes
    // New scopes are pushed to the back, searches are done in reverse order
    std::vector<std::unordered_map<std::string, SymbolInfo>> symbol_table;

    // Helper methods for Scopes
    void beginScope();
    void endScope();
    void declareVar(const std::string& name, const std::string& reg_ptr, ast::BuiltInType type);
    SymbolInfo* getVar(const std::string& name);

    // Global strings management
    struct GlobalString {
        std::string value;
        std::string var_name;
        int length;
    };
    std::vector<GlobalString> global_strings;
};

#endif // CODE_GENERATOR_H
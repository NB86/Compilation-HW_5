#ifndef SEMANTIC_ANALAYZER_VISITOR_HPP
#define SEMANTIC_ANALAYZER_VISITOR_HPP

#include <string>
#include <stack>
#include <vector>
#include <memory>

#include "visitor.hpp"
#include "nodes.hpp"
#include "output.hpp"

struct SymbolEntry {
    std::string name;
    ast::BuiltInType type;
    int offset;
};

struct FunctionSymbolEntry {
    std::string name;
    int offset;
    ast::BuiltInType return_type;
    std::vector<ast::BuiltInType> arguments;
};

class SemanticAnalayzerVisitor : public Visitor {
public:
    SemanticAnalayzerVisitor();

    void visit(ast::Num &node) override;
    void visit(ast::NumB &node) override;
    void visit(ast::String &node) override;
    void visit(ast::Bool &node) override;
    void visit(ast::ID &node) override;
    void visit(ast::BinOp &node) override;
    void visit(ast::RelOp &node) override;
    void visit(ast::Not &node) override;
    void visit(ast::And &node) override;
    void visit(ast::Or &node) override;
    void visit(ast::Type &node) override;
    void visit(ast::Cast &node) override;
    void visit(ast::ExpList &node) override;
    void visit(ast::Call &node) override;
    void visit(ast::Statements &node) override;
    void visit(ast::Break &node) override;
    void visit(ast::Continue &node) override;
    void visit(ast::Return &node) override;
    void visit(ast::If &node) override;
    void visit(ast::While &node) override;
    void visit(ast::VarDecl &node) override;
    void visit(ast::Assign &node) override;
    void visit(ast::Formal &node) override;
    void visit(ast::Formals &node) override;
    void visit(ast::FuncDecl &node) override;
    void visit(ast::Funcs &node) override;

private:
    std::stack<int> offset_stack;
    std::vector<std::vector<SymbolEntry>> symbol_table;
    std::vector<FunctionSymbolEntry> function_symbol_table;
    FunctionSymbolEntry current_function;
    int number_of_while_inside; 
    
    ast::BuiltInType getExpressionType(std::shared_ptr<ast::Exp> exp);
};

#endif // SEMANTIC_ANALAYZER_VISITOR_HPP
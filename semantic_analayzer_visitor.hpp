#include <string>
#include <stack>
#include <vector>

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
    // Return type of the function
    ast::BuiltInType return_type;
    // List of formal parameters
    std::vector<ast::BuiltInType> arguments;
};


class SemanticAnalayzerVisitor : public Visitor {
public:

    /*C'tor of the visitor*/
    SemanticAnalayzerVisitor();

    /* override the visit functions */

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

    output::ScopePrinter scope_printer;

private:
    /*
     Eeach scope requires new offset counter. therefore we maintaining stack of offets.
     The last offset (head of the stack) belongs to the current scope.
    */
    std::stack<int> offset_stack;

    /*
     Each std::vector<SymbolEntry> in the wrapper vector, represnts a scope, 
     and the elements inside this vector are the symbols in the scope. 
    */
    std::vector<std::vector<SymbolEntry>> symbol_table;
    std::vector<SymbolEntry> symbols_in_current_scope; // used in formal parameters checking
    std::vector<FunctionSymbolEntry> function_symbol_table;
    FunctionSymbolEntry current_function;
    int number_of_while_inside; 
    ast::BuiltInType getExpressionType(std::shared_ptr<ast::Exp> exp);
};
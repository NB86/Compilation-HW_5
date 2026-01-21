#include <algorithm>
#include <iostream>

#include "semantic_analayzer_visitor.hpp"

SemanticAnalayzerVisitor::SemanticAnalayzerVisitor() : number_of_while_inside(0) {}

void SemanticAnalayzerVisitor::visit(ast::Funcs &node) {
    // adding global scope offset
    offset_stack.push(0);

    // emit library functions
    scope_printer.emitFunc("print", ast::BuiltInType::VOID, {ast::BuiltInType::STRING});
    scope_printer.emitFunc("printi", ast::BuiltInType::VOID, {ast::BuiltInType::INT});
    FunctionSymbolEntry print_entry = {"print", offset_stack.top()++, ast::BuiltInType::VOID, {ast::BuiltInType::STRING}};
    FunctionSymbolEntry printi_entry = {"printi", offset_stack.top()++, ast::BuiltInType::VOID, {ast::BuiltInType::INT}};
    function_symbol_table.push_back(print_entry);
    function_symbol_table.push_back(printi_entry);

    // Adding first all the symbols of the functions to the function_symbol_table attribute. 
    bool has_valid_main = false;

    for (const auto& function : node.funcs) {
        std::vector<ast::BuiltInType> arguments;
        arguments.reserve(function->formals->formals.size());
        std::transform(function->formals->formals.begin(), function->formals->formals.end(), std::back_inserter(arguments),
            [](const std::shared_ptr<ast::Formal>& formal) {
                return formal->type->type; 
            });
        
        FunctionSymbolEntry function_entry = {function->id->value, offset_stack.top()++, function->return_type->type, arguments};
        for (const auto& function_symbol : function_symbol_table) {
            if (function_entry.name == function_symbol.name) {
                output::errorDef(function->formals->line, function_entry.name);
            }
        }
        scope_printer.emitFunc(function_entry.name, function_entry.return_type, function_entry.arguments);
        function_symbol_table.push_back(function_entry);

        if (function_entry.name == "main" && function_entry.return_type == ast::BuiltInType::VOID && function_entry.arguments.size() == 0) {
            has_valid_main = true;
        }
    }

    if (!has_valid_main) {
        output::errorMainMissing();
    }

    for (auto it = node.funcs.begin(); it != node.funcs.end(); ++it) {
        // correction of 2 because of 'print' and 'printi' functionns 
        current_function = function_symbol_table[std::distance(node.funcs.begin(), it) + 2];
        (*it)->accept(*this);
    }
}

void SemanticAnalayzerVisitor::visit(ast::FuncDecl &node) {
    // Creating a new scope in the symbol_table attribute and adding symbols for the arguments of the function. 
    // Also, creating a new scope offset corresponding to the new scope. 
    scope_printer.beginScope();
    offset_stack.push(-1);
    std::vector<SymbolEntry> symbols_in_scope; 
    for (const auto& formal : node.formals->formals) {
        for (const auto& symbol : symbols_in_scope) {
            if (symbol.name == formal->id->value) output::errorDef(formal->line, formal->id->value);
        }
        for (const auto& function : function_symbol_table) {
            if (function.name == formal->id->value) output::errorDef(formal->line, formal->id->value);
        }
        SymbolEntry entry = {formal->id->value, formal->type->type, offset_stack.top()--};
        scope_printer.emitVar(entry.name, entry.type, entry.offset);
        symbols_in_scope.push_back(entry);
    }
        
    symbol_table.push_back(symbols_in_scope);
    offset_stack.top() = 0;

    node.id->accept(*this);
    node.return_type->accept(*this);
    node.formals->accept(*this);
    node.body->accept(*this);

    // Remove the function scope
    scope_printer.endScope();
    offset_stack.pop();
    symbol_table.pop_back();
}

void SemanticAnalayzerVisitor::visit(ast::If &node) {
    // Creating a new scope in the symbol_table attribute to the 'If' statment.
    scope_printer.beginScope();
    //offset_stack.push(0);
    symbol_table.push_back(std::vector<SymbolEntry>());

    // Check if condition is boolean expression
    ast::BuiltInType condType = getExpressionType(node.condition);

    if (condType != ast::BuiltInType::BOOL) {
        output::errorMismatch(node.condition->line);
    }

    node.condition->accept(*this);

    // Create a new scope if the 'then' code starts a scope
    if (typeid(*node.then) == typeid(ast::Statements)) {
        scope_printer.beginScope();
        //offset_stack.push(0);
        symbol_table.push_back(std::vector<SymbolEntry>());

        node.then->accept(*this);

        scope_printer.endScope();
        //offset_stack.pop();
        symbol_table.pop_back();
    } else {
        node.then->accept(*this);
    }
    
    
    // Remove the 'If' statment scope
    scope_printer.endScope();
    //offset_stack.pop();
    symbol_table.pop_back();

    if (node.otherwise) {
        // Creating a new scope in the symbol_table attribute to the 'Else' statment.
        scope_printer.beginScope();
        //offset_stack.push(0);
        symbol_table.push_back(std::vector<SymbolEntry>());

        // Create a new scope if the 'otherwise' code starts a scope
        if (typeid(*node.otherwise) == typeid(ast::Statements)) {
            scope_printer.beginScope();
            //offset_stack.push(0);
            symbol_table.push_back(std::vector<SymbolEntry>());

            node.otherwise->accept(*this);

            scope_printer.endScope();
            //offset_stack.pop();
            symbol_table.pop_back();
        } else {
            node.otherwise->accept(*this);
        }

        // Remove the 'Else' statment scope
        scope_printer.endScope();
        //offset_stack.pop();
        symbol_table.pop_back();
    }
}

void SemanticAnalayzerVisitor::visit(ast::While &node) {
    // Creating a new scope in the symbol_table attribute to the 'While' statment.
    scope_printer.beginScope();
    offset_stack.push(0);
    symbol_table.push_back(std::vector<SymbolEntry>());

    // Check if condition is boolean expression
    ast::BuiltInType condType = getExpressionType(node.condition);
    if (condType != ast::BuiltInType::BOOL) {
        output::errorMismatch(node.condition->line);
    }
    node.condition->accept(*this);
    number_of_while_inside++;

    // Create a new scope if the 'body' code starts a scope
    if (typeid(*node.body) == typeid(ast::Statements)) {
        scope_printer.beginScope();
        offset_stack.push(0);
        symbol_table.push_back(std::vector<SymbolEntry>());

        node.body->accept(*this);

        scope_printer.endScope();
        offset_stack.pop();
        symbol_table.pop_back();   
    } else {
        node.body->accept(*this);
    }

    number_of_while_inside--;
    // Remove the 'While' statment scope
    scope_printer.endScope();
    offset_stack.pop();
    symbol_table.pop_back();
}

void SemanticAnalayzerVisitor::visit(ast::Statements &node) {
    for (const auto& statement : node.statements) {
        if (std::dynamic_pointer_cast<ast::Statements>(statement)) {
            scope_printer.beginScope();
            symbol_table.push_back(std::vector<SymbolEntry>());
            statement->accept(*this);
            symbol_table.pop_back();
            scope_printer.endScope();
        } else {
            statement->accept(*this);
        }
    }
}

void SemanticAnalayzerVisitor::visit(ast::VarDecl &node) {
    // Check if variable name is occupied
    for (const auto& scope : symbol_table) {
        for (const auto& symbol : scope) {
            if (symbol.name == node.id->value) {
                output::errorDef(node.line, node.id->value);
            }
        }
    } 
    
    for (const auto& function : function_symbol_table) {
        if (function.name == node.id->value) {
            output::errorDef(node.line, node.id->value);
        }
    }

    // Check if init_exp appropriate
    if (node.init_exp) {
        node.init_exp->accept(*this);
        if (typeid(*node.init_exp) == typeid(ast::ID)) {            
            for (const auto& function : function_symbol_table) {
                if (function.name == dynamic_cast<ast::ID*>(node.init_exp.get())->value) {
                    output::errorDefAsFunc(node.line, function.name);
                }
            }
        }
    }
    
    if (node.init_exp) {
        ast::BuiltInType initType = getExpressionType(node.init_exp);
        if (node.type->type == ast::BuiltInType::INT) {
            if (initType != ast::BuiltInType::INT && initType != ast::BuiltInType::BYTE) {
                output::errorMismatch(node.line);
            }
        } else if (node.type->type != initType) {
            output::errorMismatch(node.line);
        }
        node.init_exp->accept(*this);
    }

    SymbolEntry entry = {node.id->value, node.type->type, offset_stack.top()++};
    symbol_table.back().push_back(entry);
    scope_printer.emitVar(entry.name, entry.type, entry.offset);
}

void SemanticAnalayzerVisitor::visit(ast::Assign &node) {
    // Check if variable exists
    bool is_variable_exists = false;
    ast::BuiltInType varType = ast::BuiltInType::VOID;
    for (const auto& scope : symbol_table) {
        for (const auto& symbol : scope) {
            if (symbol.name == node.id->value) {
                is_variable_exists = true;
                varType = symbol.type;
            }
        }
    } 

    if (!is_variable_exists) {
        for (const auto& function : function_symbol_table) {
            if (node.id->value == function.name) {
                output::errorDefAsFunc(node.line, node.id->value);
            }
        }
        output::errorUndef(node.line, node.id->value);
    }
    
    ast::BuiltInType expType = getExpressionType(node.exp);
    if (varType == ast::BuiltInType::INT) {
        if (expType != ast::BuiltInType::INT && expType != ast::BuiltInType::BYTE) {
            output::errorMismatch(node.line);
        }
    } else if (varType != expType) {
        output::errorMismatch(node.line);
    }
    node.exp->accept(*this);
}

void SemanticAnalayzerVisitor::visit(ast::Call &node) {
    // Check if the function exists
    bool is_function_exists = false;
    FunctionSymbolEntry called_function;

    for (const auto& function : function_symbol_table) {
        if (node.func_id->value == function.name) {
            is_function_exists = true;
            called_function = function;
        }
    }

    if (!is_function_exists) {

        for (const auto& scope : symbol_table) {
            for (const auto& variable : scope) {
                if (node.func_id->value == variable.name) {
                    output::errorDefAsVar(node.line, node.func_id->value);
                }
            }
        }

        output::errorUndefFunc(node.line, node.func_id->value);
    }

    // Check if the passed args are apropriate
    bool is_args_match = true;
    if (node.args->exps.size() == called_function.arguments.size()) {
        for (size_t index = 0; index < called_function.arguments.size(); ++index) {
            auto argument = called_function.arguments[index];
            std::shared_ptr<ast::Exp> arg_exp = node.args->exps[index];
            ast::BuiltInType argType = getExpressionType(arg_exp);

            switch (argument) {
                case ast::BuiltInType::BOOL:
                    if (argType != ast::BuiltInType::BOOL) {
                        is_args_match = false;
                    }
                    break;
                case ast::BuiltInType::BYTE:
                    if (argType != ast::BuiltInType::BYTE) {
                        is_args_match = false;
                    }
                    break;
                case ast::BuiltInType::INT:
                    if (argType != ast::BuiltInType::INT && argType != ast::BuiltInType::BYTE) {
                        is_args_match = false;
                    }
                    break;
                case ast::BuiltInType::STRING:
                    if (argType != ast::BuiltInType::STRING) {
                        is_args_match = false;
                    }
                    break;
                default:
                    break;
            }
        } 
    } else {
        is_args_match = false;
    }

    if (!is_args_match) {
        std::vector<std::string> string_args; 
        std::transform(called_function.arguments.begin(), called_function.arguments.end(), std::back_inserter(string_args),
        [](const ast::BuiltInType argument) {
            return output::toString(argument);
        });
        output::errorPrototypeMismatch(node.line, node.func_id->value, string_args);
    }

    node.args->accept(*this);

}


void SemanticAnalayzerVisitor::visit(ast::Break &node) {
    if (number_of_while_inside == 0) {
        output::errorUnexpectedBreak(node.line);
    }
}

void SemanticAnalayzerVisitor::visit(ast::Continue &node) {
    if (number_of_while_inside == 0) {
        output::errorUnexpectedContinue(node.line);
    }
}

void SemanticAnalayzerVisitor::visit(ast::Return &node) {
    ast::BuiltInType type_to_return = current_function.return_type;

    if (!node.exp && type_to_return != ast::BuiltInType::VOID) {
        output::errorMismatch(node.line);
    }

    if (node.exp) {
        node.exp->accept(*this);
    }

    ast::BuiltInType expType = node.exp ? getExpressionType(node.exp) : ast::BuiltInType::VOID;

    switch (type_to_return) {
        case ast::BuiltInType::VOID:
            if (node.exp) {
                output::errorMismatch(node.line);
            }
            break;
        case ast::BuiltInType::BOOL:
            if (expType != ast::BuiltInType::BOOL) {
                output::errorMismatch(node.line);
            }
            break;
        case ast::BuiltInType::BYTE:
            if (expType != ast::BuiltInType::BYTE) {
                output::errorMismatch(node.line);
            }
            break;
        case ast::BuiltInType::INT:
            if (expType != ast::BuiltInType::INT && expType != ast::BuiltInType::BYTE) {
                output::errorMismatch(node.line);
            }
            break;
        case ast::BuiltInType::STRING:
            if (expType != ast::BuiltInType::STRING) {
                output::errorMismatch(node.line);
            }
            break;
    }
}

void SemanticAnalayzerVisitor::visit(ast::Num &node) {}

void SemanticAnalayzerVisitor::visit(ast::NumB &node) {
    if (node.value > 255) {
        output::errorByteTooLarge(node.line, node.value);
    }
}

void SemanticAnalayzerVisitor::visit(ast::String &node) {}

void SemanticAnalayzerVisitor::visit(ast::Bool &node) {}

void SemanticAnalayzerVisitor::visit(ast::ID &node) {
    for (auto it = symbol_table.rbegin(); it != symbol_table.rend(); ++it) {
        for (const auto& entry : *it) {
            if (entry.name == node.value) {
                return;
            }
        }
    }
    for (const auto& func : function_symbol_table) {
        if (func.name == node.value) {
            return;
        }
    }
    output::errorUndef(node.line, node.value);
}

void SemanticAnalayzerVisitor::visit(ast::BinOp &node) {
    node.left->accept(*this);
    node.right->accept(*this);
    ast::BuiltInType t1 = getExpressionType(node.left);
    ast::BuiltInType t2 = getExpressionType(node.right);
    if ((t1 != ast::BuiltInType::INT && t1 != ast::BuiltInType::BYTE) ||
        (t2 != ast::BuiltInType::INT && t2 != ast::BuiltInType::BYTE)) {
        output::errorMismatch(node.line);
    }
}

void SemanticAnalayzerVisitor::visit(ast::RelOp &node) {
    node.left->accept(*this);
    node.right->accept(*this);
    ast::BuiltInType t1 = getExpressionType(node.left);
    ast::BuiltInType t2 = getExpressionType(node.right);
    if ((t1 != ast::BuiltInType::INT && t1 != ast::BuiltInType::BYTE) ||
        (t2 != ast::BuiltInType::INT && t2 != ast::BuiltInType::BYTE)) {
        output::errorMismatch(node.line);
    }
}

void SemanticAnalayzerVisitor::visit(ast::Not &node) {
    node.exp->accept(*this);
    if (getExpressionType(node.exp) != ast::BuiltInType::BOOL) {
        output::errorMismatch(node.line);
    }
}

void SemanticAnalayzerVisitor::visit(ast::And &node) {
    node.left->accept(*this);
    node.right->accept(*this);
    if (getExpressionType(node.left) != ast::BuiltInType::BOOL ||
        getExpressionType(node.right) != ast::BuiltInType::BOOL) {
        output::errorMismatch(node.line);
    }
}

void SemanticAnalayzerVisitor::visit(ast::Or &node) {
    node.left->accept(*this);
    node.right->accept(*this);
    if (getExpressionType(node.left) != ast::BuiltInType::BOOL ||
        getExpressionType(node.right) != ast::BuiltInType::BOOL) {
        output::errorMismatch(node.line);
    }
}

void SemanticAnalayzerVisitor::visit(ast::Type &node) {}

void SemanticAnalayzerVisitor::visit(ast::Cast &node) {
    node.exp->accept(*this);
    ast::BuiltInType t1 = node.target_type->type;
    ast::BuiltInType t2 = getExpressionType(node.exp);
    
    if (!((t1 == ast::BuiltInType::INT && t2 == ast::BuiltInType::BYTE) ||
          (t1 == ast::BuiltInType::BYTE && t2 == ast::BuiltInType::INT) ||
          (t1 == ast::BuiltInType::INT && t2 == ast::BuiltInType::INT) ||
          (t1 == ast::BuiltInType::BYTE && t2 == ast::BuiltInType::BYTE))) {
        output::errorMismatch(node.line);
    }
}

void SemanticAnalayzerVisitor::visit(ast::ExpList &node) {
    for (auto& exp : node.exps) {
        exp->accept(*this);
    }
}
void SemanticAnalayzerVisitor::visit(ast::Formal &node) {}

void SemanticAnalayzerVisitor::visit(ast::Formals &node) {}

ast::BuiltInType SemanticAnalayzerVisitor::getExpressionType(std::shared_ptr<ast::Exp> exp) {
    if (std::dynamic_pointer_cast<ast::Num>(exp)) return ast::BuiltInType::INT;
    if (std::dynamic_pointer_cast<ast::NumB>(exp)) return ast::BuiltInType::BYTE;
    if (std::dynamic_pointer_cast<ast::String>(exp)) return ast::BuiltInType::STRING;
    if (std::dynamic_pointer_cast<ast::Bool>(exp)) return ast::BuiltInType::BOOL;
    if (std::dynamic_pointer_cast<ast::Not>(exp)) return ast::BuiltInType::BOOL;
    if (std::dynamic_pointer_cast<ast::RelOp>(exp)) return ast::BuiltInType::BOOL;
    if (std::dynamic_pointer_cast<ast::And>(exp)) return ast::BuiltInType::BOOL;
    if (std::dynamic_pointer_cast<ast::Or>(exp)) return ast::BuiltInType::BOOL;

    if (auto binOp = std::dynamic_pointer_cast<ast::BinOp>(exp)) {
        ast::BuiltInType left = getExpressionType(binOp->left);
        ast::BuiltInType right = getExpressionType(binOp->right);
        if (left == ast::BuiltInType::BYTE && right == ast::BuiltInType::BYTE)
            return ast::BuiltInType::BYTE;
        return ast::BuiltInType::INT;
    }

    if (auto id = std::dynamic_pointer_cast<ast::ID>(exp)) {
        for (auto it = symbol_table.rbegin(); it != symbol_table.rend(); ++it) {
            for (const auto& entry : *it) {
                if (entry.name == id->value) {
                    return entry.type;
                }
            }
        }
        return ast::BuiltInType::VOID;
    }

    if (auto call = std::dynamic_pointer_cast<ast::Call>(exp)) {
        for (const auto& func : function_symbol_table) {
            if (func.name == call->func_id->value) {
                return func.return_type;
            }
        }
        return ast::BuiltInType::VOID;
    }

    if (auto cast_node = std::dynamic_pointer_cast<ast::Cast>(exp)) {
        return cast_node->target_type->type;
    }

    return ast::BuiltInType::VOID;
}

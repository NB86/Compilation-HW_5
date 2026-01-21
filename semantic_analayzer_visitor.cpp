#include <algorithm>
#include <iostream>
#include <vector>
#include "semantic_analayzer_visitor.hpp"

SemanticAnalayzerVisitor::SemanticAnalayzerVisitor() : number_of_while_inside(0) {}

void SemanticAnalayzerVisitor::visit(ast::Funcs &node) {
    offset_stack.push(0);

    // Register library functions
    FunctionSymbolEntry print_entry = {"print", 0, ast::BuiltInType::VOID, {ast::BuiltInType::STRING}};
    FunctionSymbolEntry printi_entry = {"printi", 0, ast::BuiltInType::VOID, {ast::BuiltInType::INT}};
    function_symbol_table.push_back(print_entry);
    function_symbol_table.push_back(printi_entry);

    bool has_valid_main = false;

    for (const auto& function : node.funcs) {
        std::vector<ast::BuiltInType> arguments;
        if (function->formals) {
            arguments.reserve(function->formals->formals.size());
            std::transform(function->formals->formals.begin(), function->formals->formals.end(), std::back_inserter(arguments),
                [](const std::shared_ptr<ast::Formal>& formal) {
                    return formal->type->type; 
                });
        }
        
        FunctionSymbolEntry function_entry = {function->id->value, 0, function->return_type->type, arguments};
        for (const auto& function_symbol : function_symbol_table) {
            if (function_entry.name == function_symbol.name) {
                output::errorDef(function->id->line, function_entry.name);
            }
        }
        function_symbol_table.push_back(function_entry);

        if (function_entry.name == "main" && function_entry.return_type == ast::BuiltInType::VOID && function_entry.arguments.empty()) {
            has_valid_main = true;
        }
    }

    if (!has_valid_main) {
        output::errorMainMissing();
    }

    for (auto it = node.funcs.begin(); it != node.funcs.end(); ++it) {
        // Offset +2 to skip print/printi
        current_function = function_symbol_table[std::distance(node.funcs.begin(), it) + 2];
        (*it)->accept(*this);
    }
}

void SemanticAnalayzerVisitor::visit(ast::FuncDecl &node) {
    offset_stack.push(-1); // Function arguments have negative offsets? Depends on HW3 spec.
    std::vector<SymbolEntry> symbols_in_scope; 
    
    if (node.formals) {
        for (const auto& formal : node.formals->formals) {
            for (const auto& symbol : symbols_in_scope) {
                if (symbol.name == formal->id->value) output::errorDef(formal->line, formal->id->value);
            }
            for (const auto& function : function_symbol_table) {
                if (function.name == formal->id->value) output::errorDef(formal->line, formal->id->value);
            }
            SymbolEntry entry = {formal->id->value, formal->type->type, offset_stack.top()--};
            symbols_in_scope.push_back(entry);
        }
    }
        
    symbol_table.push_back(symbols_in_scope);
    // Reset offset for local variables
    offset_stack.push(0); 

    // node.id->accept(*this); // ID visit not strictly needed here unless logic requires it
    // node.return_type->accept(*this);
    if(node.formals) node.formals->accept(*this);
    node.body->accept(*this);

    offset_stack.pop(); // Pop local var offset
    offset_stack.pop(); // Pop args offset (if pushed)
    symbol_table.pop_back();
}

void SemanticAnalayzerVisitor::visit(ast::If &node) {
    symbol_table.push_back(std::vector<SymbolEntry>());
    offset_stack.push(offset_stack.top()); // Inherit offset

    ast::BuiltInType condType = getExpressionType(node.condition);
    if (condType != ast::BuiltInType::BOOL) {
        output::errorMismatch(node.condition->line);
    }
    node.condition->accept(*this);

    node.then->accept(*this);

    offset_stack.pop();
    symbol_table.pop_back();

    if (node.otherwise) {
        symbol_table.push_back(std::vector<SymbolEntry>());
        offset_stack.push(offset_stack.top());

        node.otherwise->accept(*this);

        offset_stack.pop();
        symbol_table.pop_back();
    }
}

void SemanticAnalayzerVisitor::visit(ast::While &node) {
    symbol_table.push_back(std::vector<SymbolEntry>());
    offset_stack.push(offset_stack.top());

    ast::BuiltInType condType = getExpressionType(node.condition);
    if (condType != ast::BuiltInType::BOOL) {
        output::errorMismatch(node.condition->line);
    }
    node.condition->accept(*this);
    
    number_of_while_inside++;
    node.body->accept(*this);
    number_of_while_inside--;

    offset_stack.pop();
    symbol_table.pop_back();
}

void SemanticAnalayzerVisitor::visit(ast::Statements &node) {
    for (const auto& statement : node.statements) {
        // Blocks inside statements usually create new scopes
        if (std::dynamic_pointer_cast<ast::Statements>(statement)) {
            symbol_table.push_back(std::vector<SymbolEntry>());
            offset_stack.push(offset_stack.top());
            
            statement->accept(*this);
            
            offset_stack.pop();
            symbol_table.pop_back();
        } else {
            statement->accept(*this);
        }
    }
}

void SemanticAnalayzerVisitor::visit(ast::VarDecl &node) {
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

    if (node.init_exp) {
        node.init_exp->accept(*this);
        if (auto id_exp = std::dynamic_pointer_cast<ast::ID>(node.init_exp)) {            
            for (const auto& function : function_symbol_table) {
                if (function.name == id_exp->value) {
                    output::errorDefAsFunc(node.line, function.name);
                }
            }
        }
    
        ast::BuiltInType initType = getExpressionType(node.init_exp);
        if (node.type->type == ast::BuiltInType::INT) {
            if (initType != ast::BuiltInType::INT && initType != ast::BuiltInType::BYTE) {
                output::errorMismatch(node.line);
            }
        } else if (node.type->type != initType) {
            output::errorMismatch(node.line);
        }
    }

    int current_offset = offset_stack.top();
    offset_stack.pop();
    offset_stack.push(current_offset + 1);

    SymbolEntry entry = {node.id->value, node.type->type, current_offset};
    symbol_table.back().push_back(entry);
}

void SemanticAnalayzerVisitor::visit(ast::Assign &node) {
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

    size_t args_size = node.args ? node.args->exps.size() : 0;
    if (args_size != called_function.arguments.size()) {
        std::vector<std::string> string_args; 
        for(auto t : called_function.arguments) string_args.push_back(output::toString(t));
        output::errorPrototypeMismatch(node.line, node.func_id->value, string_args);
    }

    if (node.args) {
        for (size_t index = 0; index < called_function.arguments.size(); ++index) {
            auto argument = called_function.arguments[index];
            std::shared_ptr<ast::Exp> arg_exp = node.args->exps[index];
            ast::BuiltInType argType = getExpressionType(arg_exp);
            
            bool match = false;
            if (argument == argType) match = true;
            if (argument == ast::BuiltInType::INT && argType == ast::BuiltInType::BYTE) match = true;
            
            if (!match) {
                 std::vector<std::string> string_args; 
                 for(auto t : called_function.arguments) string_args.push_back(output::toString(t));
                 output::errorPrototypeMismatch(node.line, node.func_id->value, string_args);
            }
        }
        node.args->accept(*this);
    }
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

    ast::BuiltInType expType = node.exp ? getExpressionType(node.exp) : ast::BuiltInType::VOID;

    if (type_to_return == ast::BuiltInType::VOID && node.exp) {
        output::errorMismatch(node.line);
    }
    
    if (type_to_return == ast::BuiltInType::INT && (expType != ast::BuiltInType::INT && expType != ast::BuiltInType::BYTE)) {
        output::errorMismatch(node.line);
    }
    
    if (type_to_return != ast::BuiltInType::VOID && type_to_return != ast::BuiltInType::INT && type_to_return != expType) {
        output::errorMismatch(node.line);
    }

    if (node.exp) node.exp->accept(*this);
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
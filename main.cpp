#include <iostream>
#include "output.hpp"
#include "nodes.hpp"
#include "semantic_analayzer_visitor.hpp"
#include "code_generator.hpp"

extern int yyparse();
extern std::shared_ptr<ast::Node> program;

int main() {
    yyparse();
    
    if (!program) {
        std::cerr << "Error: Failed to parse the program (AST root is null)." << std::endl;
        return 1;
    }

    // Phase 1: Semantic Analysis
    // Ensures type safety and validity before code generation.
    SemanticAnalayzerVisitor semantic_visitor; 
    program->accept(semantic_visitor);

    // Phase 2: Code Generation
    // Emits LLVM IR to the code buffer.
    output::CodeBuffer buffer;
    CodeGenerator code_gen_visitor(buffer);
    program->accept(code_gen_visitor);

    // Output the generated code to stdout
    std::cout << buffer;
    
    return 0;
}
%{

#include "nodes.hpp"
#include "output.hpp"

// bison declarations
extern int yylineno;
extern int yylex();

void yyerror(const char*);

// root of the AST, set by the parser and used by other parts of the compiler
std::shared_ptr<ast::Node> program;

using namespace std;

%}

%token INT BYTE BOOL VOID
%token TRUE FALSE
%token IF WHILE BREAK CONTINUE

%token ID
%token NUM NUM_B
%token STRING
%token RETURN

%token SC COMMA ASSIGN

//right is first shift then reduce and left is first reduce and then shift
%right ASSIGN
%right IF
%left OR
%left AND
//adding difference between the different RELOP 
%left RELOP
%left LEFTOP
%left RIGHTOP
%right NOT
%left LPAREN RPAREN LBRACE RBRACE LBRACK RBRACK

//to handle the dangling-else problem
%right ELSE

%%

// While reducing the start variable, set the root of the AST
Program:  Funcs { program = $1; }
;

Funcs: FuncDecl Funcs{$$= std::dynamic_pointer_cast<ast::Funcs>($2);
                        std::dynamic_pointer_cast<ast::Funcs>($$)->push_front(std::dynamic_pointer_cast<ast::FuncDecl>($1));}
        | {$$ = std::make_shared<ast::Funcs>();}

FuncDecl: RetType ID LPAREN Formals RPAREN LBRACE Statements RBRACE{$$=std::make_shared<ast::FuncDecl>(
    std::dynamic_pointer_cast<ast::ID>($2), 
    std::dynamic_pointer_cast<ast::Type>($1),
    std::dynamic_pointer_cast<ast::Formals>($4),
    std::dynamic_pointer_cast<ast::Statements>($7)
    );};

RetType: Type {$$ = $1;}
    | VOID { $$ = std::make_shared<ast::Type>(ast::BuiltInType::VOID); }

Formals:  FormalsList {$$ = $1;}
            | {$$ = std::make_shared<ast::Formals>();}

FormalsList: FormalDecl {$$ = std::make_shared<ast::Formals>(std::dynamic_pointer_cast<ast::Formal>($1));}
            | FormalsList COMMA FormalDecl {auto formals = std::dynamic_pointer_cast<ast::Formals>($1);
                                            formals->push_back(std::dynamic_pointer_cast<ast::Formal>($3));
                                            $$ = formals;}

FormalDecl: Type ID {$$ = std::make_shared<ast::Formal>(std::dynamic_pointer_cast<ast::ID>($2), std::dynamic_pointer_cast<ast::Type>($1));}
 
Statements: Statements Statement {
                auto statements = std::dynamic_pointer_cast<ast::Statements>($1);
                statements->push_back(std::dynamic_pointer_cast<ast::Statement>($2));
                $$ = statements;
                if(!$1)
                {
                    printf("$s1");
                }
                if(!$2)
                {
                    printf("$s2");
                }
            }
            | Statement {$$=std::make_shared<ast::Statements>(std::dynamic_pointer_cast<ast::Statement>($1));}


Statement: LBRACE Statements RBRACE {$$=std::dynamic_pointer_cast<ast::Statement>($2);}
    | Type ID SC {$$=std::make_shared<ast::VarDecl>(std::dynamic_pointer_cast<ast::ID>($2), std::dynamic_pointer_cast<ast::Type>($1));}
    | Type ID ASSIGN Exp SC {$$=std::make_shared<ast::VarDecl>(std::dynamic_pointer_cast<ast::ID>($2), std::dynamic_pointer_cast<ast::Type>($1),std::dynamic_pointer_cast<ast::Exp>($4));}
    | ID ASSIGN Exp SC {$$=std::make_shared<ast::Assign>(std::dynamic_pointer_cast<ast::ID>($1), std::dynamic_pointer_cast<ast::Exp>($3));}
    | Call SC {$$ = $1;}
    | RETURN SC {$$ = std::make_shared<ast::Return>();}
    | RETURN Exp SC {$$ = std::make_shared<ast::Return>(std::dynamic_pointer_cast<ast::Exp>($2));}
    | IF LPAREN Exp RPAREN Statement {$$ = std::make_shared<ast::If>(std::dynamic_pointer_cast<ast::Exp>($3), std::dynamic_pointer_cast<ast::Statement>($5));}
    | IF LPAREN Exp RPAREN Statement ELSE Statement {$$ = std::make_shared<ast::If>(std::dynamic_pointer_cast<ast::Exp>($3), 
                                            std::dynamic_pointer_cast<ast::Statement>($5), std::dynamic_pointer_cast<ast::Statement>($7));}
    | WHILE LPAREN Exp RPAREN Statement {$$ = std::make_shared<ast::While>(std::dynamic_pointer_cast<ast::Exp>($3),
                                         std::dynamic_pointer_cast<ast::Statement>($5));}
    | BREAK SC                  { $$ = std::make_shared<ast::Break>(); }
    | CONTINUE SC               { $$ = std::make_shared<ast::Continue>(); }
    

Call: ID LPAREN ExpList RPAREN  {$$ = std::make_shared<ast::Call>(std::dynamic_pointer_cast<ast::ID>($1), std::dynamic_pointer_cast<ast::ExpList>($3));}
    | ID LPAREN RPAREN          { $$ = std::make_shared<ast::Call>(std::dynamic_pointer_cast<ast::ID>($1));}

ExpList: Exp                     {$$= std::make_shared<ast::ExpList>(std::dynamic_pointer_cast<ast::Exp>($1)); }
    | Exp COMMA ExpList          { auto explist = std::dynamic_pointer_cast<ast::ExpList>($3); explist->push_front(std::dynamic_pointer_cast<ast::Exp>($1)); $$ = explist;}

Type: INT   { $$ = std::make_shared<ast::Type>(ast::BuiltInType::INT); }
    | BYTE  { $$ = std::make_shared<ast::Type>(ast::BuiltInType::BYTE); }
    | BOOL  { $$ = std::make_shared<ast::Type>(ast::BuiltInType::BOOL); }

Exp: LPAREN Exp RPAREN          { $$ = $2; }
    | Exp LEFTOP Exp  { $$ = std::make_shared<ast::BinOp>(std::dynamic_pointer_cast<ast::Exp>($1), std::dynamic_pointer_cast<ast::Exp>($3), std::dynamic_pointer_cast<ast::BinOp>($2)->op); }
    | Exp RIGHTOP Exp { $$ = std::make_shared<ast::BinOp>(std::dynamic_pointer_cast<ast::Exp>($1), std::dynamic_pointer_cast<ast::Exp>($3), std::dynamic_pointer_cast<ast::BinOp>($2)->op); }
    | ID                         { $$ = $1;}
    | Call                       { $$ = $1; }
    | NUM                        { $$ = $1; }
    | NUM_B                      { $$ = $1; }
    | STRING                     { $$ = $1; }
    | TRUE                       { $$ = std::make_shared<ast::Bool>(true); }
    | FALSE                      { $$ = std::make_shared<ast::Bool>(false); }
    | NOT Exp                    { $$ = std::make_shared<ast::Not>(std::dynamic_pointer_cast<ast::Exp>($2)); }
    | Exp AND Exp                { $$ = std::make_shared<ast::And>(std::dynamic_pointer_cast<ast::Exp>($1), std::dynamic_pointer_cast<ast::Exp>($3)); }
    | Exp OR Exp                 { $$ = std::make_shared<ast::Or>(std::dynamic_pointer_cast<ast::Exp>($1), std::dynamic_pointer_cast<ast::Exp>($3)); }
    | Exp RELOP Exp              { $$ = std::make_shared<ast::RelOp>(std::dynamic_pointer_cast<ast::Exp>($1), std::dynamic_pointer_cast<ast::Exp>($3), std::dynamic_pointer_cast<ast::RelOp>($2)->op); }
    | LPAREN Type RPAREN Exp     { $$ = std::make_shared<ast::Cast>(std::dynamic_pointer_cast<ast::Exp>($4), std::dynamic_pointer_cast<ast::Type>($2)); }



%%

void yyerror(const char * message){
	output::errorSyn(yylineno);
	exit(0);
}
%{
    /* Declarations section */
    #include "nodes.hpp"
    #include <stdio.h>
    #include <iostream>
    #include "parser.tab.h"
    #include "output.hpp"
    #include <unordered_map>
    #include <stdexcept>
    #include <string>

    ast::RelOpType mapRelOpType(const std::string &op);
    ast::BinOpType mapBinOpType(const std::string &op);
%}

%option yylineno
%option noyywrap

/* --- 1. DEFINITIONS SECTION --- */
relop      		(<)|(>)|(>=)|(<=)|(==)|(!=)
leftop      	(\+)|(-)
rightop         (\*)|(\/)
digit   		([0-9])
letter  		([a-zA-Z])
whitespace		([\t\n\r])*|(" ")*
number			(0)|[1-9]{digit}*

safeChar         [\x20-\x21\x23-\x5B\x5D-\x7E]
validEscape        (\\[nrt"\\0])|(\\x([2-6][0-9A-Fa-f]|7[0-9A-Ea-e]|0[9AaDd]))
stringChar         {safeChar}|{validEscape}|("	")


/* --- 2. RULES SECTION --- */

%%
void 						return VOID;
int							return INT;
byte						return BYTE;
bool						return BOOL;
and							return AND;
or							return OR;
not							return NOT;
true						return TRUE;
false						return FALSE;
return 						return RETURN;
if							return IF;
else						return ELSE;
while						return WHILE;
break						return BREAK;
continue					return CONTINUE;
;							return SC;
,							return COMMA;
\)           				return RPAREN;
\}           				return RBRACE;
=							return ASSIGN;
\{           				return LBRACE;
\(           				return LPAREN;
{relop}						{try { yylval = std::make_shared<ast::RelOp>(nullptr, nullptr, mapRelOpType(yytext)); 
                            }catch (const std::exception &e) {
                                output::errorLex(yylineno);
                                exit(1);
                            }return RELOP;}
{leftop}					{try { yylval = std::make_shared<ast::BinOp>(nullptr, nullptr, mapBinOpType(yytext)); 
                            } catch (const std::exception &e) {
                                output::errorLex(yylineno);
                                exit(1);
                            }
    return LEFTOP;}
{rightop}    {try {
                yylval = std::make_shared<ast::BinOp>(nullptr, nullptr, mapBinOpType(yytext)); 
            } catch (const std::exception &e) {
                output::errorLex(yylineno);
                exit(1);
            }
            return RIGHTOP;}

{letter}({digit}|{letter})*	{yylval= std::make_shared<ast::ID>(yytext); return ID;}

{number}          	        {yylval= std::make_shared<ast::Num>(yytext); return NUM;}
{number}b					{yylval = std::make_shared<ast::NumB>(yytext); return NUM_B;}
\"({stringChar})*\"   { yylval= std::make_shared<ast::String>(yytext);return STRING; }
{whitespace}    			/* skip whitespace and new lines */ ;
"//".*\n     ;
.   {output::errorLex(yylineno);}/* catch-all for illegal characters if needed */
%%

ast::RelOpType mapRelOpType(const std::string &op) {
    if (op == "==") return ast::EQ;
    if (op == "!=") return ast::NE;
    if (op == "<")  return ast::LT;
    if (op == ">")  return ast::GT;
    if (op == "<=") return ast::LE;
    if (op == ">=") return ast::GE;

    throw std::invalid_argument("Invalid relational operator: " + op);
}

ast::BinOpType mapBinOpType(const std::string &op) {
    if (op == "+") return ast::ADD;
    if (op == "-") return ast::SUB;
    if (op == "*") return ast::MUL;
    if (op == "/") return ast::DIV;

    throw std::invalid_argument("Invalid binary operator: " + op);
}

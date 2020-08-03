// C declarations
%{
#define YYDEBUG 1
#include <string>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <vector>

#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <typeinfo>

#include "include/ast_nodes.h"
#include "include/ast_nodes_p4.h"
#include "include/ast_nodes_p4r.h"
#include "include/compile.h"
#include "include/find_nodes.h"
#include "include/helper.h"

// Only support tofino target
int with_tofino=1;
int done_init=0;

extern int yylex();
extern int yyparse();
extern FILE* yyin;
extern int yylineno;
extern char* yytext;
void yyerror(const char* s);

using namespace std;

char* in_fn = NULL;
char* out_fn_base = NULL;
std::string p4_out_fn, c_out_fn;
FILE* in_file;

// From: https://stackoverflow.com/questions/865668/how-to-parse-command-line-arguments-in-c
char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return *itr;
    }
    return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}

void parseArgs(int argc, char* argv[]){ 
    in_fn = getCmdOption(argv, argv+argc, "-i");
    out_fn_base = getCmdOption(argv, argv+argc, "-o");
    if ((in_fn == NULL) || (out_fn_base == NULL)){
        cout << "expected arguments: "
             << argv[0]
             << " -i <input P4R filename> -o <output filename base> "
             << endl;
        exit(0);
    }

    in_file = fopen(in_fn, "r");
    if (in_file == 0) {
        PANIC("Input P4R file not found");
    }
    
    p4_out_fn = string(string(out_fn_base) + string("_mantis.p4"));
    c_out_fn = string(string(out_fn_base) + string("_mantis.c"));
}

std::vector<AstNode*> node_array;
AstNode* root;
%}

%union {
    AstNode* aval;
    std::string* pval;
    char* sval;
}

// One could extend tofino-specifc P4 syntax to support other variants by branching
// bmv2 target currently not supported
%token START_TOFINO START_BMV2

// Parsed tokens
%token L_BRACE "{"
%token R_BRACE "}"
%token L_PAREN "("
%token R_PAREN ")"
%token SEMICOLON ";"
%token COLON ":"
%token COMMA ","
%token PERIOD "."
%token DOLLAR "$"
%token L_BRACKET "["
%token R_BRACKET "]"
%token SLASH "/"

// Parsed keywords
%token P4R_MALLEABLE
%token TABLE
%token VALUE
%token FIELD
%token WIDTH
%token INIT
%token ALTS
%token READS
%token EXACT
%token TERNARY
%token ACTIONS
%token HEADER_TYPE
%token HEADER
%token METADATA
%token FIELDS
%token ACTION

%token BLACKBOX

%token P4R_INIT_BLOCK

%token P4R_REACTION
%token REGISTER
%token REACTION_ARG_REG
%token REACTION_ARG_ING
%token REACTION_ARG_EGR

// Parsed words
%token <sval> INCLUDE
%token <sval> PRAGMA
%token <sval> IDENTIFIER
%token <sval> STRING
%token <sval> INTEGER


%type <aval> rootTofino;
%type <aval> rootBmv2;

// Nonterminals
%type <aval> inputTofino
%type <aval> inputBmv2
%type <aval> p4ExprTofino
%type <aval> p4ExprBmv2
%type <aval> p4rExpr
%type <aval> include
%type <aval> nameList
%type <aval> keyWord
%type <aval> body
%type <aval> opts
%type <aval> p4rMalleable
%type <aval> varWidth
%type <aval> varValueInit
%type <aval> varFieldInit
%type <aval> varAlts
%type <aval> fieldList
%type <aval> field
%type <aval> varRef
%type <aval> bodyWord
%type <aval> p4rInitBlock
%type <aval> p4rReaction
%type <aval> reactionArgs
%type <aval> reactionArg

%type <aval> registerDecl

%type <aval> tableDecl
%type <aval> tableReads
%type <aval> tableReadStmt
%type <aval> tableReadStmts
%type <aval> tableActions
%type <aval> tableActionStmt
%type <aval> tableActionStmts

%type <aval> headerTypeDeclaration
%type <aval> headerDecBody
%type <aval> headerInstance
%type <aval> metadataInstance
%type <aval> fieldDec
%type <aval> fieldDecList

%type <aval> actionFunctionDeclaration
%type <aval> actionParamList
%type <aval> actionParam
%type <aval> actionStatements
%type <aval> actionStatement
%type <aval> argList
%type <aval> arg

// Leaves
%type <aval> name
%type <aval> specialChar
%type <aval> integer

%type <aval> includes

// Define start state
%start root


%%
/*=====================================
=               GRAMMAR               =
=====================================*/

root:
    START_TOFINO rootTofino
    | START_BMV2 rootBmv2
    ;


rootTofino :
    inputTofino {
        root = $1;
    }
;

rootBmv2 :
    inputBmv2 {
        root = $1;
    }   

inputTofino :
    /* epsilon */ {
        $$=NULL;
    }
    | inputTofino include {
        AstNode* rv = new InputNode($1, $2);
        node_array.push_back(rv);
        $$=rv;
    }
    | inputTofino p4rExpr {
        AstNode* rv = new InputNode($1, $2);
        node_array.push_back(rv);
        $$=rv;
        PRINT_VERBOSE("----- parsed P4R Expr ------ \n");
        PRINT_VERBOSE("%s\n", $2 -> toString().c_str());
        PRINT_VERBOSE("---------------------------\n");        
    }
    | inputTofino p4ExprTofino  {
        AstNode* rv = new InputNode($1, $2);
        node_array.push_back(rv);
        $$=rv;
        PRINT_VERBOSE("----- parsed P4 Expr ------ \n");
        PRINT_VERBOSE("%s\n", $2 -> toString().c_str());
        PRINT_VERBOSE("---------------------------\n");
    }
;

inputBmv2 :
    /* epsilon */ {
        $$=NULL;
    }
    | inputBmv2 include {
        AstNode* rv = new InputNode($1, $2);
        node_array.push_back(rv);
        $$=rv;
    }
    | inputBmv2 p4rExpr {
        AstNode* rv = new InputNode($1, $2);
        node_array.push_back(rv);
        $$=rv;
        PRINT_VERBOSE("- Parsed REACTIVE P4 Expr -- \n");
        PRINT_VERBOSE("%s\n", $2 -> toString().c_str());
        PRINT_VERBOSE("---------------------------\n");
    }
    | inputBmv2 p4ExprBmv2  {
        AstNode* rv = new InputNode($1, $2);
        node_array.push_back(rv);
        $$=rv;
        PRINT_VERBOSE("----- parsed P4 Expr ------ \n");
        PRINT_VERBOSE("%s\n", $2 -> toString().c_str());
        PRINT_VERBOSE("---------------------------\n");
    }
;

// Include statements
include :
    INCLUDE {
        string* strVal = new string(string($1));
        AstNode* rv = new IncludeNode(strVal, IncludeNode::P4);
        node_array.push_back(rv);
        $$=rv;
        free($1);
    }
;


/*=====================================
=            P4 expressions           =
=====================================*/

registerDecl :
    REGISTER name "{" body "}" {
        AstNode* rv = new P4RegisterNode($2, $4);
        node_array.push_back(rv);
        $$=rv;    
    }
;

tableDecl : 
    TABLE name "{" tableReads tableActions body "}" {
        auto rv = new TableNode($2, $4, $5,
                                $6->toString(), "");
        node_array.push_back(rv);
        $$=rv;
    }
    | TABLE name "{" tableActions body "}" {
        // Reads are optional
        auto rv = new TableNode($2, NULL, $4,
                                $5->toString(), "");
        node_array.push_back(rv);
        $$=rv;
    }
    | PRAGMA TABLE name "{" tableReads tableActions body "}" {
        auto rv = new TableNode($3, $5, $6,
                                $7->toString(), $1);
        node_array.push_back(rv);
        $$=rv;
    }
    | PRAGMA TABLE name "{" tableActions body "}" {
        auto rv = new TableNode($3, NULL, $5,
                                $6->toString(), $1);
        node_array.push_back(rv);
        $$=rv;
    }    
;

tableReads :
    READS "{" tableReadStmts "}" {
        $$=$3;
    }
;

tableReadStmts :
    /* empty */ {
        $$=new TableReadStmtsNode();
    }
    | tableReadStmts tableReadStmt {
        TableReadStmtsNode* rv = dynamic_cast<TableReadStmtsNode*>($1);
        TableReadStmtNode* trs = dynamic_cast<TableReadStmtNode*>($2);
        rv->push_back(trs);
        node_array.push_back(rv);
        $$=rv;        
    }
;

tableReadStmt :
    field ":" EXACT ";" {
        AstNode* rv = new TableReadStmtNode(TableReadStmtNode::EXACT, $1);
        node_array.push_back(rv);
        $$=rv;
    }
    | field ":" TERNARY ";" {
        AstNode* rv = new TableReadStmtNode(TableReadStmtNode::TERNARY, $1);
        node_array.push_back(rv);
        $$=rv;
    }
    | varRef ":" EXACT ";" {
        AstNode* rv = new TableReadStmtNode(TableReadStmtNode::EXACT, $1);
        node_array.push_back(rv);
        $$=rv;
    }
    | varRef ":" TERNARY ";" {
        AstNode* rv = new TableReadStmtNode(TableReadStmtNode::TERNARY, $1);
        node_array.push_back(rv);
        $$=rv;
    }
;

tableActions :
    ACTIONS "{" tableActionStmts "}" {
        $$=$3;
    }
;

tableActionStmts :
    /* empty */ {
        $$=new TableActionStmtsNode();
    }
    | tableActionStmts tableActionStmt {
        TableActionStmtsNode* rv = dynamic_cast<TableActionStmtsNode*>($1);
        TableActionStmtNode* tas = dynamic_cast<TableActionStmtNode*>($2);
        rv->push_back(tas);
        node_array.push_back(rv);
        $$=rv;        
    }
;

tableActionStmt :
    name ";" {
        AstNode* rv = new TableActionStmtNode($1);
        node_array.push_back(rv);
        $$=rv;
    }
;

headerTypeDeclaration :
    HEADER_TYPE name "{" headerDecBody body "}" {
        AstNode* rv = new HeaderTypeDeclarationNode($2, $4, $5);
        node_array.push_back(rv);
        $$=rv;
    }
;

headerDecBody :
    FIELDS "{" fieldDecList fieldDec "}" {
        FieldDecsNode* rv = dynamic_cast<FieldDecsNode*>($3);
        FieldDecNode* fd = dynamic_cast<FieldDecNode*>($4);
        rv->push_back(fd);
        $$=rv;
    }
;

fieldDec :
    name ":" integer ";" {
        AstNode* rv = new FieldDecNode($1, $3);
        $$=rv;
    }
;

fieldDecList :
    /* empty */ {
        $$=new FieldDecsNode();
    }
    | fieldDecList fieldDec {
        FieldDecsNode* rv = dynamic_cast<FieldDecsNode*>($1);
        FieldDecNode* fd = dynamic_cast<FieldDecNode*>($2);
        rv->push_back(fd);
        $$=rv;
    }
;

headerInstance :
    HEADER name name ";" {
        HeaderInstanceNode* rv = new HeaderInstanceNode($2, $3);
        node_array.push_back(rv);
        $$=rv;
    }
;

metadataInstance :
    METADATA name name ";" {
        MetadataInstanceNode* rv = new MetadataInstanceNode($2, $3);
        node_array.push_back(rv);
        $$=rv;
    }
;    

actionFunctionDeclaration :
    ACTION name "(" actionParamList ")" "{" actionStatements "}" {
        ActionNode* rv = new ActionNode($2, $4, $7);
        node_array.push_back(rv);
        $$=rv;
    }
;

actionParamList :
    /* empty */ {
        $$=new ActionParamsNode();
    }
    | actionParam {
        ActionParamsNode* rv = new ActionParamsNode();
        ActionParamNode* ap = dynamic_cast<ActionParamNode*>($1);
        rv->push_back(ap);
        $$=rv;
    }
    | actionParamList "," actionParam {
        ActionParamsNode* rv = dynamic_cast<ActionParamsNode*>($1);
        ActionParamNode* ap = dynamic_cast<ActionParamNode*>($3);
        rv->push_back(ap);
        $$=rv;
    }
;

actionParam :
    field {
        AstNode* rv = new ActionParamNode($1);
        node_array.push_back(rv);
        $$=rv;
    }
    | varRef {
        AstNode* rv = new ActionParamNode($1);
        node_array.push_back(rv);
        $$=rv;
    }
    | name {
        AstNode* rv = new ActionParamNode($1);
        node_array.push_back(rv);
        $$=rv;
    }
;

actionStatements :
    /* empty */ {
        AstNode* rv = new ActionStmtsNode();
        node_array.push_back(rv);
        $$=rv;
    }
    | actionStatements actionStatement {
        ActionStmtsNode* rv = dynamic_cast<ActionStmtsNode*>($1);
        ActionStmtNode* as = dynamic_cast<ActionStmtNode*>($2);
        rv->push_back(as);
        $$=rv;
    }
;

actionStatement :
    name "(" argList ")" ";" {
        ActionStmtNode* rv = new ActionStmtNode($1, $3, ActionStmtNode::NAME_ARGLIST, NULL, NULL);
        node_array.push_back(rv);
        $$=rv;
    }
    /* e.g., bi.execute_stateful_alu(eg_intr_md.egress_port) or index */
    | name "." name "(" argList ")" ";" {
        ActionStmtNode* rv = new ActionStmtNode($1, $5, ActionStmtNode::PROG_EXEC, $3, NULL);
        node_array.push_back(rv);
        $$=rv;    
    }
;

argList :
    /* empty */ {
        ArgsNode* rv = new ArgsNode();
        node_array.push_back(rv);
        $$=rv;
    }
    | arg {
        ArgsNode* rv = new ArgsNode();
        BodyWordNode* bw = dynamic_cast<BodyWordNode*>($1);
        rv->push_back(bw);
        node_array.push_back(rv);
        $$=rv;
    }
    | argList "," arg {
        ArgsNode* rv = dynamic_cast<ArgsNode*>($1);
        BodyWordNode* bw = dynamic_cast<BodyWordNode*>($3);
        rv->push_back(bw);
        $$=rv;
    }
;

arg :
    varRef {
        AstNode* rv = new BodyWordNode(BodyWordNode::VARREF, $1);
        node_array.push_back(rv);
        $$=rv;
    }
    | name {
        AstNode* rv = new BodyWordNode(BodyWordNode::NAME, $1);
        node_array.push_back(rv);
        $$=rv;
    }
    | field {
        AstNode* rv = new BodyWordNode(BodyWordNode::FIELD, $1);
        node_array.push_back(rv);
        $$=rv;
    }
    | integer {
        AstNode* rv = new BodyWordNode(BodyWordNode::INTEGER, $1);
        node_array.push_back(rv);
        $$=rv;
    }
;

/* 
General form of P4-14 declarations: 
keyWord name [ name ] ["(" opts ")"] [ [";"] | "{" body "}"]
*/

p4ExprTofino :
    // Parsed statements.
    tableDecl {
        node_array.push_back($1);
        $$=$1;
    }
    | headerTypeDeclaration {
        node_array.push_back($1);
        $$=$1;
    }
    | headerInstance {
        node_array.push_back($1);
        $$=$1;
    } 
    | metadataInstance {
        node_array.push_back($1);
        $$=$1;
    }
    | actionFunctionDeclaration {
        node_array.push_back($1);
        $$=$1;
    } 
    | registerDecl {
        // necessary for mirroring measurement register
        node_array.push_back($1);
        $$=$1;
    }
    // Generic statements.
    | keyWord name ";" {
        AstNode* rv = new P4ExprNode($1, $2, NULL, NULL, NULL);
        node_array.push_back(rv);
        $$=rv;
    }
    | keyWord name "{" body "}" {
        AstNode* rv = new P4ExprNode($1, $2, NULL, NULL, $4);
        node_array.push_back(rv);
        $$=rv;
    }
    /* name2, no opts */
    | keyWord name name ";" {
        AstNode* rv = new P4ExprNode($1, $2, $3, NULL, NULL);
        node_array.push_back(rv);
        $$=rv;
    }
    | keyWord name name "{" body "}" {
        AstNode* rv = new P4ExprNode($1, $2, $3, NULL, $5);
        node_array.push_back(rv);
        $$=rv;
    }
    /* To parse e.g., "calculated_field ipv4.hdrChecksum" */
    | keyWord name "." name "{" body "}" {    
        AstNode* rv = new P4ExprNode($1, $2, $4, NULL, $6);
        node_array.push_back(rv);
        $$=rv;
    }
    /* no name2, opts */
    | keyWord name "(" opts ")" ";" {
        AstNode* rv = new P4ExprNode($1, $2, NULL, $4, NULL);
        node_array.push_back(rv);
        $$=rv;        
    }
    | keyWord name "(" opts ")" "{" body "}" {
        AstNode* rv = new P4ExprNode($1, $2, NULL, $4, $7);
        node_array.push_back(rv);
        $$=rv;                
    }        
    /* name2, opts */
    | keyWord name name "(" opts ")" ";" {
        AstNode* rv = new P4ExprNode($1, $2, $3, $5, NULL);
        node_array.push_back(rv);
        $$=rv;
    }
    | keyWord name name "(" opts ")" "{" body "}" {
        AstNode* rv = new P4ExprNode($1, $2, $3, $5, $8);
        node_array.push_back(rv);
        $$=rv;
    }
;

p4ExprBmv2 :
    // Parsed statements.
    tableDecl {
        node_array.push_back($1);
        $$=$1;
    }
    | headerTypeDeclaration {
        node_array.push_back($1);
        $$=$1;
    }
    | headerInstance {
        node_array.push_back($1);
        $$=$1;
    }
    | actionFunctionDeclaration {
        node_array.push_back($1);
        $$=$1;
    }
    // Generic statements.
    | keyWord name ";" {
        AstNode* rv = new P4ExprNode($1, $2, NULL, NULL, NULL);
        node_array.push_back(rv);
        $$=rv;
    }
    | keyWord name "{" body "}" {
        AstNode* rv = new P4ExprNode($1, $2, NULL, NULL, $4);
        node_array.push_back(rv);
        $$=rv;
    }
    /* name2, no opts */
    | keyWord name name ";" {
        AstNode* rv = new P4ExprNode($1, $2, $3, NULL, NULL);
        node_array.push_back(rv);
        $$=rv;
    }
    | keyWord name name "{" body "}" {
        AstNode* rv = new P4ExprNode($1, $2, $3, NULL, $5);
        node_array.push_back(rv);
        $$=rv;
    }
    /* no name2, opts */
    | keyWord name "(" opts ")" ";" {
        AstNode* rv = new P4ExprNode($1, $2, NULL, $4, NULL);
        node_array.push_back(rv);
        $$=rv;        
    }
    | keyWord name "(" opts ")" "{" body "}" {
        AstNode* rv = new P4ExprNode($1, $2, NULL, $4, $7);
        node_array.push_back(rv);
        $$=rv;                
    }        
    /* name2, opts */
    | keyWord name name "(" opts ")" ";" {
        AstNode* rv = new P4ExprNode($1, $2, $3, $5, NULL);
        node_array.push_back(rv);
        $$=rv;
    }
    | keyWord name name "(" opts ")" "{" body "}" {
        AstNode* rv = new P4ExprNode($1, $2, $3, $5, $8);
        node_array.push_back(rv);
        $$=rv;
    }
;

// KeyWord should be a list of P4 keyWords.
// For now its a generic identifier.
keyWord :
    IDENTIFIER {
        std::string* newStr = new string(string($1));
        AstNode* rv = new KeywordNode(newStr);
        node_array.push_back(rv);
        $$=rv;
    }
;

// A name is a valid P4 identifier
name : 
    IDENTIFIER {
        std::string* newStr = new string(string($1));
        AstNode* rv = new NameNode(newStr);
        node_array.push_back(rv);
        $$=rv;
    }
;

// Options are empty or a list of names
opts :
    /* empty */ {
        AstNode* rv = new OptsNode(NULL);
        node_array.push_back(rv);
        $$=rv;
    }
    | nameList {
        AstNode* rv = new OptsNode($1);
        node_array.push_back(rv);
        $$=rv;
    }
;

// A name list is a comma-separated list of names
nameList : 
    name {
        AstNode* rv = new NameListNode(NULL, $1);
        node_array.push_back(rv);
        $$=rv;
    }
    | nameList "," name {
        AstNode* rv = new NameListNode($1, $3);
        node_array.push_back(rv);
        $$=rv;
    }
;


// A body is an unparsed block of code in any whitespace-insensitive language
// e.g., P4, C, C++
body :
    /* empty */ {
        AstNode* empty = new EmptyNode();
        $$ = empty;
    }
    | body bodyWord {
        AstNode* rv = new BodyNode($1, NULL, $2);
        node_array.push_back(rv);
        $$=rv;
    }
    | body "{" body "}" {
        AstNode* rv = new BodyNode($1, $3, NULL);
        node_array.push_back(rv);
        $$=rv;
    }
;

// A string can be an identifier, word, or parsed special character.
bodyWord :
    varRef {
        AstNode* rv = new BodyWordNode(BodyWordNode::VARREF, $1);
        node_array.push_back(rv);
        $$=rv;
    }
    | name {
        AstNode* rv = new BodyWordNode(BodyWordNode::NAME, $1);
        node_array.push_back(rv);
        $$=rv;
    }
    | integer {
        AstNode* rv = new BodyWordNode(BodyWordNode::INTEGER, $1);
        node_array.push_back(rv);
        $$=rv;
    }     
    | specialChar {
        AstNode* rv = new BodyWordNode(BodyWordNode::SPECIAL, $1);
        node_array.push_back(rv);
        $$=rv;
    }       
    | STRING {
        AstNode* sv = new StrNode(new string($1));
        AstNode* rv = new BodyWordNode(BodyWordNode::STRING, sv);
        node_array.push_back(rv);
        $$=rv;
        free($1);
    }
    // Better to set up blackbox declaration itself
    | REACTION_ARG_REG {
        AstNode* sv = new StrNode(new string("reg"));
        AstNode* rv = new BodyWordNode(BodyWordNode::STRING, sv);
        node_array.push_back(rv);
        $$=rv;
    }
;

varRef :
    "$" "{" name "}" {
        AstNode* rv = new MblRefNode($3);
        node_array.push_back(rv);
        $$=rv;
    }
;

specialChar:
    L_PAREN {
        string* newStr = new string("(");
        AstNode* rv = new SpecialCharNode(newStr);
        node_array.push_back(rv);
        $$=rv;}
    | R_PAREN {
        string* newStr = new string(")");
        AstNode* rv = new SpecialCharNode(newStr);
        node_array.push_back(rv);
        $$=rv;}
    | L_BRACKET {
        string* newStr = new string("[");
        AstNode* rv = new SpecialCharNode(newStr);
        node_array.push_back(rv);
        $$=rv;}
    | R_BRACKET {
        string* newStr = new string("]");
        AstNode* rv = new SpecialCharNode(newStr);
        node_array.push_back(rv);
        $$=rv;}
    | SEMICOLON {
        string* newStr = new string(";");
        AstNode* rv = new SpecialCharNode(newStr);
        node_array.push_back(rv);
        $$=rv;}
    | COLON {
        string* newStr = new string(":");
        AstNode* rv = new SpecialCharNode(newStr);
        node_array.push_back(rv);
        $$=rv;}
    | COMMA {
        string* newStr = new string(",");
        AstNode* rv = new SpecialCharNode(newStr);
        node_array.push_back(rv);
        $$=rv;}
    | PERIOD {
        string* newStr = new string(".");
        AstNode* rv = new SpecialCharNode(newStr);
        node_array.push_back(rv);
        $$=rv;}
    | WIDTH {
        string* newStr = new string("width");
        AstNode* rv = new SpecialCharNode(newStr);
        node_array.push_back(rv);
        $$=rv;}
    | SLASH {
        string* newStr = new string("/");
        AstNode* rv = new SpecialCharNode(newStr);
        node_array.push_back(rv);
        $$=rv;}
;

integer :
    INTEGER {
        string* strVal = new string(string($1));
        AstNode* rv = new IntegerNode(strVal);
        node_array.push_back(rv);
        $$=rv;        
        free($1);    
    }
;

/*=====  End of P4 expressions  ======*/



/*=====================================
=           P4R expressions           =
=====================================*/

p4rExpr :
    p4rInitBlock {
        AstNode* rv = new P4RExprNode($1);
        node_array.push_back(rv);
        $$=rv;        
    }
    |
    p4rReaction {
        AstNode* rv = new P4RExprNode($1);
        node_array.push_back(rv);
        $$=rv;
    }
    | p4rMalleable {
        AstNode* rv = new P4RExprNode($1);
        node_array.push_back(rv);
        $$=rv;
    }
;

/************** Reactions **************/

p4rInitBlock :
    P4R_INIT_BLOCK name "{" body "}" {
        AstNode* rv = new P4RInitBlockNode($2, $4);
        node_array.push_back(rv);
        $$=rv;
    }
;

p4rReaction :
    P4R_REACTION name "(" reactionArgs ")" "{" includes body "}" {
        AstNode* rv = new P4RReactionNode($2, $4, $8);
        node_array.push_back(rv);
        $$=rv;
    }  
;

includes :
    /* empty */ {
        AstNode* empty = new EmptyNode();
        $$ = empty;
    }
    | includes INCLUDE {
        string* strVal = new string(string($2));
        AstNode* rv = new IncludeNode(strVal, IncludeNode::C);
        node_array.push_back(rv);
        $$ = rv;
    }
;



reactionArgs :
    /* empty */ {
        $$=new ReactionArgsNode();
    }
    | reactionArg {
        ReactionArgsNode* rv = new ReactionArgsNode();
        ReactionArgNode* ra = dynamic_cast<ReactionArgNode*>($1);
        rv->push_back(ra);
        node_array.push_back(rv);
        $$=rv;
    }
    | reactionArgs "," reactionArg {
        ReactionArgsNode* rv = dynamic_cast<ReactionArgsNode*>($1);
        ReactionArgNode* ra = dynamic_cast<ReactionArgNode*>($3);
        rv->push_back(ra);
        node_array.push_back(rv);
        $$=rv;        
    }
;

reactionArg :
    REACTION_ARG_ING field {
        AstNode* rv = new ReactionArgNode(ReactionArgNode::INGRESS_FIELD, $2, NULL, NULL);
        node_array.push_back(rv);
        $$=rv;
    }
    | REACTION_ARG_EGR field {
        AstNode* rv = new ReactionArgNode(ReactionArgNode::EGRESS_FIELD, $2, NULL, NULL);
        node_array.push_back(rv);
        $$=rv;
    }
    | REACTION_ARG_ING varRef {
        AstNode* rv = new ReactionArgNode(ReactionArgNode::INGRESS_MBL_FIELD, $2, NULL, NULL);
        node_array.push_back(rv);
        $$=rv;        
    } 
    | REACTION_ARG_EGR varRef {
        AstNode* rv = new ReactionArgNode(ReactionArgNode::EGRESS_MBL_FIELD, $2, NULL, NULL);
        node_array.push_back(rv);
        $$=rv;        
    }
    // Better to treat reg as a token parsed with blackbox
    | REACTION_ARG_REG name {
        AstNode* rv = new ReactionArgNode(ReactionArgNode::REGISTER, $2, NULL, NULL);
        node_array.push_back(rv);
        $$=rv;        
    }       
    | REACTION_ARG_REG name "[" integer ":" integer "]" {
        AstNode* rv = new ReactionArgNode(ReactionArgNode::REGISTER, $2, $4, $6);
        node_array.push_back(rv);
        $$=rv;        
    }   
;

/************** Malleable **************/

p4rMalleable :
    P4R_MALLEABLE VALUE name "{" varWidth varValueInit "}" {
        AstNode* rv = new P4RMalleableValueNode($3, $5, $6);
        node_array.push_back(rv);
        $$=rv;
    }
    | P4R_MALLEABLE FIELD name "{" varWidth varFieldInit varAlts "}" {
        AstNode* rv = new P4RMalleableFieldNode($3, $5, $6, $7);
        node_array.push_back(rv);
        $$=rv;
    }
    | P4R_MALLEABLE tableDecl {
        AstNode* rv = new P4RMalleableTableNode($2, "");
        node_array.push_back(rv);
        $$=rv;
    }
    | PRAGMA P4R_MALLEABLE tableDecl {
        AstNode* rv = new P4RMalleableTableNode($3, $1);
        node_array.push_back(rv);
        $$=rv;
    }
;

varWidth :
    WIDTH ":" integer ";" {
        AstNode* rv = new VarWidthNode($3);
        node_array.push_back(rv);
        $$=rv;
    }
;

varValueInit :
    INIT ":" integer ";" {
        AstNode* rv = new VarInitNode($3);
        node_array.push_back(rv);
        $$=rv;
    }
;

varFieldInit :
    INIT ":" integer ";" {
        AstNode* rv = new VarInitNode($3);
        node_array.push_back(rv);
        $$=rv;
    }
    | INIT ":" field ";" {
        AstNode* rv = new VarInitNode($3);
        node_array.push_back(rv);
        $$=rv;
    }
;

varAlts :
    ALTS "{" fieldList "}" {
        AstNode* rv = new VarAltNode($3);
        node_array.push_back(rv);
        $$=rv;
    }
;

// One or more fields names.
fieldList :
    /* empty */ {
        FieldsNode* rv = new FieldsNode();
        node_array.push_back(rv);
        $$=rv;
    }
    | field {
        FieldsNode* rv = new FieldsNode();
        FieldNode* fld = dynamic_cast<FieldNode*>($1);
        rv->push_back(fld);
        node_array.push_back(rv);
        $$=rv;
    }
    | fieldList "," field {
        FieldsNode* rv = dynamic_cast<FieldsNode*>($1);
        FieldNode* fld = dynamic_cast<FieldNode*>($3);
        rv->push_back(fld);
        node_array.push_back(rv);
        $$=rv;
    }
;

field :
    name "." name {
        AstNode* rv = new FieldNode($1, $3);
        node_array.push_back(rv);
        $$=rv;
    }
;

/* END NEW GRAMMAR */

%%


void handler(int sig);

int main(int argc, char* argv[]) {
    signal(SIGSEGV, handler);

    parseArgs(argc, argv);  

    yyin = in_file;
    yyparse();
    fclose(in_file);

    PRINT_VERBOSE("Number of syntax tree nodes: %d\n", node_array.size());

    vector<AstNode*> newP4Nodes = compileP4Code(&node_array);

    vector<P4RReactionNode*> reactions;
    findAndRemoveReactions(&reactions, node_array);

    ofstream os;
    os.open(p4_out_fn);
    os << root->toString() << endl << endl;
    for (auto n : newP4Nodes) {
        os << n->toString();
    }
    os.close();

    vector<UnanchoredNode*> cNodes = compileCCode(node_array, out_fn_base);

	os.open(c_out_fn);

    PRINT_VERBOSE("Number of C nodes: %d\n", cNodes.size());

	for (auto node : cNodes){
		os << node -> toString() << endl;
		os << "\n" << endl;		
	}
	os.close();

    return 0;
}



void yyerror(const char* s) {
    printf("Line %d: %s when parsing '%s'\n", yylineno, s, yytext);
    exit(1);
}

// Crash dump handler: https://stackoverflow.com/questions/77005/how-to-automatically-generate-a-stacktrace-when-my-program-crashes
void handler(int sig) {
  void* array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

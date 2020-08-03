%option noyywrap
%option yylineno

/* Currently only support Tofino P4_14 */
%s BMV2_S TOFINO_S

%{
#include <string>
#include <cstdio>
#include "include/ast_nodes.h" // Must be before tab.h
#include "include/ast_nodes_p4.h"
#include "include/ast_nodes_p4r.h"
#include "frontend.tab.h"

%}


%%
%{
    // Copied verbatim at the very front of yylex()
    extern int with_tofino;
    extern int done_init;
    if (!done_init)
    {
        if(with_tofino) {
            BEGIN TOFINO_S;
            done_init=1;  // necessary
            return START_TOFINO;
        } else {
            BEGIN BMV2_S;
            done_init=1;
            return START_BMV2;            
        }
    }    
%}

"#".* { 
    /* Includes are just copied to output */
    // Currently assumes no p4r include
    yylval.sval = strdup(yytext);
    return INCLUDE; 
}

<TOFINO_S>"@".* { 
    /* For tofino compiler pragma only */
    yylval.sval = strdup(yytext);
    return PRAGMA;
}

"//".*                              { /* comments,do nothing  */ }
[/][*][^*]*[*]+([^*/][^*]*[*]+)*[/] { /* comments,do nothing  */ }
[ \t\r]+                            { /* whitespace*/ }
[\n]                                { /* newline */ }

    /* Reserved characters */
"{"         {return L_BRACE;}
"}"         {return R_BRACE;}
"("         {return L_PAREN;}
")"         {return R_PAREN;}
";"         {return SEMICOLON;}
":"         {return COLON;}
","         {return COMMA;}
"."         {return PERIOD;}
"$"         {return DOLLAR;}
"["         {return L_BRACKET;}
"]"         {return R_BRACKET;}
"/"         {return SLASH;}

    /* Reserved words - variables */
"malleable"  {return P4R_MALLEABLE;}
"table"     {return TABLE;}
"value"     {return VALUE;}
"field"     {return FIELD;}
"width"     {return WIDTH;}
"init"      {return INIT;}
"alts"      {return ALTS;}
"reads"     {return READS;}
"actions"   {return ACTIONS;}

    /* Reserved words - initialization */
"init_block" {return P4R_INIT_BLOCK;}

"register"  {return REGISTER;}
    /* Reserved words - reactions */
"reaction"  {return P4R_REACTION;}
    /* Note that tofino blackbox also uses reg as keyword */
"reg"       {return REACTION_ARG_REG;}
"ing"       {return REACTION_ARG_ING;}
"egr"       {return REACTION_ARG_EGR;}

    /* Reserved words - P4 */
"header"        {return HEADER;}
"metadata"      {return METADATA;}
"header_type"   {return HEADER_TYPE;}
"fields"        {return FIELDS;}
"exact"         {return EXACT;}
"ternary"       {return TERNARY;}
"action"        {return ACTION;}

    /* Parsed identifier word in P4 code. */
[A-Za-z_][A-Za-z0-9_]* {
    yylval.sval = strdup(yytext);
    return IDENTIFIER;  
}

    /* Integer */
[-]?[0-9]+ {
    yylval.sval = strdup(yytext);
    return INTEGER; 
}

[^{}\/()\[\]:;,\.$ \t\n]+ {
    yylval.sval = strdup(yytext);
    return STRING;
}


%%
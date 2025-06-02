%{
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>

#define YYSTYPE int

int yylex();
void yyerror(char const *);
%}

%token T_INT
%token T_SET
%token T_NOP
%token T_STOP
%token T_LOAD
%token T_STORE
%token T_BLOAD
%token T_BSTORE
%token T_PUSH
%token T_POP
%token T_DUP
%token T_INVERT
%token T_ADD
%token T_SUB
%token T_MULT
%token T_DIV
%token T_COMPARE
%token T_JUMP
%token T_JUMP_YES
%token T_JUMP_NO
%token T_INPUT
%token T_PRINT
%token T_COLON
%token T_SLOAD
%token T_SSTORE
%token T_SJUMP
%token T_BP
%token T_SBLOAD
%token T_SBSTORE

%%

program         : line program
                | line
                ;

line            : T_INT T_COLON T_NOP                    { put_command($1, NOP,      0);  }
                | T_INT T_COLON T_STOP                   { put_command($1, STOP,     0);  }
                | T_INT T_COLON T_LOAD      T_INT        { put_command($1, LOAD,     $4); }
                | T_INT T_COLON T_STORE     T_INT        { put_command($1, STORE,    $4); }
                | T_INT T_COLON T_BLOAD     T_INT        { put_command($1, BLOAD,    $4); }
                | T_INT T_COLON T_BSTORE    T_INT        { put_command($1, BSTORE,   $4); }
                | T_INT T_COLON T_PUSH      T_INT        { put_command($1, PUSH,     $4); }
                | T_INT T_COLON T_POP                    { put_command($1, POP,      0);  }
                | T_INT T_COLON T_DUP                    { put_command($1, DUP,      0);  }
                | T_INT T_COLON T_INVERT                 { put_command($1, INVERT,   0);  }
                | T_INT T_COLON T_ADD                    { put_command($1, ADD,      0);  }
                | T_INT T_COLON T_SUB                    { put_command($1, SUB,      0);  }
                | T_INT T_COLON T_MULT                   { put_command($1, MULT,     0);  }
                | T_INT T_COLON T_DIV                    { put_command($1, DIV,      0);  }
                | T_INT T_COLON T_COMPARE   T_INT        { put_command($1, COMPARE,  $4); }
                | T_INT T_COLON T_JUMP      T_INT        { put_command($1, JUMP,     $4); }
                | T_INT T_COLON T_JUMP_YES  T_INT        { put_command($1, JUMP_YES, $4); }
                | T_INT T_COLON T_JUMP_NO   T_INT        { put_command($1, JUMP_NO,  $4); }
                | T_INT T_COLON T_INPUT                  { put_command($1, INPUT,    0);  }
                | T_INT T_COLON T_PRINT                  { put_command($1, PRINT,    0);  }
                | T_SET T_INT T_INT                      { set_mem($2, $3);               }
                | T_INT T_COLON T_SLOAD     T_INT        { put_command($1, SLOAD,    $4); }
                | T_INT T_COLON T_SSTORE    T_INT        { put_command($1, SSTORE,   $4); }
                | T_INT T_COLON T_SJUMP                  { put_command($1, SJUMP,		 0); }
                | T_INT T_COLON T_BP        T_INT        { put_command($1, BP,   		 $4); }
                | T_INT T_COLON T_SBLOAD    T_INT        { put_command($1, SBLOAD,    $4); }
                | T_INT T_COLON T_SBSTORE   T_INT        { put_command($1, SBSTORE,   $4); }
                ;
%%

void yyerror(char const *str)
{
        printf("Error: %s\n", str);
}


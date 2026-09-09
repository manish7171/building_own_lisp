#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"
/* If we are compiling in windows, compile these functions*/
#ifdef _WIN32
#include <string.h>

#define SIZE 2048

static char buffer[SIZE];

/*Fake readline*/

char *readline(char *prompt) {
  fputs(prompt, stdout);
  fgets(buffer, SIZE, stdin);

  char *cpy = malloc(strlen(buffer) + 1);
  stcpy(cpy, buffer);
  cpy[strlen[cpy] - 1] = '\0';
  return cpy;
}

void add_history(char *unused) {}

#else
#include <editline/readline.h>
#endif

enum {
    LVAL_NUM, LVAL_ERR
};

struct lval {
    int type;
    long num;
    int err;
};


void lval_print(struct lval v){
    switch(v.type) {
        case LVAL_ERR:
            printf("%s\n", "Error Occured");
            break;
        case LVAL_NUM:
            printf("%li\n", v.num);
            break;
        default:
            break;
    }
}

void lval_println(struct lval v) {
    lval_print(v);
    putchar('\n');
}

struct lval lval_num(long x){
    struct lval l = {.type = LVAL_NUM, .num = x};
    return l;
}

struct lval lval_err(int x){
    struct lval l;// = {.type =  LVAL_ERR, .num = 0};
    l.type = LVAL_ERR;
    l.err = 0;
    return l;
}

struct lval eval_op(struct lval x, char* op, struct lval y) {
    if (strcmp(op, "+") == 0) return lval_num(x.num + y.num);
    if (strcmp(op, "-") == 0) return lval_num(x.num - y.num);
    if (strcmp(op, "*") == 0) return lval_num(x.num * y.num);

    if (y.num == 0) {
        return lval_err(0);
    }
    if (strcmp(op, "/") == 0) return lval_num(x.num / y.num);
    return lval_err(0);
}

struct lval eval(mpc_ast_t* t) {
    //printf("children number %d\n", t->children_num);
    //mpc_ast_print(t);
    if (strstr(t->tag, "number")) {
        return lval_num(atoi(t->contents));
    }

    char* op = t->children[1]->contents;

    struct lval x = eval(t->children[2]);
    
    int i = 3;
    while(strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }
    
    return x;
}

int main(int argc, char **argv) {
    mpc_parser_t* Number   = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr     = mpc_new("expr");
    mpc_parser_t* Lispy    = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
            "                                                     \
            number   : /-?[0-9]+/ ;                             \
            operator : '+' | '-' | '*' | '/' ;                  \
            expr     : <number> | '(' <operator> <expr>+ ')' ;  \
            lispy    : /^/ <operator> <expr>+ /$/ ;             \
            ",
            Number, Operator, Expr, Lispy);
    puts("Lipsy version 0.0.0.0.1");
    puts("Press CTRL+c to exit\n");

    while(1) {
        char *input = readline("lipsy> ");

        add_history(input);

        mpc_result_t r;

        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval_println(eval(r.output));
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input); 
    }
    mpc_cleanup(4, Number, Operator, Expr, Lispy);
    return 0;
}

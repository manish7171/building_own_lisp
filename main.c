#include "mpc.h"
#include <stdio.h>
#include <stdlib.h>

/* If we are compiling on Windows compoile these functions */

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline function */

char *readline(char *prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\0';
  return cpy;
}
/* Fake add_history function */
void add_history(char *unused) {}

/* Otherwise include the editline headers */
#else
#include <readline/history.h>
#include <readline/readline.h>
#endif

typedef enum { LVAL_NUM, LVAL_DEC, LVAL_ERR } lval_type;
typedef enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM } lerr;

typedef struct {
  int type;
  union {
    long num;
    double dec;
    lerr err;
  };
} lval;

lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

lval lval_dec(double x) {
  lval v;
  v.type = LVAL_DEC;
  v.dec = x;
  return v;
}

lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

void lval_print(lval v) {
  switch (v.type) {
  case LVAL_NUM:
    printf("%li", v.num);
    break;

  case LVAL_DEC:
    printf("%f", v.dec);
    break;

  case LVAL_ERR:
    if (v.err == LERR_DIV_ZERO) {
      printf("Error: Division by zero");
    }
    if (v.err == LERR_BAD_OP) {
      printf("Error: Invalid Operator");
    }
    if (v.err == LERR_BAD_NUM) {
      printf("Error: Invalid Number");
    }
    break;
  }
}

void lval_println(lval v) {
  lval_print(v);
  putchar('\n');
}

lval eval_op(lval x, char *op, lval y) {
  if (x.type == LVAL_ERR) {
    return x;
  };
  if (y.type == LVAL_ERR) {
    return y;
  };

  if (x.type == LVAL_DEC || y.type == LVAL_DEC) {
    double a = (x.type == LVAL_DEC) ? x.dec : (double)x.num;
    double b = (y.type == LVAL_DEC) ? y.dec : (double)y.num;
    if (strcmp(op, "+") == 0) {
      return lval_dec(a + b);
    }
    if (strcmp(op, "-") == 0) {
      return lval_dec(a - b);
    }
    if (strcmp(op, "*") == 0) {
      return lval_dec(a * b);
    }
    if (strcmp(op, "/") == 0) {
      return y.dec == 0.0 ? lval_err(LERR_DIV_ZERO) : lval_dec(a / b);
    }

    return lval_err(LERR_BAD_OP);
  }
  if (strcmp(op, "+") == 0) {
    return lval_num(x.num + y.num);
  }
  if (strcmp(op, "-") == 0) {
    return lval_num(x.num - y.num);
  }
  if (strcmp(op, "*") == 0) {
    return lval_num(x.num * y.num);
  }
  if (strcmp(op, "/") == 0) {
    return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
  }

  if (strcmp(op, "%") == 0) {
    return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num % y.num);
  }

  return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t *t) {
  if (strstr(t->tag, "decimal")) {
    errno = 0;
    double x = strtod(t->contents, NULL);
    return errno != ERANGE ? lval_dec(x) : lval_err(LERR_BAD_NUM);
  }
  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  char *op = t->children[1]->contents;

  lval x = eval(t->children[2]);

  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }

  return x;
}

int main(int argc, char **argv) {
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Decimal = mpc_new("decimal");
  mpc_parser_t *Operator = mpc_new("operator");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT,
            "                                                     \
    number   : /-?[0-9]+/ ;                             \
    decimal  : /-?[0-9]+\\.[0-9]+/ ;           \
    operator : '+' | '-' | '*' | '/' | '%';                  \
    expr     : <decimal> | <number> | '(' <operator> <expr>+ ')' ;  \
    lispy    : /^/ <operator> <expr>+ /$/ ;             \
  ",
            Number, Decimal, Operator, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  while (1) {
    char *input = readline("lispy> ");
    add_history(input);
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      mpc_ast_print(r.output);
      lval result = eval(r.output);
      lval_println(result);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    // printf("No you're a %s", input);
    free(input);
  }

  mpc_cleanup(5, Number, Decimal,  Operator, Expr, Lispy);
  return 0;
}

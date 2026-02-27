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

typedef enum { LVAL_NUM, LVAL_DEC, LVAL_ERR, LVAL_SYM, LVAL_SEXPR } lval_type;
typedef enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM } lerr;

typedef struct lval {
  int type;
  long num;
  double dec;
  char *err;
  char *sym;
  int count;
  struct lval **cell;
} lval;

lval *lval_num(long x) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

lval *lval_dec(double x) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_DEC;
  v->dec = x;
  return v;
}

lval *lval_err(char *m) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

lval *lval_sym(char *s) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

lval *lval_sexpr(void) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lval_del(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    break;

  case LVAL_DEC:
    break;

  case LVAL_ERR:
    free(v->err);
    break;
  case LVAL_SYM:
    free(v->sym);
    break;

  case LVAL_SEXPR:
    for (int i = 0; i < v->count; ++i) {
      lval_del(v->cell[i]);
    }
    free(v->cell);
    break;
  }

  free(v);
}

lval *lval_read_num(mpc_ast_t *t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval *lval_read_dec(mpc_ast_t *t) {
  errno = 0;
  double x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_dec(x) : lval_err("invalid number");
}

lval *lval_add(lval *v, lval *x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval *) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}
lval *lval_read(mpc_ast_t *t) {
  if (strstr(t->tag, "decimal")) {
    return lval_read_dec(t);
  }
  if (strstr(t->tag, "number")) {
    return lval_read_num(t);
  }

  if (strstr(t->tag, "symbol")) {
    return lval_sym(t->contents);
  }

  lval *x = NULL;

  if (strcmp(t->tag, ">") == 0) {
    x = lval_sexpr();
  }

  if (strstr(t->tag, "sexpr")) {
    x = lval_sexpr();
  }

  for (int i = 0; i < t->children_num; ++i) {
    if (strcmp(t->children[i]->contents, "(") == 0) {
      continue;
    }

    if (strcmp(t->children[i]->contents, ")") == 0) {
      continue;
    }

    if (strcmp(t->children[i]->tag, "regex") == 0) {
      continue;
    }

    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}
void lval_print(lval *v);

void lval_print_expr(lval *v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; ++i) {
    lval_print(v->cell[i]);
    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    printf("%li", v->num);
    break;

  case LVAL_DEC:
    printf("%f", v->dec);
    break;

  case LVAL_ERR:
    printf("Error: %s", v->err);
    break;

  case LVAL_SYM:
    printf("%s", v->sym);
    break;

  case LVAL_SEXPR:
    lval_print_expr(v, '(', ')');
  }
}

void lval_println(lval *v) {
  lval_print(v);
  putchar('\n');
}

lval* eval_op(lval* x, char *op, lval* y) {
  if (x->type == LVAL_ERR) {
    return x;
  };
  if (y->type == LVAL_ERR) {
    return y;
  };

  if (x->type == LVAL_DEC || y->type == LVAL_DEC) {
    double a = (x->type == LVAL_DEC) ? x->dec : (double)x->num;
    double b = (y->type == LVAL_DEC) ? y->dec : (double)y->num;
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
      return y->dec == 0.0 ? lval_err("Divided by zero") : lval_dec(a / b);
    }

    return lval_err("Bad operation");
  }
  if (strcmp(op, "+") == 0) {
    return lval_num(x->num + y->num);
  }
  if (strcmp(op, "-") == 0) {
    return lval_num(x->num - y->num);
  }
  if (strcmp(op, "*") == 0) {
    return lval_num(x->num * y->num);
  }
  if (strcmp(op, "/") == 0) {
    return y->num == 0 ? lval_err("LERR_DIV_ZERO") : lval_num(x->num / y->num);
  }

  if (strcmp(op, "%") == 0) {
    return y->num == 0 ? lval_err("LERR_DIV_ZERO") : lval_num(x->num % y->num);
  }

  return lval_err("LERR_BAD_OP");
}

lval *eval(mpc_ast_t *t) {
  if (strstr(t->tag, "decimal")) {
    errno = 0;
    double x = strtod(t->contents, NULL);
    return errno != ERANGE ? lval_dec(x) : lval_err("LERR_BAD_NUM");
  }
  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("LERR_BAD_NUM");
  }

  char *op = t->children[1]->contents;

  lval *x = eval(t->children[2]);

  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }

  return x;
}
lval* lval_eval(lval* v);

lval* lval_pop(lval* v, int i) {
    lval* x = v->cell[i];
    /* shift memory after the item at i over the top */
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count -i - 1));
    /* Decrease the count of items in the lists */
    v->count --;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* buildin_op(lval* a, char* op) {
    for(int i = 0; i < a->count; ++i) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non-number");
        }
    }

    // pop the first element
    lval* x = lval_pop(a, 0);

    //if no arguments and sub then perform unary negation */
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    //while there are still elements remaining
    while (a->count > 0) {
        lval* y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0) {
            x->num += y->num;
        }

        if (strcmp(op, "-") == 0) {
            x->num -= y->num;
        }

        if (strcmp(op, "*") == 0) {
            x->num *= y->num;
        }

        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x);
                lval_del(y);
                lval_err("Division by zero!");
                break;
            }

            x->num /= y->num;
        }

        lval_del(y);
    }

    lval_del(a);
    return x;
}

lval* lval_eval_sexpr(lval* v) {
    // Evaluate children
    for (int i = 0; i < v->count; ++i) {
        v->cell[i] = lval_eval(v->cell[i]);
    } 

    // Error checking
    for (int i = 0; i < v->count; ++i) {
        if (v->cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    //Empty expression
    if (v->count == 0) {
        return v;
    }

    //single expression
    if (v->count == 1) {
        return lval_take(v, 0);
    }

    //Ensure first element is symbol
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression Doesnot start with symbol!");
    }


    //Call buildin with operator
    lval* result = buildin_op(v, f->sym);
    lval_del(f);
    return result;
}

lval* lval_eval(lval* v) {
    // evalulate s-expression
    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(v);
    }
    return v;
}

int main(int argc, char **argv) {

  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Decimal = mpc_new("decimal");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");
mpca_lang(MPCA_LANG_DEFAULT,
  "                                          \
    number : /-?[0-9]+/ ;                    \
    decimal  : /-?[0-9]+\\.[0-9]+/ ;            \
    symbol : '+' | '-' | '*' | '/' ;         \
    sexpr  : '(' <expr>* ')' ;               \
    expr   : <decimal> | <number> | <symbol> | <sexpr> ; \
    lispy  : /^/ <expr>* /$/ ;               \
  ",
  Number, Decimal, Symbol, Sexpr, Expr, Lispy);
  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  while (1) {
    char *input = readline("lispy> ");
    add_history(input);
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
        lval* x = lval_eval(lval_read(r.output));
        lval_println(x);
        lval_del(x);
      //mpc_ast_print(r.output);
      //lval* result = eval(r.output);
      //lval_println(result);
      //mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    free(input);
  }

  //mpc_cleanup(6, Number, Decimal, Symbol, Sexpr, Expr, Lispy);
  mpc_cleanup(6, Decimal, Number,  Symbol, Sexpr, Expr, Lispy);
  return 0;
}

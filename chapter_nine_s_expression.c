#include "mpc.h"
#include <math.h>

#ifdef _WIN32

static char buffer[2048];

char *readline(char *prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\0';
  return cpy;
}

void add_history(char *unused) {}

#else
#include <editline/readline.h>
#endif
/*
 *
typedef struct mpc_ast_t {
  char *tag;
  char *contents;
  mpc_state_t state;
  int children_num;
  struct mpc_ast_t** children;
} mpc_ast_t;

 * */
struct lval;
typedef struct lval lval;
typedef struct lval {
  int type;
  long num;
  /* Errors and Symbol types have some string data*/
  char *err;
  char *sym;
  /* Count and Pointer to a list of  "lval"*/
  int count;
  lval **cell;
} lval;

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lval *lval_num(long x) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
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

lval *lval_sexpr() {
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
  }
  free(v);
}
void lval_print(lval *v);

void lval_expr_print(lval *v, char open, char close) {
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
  case LVAL_ERR:
    printf("Error %s", v->err);
    break;

  case LVAL_SYM:
    printf("%s", v->sym);
    break;

  case LVAL_SEXPR:
    lval_expr_print(v, '(', ')');
    break;
  }
}

void lval_println(lval *v) {
  lval_print(v);
  putchar('\n');
}

lval *lval_eval(lval *v);
lval *lval_take(lval *v, int i);
lval *lval_pop(lval *v, int i);
lval *builtin_op(lval *v, char* c);
lval *lval_eval_sexpr(lval *v);

lval *lval_eval(lval* v) {
    /* evaluate Sexpressions */
    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(v);
    }

    /* All other lval types remain the same*/
    return v;
}

lval *lval_eval_sexpr(lval* v) {
    /* Evalutate Children*/
    for (int i = 0; i < v->count; ++i) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    /*Error checking*/
    for (int i = 0; i < v->count; ++i) {
        if (v->cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    /* Empty Expression*/
    if (v->count == 0) {return v;}

    /* single expression*/
    if (v->count == 1) {
        return lval_take(v, 0);
    }

    /* Ensure first element is Symbol*/
    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression does not start with symbol!");
    }

    /* Call Builtin with operator*/
    lval*  result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
}

lval *lval_pop(lval* v, int i) {
    /* Find the item at "i"*/
    lval* x = v->cell[i];

    /* Shift memory after the item at "i" over the top*/
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count - i - 1));

    /* Decrease the count of items in the list */
    v->count--;

    /* Reallocate the momory used*/
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval *lval_take(lval*v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* builtin_op(lval* a, char* op) {
    /* Ensure all arguments are numbers*/
    for(int i = 0; i < a->count; ++i) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non-number!");
        }
    }

    /* pop the first element*/
    lval *x = lval_pop(a, 0);

    /* If no arguments and sub then perform unary negation*/
    if((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    /*While there are still elements remaining*/
    while(a->count > 0) {
        /* Pop the next element*/
        lval *y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0) { x->num += y->num;}
        if (strcmp(op, "-") == 0) { x->num -= y->num;}
        if (strcmp(op, "*") == 0) { x->num *= y->num;}
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x);
                lval_del(y);
                x = lval_err("Division by zero!");
                break;
            }
            x->num /= y->num;
        }
        lval_del(y);
    }
    lval_del(a);
    return x;
}

/*Use operator string to see which operation to perform*/

lval *eval_op(lval *x, char *op, lval *y) {

  if (x->type == LVAL_ERR) {
    return lval_err("Error y");
  }

  if (y->type == LVAL_ERR) {
    return lval_err("Error y");
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
    return y->num == 0 ? lval_err("Divison by zero")
                       : lval_num(x->num / y->num);
  }
  if (strcmp(op, "%") == 0) {
    return lval_num(x->num % y->num);
  }
  if (strcmp(op, "min") == 0) {
    return lval_num(x->num < y->num ? x->num : y->num);
  }
  if (strcmp(op, "max.num") == 0) {
    return lval_num(x->num > y->num ? x->num : y->num);
  }
  if (strcmp(op, "^") == 0) {
    return lval_num(pow(x->num, y->num));
  }
  return lval_err("BAD OPERATOR");
}

lval *lval_read_num(mpc_ast_t *t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("Invalid Number");
}

lval *lval_add(lval *v, lval *x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval *) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

lval* lval_read(mpc_ast_t* t) {
  
  /* If Symbol or Number return conversion to that type */
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
  
  /* If root (>) or sexpr then create empty list */
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); } 
  if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
  
  /* Fill this list with any valid expression contained within */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }
  
  return x;
}

int number_of_expr_nodes(mpc_ast_t *t) {
  int total = strstr(t->tag, "expr") ? 1 : 0;
  for (int i = 0; i < t->children_num; ++i) {
    total = total + number_of_expr_nodes(t->children[i]);
  }
  return total;
}

int number_of_bracket_nodes(mpc_ast_t *t) {
  int total =
      ((strcmp(t->contents, "(") == 0) || (strcmp(t->contents, ")") == 0)) ? 1
                                                                           : 0;
  for (int i = 0; i < t->children_num; ++i) {
    total = total + number_of_bracket_nodes(t->children[i]);
  }
  return total;
}

int main() {

 mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr  = mpc_new("sexpr");
  mpc_parser_t* Expr   = mpc_new("expr");
  mpc_parser_t* Lispy  = mpc_new("lispy");
  
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                          \
      number : /-?[0-9]+/ ;                    \
      symbol : '+' | '-' | '*' | '/' ;         \
      sexpr  : '(' <expr>* ')' ;               \
      expr   : <number> | <symbol> | <sexpr> ; \
      lispy  : /^/ <expr>* /$/ ;               \
    ",
    Number, Symbol, Sexpr, Expr, Lispy);
  puts("Lispy Version 0.0.0.0.2");
  puts("Press Ctrl+c to Exit\n");

while (1) {
  
    char* input = readline("lispy> ");
    add_history(input);
    
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval* x = lval_eval(lval_read(r.output));
      lval_println(x);
      lval_del(x);
      mpc_ast_delete(r.output);
    } else {    
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
    free(input);
    
  }

  /* Undefine and delete our parsers */
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);

  return 0;
}

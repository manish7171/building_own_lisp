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

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

struct lval {
  int type;
  long num;
  char *err;
  char *sym;

  int count;
  struct lval **cell;
};

void lval_print(struct lval *v);

void lval_print_sexpr(struct lval *v) {
  putchar('(');
  for (int i = 0; i < v->count; ++i) {
    lval_print(v->cell[i]);
    putchar(' ');
  }
  putchar(')');
}

void lval_print(struct lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    printf("%li", v->num);
    break;
  case LVAL_SYM:
    printf("%s", v->sym);
    break;
  case LVAL_SEXPR:
    lval_print_sexpr(v);
    break;
  default:
    break;
  }
}

void lval_println(struct lval *v) {
  lval_print(v);
  putchar('\n');
}

struct lval *lval_num(long x) {
  struct lval *l = malloc(sizeof(struct lval));
  l->type = LVAL_NUM;
  l->num = x;
  return l;
}

struct lval *lval_err(char *m) {
  struct lval *l = malloc(sizeof(struct lval));
  l->type = LVAL_ERR;
  l->err = malloc(strlen(m) + 1);
  strcpy(l->err, m);
  return l;
}

struct lval *eval_op(struct lval *x, char *op, struct lval *y) {
  if (strcmp(op, "+") == 0)
    return lval_num(x->num + y->num);
  if (strcmp(op, "-") == 0)
    return lval_num(x->num - y->num);
  if (strcmp(op, "*") == 0)
    return lval_num(x->num * y->num);

  if (y->num == 0) {
    return lval_err(0);
  }
  if (strcmp(op, "/") == 0)
    return lval_num(x->num / y->num);
  return lval_err(0);
}

struct lval *builtin_op(struct lval* v, char* op);

struct lval* lval_eval(struct lval* v);
void lval_del(struct lval* v);
struct lval* lval_take(struct lval* v, int i);
struct lval* lval_pop(struct lval* v, int i);

struct lval* lval_take(struct lval* v, int i) {
    struct lval* e = lval_pop(v, i);
    lval_del(v);
    return e;
}

struct lval* lval_pop(struct lval* v, int i) {
    struct lval* x = v->cell[i];
    memmove(&v->cell[i], &v->cell[i + 1], sizeof(struct lval*) * (v->count -i -1));
    v->count--;
    
    v->cell = realloc(v->cell, sizeof(struct lval*)*v->count);

    return x;
}

struct lval *builtin_op(struct lval* v, char* op) {
   // Ensure all arguments are number
   for(int i = 0; i < v->count; ++i) {
       if (v->cell[i]->type != LVAL_NUM) {
           lval_del(v);
           return lval_err("Cannot operate on non-numbers");
       }
   }

   // Pop the first element
   struct lval* x = lval_pop(v, 0);

   if ((strcmp(op, "-") == 0) && v->count == 0) {
       x->num = -x->num;
   }

   while(v->count > 0) {
       struct lval* y = lval_pop(v, 0);

       if (strcmp(op, "+") == 0)
           x->num += y->num;
       if (strcmp(op, "-") == 0)
           x->num -= y->num;
       if (strcmp(op, "*") == 0)
           x->num *= y->num;

       if (strcmp(op, "/") == 0) {
           if (y->num == 0) {
               lval_del(x);
               lval_del(y);
               x = lval_err("Division by zero");
               break;
           }
           x->num /= y->num;
       }

       lval_del(y);
   }

   lval_del(v);

   return x;
}

struct lval* lval_eval_sexpr(struct lval* t) {

    for(int i = 0; i < t->count; ++i) {
        t->cell[i] = lval_eval(t->cell[i]);
    }

    for(int i = 0; i < t->count; ++i) {
        if(t->cell[i]->type == LVAL_ERR) {
            return lval_take(t, i);
        }
    }

    if (t->count == 0) {
        return t;
    }

    if (t->count == 1) {
        return lval_take(t, 0);
    }

    struct lval* f = lval_pop(t, 0);

    if (f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(t);
        return lval_err("S-expression doesnot start with symbol");
    }

    struct lval* result = builtin_op(t, f->sym);
    lval_del(f);
    return result;
}

struct lval *lval_eval(struct lval *t) {
   if (t->type == LVAL_SEXPR) {
       return lval_eval_sexpr(t);
   } 
   return t;  
}

struct lval *lval_read_num(mpc_ast_t *t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return (x != ERANGE) ? lval_num(x) : lval_err("invalid number");
}

struct lval *lval_sym(char *sym) {
  struct lval *l = malloc(sizeof(struct lval));
  l->type = LVAL_SYM;
  l->sym = malloc(strlen(sym) + 1);
  strcpy(l->sym, sym);
  return l;
}

struct lval *lval_sexpr() {
  struct lval *l = malloc(sizeof(struct lval));
  l->type = LVAL_SEXPR;
  l->count = 0;
  l->cell = NULL;
  return l;
}

struct lval *lval_add(struct lval *x, struct lval *y) {
  x->count++;
  x->cell = realloc(x->cell, sizeof(struct lval *) * x->count);
  x->cell[x->count - 1] = y;
  return x;
}

struct lval *lval_read(mpc_ast_t *t) {
  if (strstr(t->tag, "number")) {
    return lval_read_num(t);
  }

  if (strstr(t->tag, "symbol")) {
    return lval_sym(t->contents);
  }

  struct lval *x = NULL;

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

void lval_del(struct lval* l) {
    switch(l->type){
        case LVAL_NUM:
            break;
        case LVAL_SYM:
            free(l->sym);
            break;
        case LVAL_ERR:
            free(l->err);
            break;
        case LVAL_SEXPR:
            for(int i = 0; i < l->count; ++i) {
                lval_del(l->cell[i]);
            }
            free(l->cell);
    }
    free(l);
}

int main(int argc, char **argv) {
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
            "                                                   \
            number   : /-?[0-9]+/ ;                             \
            symbol   : '+' | '-' | '*' | '/' ;                  \
            sexpr    : '(' <expr>* ')';                         \
            expr     : <number> | <symbol> | <sexpr>;           \
            lispy    : /^/ <expr>* /$/ ;                        \
            ",
            Number, Symbol, Sexpr, Expr, Lispy);
  puts("Lipsy version 0.0.0.0.1");
  puts("Press CTRL+c to exit\n");

  while (1) {
    char *input = readline("lipsy> ");

    add_history(input);

    mpc_result_t r;

    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      mpc_ast_print(r.output);
      // lval_println(eval(r.output));
      struct lval* l = lval_eval(lval_read(r.output));
      printf("=======\n");
      lval_println(l);
      lval_del(l);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
  return 0;
}

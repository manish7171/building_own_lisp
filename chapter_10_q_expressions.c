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

#define LASSERT(args, cond, err)                                               \
  if (!(cond)) {                                                               \
    lval_del(args);                                                            \
    lval_err(err);                                                             \
  }

#define LASSERT_ARGS_NUM(args, cond)                                               \
    LASSERT(args, cond, "Function 'head' passed too many arguments!"); \

#define LASSERT_ARGS_TYPE(args, cond)                                               \
    LASSERT(args, cond, "Function passed incorrect type!"); \

#define LASSERT_EMPTY(args, cond)                                               \
    LASSERT(args, cond, "Function 'head' passed {}!"); \

enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

struct lval {
  int type;
  long num;
  char *err;
  char *sym;

  int count;
  struct lval **cell;
};

void lval_print(struct lval *v);

void lval_print_expr(struct lval *v, char start, char end) {
  putchar(start);
  for (int i = 0; i < v->count; ++i) {
    lval_print(v->cell[i]);
    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }
  putchar(end);
}

void lval_print(struct lval *v) {
  switch (v->type) {
  case LVAL_ERR:
    printf("%s", v->err);
    break;
  case LVAL_NUM:
    printf("%li", v->num);
    break;
  case LVAL_SYM:
    printf("%s", v->sym);
    break;
  case LVAL_QEXPR:
    lval_print_expr(v, '{', '}');
    break;
  case LVAL_SEXPR:
    lval_print_expr(v, '(', ')');
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

struct lval *builtin_op(struct lval *v, char *op);

struct lval *lval_eval(struct lval *v);
void lval_del(struct lval *v);
struct lval *lval_take(struct lval *v, int i);
struct lval *lval_pop(struct lval *v, int i);

struct lval *lval_take(struct lval *v, int i) {
  struct lval *e = lval_pop(v, i);
  lval_del(v);
  return e;
}

struct lval *lval_pop(struct lval *v, int i) {
  struct lval *x = v->cell[i];
  memmove(&v->cell[i], &v->cell[i + 1],
          sizeof(struct lval *) * (v->count - i - 1));
  v->count--;
  v->cell = realloc(v->cell, sizeof(struct lval *) * v->count);
  return x;
}

struct lval *builtin_head(struct lval *v) {
  // check error conditions
  //LASSERT(v, v->count == 1, "Function 'head' passed too many arguments!");
  LASSERT_ARGS_NUM(v, v->count == 1);
  LASSERT_ARGS_TYPE(v, v->cell[0]->type == LVAL_QEXPR);
  //LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
  //        "Function 'head' passed incorrect type!");
  //LASSERT(v, v->cell[0]->count != 0, "Function 'head' passed {}!");
  LASSERT_EMPTY(v, v->cell[0]->count != 0);
  // otherwise take the first argument
  struct lval *a = lval_take(v, 0);
  while (a->count > 1) {
    lval_del(lval_pop(a, 1));
  }

  return a;
}

struct lval *builtin_tail(struct lval *v) {

  LASSERT_ARGS_NUM(v, v->count == 1);
  LASSERT_ARGS_TYPE(v, v->cell[0]->type == LVAL_QEXPR);
  //LASSERT(v, v->count == 1, "Function 'tail' passed too many arguments!");
  //LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
  //       "Function 'tail' passed too many arguments!");
  //LASSERT(v, v->cell[0]->count != 0, "Function 'tail' passed {}!");
  LASSERT_EMPTY(v, v->cell[0]->count != 0);
  struct lval *a = lval_take(v, 0);
  lval_del(lval_pop(a, 0));
  return a;
}

struct lval *builtin_list(struct lval *v) {
  v->type = LVAL_QEXPR;
  return v;
}

struct lval *builtin_eval(struct lval *v) {
  //LASSERT(v, v->count == 1, "Function 'eval' passed too many arguments!");
  LASSERT_ARGS_NUM(v, v->count == 1);
  LASSERT_ARGS_TYPE(v, v->cell[0]->type == LVAL_QEXPR);
  //LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
  //        "Function 'eval' passed too many arguments!");

  struct lval *x = lval_take(v, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(x);
}

struct lval *lval_add(struct lval *x, struct lval *y);

struct lval *lval_join(struct lval *x, struct lval *y) {
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  lval_del(y);
  return x;
}

struct lval *builtin_join(struct lval *a) {
  for (int i = 0; i < a->count; ++i) {
    LASSERT_ARGS_TYPE(a, a->cell[0]->type == LVAL_QEXPR);
    //LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
    //        "Function 'join' passed incorrect type!");
  }

  struct lval *x = lval_pop(a, 0);

  while (a->count > 1) {
    x = lval_join(x, lval_pop(a, 0));
  }
  lval_del(a);
  return x;
}

struct lval *builtin_op(struct lval *v, char *op) {
  // Ensure all arguments are number
  for (int i = 0; i < v->count; ++i) {
    if (v->cell[i]->type != LVAL_NUM) {
      lval_del(v);
      return lval_err("Cannot operate on non-numbers");
    }
  }

  // Pop the first element
  struct lval *x = lval_pop(v, 0);

  if ((strcmp(op, "-") == 0) && v->count == 0) {
    x->num = -x->num;
  }

  while (v->count > 0) {
    struct lval *y = lval_pop(v, 0);

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

struct lval* lval_add_front(struct lval* q, struct lval* v) {
  q->count++;
  q->cell = realloc(q->cell, sizeof(struct lval *) * q->count);
  memmove(&q->cell[1], &q->cell[0], sizeof(struct lval*) * (q->count - 1));
  q->cell[0] = v;
  return q;
}

struct lval* builtin_cons(struct lval* x) {
  LASSERT_ARGS_TYPE(x->cell[1], x->cell[1]->type == LVAL_QEXPR);
  /*x->cell[1]->cell = realloc(x->cell[1]->cell, sizeof(struct lval *) * (x->cell[1]->count + 1));
  memmove(&x->cell[1]->cell[1], &x->cell[1]->cell[0],
          sizeof(struct lval *) * (x->cell[1]->count));
  x->cell[1]->cell[0] = x->cell[0];
  x->cell[1]->count++;
  struct lval* r = x->cell[1]; */

  struct lval* v = lval_pop(x, 0);
  struct lval* q = lval_pop(x, 0);
  q = lval_add_front(q, v);
  lval_del(x);
  return q;
}

struct lval *builtin(struct lval *a, char *func) {
  if (strcmp("list", func) == 0) {
    return builtin_list(a);
  }
  if (strcmp("head", func) == 0) {
    return builtin_head(a);
  }
  if (strcmp("tail", func) == 0) {
    return builtin_tail(a);
  }
  if (strcmp("join", func) == 0) {
    return builtin_join(a);
  }
  if (strcmp("cons", func) == 0) {
    return builtin_cons(a);
  }
  if (strcmp("eval", func) == 0) {
    return builtin_eval(a);
  }
  if (strstr("+-/*", func)) {
    return builtin_op(a, func);
  }
  lval_del(a);
  return lval_err("Unknown Function!");
}

struct lval *lval_eval_sexpr(struct lval *t) {

  for (int i = 0; i < t->count; ++i) {
    t->cell[i] = lval_eval(t->cell[i]);
  }

  for (int i = 0; i < t->count; ++i) {
    if (t->cell[i]->type == LVAL_ERR) {
      return lval_take(t, i);
    }
  }

  if (t->count == 0) {
    return t;
  }

  if (t->count == 1) {
    return lval_take(t, 0);
  }

  struct lval *f = lval_pop(t, 0);

  if (f->type != LVAL_SYM) {
    lval_del(f);
    lval_del(t);
    return lval_err("S-expression doesnot start with symbol");
  }

  struct lval *result = builtin(t, f->sym);
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

struct lval *lval_qexpr() {
  struct lval *l = malloc(sizeof(struct lval));
  l->type = LVAL_QEXPR;
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

  if (strstr(t->tag, "qexpr")) {
    x = lval_qexpr();
  }

  for (int i = 0; i < t->children_num; ++i) {
    if (strcmp(t->children[i]->contents, "(") == 0) {
      continue;
    }
    if (strcmp(t->children[i]->contents, ")") == 0) {
      continue;
    }
    if (strcmp(t->children[i]->contents, "{") == 0) {
      continue;
    }
    if (strcmp(t->children[i]->contents, "}") == 0) {
      continue;
    }
    if (strcmp(t->children[i]->tag, "regex") == 0) {
      continue;
    }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

void lval_del(struct lval *l) {
  switch (l->type) {
  case LVAL_NUM:
    break;
  case LVAL_SYM:
    free(l->sym);
    break;
  case LVAL_ERR:
    free(l->err);
    break;
  case LVAL_SEXPR:
  case LVAL_QEXPR:
    for (int i = 0; i < l->count; ++i) {
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
  mpc_parser_t *Qexpr = mpc_new("qexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
            "                                                   \
            number   : /-?[0-9]+/ ;                             \
            symbol   : \"list\" | \"head\" | \"tail\" | \"join\" \
                        | \"cons\" | \"eval\" |'+' | '-' | '*' | '/' ;                  \
            sexpr    : '(' <expr>* ')';                         \
            qexpr    : '{' <expr>* '}';                         \
            expr     : <number> | <symbol> | <sexpr> | <qexpr>;           \
            lispy    : /^/ <expr>* /$/ ;                        \
            ",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
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
      //struct lval *l = lval_read(r.output);
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

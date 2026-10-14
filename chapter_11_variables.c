#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"
/* If we are compiling in windows, compile these functions*/
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };
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
char* ltype_name(int t) {
  switch(t) {
    case LVAL_FUN: return "Function";
    case LVAL_NUM: return "Number";
    case LVAL_ERR: return "Error";
    case LVAL_SYM: return "Symbol";
    case LVAL_SEXPR: return "S-Expression";
    case LVAL_QEXPR: return "Q-Expression";
    default: return "Unknown";
  }
}
#define LASSERT(args, cond, fmt, ...) \
  if (!(cond)) { struct lval* err = lval_err(fmt, ##__VA_ARGS__); lval_del(args); return err; }

#define LASSERT_ARGS_NUM(func, args, num)                                      \
  LASSERT(args, args->count == num,                                            \
          "Function '%s' passed incorrect number of arguments. Got %i, "       \
          "Expected %i.",                                                      \
          func, args->count, num)

#define LASSERT_ARGS_TYPE(func, args, index, expect)                           \
  LASSERT(args, args->cell[index]->type == expect,                             \
          "Function '%s' passed incorrect type "                                \
          "for argument %i, Got %s, Expected %s",                              \
          func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_EMPTY(func, args, index)                                       \
  LASSERT(args, args->cell[index]->count != 0,                                 \
          "Function '%s' passed {} for argument %i.", func, index);


struct lenv {
  int count;
  char **syms;
  struct lval **vals;
};

typedef struct lval *(*lbuiltin)(struct lenv *, struct lval *);

struct lval {
  int type;
  long num;
  char *err;
  char *sym;
  lbuiltin fun;
  int count;
  struct lval **cell;
};

void lval_print(struct lval *v);

struct lval *builtin_op(struct lenv *e, struct lval *v, char *op);
struct lval *lval_eval(struct lenv *e, struct lval *v);
void lval_del(struct lval *v);
struct lval *lval_take(struct lval *v, int i);
struct lval *lval_pop(struct lval *v, int i);
struct lval *lval_sym(char *sym);
struct lval *lval_func(lbuiltin fun);
void lenv_put(struct lenv *e, struct lval *k, struct lval *v);
struct lval *lval_add(struct lval *x, struct lval *y);
struct lval *lval_sexpr();

struct lenv *lenv_new(void) {
  struct lenv *e = malloc(sizeof(struct lenv));
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
};
void lenv_del(struct lenv *e) {
  for (int i = 0; i < e->count; ++i) {
    free(e->syms[i]);
    free(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}
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
  case LVAL_FUN:
    printf("<function>");
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
struct lval *lval_err(char *fmt, ...) {
  struct lval *l = malloc(sizeof(struct lval));
  l->type = LVAL_ERR;

  /* Creat a va list and initialize it*/
  va_list va;
  va_start(va, fmt);

  /* Allocate 512 bytes of space*/
  l->err = malloc(512);

  vsnprintf(l->err, 511, fmt, va);

  l->err = realloc(l->err, strlen(l->err) + 1);
  va_end(va);
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
struct lval *builtin_head(struct lenv *e, struct lval *v) {
  LASSERT_ARGS_NUM("head", v, 1);
  LASSERT_ARGS_TYPE("head", v, 0, LVAL_QEXPR);
  LASSERT_EMPTY("head", v, 0);
  // otherwise take the first argument
  struct lval *a = lval_take(v, 0);
  while (a->count > 1) {
    lval_del(lval_pop(a, 1));
  }

  return a;
}
struct lval *builtin_tail(struct lenv *e, struct lval *v) {
  LASSERT_ARGS_NUM("tail", v, 1);
  LASSERT_ARGS_TYPE("tail", v, 0, LVAL_QEXPR);
  LASSERT_EMPTY("tail", v, 0);
  struct lval *a = lval_take(v, 0);
  lval_del(lval_pop(a, 0));
  return a;
}
struct lval *builtin_add(struct lenv *e, struct lval *v) {
  return builtin_op(e, v, "+");
}
struct lval *builtin_sub(struct lenv *e, struct lval *v) {
  return builtin_op(e, v, "-");
}
struct lval *builtin_mul(struct lenv *e, struct lval *v) {
  return builtin_op(e, v, "*");
}
struct lval *builtin_div(struct lenv *e, struct lval *v) {
  return builtin_op(e, v, "/");
}

struct lval *builtin_list(struct lenv *e, struct lval *v) {
  v->type = LVAL_QEXPR;
  return v;
}
struct lval *builtin_eval(struct lenv *e, struct lval *v) {

  LASSERT_ARGS_NUM("eval", v, 1);
  LASSERT_ARGS_TYPE("eval", v, 0, LVAL_QEXPR);
  struct lval *x = lval_take(v, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}
struct lval *lval_join(struct lval *x, struct lval *y) {
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  lval_del(y);
  return x;
}
struct lval *builtin_join(struct lenv *e, struct lval *a) {
  for (int i = 0; i < a->count; ++i) {
    LASSERT_ARGS_TYPE("join", a, i, LVAL_QEXPR);
  }

  struct lval *x = lval_pop(a, 0);

  while (a->count > 1) {
    x = lval_join(x, lval_pop(a, 0));
  }
  lval_del(a);
  return x;
}
struct lval *builtin_op(struct lenv *e, struct lval *v, char *op) {
  // Ensure all arguments are number
  for (int i = 0; i < v->count; ++i) {
    LASSERT_ARGS_TYPE(op, v, i, LVAL_NUM);
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
struct lval *lval_add_front(struct lval *q, struct lval *v) {
  q->count++;
  q->cell = realloc(q->cell, sizeof(struct lval *) * q->count);
  memmove(&q->cell[1], &q->cell[0], sizeof(struct lval *) * (q->count - 1));
  q->cell[0] = v;
  return q;
}
struct lval *builtin_len(struct lenv *e, struct lval *x) {
  LASSERT_ARGS_TYPE("len", x, 0, LVAL_QEXPR);

  return lval_num(x->cell[0]->count);
}
struct lval *builtin_init(struct lenv *e, struct lval *x) {
  LASSERT_ARGS_TYPE("int", x, 0, LVAL_QEXPR);
  struct lval *q = lval_pop(x, 0);
  struct lval *v = lval_pop(q, q->count - 1);
  lval_del(v);
  lval_del(x);
  return q;
}
struct lval *builtin_cons(struct lenv *e, struct lval *x) {
  LASSERT_ARGS_TYPE("cons", x, 0, LVAL_QEXPR);
  struct lval *v = lval_pop(x, 0);
  struct lval *q = lval_pop(x, 0);
  q = lval_add_front(q, v);
  lval_del(x);
  return q;
}
struct lval *builtin_def(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_TYPE("def", a, 0, LVAL_QEXPR);

  /* First argument is symbol list */
  struct lval *syms = a->cell[0];

  /* Ensure all elements of first list are symbols */
  for (int i = 0; i < syms->count; i++) {
      LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
              "Function 'def' cannot define non-symbol. "
              "Got %s, Expected %s.",
              ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
  }

  /* Check correct number of symbols and values */
  LASSERT(a, (syms->count == a->count-1),
    "Function 'def' passed too many arguments for symbols. "
    "Got %i, Expected %i.",
    syms->count, a->count-1);

  /* Assign copies of values to symbols */
  for (int i = 0; i < syms->count; i++) {
    lenv_put(e, syms->cell[i], a->cell[i + 1]);
  }

  lval_del(a);
  return lval_sexpr();
}
struct lval *builtin(struct lenv *e, struct lval *a, char *func) {
  if (strcmp("list", func) == 0) {
    return builtin_list(e, a);
  }
  if (strcmp("head", func) == 0) {
    return builtin_head(e, a);
  }
  if (strcmp("tail", func) == 0) {
    return builtin_tail(e, a);
  }
  if (strcmp("join", func) == 0) {
    return builtin_join(e, a);
  }
  if (strcmp("cons", func) == 0) {
    return builtin_cons(e, a);
  }
  if (strcmp("len", func) == 0) {
    return builtin_len(e, a);
  }
  if (strcmp("init", func) == 0) {
    return builtin_init(e, a);
  }

  if (strcmp("eval", func) == 0) {
    return builtin_eval(e, a);
  }
  if (strstr("+-/*", func)) {
    return builtin_op(e, a, func);
  }
  lval_del(a);
  return lval_err("Unknown Function!");
}
void lenv_add_builtin(struct lenv *e, char *name, lbuiltin func) {
  struct lval *k = lval_sym(name);
  struct lval *v = lval_func(func);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}
struct lval *lval_eval_sexpr(struct lenv *e, struct lval *v) {

  for (int i = 0; i < v->count; ++i) {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }

  for (int i = 0; i < v->count; ++i) {
    if (v->cell[i]->type == LVAL_ERR) {
      return lval_take(v, i);
    }
  }

  if (v->count == 0) {
    return v;
  }

  if (v->count == 1) {
    return lval_take(v, 0);
  }

  struct lval *f = lval_pop(v, 0);

  if (f->type != LVAL_FUN) {
      struct lval* err = lval_err(
      "S-Expression starts with incorrect type. "
      "Got %s, Expected %s.",
      ltype_name(f->type), ltype_name(LVAL_FUN));
    lval_del(f); lval_del(v);
    return err;
  }

  // struct lval *result = builtin(t, f->sym);
  struct lval *result = f->fun(e, v); // builtin(t, f->sym);
  lval_del(f);
  return result;
}
struct lval *lval_copy(struct lval *v) {
  struct lval *x = malloc(sizeof(struct lval));
  x->type = v->type;
  switch (v->type) {
  case LVAL_FUN:
    x->fun = v->fun;
    break;
  case LVAL_NUM:
    x->num = v->num;
    break;
  case LVAL_ERR:
    x->err = malloc(strlen(v->err) + 1);
    strcpy(x->err, v->err);
    break;
  case LVAL_SYM:
    x->sym = malloc(strlen(v->sym) + 1);
    strcpy(x->sym, v->sym);
    break;

  case LVAL_SEXPR:
  case LVAL_QEXPR:
    x->count = v->count;
    x->cell = malloc(sizeof(struct lval) * v->count);

    for (int i = 0; i < v->count; ++i) {
      x->cell[i] = lval_copy(v->cell[i]);
    }
    break;
  }
  return x;
}
struct lval *lenv_get(struct lenv *e, struct lval *v) {
  for (int i = 0; i < e->count; ++i) {
    if (strcmp(e->syms[i], v->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }
  return lval_err("Unbound symbol '%s'", v->sym);
}
struct lval *lval_eval(struct lenv *e, struct lval *t) {
  if (t->type == LVAL_SYM) {
    struct lval *x = lenv_get(e, t);
    lval_del(t);
    return x;
  }
  if (t->type == LVAL_SEXPR) {
    return lval_eval_sexpr(e, t);
  }
  return t;
}
void lenv_put(struct lenv *e, struct lval *d, struct lval *s) {
  // see if variable already exists
  for (int i = 0; i < e->count; ++i) {
    if (strcmp(e->syms[i], d->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(s);
      return;
    }
  }

  e->count++;
  e->vals = realloc(e->vals, sizeof(struct lval *) * e->count);
  e->syms = realloc(e->syms, sizeof(char *) * e->count);
  e->vals[e->count - 1] = lval_copy(s);
  e->syms[e->count - 1] = malloc(strlen(d->sym) + 1);
  strcpy(e->syms[e->count - 1], d->sym);
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
struct lval *lval_func(lbuiltin fun) {
  struct lval *x = malloc(sizeof(struct lval));
  x->type = LVAL_FUN;
  x->fun = fun;
  return x;
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
  case LVAL_FUN:
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
void lenv_add_builtins(struct lenv *e) {
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  /* Mathematical Functions */
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
  lenv_add_builtin(e, "def", builtin_def);
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
            symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/;                  \
            sexpr    : '(' <expr>* ')';                         \
            qexpr    : '{' <expr>* '}';                         \
            expr     : <number> | <symbol> | <sexpr> | <qexpr>;           \
            lispy    : /^/ <expr>* /$/ ;                        \
            ",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  puts("Lipsy version 0.0.0.0.1");
  puts("Press CTRL+c to exit\n");
  struct lenv *e = lenv_new();
  lenv_add_builtins(e);
  while (1) {
    char *input = readline("lipsy-> ");

    add_history(input);

    mpc_result_t r;

    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      mpc_ast_print(r.output);
      // lval_println(eval(r.output));
      struct lval *l = lval_eval(e, lval_read(r.output));
      // struct lval *l = lval_read(r.output);
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
  lenv_del(e);
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
  return 0;
}

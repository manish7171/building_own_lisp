/* ========================================================================
 * Lispy - chapter 11 (variables)
 *
 * Layout:
 *   1. Includes
 *   2. Platform shim (Windows readline)
 *   3. Types
 *   4. Macros
 *   5. Function prototypes
 *   6. Definitions
 *        constructors -> memory/list helpers -> environment -> printing
 *        -> reading -> builtins -> evaluation -> main
 * ======================================================================== */

/* ---------- 1. Includes ------------------------------------------------ */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpc.h"

/* ---------- 2. Platform shim ------------------------------------------- */

/* If we are compiling on Windows, provide our own readline/add_history */
#ifdef _WIN32

#define SIZE 2048

static char buffer[SIZE];

/* Fake readline */
char *readline(char *prompt) {
  fputs(prompt, stdout);
  fgets(buffer, SIZE, stdin);

  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\0';
  return cpy;
}

void add_history(char *unused) {}

#else
#include <editline/readline.h>
#endif

/* ---------- 3. Types --------------------------------------------------- */

enum {
  LVAL_ERR,
  LVAL_NUM,
  LVAL_SYM,
  LVAL_STR,
  LVAL_FUN,
  LVAL_SEXPR,
  LVAL_QEXPR,
  LVAL_OK,
  LVAL_FILE
};

/* Parser Declariations */
mpc_parser_t *Number;
mpc_parser_t *Symbol;
mpc_parser_t *String;
mpc_parser_t *Comment;
mpc_parser_t *Sexpr;
mpc_parser_t *Qexpr;
mpc_parser_t *Expr;
mpc_parser_t *Lispy;
struct lval;
struct lenv;

typedef struct lval *(*lbuiltin)(struct lenv *, struct lval *);

struct lenv {
  struct lenv *parent;
  int count;
  char **syms;
  struct lval **vals;
};

struct lval {
  int type;

  /* Basic */
  long num;
  char *err;
  char *sym;
  char *str;
  FILE *file;

  /* Function */
  lbuiltin builtin;
  struct lenv *env;
  struct lval *formals;
  struct lval *body;
  char *func_name;

  /* Expressions */
  int count;
  struct lval **cell;
};

/* ---------- 4. Macros -------------------------------------------------- */

#define LASSERT(args, cond, fmt, ...)                                          \
  if (!(cond)) {                                                               \
    struct lval *err = lval_err(fmt, ##__VA_ARGS__);                           \
    lval_del(args);                                                            \
    return err;                                                                \
  }

#define LASSERT_ARGS_NUM(func, args, num)                                      \
  LASSERT(args, args->count == num,                                            \
          "Function '%s' passed incorrect number of arguments. Got %i, "       \
          "Expected %i.",                                                      \
          func, args->count, num)

#define LASSERT_ARGS_TYPE(func, args, index, expect)                           \
  LASSERT((args), (args)->cell[index]->type == (expect),                       \
          "Function '%s' passed incorrect type "                               \
          "for argument %i, Got %s, Expected %s",                              \
          func, index, ltype_name(args->cell[index]->type),                    \
          ltype_name(expect))

#define LASSERT_EMPTY(func, args, index)                                       \
  LASSERT(args, args->cell[index]->count != 0,                                 \
          "Function '%s' passed {} for argument %i.", func, index);

/* ---------- 5. Prototypes ---------------------------------------------- */

/* Utilities */
char *ltype_name(int t);

/* Constructors */
struct lval *lval_num(long x);
struct lval *lval_err(char *fmt, ...);
struct lval *lval_sym(char *sym);
struct lval *lval_func(lbuiltin fun, char *name);
struct lval *lval_sexpr(void);
struct lval *lval_qexpr(void);
struct lval *lval_ok(void);
struct lval *lval_str(char *s);
struct lval *lval_file(FILE *f);

/* Memory and list helpers */
void lval_del(struct lval *l);
struct lval *lval_copy(struct lval *v);
struct lval *lval_add(struct lval *x, struct lval *y);
struct lval *lval_add_front(struct lval *q, struct lval *v);
struct lval *lval_pop(struct lval *v, int i);
struct lval *lval_take(struct lval *v, int i);
struct lval *lval_join(struct lval *x, struct lval *y);
struct lval *lval_lambda(struct lval *formals, struct lval *body);
int lval_eq(struct lval *x, struct lval *y);

/* Environment */
struct lenv *lenv_new();
void lenv_del(struct lenv *e);
struct lval *lenv_get(struct lenv *e, struct lval *v);
struct lenv *lenv_copy(struct lenv *e);
void lenv_def(struct lenv *e, struct lval *k, struct lval *v);
void lenv_put(struct lenv *e, struct lval *k, struct lval *v);
void lenv_add_builtin(struct lenv *e, char *name, lbuiltin func);
void lenv_add_builtins(struct lenv *e);
void lenv_add_file(struct lenv *e, char *name, FILE *f);

/* Printing */
void lval_print_expr(struct lval *v, char start, char end);
void lval_print(struct lval *v);
void lval_println(struct lval *v);
void lval_print_str(struct lval *v);

/* Reading */
struct lval *lval_read_num(mpc_ast_t *t);
struct lval *lval_read(mpc_ast_t *t);
struct lval *lval_read_str(mpc_ast_t *t);

/* Builtins */
struct lval *builtin_print(struct lenv *e, struct lval *a);
struct lval *builtin_error(struct lenv *e, struct lval *a);
struct lval *builtin_load(struct lenv *e, struct lval *a);
struct lval *builtin_op(struct lenv *e, struct lval *v, char *op);
struct lval *builtin_add(struct lenv *e, struct lval *v);
struct lval *builtin_sub(struct lenv *e, struct lval *v);
struct lval *builtin_mul(struct lenv *e, struct lval *v);
struct lval *builtin_div(struct lenv *e, struct lval *v);
struct lval *builtin_list(struct lenv *e, struct lval *v);
struct lval *builtin_head(struct lenv *e, struct lval *v);
struct lval *builtin_tail(struct lenv *e, struct lval *v);
struct lval *builtin_eval(struct lenv *e, struct lval *v);
struct lval *builtin_join(struct lenv *e, struct lval *v);
struct lval *builtin_cons(struct lenv *e, struct lval *v);
struct lval *builtin_len(struct lenv *e, struct lval *v);
struct lval *builtin_init(struct lenv *e, struct lval *v);
struct lval *builtin_lambda(struct lenv *e, struct lval *v);
struct lval *builtin_def(struct lenv *e, struct lval *v);
struct lval *builtin_put(struct lenv *e, struct lval *v);
struct lval *builtin_var(struct lenv *e, struct lval *a, char *func);
struct lval *builtin_exit(struct lenv *e, struct lval *v);
struct lval *builtin_gt(struct lenv *e, struct lval *a);
struct lval *builtin_lt(struct lenv *e, struct lval *a);
struct lval *builtin_ge(struct lenv *e, struct lval *a);
struct lval *builtin_le(struct lenv *e, struct lval *a);
struct lval *builtin_le(struct lenv *e, struct lval *a);
struct lval *builtin_eq(struct lenv *e, struct lval *a);
struct lval *builtin_ne(struct lenv *e, struct lval *a);
struct lval *builtin_ord(struct lenv *e, struct lval *a, char *op);
struct lval *builtin_cmp(struct lenv *e, struct lval *a, char *op);
struct lval *builtin_if(struct lenv *e, struct lval *v);
struct lval *builtin_or(struct lenv *e, struct lval *v);
struct lval *builtin_and(struct lenv *e, struct lval *v);
struct lval *builtin_not(struct lenv *e, struct lval *v);
struct lval *builtin_read(struct lenv *e, struct lval *v);
struct lval *builtin_show(struct lenv *e, struct lval *a);
struct lval *builtin_fopen(struct lenv *e, struct lval *a);
struct lval *builtin_fclose(struct lenv *e, struct lval *a);
struct lval *builtin_fgets(struct lenv *e, struct lval *a);
struct lval *builtin_fread(struct lenv *e, struct lval *a);
struct lval *builtin_fgetc(struct lenv *e, struct lval *a);
struct lval *builtin_fputs(struct lenv *e, struct lval *a);
struct lval *builtin_fputc(struct lenv *e, struct lval *a);
struct lval *builtin_feof(struct lenv *e, struct lval *a);
struct lval *builtin_fflush(struct lenv *e, struct lval *a);
struct lval *builtin_fseek(struct lenv *e, struct lval *a);
struct lval *builtin_ftell(struct lenv *e, struct lval *a);
struct lval *builtin_rewind(struct lenv *e, struct lval *a);
struct lval *builtin_remove(struct lenv *e, struct lval *a);
struct lval *builtin_rename(struct lenv *e, struct lval *a);

/* Evaluation */
struct lval *lval_eval(struct lenv *e, struct lval *t);
struct lval *lval_eval_sexpr(struct lenv *e, struct lval *v);

/* ---------- 6. Definitions --------------------------------------------- */

/* ----- Utilities ----- */

char *ltype_name(int t) {
  switch (t) {
  case LVAL_FUN:
    return "Function";
  case LVAL_NUM:
    return "Number";
  case LVAL_ERR:
    return "Error";
  case LVAL_SYM:
    return "Symbol";
  case LVAL_STR:
    return "String";
  case LVAL_OK:
    return "Ok";
  case LVAL_FILE:
    return "file";
  case LVAL_SEXPR:
    return "S-Expression";
  case LVAL_QEXPR:
    return "Q-Expression";
  default:
    return "Unknown";
  }
}

/* ----- Constructors ----- */

struct lval *lval_num(long x) {
  struct lval *l = malloc(sizeof(struct lval));
  l->type = LVAL_NUM;
  l->num = x;
  return l;
}

struct lval *lval_err(char *fmt, ...) {
  struct lval *l = malloc(sizeof(struct lval));
  l->type = LVAL_ERR;

  /* Create a va list and initialize it */
  va_list va;
  va_start(va, fmt);

  /* Allocate 512 bytes of space */
  l->err = malloc(512);

  vsnprintf(l->err, 511, fmt, va);

  l->err = realloc(l->err, strlen(l->err) + 1);
  va_end(va);
  return l;
}

struct lval *lval_sym(char *sym) {
  struct lval *l = malloc(sizeof(struct lval));
  l->type = LVAL_SYM;
  l->sym = malloc(strlen(sym) + 1);
  strcpy(l->sym, sym);
  return l;
}

struct lval *lval_func(lbuiltin fun, char *name) {
  struct lval *x = malloc(sizeof(struct lval));
  x->type = LVAL_FUN;
  x->builtin = fun;
  x->func_name = malloc(strlen(name) + 1);
  strcpy(x->func_name, name);
  return x;
}

struct lval *lval_lambda(struct lval *formals, struct lval *body) {
  struct lval *v = malloc(sizeof(struct lval));
  v->type = LVAL_FUN;
  v->func_name = NULL;
  /* Set builtin to NULL*/
  v->builtin = NULL;

  /* Build new environment*/
  v->env = lenv_new();

  /* Set formals and body*/
  v->formals = formals;
  v->body = body;

  return v;
}

struct lval *lval_sexpr(void) {
  struct lval *l = malloc(sizeof(struct lval));
  l->type = LVAL_SEXPR;
  l->count = 0;
  l->cell = NULL;
  return l;
}

struct lval *lval_qexpr(void) {
  struct lval *l = malloc(sizeof(struct lval));
  l->type = LVAL_QEXPR;
  l->count = 0;
  l->cell = NULL;
  return l;
}

struct lenv *lenv_new() {
  struct lenv *e = malloc(sizeof(struct lenv));
  e->parent = NULL;
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

struct lval *lval_str(char *s) {
  struct lval *v = malloc(sizeof(struct lval));
  v->type = LVAL_STR;
  v->str = malloc(strlen(s) + 1);
  strcpy(v->str, s);
  return v;
}

struct lval *lval_ok() {
  struct lval *v = malloc(sizeof(struct lval));
  v->type = LVAL_OK;
  return v;
}

struct lval *lval_file(FILE *f) {
  struct lval *v = malloc(sizeof(struct lval));
  v->type = LVAL_FILE;
  v->file = f;
  return v;
}
/* ----- Memory and list helpers ----- */

void lval_del(struct lval *l) {
  switch (l->type) {
  case LVAL_NUM:
    break;
  case LVAL_FUN:
    if (!l->builtin) {
      lenv_del(l->env);
      lval_del(l->formals);
      lval_del(l->body);
    }
    free(l->func_name);
    break;
  case LVAL_SYM:
    free(l->sym);
    break;
  case LVAL_STR:
    free(l->str);
    break;
  case LVAL_ERR:
    free(l->err);
    break;
  case LVAL_OK:
    break;
  case LVAL_FILE:
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

struct lval *lval_copy(struct lval *v) {
  struct lval *x = malloc(sizeof(struct lval));
  x->type = v->type;

  switch (v->type) {
  case LVAL_FUN:
    if (v->builtin) {
      x->builtin = v->builtin;
    } else {
      x->builtin = NULL;
      x->env = lenv_copy(v->env);
      x->formals = lval_copy(v->formals);
      x->body = lval_copy(v->body);
    }
    if (v->func_name != NULL) {
      x->func_name = malloc(strlen(v->func_name) + 1);
      strcpy(x->func_name, v->func_name);
    } else {
      x->func_name = NULL;
    }
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
  case LVAL_STR:
    x->str = malloc(strlen(v->str) + 1);
    strcpy(x->str, v->str);
    break;
  case LVAL_OK:
    break;
  case LVAL_FILE:
    x->file = v->file;
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

struct lenv *lenv_copy(struct lenv *e) {
  struct lenv *n = malloc(sizeof(struct lenv));
  n->parent = e->parent;
  n->count = e->count;
  n->syms = malloc(sizeof(char *) * n->count);
  n->vals = malloc(sizeof(struct lval *) * n->count);

  for (int i = 0; i < e->count; ++i) {
    n->syms[i] = malloc(strlen(e->syms[i]) + 1);
    strcpy(n->syms[i], e->syms[i]);
    n->vals[i] = lval_copy(e->vals[i]);
  }
  return n;
}

void lenv_def(struct lenv *e, struct lval *k, struct lval *v) {
  /* Iterate till e has no parent */
  while (e->parent) {
    e = e->parent;
  }
  lenv_put(e, k, v);
}

struct lval *lval_add(struct lval *x, struct lval *y) {
  x->count++;
  x->cell = realloc(x->cell, sizeof(struct lval *) * x->count);
  x->cell[x->count - 1] = y;
  return x;
}

struct lval *lval_add_front(struct lval *q, struct lval *v) {
  q->count++;
  q->cell = realloc(q->cell, sizeof(struct lval *) * q->count);
  memmove(&q->cell[1], &q->cell[0], sizeof(struct lval *) * (q->count - 1));
  q->cell[0] = v;
  return q;
}

struct lval *lval_pop(struct lval *v, int i) {
  struct lval *x = v->cell[i];
  memmove(&v->cell[i], &v->cell[i + 1],
          sizeof(struct lval *) * (v->count - i - 1));
  v->count--;
  v->cell = realloc(v->cell, sizeof(struct lval *) * v->count);
  return x;
}

struct lval *lval_take(struct lval *v, int i) {
  struct lval *e = lval_pop(v, i);
  lval_del(v);
  return e;
}

struct lval *lval_join(struct lval *x, struct lval *y) {
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  lval_del(y);
  return x;
}

struct lval *lval_call(struct lenv *e, struct lval *f, struct lval *a) {

  /* If Builtin then simply apply that */
  if (f->builtin) {
    return f->builtin(e, a);
  }

  /* Record Argument Counts */
  int given = a->count;
  int total = f->formals->count;

  /* While arguments still remain to be processed */
  while (a->count) {

    /* If we've ran out of formal arguments to bind */
    if (f->formals->count == 0) {
      lval_del(a);
      return lval_err("Function passed too many arguments. "
                      "Got %i, Expected %i.",
                      given, total);
    }

    /* Pop the first symbol from the formals */
    struct lval *sym = lval_pop(f->formals, 0);

    /* Special Case to deal with '&' */
    if (strcmp(sym->sym, "&") == 0) {

      /* Ensure '&' is followed by another symbol */
      if (f->formals->count != 1) {
        lval_del(a);
        return lval_err("Function format invalid. "
                        "Symbol '&' not followed by single symbol.");
      }

      /* Next formal should be bound to remaining arguments */
      struct lval *nsym = lval_pop(f->formals, 0);
      lenv_put(f->env, nsym, builtin_list(e, a));
      lval_del(sym);
      lval_del(nsym);
      break;
    }

    /* Pop the next argument from the list */
    struct lval *val = lval_pop(a, 0);

    /* Bind a copy into the function's environment */
    lenv_put(f->env, sym, val);

    /* Delete symbol and value */
    lval_del(sym);
    lval_del(val);
  }

  /* Argument list is now bound so can be cleaned up */
  lval_del(a);

  /* If '&' remains in formal list bind to empty list */
  if (f->formals->count > 0 && strcmp(f->formals->cell[0]->sym, "&") == 0) {

    /* Check to ensure that & is not passed invalidly. */
    if (f->formals->count != 2) {
      return lval_err("Function format invalid. "
                      "Symbol '&' not followed by single symbol.");
    }

    /* Pop and delete '&' symbol */
    lval_del(lval_pop(f->formals, 0));

    /* Pop next symbol and create empty list */
    struct lval *sym = lval_pop(f->formals, 0);
    struct lval *val = lval_qexpr();

    /* Bind to environment and delete */
    lenv_put(f->env, sym, val);
    lval_del(sym);
    lval_del(val);
  }

  /* If all formals have been bound evaluate */
  if (f->formals->count == 0) {

    /* Set environment parent to evaluation environment */
    f->env->parent = e;

    /* Evaluate and return */
    return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
  } else {
    /* Otherwise return partially evaluated function */
    return lval_copy(f);
  }
}

int lval_eq(struct lval *x, struct lval *y) {
  /* Different Types are always unequal */
  if (x->type != y->type) {
    return 0;
  }

  /* Compare Based upon type */
  switch (x->type) {
  case LVAL_NUM:
    return x->num == y->num;

  case LVAL_ERR:
    return (strcmp(x->err, y->err) == 0);
  case LVAL_SYM:
    return (strcmp(x->sym, y->sym) == 0);
  case LVAL_STR:
    return (strcmp(x->str, y->str) == 0);

  case LVAL_FUN:
    if (x->builtin || y->builtin) {
      return x->builtin == y->builtin;
    } else {
      return lval_eq(x->formals, y->formals) && lval_eq(x->formals, y->formals);
    }

  case LVAL_OK:
    break;

  case LVAL_FILE:
    return x->file == y->file;
  case LVAL_QEXPR:
  case LVAL_SEXPR:
    if (x->count != y->count) {
      return 0;
    }
    for (int i = 0; i < x->count; i++) {
      /* If any element not equal then whole list not equal */
      if (!(lval_eq(x->cell[i], y->cell[i]))) {
        return 0;
      }
    }
    return 1;
    break;
  }
  return 0;
}

struct lval *builtin_error(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("error", a, 1);
  LASSERT_ARGS_TYPE("error", a, 0, LVAL_STR);

  /* Construct Error from first argument */
  struct lval *err = lval_err(a->cell[0]->str);

  /* Delete arguments and return */
  lval_del(a);
  return err;
}

/* ----- Environment ----- */

void lenv_del(struct lenv *e) {
  for (int i = 0; i < e->count; ++i) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

struct lval *lenv_get(struct lenv *e, struct lval *v) {
  for (int i = 0; i < e->count; ++i) {
    if (strcmp(e->syms[i], v->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }
  /* If no symbol found then check in parent otherwise error */
  if (e->parent) {
    return lenv_get(e->parent, v);
  } else {
    return lval_err("Unbound symbol '%s'", v->sym);
  }
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

void lenv_add_builtin(struct lenv *e, char *name, lbuiltin func) {
  struct lval *k = lval_sym(name);
  struct lval *v = lval_func(func, name);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

void lenv_add_builtins(struct lenv *e) {
  /* List functions */
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "cons", builtin_cons);
  lenv_add_builtin(e, "len", builtin_len);
  lenv_add_builtin(e, "init", builtin_init);
  lenv_add_builtin(e, "read", builtin_read);
  lenv_add_builtin(e, "show", builtin_show);
  lenv_add_builtin(e, "\\", builtin_lambda);
  lenv_add_builtin(e, "if", builtin_if);
  lenv_add_builtin(e, "||", builtin_or);
  lenv_add_builtin(e, "&&", builtin_and);
  lenv_add_builtin(e, "!", builtin_not);
  lenv_add_builtin(e, ">", builtin_gt);
  lenv_add_builtin(e, "<", builtin_lt);
  lenv_add_builtin(e, ">=", builtin_gt);
  lenv_add_builtin(e, "<=", builtin_lt);
  lenv_add_builtin(e, "==", builtin_eq);
  lenv_add_builtin(e, "!=", builtin_ne);
  /* Mathematical functions */
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);

  /* Variable and system functions */
  lenv_add_builtin(e, "def", builtin_def);
  lenv_add_builtin(e, "=", builtin_put);
  lenv_add_builtin(e, "exit", builtin_exit);
  /* String Functions */
  lenv_add_builtin(e, "load", builtin_load);
  lenv_add_builtin(e, "error", builtin_error);
  lenv_add_builtin(e, "print", builtin_print);
/* File functions */
  lenv_add_builtin(e, "fopen", builtin_fopen);
  lenv_add_builtin(e, "fclose", builtin_fclose);
  lenv_add_builtin(e, "fgets", builtin_fgets);
  lenv_add_builtin(e, "fread", builtin_fread);
  lenv_add_builtin(e, "fgetc", builtin_fgetc);
  lenv_add_builtin(e, "fputs", builtin_fputs);
  lenv_add_builtin(e, "fputc", builtin_fputc);
  lenv_add_builtin(e, "feof", builtin_feof);
  lenv_add_builtin(e, "fflush", builtin_fflush);
  lenv_add_builtin(e, "fseek", builtin_fseek);
  lenv_add_builtin(e, "ftell", builtin_ftell);
  lenv_add_builtin(e, "rewind", builtin_rewind);
  lenv_add_builtin(e, "remove", builtin_remove);
  lenv_add_builtin(e, "rename", builtin_rename);

  lenv_add_file(e, "stdin", stdin);
  lenv_add_file(e, "stdout", stdout);
  lenv_add_file(e, "stderr", stderr);
}

void lenv_add_file(struct lenv *e, char *name, FILE *f) {
  struct lval *k = lval_sym(name);
  struct lval *v = lval_file(f);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}
/* ----- Printing ----- */

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
  case LVAL_STR:
    lval_print_str(v);
    break;
  case LVAL_FUN:
    if (v->builtin) {
      printf("<function> %s", v->func_name);
    } else {
      printf("(\\ ");
      lval_print(v->formals);
      putchar(' ');
      lval_print(v->body);
      putchar(')');
    }
    break;
  case LVAL_OK:
    printf("%s", "Ok");
    break;
  case LVAL_FILE:
    printf("<file>");
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

void lval_print_str(struct lval *v) {
  char *escaped = malloc(strlen(v->str) + 1);
  strcpy(escaped, v->str);

  escaped = mpcf_escape(escaped);
  printf("\"%s\"", escaped);
  free(escaped);
}

struct lval *builtin_print(struct lenv *e, struct lval *a) {

  /* Print each argument followed by a space */
  for (int i = 0; i < a->count; i++) {
    lval_print(a->cell[i]);
    putchar(' ');
  }

  /* Print a newline and delete arguments */
  putchar('\n');
  lval_del(a);

  return lval_ok();
}

/* ----- Reading (AST -> lval) ----- */

struct lval *lval_read_num(mpc_ast_t *t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return (x != ERANGE) ? lval_num(x) : lval_err("invalid number");
}

struct lval *lval_read(mpc_ast_t *t) {
  if (strstr(t->tag, "number")) {
    return lval_read_num(t);
  }

  if (strstr(t->tag, "string")) {
    return lval_read_str(t);
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

    if (strstr(t->children[i]->tag, "comment")) {
      continue;
    }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

struct lval *lval_read_str(mpc_ast_t *t) {
  /* Cut off the final quote character */
  t->contents[strlen(t->contents) - 1] = '\0';
  /* Copy the string missing out the first quote character */
  char *unescaped = malloc(strlen(t->contents + 1) + 1);
  strcpy(unescaped, t->contents + 1);

  /* Pass through the unescape function */
  unescaped = mpcf_unescape(unescaped);
  /* Construct a new lval using the string */
  struct lval *str = lval_str(unescaped);
  /* Free the string and return */
  free(unescaped);
  return str;
}
/* ----- Builtins: math ----- */

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

/* ----- Builtins: lists ----- */

struct lval *builtin_list(struct lenv *e, struct lval *v) {
  v->type = LVAL_QEXPR;
  return v;
}

struct lval *builtin_head(struct lenv *e, struct lval *v) {
  LASSERT_ARGS_NUM("head", v, 1);
  // LASSERT_ARGS_TYPE("head", v, 0, LVAL_QEXPR);
  // LASSERT_EMPTY("head", v, 0);
  /* The first argument decides what we are joining */
  int type = v->cell[0]->type;
  LASSERT(v, type == LVAL_QEXPR || type == LVAL_STR,
          "Function 'join' passed incorrect type for argument 0. "
          "Got %s, Expected %s or %s.",
          ltype_name(type), ltype_name(LVAL_QEXPR), ltype_name(LVAL_STR));

  if (type == LVAL_STR) {
    LASSERT(v, strlen(v->cell[0]->str) > 0,
            "Function 'head' passed empty string for argument 0.");

    struct lval *a = lval_take(v, 0);
    a->str[1] = '\0';
    a->str = realloc(a->str, 2);
    return a;
  }

  // otherwise take the first argument
  struct lval *a = lval_take(v, 0);
  while (a->count > 1) {
    lval_del(lval_pop(a, 1));
  }

  return a;
}

struct lval *builtin_tail(struct lenv *e, struct lval *v) {
  LASSERT_ARGS_NUM("tail", v, 1);

  int type = v->cell[0]->type;
  LASSERT(v, type == LVAL_QEXPR || type == LVAL_STR,
          "Function 'tail' passed incorrect type for argument 0. "
          "Got %s, Expected %s or %s.",
          ltype_name(type), ltype_name(LVAL_QEXPR), ltype_name(LVAL_STR));

  if (type == LVAL_STR) {
    LASSERT(v, strlen(v->cell[0]->str) > 0,
            "Function 'tail' passed empty string for argument 0.");

    struct lval *a = lval_take(v, 0);
    size_t len = strlen(a->str);
    memmove(a->str, a->str + 1, len);
    a->str = realloc(a->str, len);
    return a;
  }
  struct lval *a = lval_take(v, 0);
  lval_del(lval_pop(a, 0));
  return a;
}

struct lval *builtin_eval(struct lenv *e, struct lval *v) {
  LASSERT_ARGS_NUM("eval", v, 1);
  LASSERT_ARGS_TYPE("eval", v, 0, LVAL_QEXPR);
  struct lval *x = lval_take(v, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

struct lval *builtin_join(struct lenv *e, struct lval *a) {
  LASSERT(a, a->count >= 1, "Function 'join' passed no arguments.");

  /* The first argument decides what we are joining */
  int type = a->cell[0]->type;
  LASSERT(a, type == LVAL_QEXPR || type == LVAL_STR,
          "Function 'join' passed incorrect type for argument 0. "
          "Got %s, Expected %s or %s.",
          ltype_name(type), ltype_name(LVAL_QEXPR), ltype_name(LVAL_STR));

  /* Every argument must have that same type */
  for (int i = 0; i < a->count; i++) {
    LASSERT_ARGS_TYPE("join", a, i, type);
  }

  struct lval *x = lval_pop(a, 0);

  if (type == LVAL_STR) {
    /* Strings: append each following string onto x */
    while (a->count > 0) {
      struct lval *y = lval_pop(a, 0);
      x->str = realloc(x->str, strlen(x->str) + strlen(y->str) + 1);
      strcat(x->str, y->str);
      lval_del(y);
    }
  } else {
    /* Q-expressions: same as before */
    while (a->count > 0) {
      x = lval_join(x, lval_pop(a, 0));
    }
  }

  lval_del(a);
  return x;
}

struct lval *builtin_cons(struct lenv *e, struct lval *x) {
  LASSERT_ARGS_NUM("cons", x, 2);
  LASSERT_ARGS_TYPE("cons", x, 0, LVAL_NUM);
  LASSERT_ARGS_TYPE("cons", x, 1, LVAL_QEXPR);

  struct lval *v = lval_pop(x, 0);
  struct lval *q = lval_pop(x, 0);

  q = lval_add_front(q, v);
  lval_del(x);
  return q;
}

struct lval *builtin_len(struct lenv *e, struct lval *x) {
  LASSERT_ARGS_NUM("len", x, 1);
  LASSERT_ARGS_TYPE("len", x, 0, LVAL_QEXPR);

  struct lval *q = lval_pop(x, 0);
  struct lval *result = lval_num(q->count);

  lval_del(q);
  lval_del(x);

  return result;
}

struct lval *builtin_init(struct lenv *e, struct lval *x) {
  LASSERT_ARGS_NUM("init", x, 1);
  LASSERT_ARGS_TYPE("init", x, 0, LVAL_QEXPR);
  LASSERT_EMPTY("init", x, 0);

  struct lval *q = lval_pop(x, 0);
  lval_del(x);

  lval_del(lval_pop(q, q->count - 1));
  return q;
}

struct lval *builtin_lambda(struct lenv *e, struct lval *a) {
  /* Check Two arguments, each of which are Q-Expressions*/
  LASSERT_ARGS_NUM("\\", a, 2);
  LASSERT_ARGS_TYPE("\\", a, 0, LVAL_QEXPR);
  LASSERT_ARGS_TYPE("\\", a, 1, LVAL_QEXPR);

  /* Check first Q-Expression contains only symbols*/
  for (int i = 0; i < a->cell[0]->count; ++i) {
    LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "Cannot define non-symbol. Got %s, Expected %s",
            ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
  }

  /* Pop first two arguments and pass them to lval_lamda*/
  struct lval *formals = lval_pop(a, 0);
  struct lval *body = lval_pop(a, 0);
  lval_del(a);

  return lval_lambda(formals, body);
}

struct lval *builtin_if(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("if", a, 3);
  LASSERT_ARGS_TYPE("if", a, 0, LVAL_NUM);
  LASSERT_ARGS_TYPE("if", a, 1, LVAL_QEXPR);
  LASSERT_ARGS_TYPE("if", a, 2, LVAL_QEXPR);

  struct lval *x;
  a->cell[1]->type = LVAL_SEXPR;
  a->cell[2]->type = LVAL_SEXPR;

  if (a->cell[0]->num) {
    x = lval_eval(e, lval_pop(a, 1));
  } else {
    x = lval_eval(e, lval_pop(a, 2));
  }
  lval_del(a);
  return x;
}

struct lval *builtin_or(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("||", a, 2);
  LASSERT_ARGS_TYPE("||", a, 0, LVAL_NUM);
  LASSERT_ARGS_TYPE("||", a, 1, LVAL_NUM);
  int x = (a->cell[0]->num || a->cell[1]->num);
  lval_del(a);
  return lval_num(x);
}

struct lval *builtin_and(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("&&", a, 2);
  LASSERT_ARGS_TYPE("&&", a, 0, LVAL_NUM);
  LASSERT_ARGS_TYPE("&&", a, 1, LVAL_NUM);
  int x = (a->cell[0]->num && a->cell[1]->num);
  lval_del(a);
  return lval_num(x);
}

struct lval *builtin_not(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("!", a, 1);
  LASSERT_ARGS_TYPE("!", a, 0, LVAL_NUM);
  int x = !(a->cell[0]->num);
  lval_del(a);
  return lval_num(x);
}

struct lval *builtin_read(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("read", a, 1);
  LASSERT_ARGS_TYPE("read", a, 0, LVAL_STR);

  mpc_result_t r;
  if (mpc_parse("<string>", a->cell[0]->str, Lispy, &r)) {
    struct lval *expr = lval_read(r.output);
    mpc_ast_delete(r.output);
    expr->type = LVAL_QEXPR;
    lval_del(a);
    return expr;
  } else {
    char *err_msg = mpc_err_string(r.error);
    mpc_err_delete(r.error);

    struct lval *err = lval_err("Could not read string %s", err_msg);
    free(err_msg);
    lval_del(a);
    return err;
  }
}

struct lval *builtin_show(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("show", a, 1);
  LASSERT_ARGS_TYPE("show", a, 0, LVAL_STR);
  printf("%s", a->cell[0]->str);
  lval_del(a);
  return lval_ok();
}

struct lval *builtin_fopen(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("fopen", a, 2);
  LASSERT_ARGS_TYPE("fopen", a, 0, LVAL_STR);
  LASSERT_ARGS_TYPE("fopen", a, 1, LVAL_STR);

  FILE *f = fopen(a->cell[0]->str, a->cell[1]->str);

  if (!f) {
    struct lval *err =
        lval_err("Could not open '%s': '%s', a->cell[0]->str, strerror(errno)");
    lval_del(a);
    return err;
  }
  lval_del(a);
  return lval_file(f);
}

struct lval *builtin_fclose(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("fclose", a, 1);
  LASSERT_ARGS_TYPE("fclose", a, 0, LVAL_FILE);

  int r = fclose(a->cell[0]->file);
  lval_del(a);

  if (!r) {
    return lval_err("Could not close file: %s', strerror(errno)");
  }

  return lval_ok();
}

/* Reads at most n-1 characters, stopping after a newline.
   Returns "" at end of file. */
struct lval *builtin_fgets(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("fgets", a, 2);
  LASSERT_ARGS_TYPE("fgets", a, 0, LVAL_FILE);
  LASSERT_ARGS_TYPE("fgets", a, 1, LVAL_NUM);
  long n = a->cell[1]->num;
  LASSERT(a, n > 1, "Function 'fgets' passed size %li, expected at least 2.",
          n);

  FILE *f = a->cell[0]->file;
  char *buf = malloc(n);
  struct lval *res;

  if (fgets(buf, n, f)) {
    res = lval_str(buf);
  } else if (ferror(f)) {
    res = lval_err("Could not read from file: %s", strerror(errno));
  } else {
    res = lval_str("");
  }
  free(buf);
  lval_del(a);
  return res;
}

/* Reads up to n bytes. Returns "" at end of file. */
struct lval *builtin_fread(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("fread", a, 2);
  LASSERT_ARGS_TYPE("fread", a, 0, LVAL_FILE);
  LASSERT_ARGS_TYPE("fread", a, 1, LVAL_NUM);
  long n = a->cell[1]->num;
  LASSERT(a, n > 0, "Function 'fread' passed size %li, expected at least 1.",
          n);

  FILE *f = a->cell[0]->file;
  char *buf = malloc(n + 1);
  size_t got = fread(buf, 1, n, f);
  buf[got] = '\0';

  struct lval *res;
  if (got == 0 && ferror(f)) {
    res = lval_err("Could not read from file: %s", strerror(errno));
  } else {
    res = lval_str(buf);
  }
  free(buf);
  lval_del(a);
  return res;
}

/* Returns the character code, or -1 at end of file. */
struct lval *builtin_fgetc(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("fgetc", a, 1);
  LASSERT_ARGS_TYPE("fgetc", a, 0, LVAL_FILE);
  int c = fgetc(a->cell[0]->file);
  lval_del(a);
  return lval_num(c);
}

struct lval *builtin_fputs(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("fputs", a, 2);
  LASSERT_ARGS_TYPE("fputs", a, 0, LVAL_FILE);
  LASSERT_ARGS_TYPE("fputs", a, 1, LVAL_STR);

  int r = fputs(a->cell[1]->str, a->cell[0]->file);
  int saved = errno;
  lval_del(a);
  if (r == EOF)
    return lval_err("Could not write to file: %s", strerror(saved));
  return lval_ok();
}

struct lval *builtin_fputc(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("fputc", a, 2);
  LASSERT_ARGS_TYPE("fputc", a, 0, LVAL_FILE);
  LASSERT_ARGS_TYPE("fputc", a, 1, LVAL_NUM);

  int r = fputc((int)a->cell[1]->num, a->cell[0]->file);
  int saved = errno;
  lval_del(a);
  if (r == EOF)
    return lval_err("Could not write to file: %s", strerror(saved));
  return lval_ok();
}

struct lval *builtin_feof(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("feof", a, 1);
  LASSERT_ARGS_TYPE("feof", a, 0, LVAL_FILE);
  int r = feof(a->cell[0]->file) != 0;
  lval_del(a);
  return lval_num(r);
}

struct lval *builtin_fflush(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("fflush", a, 1);
  LASSERT_ARGS_TYPE("fflush", a, 0, LVAL_FILE);
  int r = fflush(a->cell[0]->file);
  int saved = errno;
  lval_del(a);
  if (r != 0)
    return lval_err("Could not flush file: %s", strerror(saved));
  return lval_ok();
}

/* whence: 0 = from start, 1 = from current position, 2 = from end */
struct lval *builtin_fseek(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("fseek", a, 3);
  LASSERT_ARGS_TYPE("fseek", a, 0, LVAL_FILE);
  LASSERT_ARGS_TYPE("fseek", a, 1, LVAL_NUM);
  LASSERT_ARGS_TYPE("fseek", a, 2, LVAL_NUM);
  long w = a->cell[2]->num;
  LASSERT(a, w >= 0 && w <= 2,
          "Function 'fseek' passed whence %li, expected 0, 1 or 2.", w);

  int whence[] = {SEEK_SET, SEEK_CUR, SEEK_END};
  int r = fseek(a->cell[0]->file, a->cell[1]->num, whence[w]);
  int saved = errno;
  lval_del(a);
  if (r != 0)
    return lval_err("Could not seek in file: %s", strerror(saved));
  return lval_ok();
}

struct lval *builtin_ftell(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("ftell", a, 1);
  LASSERT_ARGS_TYPE("ftell", a, 0, LVAL_FILE);
  long pos = ftell(a->cell[0]->file);
  int saved = errno;
  lval_del(a);
  if (pos < 0)
    return lval_err("Could not get file position: %s", strerror(saved));
  return lval_num(pos);
}

struct lval *builtin_rewind(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("rewind", a, 1);
  LASSERT_ARGS_TYPE("rewind", a, 0, LVAL_FILE);
  rewind(a->cell[0]->file);
  lval_del(a);
  return lval_ok();
}

struct lval *builtin_remove(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("remove", a, 1);
  LASSERT_ARGS_TYPE("remove", a, 0, LVAL_STR);

  if (remove(a->cell[0]->str) != 0) {
    struct lval *err =
        lval_err("Could not remove '%s': %s", a->cell[0]->str, strerror(errno));
    lval_del(a);
    return err;
  }
  lval_del(a);
  return lval_ok();
}

struct lval *builtin_rename(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("rename", a, 2);
  LASSERT_ARGS_TYPE("rename", a, 0, LVAL_STR);
  LASSERT_ARGS_TYPE("rename", a, 1, LVAL_STR);

  if (rename(a->cell[0]->str, a->cell[1]->str) != 0) {
    struct lval *err =
        lval_err("Could not rename '%s': %s", a->cell[0]->str, strerror(errno));
    lval_del(a);
    return err;
  }
  lval_del(a);
  return lval_ok();
}
/* ----- Builtins: variables and system ----- */

struct lval *builtin_def(struct lenv *e, struct lval *a) {
  return builtin_var(e, a, "def");
}

struct lval *builtin_put(struct lenv *e, struct lval *a) {
  return builtin_var(e, a, "=");
}

struct lval *builtin_var(struct lenv *e, struct lval *a, char *func) {
  LASSERT_ARGS_TYPE(func, a, 0, LVAL_QEXPR);

  /* First argument is symbol list */
  struct lval *syms = a->cell[0];

  /* Ensure all elements of first list are symbols */
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
            "Function '%s' cannot define non-symbol. "
            "Got %s, Expected %s.",
            func, ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
  }

  /* Check correct number of symbols and values */
  LASSERT(a, (syms->count == a->count - 1),
          "Function 'def' passed too many arguments for symbols. "
          "Got %i, Expected %i.",
          syms->count, a->count - 1);

  /* Assign copies of values to symbols */
  for (int i = 0; i < syms->count; i++) {
    struct lval *existing = lenv_get(e, syms->cell[i]);
    if (existing->type == LVAL_FUN && existing->builtin) {
      struct lval *err =
          lval_err("Cannot redefine builtin '%s'", syms->cell[i]->sym);
      lval_del(existing);
      lval_del(a);
      return err;
    }
    if (strcmp(func, "def") == 0) {
      lenv_def(e, syms->cell[i], a->cell[i + 1]);
    } else {
      lenv_put(e, syms->cell[i], a->cell[i + 1]);
    }
  }

  lval_del(a);
  return lval_ok();
}

struct lval *builtin_exit(struct lenv *e, struct lval *a) {
  lenv_del(e);
  lval_del(a);
  exit(0);
  // return lval_err("exit");
}

struct lval *builtin_gt(struct lenv *e, struct lval *a) {
  return builtin_ord(e, a, ">");
}

struct lval *builtin_lt(struct lenv *e, struct lval *a) {
  return builtin_ord(e, a, "<");
}

struct lval *builtin_ge(struct lenv *e, struct lval *a) {
  return builtin_ord(e, a, ">=");
}

struct lval *builtin_le(struct lenv *e, struct lval *a) {
  return builtin_ord(e, a, "<=");
}

struct lval *builtin_ord(struct lenv *e, struct lval *a, char *op) {
  LASSERT_ARGS_NUM(op, a, 2);
  LASSERT_ARGS_TYPE(op, a, 0, LVAL_NUM);
  LASSERT_ARGS_TYPE(op, a, 1, LVAL_NUM);

  int r;
  if (strcmp(op, ">") == 0) {
    r = (a->cell[0]->num > a->cell[1]->num);
  }
  if (strcmp(op, "<") == 0) {
    r = (a->cell[0]->num < a->cell[1]->num);
  }
  if (strcmp(op, ">=") == 0) {
    r = (a->cell[0]->num >= a->cell[1]->num);
  }
  if (strcmp(op, "<=") == 0) {
    r = (a->cell[0]->num <= a->cell[1]->num);
  }
  lval_del(a);
  return lval_num(r);
}

struct lval *builtin_eq(struct lenv *e, struct lval *a) {
  return builtin_cmp(e, a, "==");
}

struct lval *builtin_ne(struct lenv *e, struct lval *a) {
  return builtin_cmp(e, a, "!=");
}

struct lval *builtin_cmp(struct lenv *e, struct lval *a, char *op) {
  LASSERT_ARGS_NUM(op, a, 2);
  int r;
  if (strcmp(op, "==") == 0) {
    r = lval_eq(a->cell[0], a->cell[1]);
  }
  if (strcmp(op, "!=") == 0) {
    r = !lval_eq(a->cell[0], a->cell[1]);
  }

  lval_del(a);
  return lval_num(r);
}

/* ----- Evaluation ----- */

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
    struct lval *err = lval_err("S-Expression starts with incorrect type. "
                                "Got %s, Expected %s.",
                                ltype_name(f->type), ltype_name(LVAL_FUN));
    lval_del(f);
    lval_del(v);
    return err;
  }

  struct lval *result = lval_call(e, f, v);
  lval_del(f);
  return result;
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

struct lval *builtin_load(struct lenv *e, struct lval *a) {
  LASSERT_ARGS_NUM("load", a, 1);
  LASSERT_ARGS_TYPE("load", a, 0, LVAL_STR);

  /* Parse File given by string name */
  mpc_result_t r;
  if (mpc_parse_contents(a->cell[0]->str, Lispy, &r)) {

    /* Read contents */
    struct lval *expr = lval_read(r.output);
    mpc_ast_delete(r.output);

    /* Evaluate each Expression */
    while (expr->count) {
      struct lval *x = lval_eval(e, lval_pop(expr, 0));
      /* If Evaluation leads to error print it */
      if (x->type == LVAL_ERR) {
        lval_println(x);
      }
      lval_del(x);
    }

    /* Delete expressions and arguments */
    lval_del(expr);
    lval_del(a);

    return lval_ok();

  } else {
    /* Get Parse Error as String */
    char *err_msg = mpc_err_string(r.error);
    mpc_err_delete(r.error);

    /* Create new error message using it */
    struct lval *err = lval_err("Could not load Library %s", err_msg);
    free(err_msg);
    lval_del(a);

    /* Cleanup and return error */
    return err;
  }
}
/* ----- Entry point ----- */

int main(int argc, char **argv) {
  Number = mpc_new("number");
  Symbol = mpc_new("symbol");
  String = mpc_new("string");
  Comment = mpc_new("comment");
  Sexpr = mpc_new("sexpr");
  Qexpr = mpc_new("qexpr");
  Expr = mpc_new("expr");
  Lispy = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
            "                                                   \
            number   : /-?[0-9]+/ ;                             \
            symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&|]+/;       \
            string  : /\"(\\\\.|[^\"])*\"/;                      \
            comment : /;[^\\r\\n]*/ ; \
            sexpr    : '(' <expr>* ')';                         \
            qexpr    : '{' <expr>* '}';                         \
            expr     : <number> | <symbol> |<comment> | <sexpr> | <qexpr> | <string>;           \
            lispy    : /^/ <expr>* /$/ ;                        \
            ",
            Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);

  puts("Lipsy version 0.0.0.0.1");
  puts("Press CTRL+c to exit\n");

  struct lenv *e = lenv_new();
  lenv_add_builtins(e);
  /* Interactive Prompt */
  if (argc == 1) {

    while (1) {
      char *input = readline("lipsy-> ");

      add_history(input);

      mpc_result_t r;

      if (mpc_parse("<stdin>", input, Lispy, &r)) {
        mpc_ast_print(r.output);
        struct lval *l = lval_eval(e, lval_read(r.output));
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
  }
  /* Supplied with list of files */
  if (argc >= 2) {

    /* loop over each supplied filename (starting from 1) */
    for (int i = 1; i < argc; i++) {

      /* Argument list with a single argument, the filename */
      struct lval *args = lval_add(lval_sexpr(), lval_str(argv[i]));

      /* Pass to builtin load and get the result */
      struct lval *x = builtin_load(e, args);

      /* If the result is an error be sure to print it */
      if (x->type == LVAL_ERR) {
        lval_println(x);
      }
      lval_del(x);
    }
  }
  lenv_del(e);
  mpc_cleanup(8, Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);
  return 0;
}

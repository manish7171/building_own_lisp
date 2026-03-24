#include "mpc.h"
#include <stdio.h>
#include <stdlib.h>

#define LASSERT(args, cond, fmt, ...)                                          \
  if (!(cond)) {                                                               \
    lval *err = lval_err(fmt, ##__VA_ARGS__);                                  \
    lval_del(args);                                                            \
    return err;                                                                \
  }

#define LASSERT_NUM(func, args, num)                                           \
  LASSERT(args, args->count == num,                                            \
          "function '%s' passed incorrect number of arguments"                 \
          "Got %i, expected %i" func,                                          \
          args->count, num)

#define LASSERT_TYPE(func, args, index, expect)                                \
  LASSERT(args, args->cell[index]->type == expect,                             \
          "Function '%s' passed incorrect type for argument %i"                \
          "Got  %s, Expected %s",                                              \
          func, index, ltype_name(args->cell[index]->type),                    \
          ltype_name(expect))

#define LASSERT_NOT_EMPTY(func, args, index)                                   \
  LASSERT(args, args->cell[index]->count != 0,                                 \
          "Function '%s' passed {} for argument %i.", func, index);
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

struct lval;
struct lenv;

typedef struct lval lval;
typedef struct lenv lenv;

struct lenv {
  lenv *par;
  int count;
  char **syms;
  lval **vals;
};
typedef lval *(*lbuiltin)(lenv *, lval *);

typedef enum {
  LVAL_NUM,
  LVAL_FUNC,
  LVAL_DEC,
  LVAL_ERR,
  LVAL_SYM,
  LVAL_SEXPR,
  LVAL_QEXPR
} lval_type;
typedef enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM } lerr;

typedef struct lval {
  int type;
  /* Basic */
  long num;
  double dec;
  char *err;
  char *sym;

  /* Funtion */
  lbuiltin builtin;
  lenv *env;
  lval *formals;
  lval *body;

  /* Expression */
  int count;
  struct lval **cell;
} lval;

lval *lval_func(lbuiltin builtin) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_FUNC;
  v->builtin = builtin;
  return v;
}

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

lval *lval_err(char *fmt, ...) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  va_list va;
  va_start(va, fmt);
  v->err = malloc(512);

  vsnprintf(v->err, 511, fmt, va);
  v->err = realloc(v->err, strlen(v->err) + 1);
  va_end(va);
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

void lenv_del(lenv *e);
lval *lval_qexpr(void) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lval_del(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    break;

  case LVAL_FUNC:
    if (!v->builtin) {
      lenv_del(v->env);
      lval_del(v->formals);
      lval_del(v->body);
    }
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
  case LVAL_QEXPR:
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

lval *lval_copy(lval *v);

lenv *lenv_copy(lenv *e) {
  lenv *n = malloc(sizeof(lenv));
  n->par = e->par;
  n->count = e->count;
  n->syms = malloc(sizeof(char *) * n->count);
  n->vals = malloc(sizeof(lval *) * n->count);
  for (int i = 0; i < e->count; ++i) {
    n->syms[i] = malloc(strlen(e->syms[i]) + 1);
    strcpy(n->syms[i], e->syms[i]);
    n->vals[i] = lval_copy(e->vals[i]);
  }
  return n;
}

lval *lval_copy(lval *v) {
  lval *x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type) {
  case LVAL_FUNC:
    if (v->builtin) {
      x->builtin = v->builtin;
    } else {
      x->builtin = NULL;
      x->env = lenv_copy(v->env);
      x->formals = lval_copy(v->formals);
      x->body = lval_copy(v->body);
    }
    x->builtin = v->builtin;
    break;
  case LVAL_NUM:
    x->num = v->num;
    break;
  case LVAL_ERR:
    x->err = malloc(strlen(v->err) + 1);
    strcpy(x->err, v->err);

  case LVAL_SYM:
    x->sym = malloc(strlen(v->sym) + 1);
    strcpy(x->sym, v->sym);
    break;

  case LVAL_SEXPR:
  case LVAL_QEXPR:
    x->count = v->count;
    x->cell = malloc(sizeof(lval) * x->count);
    for (int i = 0; i < x->count; i++) {
      x->cell[i] = lval_copy(v->cell[i]);
    }
    break;
  }
  return x;
}

void lval_print(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    printf("%li", v->num);
    break;

  case LVAL_FUNC:
    if (v->builtin) {
      printf("<builtin>");
    } else {
      printf("(\\ ");
      lval_print(v->formals);
      putchar(' ');
      lval_print(v->body);
      putchar(')');
    }
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
    break;

  case LVAL_QEXPR:
    lval_print_expr(v, '{', '}');
    break;
  }
}

lenv *lenv_new() {
  lenv *e = malloc(sizeof(lenv));
  e->par = NULL;
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lenv_del(lenv *e) {
  for (int i = 0; i < e->count; ++i) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
}

lval *lenv_get(lenv *e, lval *k) {
  for (int i = 0; i < e->count; ++i) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }
  // If no symbol check in parent otherwise error
  if (e->par) {
    return lenv_get(e->par, k);
  } else {
    return lval_err("unbound symbol %s", k->sym);
  }
}

void lenv_put(lenv *e, lval *k, lval *v) {
  for (int i = 0; i < e->count; ++i) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  e->count++;
  e->vals = realloc(e->vals, sizeof(lval *) * e->count);
  e->syms = realloc(e->syms, sizeof(char *) * e->count);

  e->vals[e->count - 1] = lval_copy(v);
  e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count - 1], k->sym);
}
void lval_println(lval *v) {
  lval_print(v);
  putchar('\n');
}

lval *eval_op(lval *x, char *op, lval *y) {
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
lval *lval_eval(lenv *e, lval *v);

lval *lval_pop(lval *v, int i) {
  lval *x = v->cell[i];
  /* shift memory after the item at i over the top */
  memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval *) * (v->count - i - 1));
  /* Decrease the count of items in the lists */
  v->count--;
  v->cell = realloc(v->cell, sizeof(lval *) * v->count);
  return x;
}

lval *lval_take(lval *v, int i) {
  lval *x = lval_pop(v, i);
  lval_del(v);
  return x;
}

char *ltype_name(int t) {
  switch (t) {
  case LVAL_FUNC:
    return "Function";
  case LVAL_NUM:
    return "Number";
  case LVAL_ERR:
    return "Error";
  case LVAL_SYM:
    return "Symbol";
  case LVAL_SEXPR:
    return "S-Expression";
  case LVAL_QEXPR:
    return "Q-Expression";
  default:
    return "Unknown";
  }
}

lval *builtin_head(lenv *e, lval *a) {
  // check error condition
  LASSERT_NUM("head", a, 1);
  LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
  LASSERT_NOT_EMPTY("head", a, 0);
  LASSERT(a, a->count == 1,
          "Function head passed too many arguments. Got %i, Expected %i.",
          a->count, 1);
  LASSERT(
      a, a->cell[0]->type == LVAL_QEXPR,
      "Funtion head passed invalid type for argument 0. Got %s, Expected %s",
      ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
  LASSERT(a, a->cell[0]->count != 0, "Funtion head {}");
  // otherwise take first element
  lval *v = lval_take(a, 0);

  while (v->count != 1) {
    lval_del(lval_pop(v, 1));
  }

  return v;
}

lval *builtin_tail(lenv *e, lval *a) {

  LASSERT(a, a->count == 1, "Function tail passed too many arguments");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function tail passed invalid type");
  LASSERT(a, a->cell[0]->count != 0, "Function tail {}");

  lval *v = lval_take(a, 0);

  lval_del(lval_pop(v, 0));

  return lval_eval(e, v);
}
lval *lval_join(lenv *e, lval *x, lval *y);
lval *builtin_join(lenv *e, lval *a) {
  for (int i = 0; i < a->count; ++i) {
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function tail passed invalid type");
  }

  lval *x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(e, x, lval_pop(a, 0));
  }

  lval_del(a);
  return lval_eval(e, x);
}

lval *builtin_list(lenv *e, lval *a) {
  a->type = LVAL_QEXPR;
  return lval_eval(e, a);
}

lval *builtin_eval(lenv *e, lval *a) {
  LASSERT(a, a->count == 1, "Funtion eval passed too many arguments");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function eval passed incorrect type");
  lval *x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval *lval_join(lenv *e, lval *x, lval *y) {
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  lval_del(y);
  return lval_eval(e, x);
}

lval *lval_lambda(lval *formals, lval *body);

lval *builtin_lambda(lenv *e, lval *a) {
  // check two arguments, each of which  are Q-expression
  LASSERT_NUM("\\ ", a, 2);
  LASSERT_TYPE("\\ ", a, 0, LVAL_QEXPR);
  LASSERT_TYPE("\\ ", a, 1, LVAL_QEXPR);

  // check First Q-expr contains only symbols
  for (int i = 0; i < a->cell[0]->count; ++i) {

    LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "Cannot define non-symbol. Got %s, Expected %s.",
            ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
  }

  // Pop first two arguments and pass it to lval_lamda
  lval *formals = lval_pop(a, 0);
  lval *body = lval_pop(a, 0);
  lval_del(a);
  return lval_lambda(formals, body);
}

lval *builtin_op(lenv *e, lval *a, char *op);

lval *builtin(lenv *e, lval *a, char *builtin) {
  if (strcmp("list", builtin) == 0) {
    return builtin_list(e, a);
  }
  if (strcmp("head", builtin) == 0) {
    return builtin_head(e, a);
  }
  if (strcmp("join", builtin) == 0) {
    return builtin_join(e, a);
  }
  if (strcmp("tail", builtin) == 0) {
    return builtin_tail(e, a);
  }
  if (strcmp("eval", builtin) == 0) {
    return builtin_eval(e, a);
  }

  if (strstr("+-*/%", builtin)) {
    return builtin_op(e, a, builtin);
  }
  lval_del(a);
  return lval_err("Unknown operator");
}

lval *builtin_add(lenv *e, lval *a) { return builtin_op(e, a, "+"); }

lval *builtin_sub(lenv *e, lval *a) { return builtin_op(e, a, "-"); }

lval *builtin_mul(lenv *e, lval *a) { return builtin_op(e, a, "*"); }

lval *builtin_div(lenv *e, lval *a) { return builtin_op(e, a, "/"); }

void lenv_def(lenv *e, lval *k, lval *v) {
  while (e->par) {
    e = e->par;
  }

  lenv_put(e, k, v);
}

lval *builtin_var(lenv *e, lval *a, char *func) {

  LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

  lval *syms = a->cell[0];

  for (int i = 0; i < syms->count; ++i) {
    LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
            "Function '%s' cannot define non-symbol."
            "Got %s, Expect %s.",
            func, ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
  }

  LASSERT(a, (syms->count == a->count - 1),
          "function '%s' passed too many arguments for symbols. "
          "Got %i, Expected %i.",
          func, syms->count, a->count - 1);

  // assign copies of  values to symbols

  for (int i = 0; i < syms->count; ++i) {
    /* If 'def' define in globally. If 'put' define in locally. */
    if (strcmp(func, "def")) {
      lenv_def(e, syms->cell[i], a->cell[i + 1]);
    }
    if (strcmp(func, "=") == 0) {
      lenv_put(e, syms->cell[i], a->cell[i + 1]);
    }
  }
  lval_del(a);
  return lval_sexpr();
}

lval *builtin_def(lenv *e, lval *a) { return builtin_var(e, a, "def"); }

lval *builtin_put(lenv *e, lval *a) { return builtin_var(e, a, "="); }

lval *lval_call(lenv *e, lval *f, lval *a) {
  /* If Builtin then simply call that */
  if (f->builtin) {
    return f->builtin(e, a);
  }

  /* Record Argument Counts */

  int given = a->count;
  int total = f->formals->count;

  /* While arguments still remain to te processed */
  while (a->count) {
    /* If we've ran out of formal arguments to bind */
    if (f->formals->count == 0) {
      lval_del(a);
      return lval_err("Function passed too many arguments. "
                      "Got %i, Expected %i.",
                      given, total);
    }
    /* Pop the first symbol from the formals */
    lval *sym = lval_pop(f->formals, 0);

    /* Pop the next argument from the list */
    lval *val = lval_pop(a, 0);

    /* Bind a copy into the functions's environment */
    lenv_put(f->env, sym, val);

    /* Delete symbol and value */
    lval_del(sym);
    lval_del(val);
  }
  /* Argument list is now bound so can be cleaned up */
  lval_del(a);

  /* If all formals have been bound evaluate */
  if (f->formals->count == 0) {
    /* Set environment parent to evaluation environment */
    f->env->par = e;

    /* Evaluate and return */
    return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
  } else {
    /* Otherwise return partially evaluated function*/
    return lval_copy(f);
  }

  /* Assign each argument to each formal in order */
  for (int i = 0; i < a->count; i++) {
    lenv_put(f->env, f->formals->cell[i], a->cell[i]);
  }

  lval_del(a);

  /* Set the parent environment */
  f->env->par = e;

  /* Evaluate the body */
  return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
}

void lenv_add_builtin(lenv *e, char *name, lbuiltin func) {
  lval *k = lval_sym(name);
  lval *l = lval_func(func);
  lenv_put(e, k, l);
  lval_del(k);
  lval_del(l);
}

void lenv_add_builtins(lenv *e) {
  /* Variable Function */
  lenv_add_builtin(e, "\\", builtin_lambda);
  lenv_add_builtin(e, "def", builtin_def);
  lenv_add_builtin(e, "=", builtin_put);
  /* List Function */
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  /* Mathmatical Function*/
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);

}

lval *builtin_op(lenv *e, lval *a, char *op) {
  for (int i = 0; i < a->count; ++i) {
    if (a->cell[i]->type != LVAL_NUM) {
      int type = a->cell[i]->type;
      lval_del(a);
      return lval_err("Error: Function '%s' passed incorrect type for argument "
                      "1. Got %s, Expected Number",
                      op, ltype_name(type));
    }
  }

  // pop the first element
  lval *x = lval_pop(a, 0);

  // if no arguments and sub then perform unary negation */
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  // while there are still elements remaining
  while (a->count > 0) {
    lval *y = lval_pop(a, 0);

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

lval *lval_eval_sexpr(lenv *e, lval *v) {
  // Evaluate children
  for (int i = 0; i < v->count; ++i) {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }

  // Error checking
  for (int i = 0; i < v->count; ++i) {
    if (v->cell[i]->type == LVAL_ERR) {
      return lval_take(v, i);
    }
  }

  // Empty expression
  if (v->count == 0) {
    return v;
  }

  // single expression
  if (v->count == 1) {
    return lval_take(v, 0);
  }

  // Ensure first element is symbol
  lval *f = lval_pop(v, 0);
  if (f->type != LVAL_FUNC) {
    lval *err = lval_err("S-Expression starts with incorrect type. "
                         "Got %s, Expected %s.",
                         ltype_name(f->type), ltype_name(LVAL_FUNC));
    lval_del(f);
    lval_del(v);
    return err;
  }

  lval *result = lval_call(e, f, v);
  // Call builtin with operator
  // lval *result = builtin(v, f->sym);
  lval_del(f);
  return result;
}

lval *lval_eval(lenv *e, lval *v) {
  if (v->type == LVAL_SYM) {
    lval *x = lenv_get(e, v);
    lval_del(v);
    return x;
  }
  // evalulate s-expression
  if (v->type == LVAL_SEXPR) {
    return lval_eval_sexpr(e, v);
  }
  return v;
}

lval *lval_lambda(lval *formals, lval *body) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_FUNC;
  // set builtin to null
  v->builtin = NULL;
  // build new envrionment
  v->env = lenv_new();
  // set formals and body
  v->formals = formals;
  v->body = body;

  return v;
}

int main(int argc, char **argv) {
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Decimal = mpc_new("decimal");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Qexpr = mpc_new("qexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");
  /* symbol : \"list\"| \"head\"|\"tail\"|\"join\"|\"eval\"|'+' | '-' | '*' |
   * '/' ;*/

  mpca_lang(MPCA_LANG_DEFAULT, "                                          \
    number : /-?[0-9]+/ ;                    \
    decimal  : /-?[0-9]+\\.[0-9]+/ ;            \
    symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;         \
    sexpr  : '(' <expr>* ')' ;               \
    qexpr  : '{' <expr>* '}' ;               \
    expr   : <decimal> | <number> | <symbol> | <sexpr> | <qexpr> ; \
    lispy  : /^/ <expr>* /$/ ;               \
  ",
            Number, Decimal, Symbol, Sexpr, Qexpr, Expr, Lispy);
  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  lenv *e = lenv_new();
  lenv_add_builtins(e);
  while (1) {
    char *input = readline("lispy> ");
    add_history(input);
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval *x = lval_eval(e, lval_read(r.output));
      lval_println(x);
      lval_del(x);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    free(input);
  }
  free(e);

  mpc_cleanup(7, Decimal, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  return 0;
}

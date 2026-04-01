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
int number_of_nodes(mpc_ast_t *t) {

  printf("Tag: %s\n", t->tag);
  printf("Contents: %s\n", t->contents);
  printf("Number of childre: %i\n", t->children_num);
  if (t->children_num == 0) {
    return 1;
  }
  if (t->children_num >= 1) {
    int total = 1;
    for (int i = 0; i < t->children_num; ++i) {
      total = total + number_of_nodes(t->children[i]);
    }
    return total;
  }
  return 0;
}

int number_of_leaves(mpc_ast_t *t) {
  if (t->children_num == 0) {
    return 1;
  }
  int total = 0;
  for (int i = 0; i < t->children_num; ++i) {
    total = total + number_of_leaves(t->children[i]);
  }
  return total;
}

int number_of_branches(mpc_ast_t *t) {
  if (t->children_num == 0) {
    return 0;
  }

  int total = 1;
  for (int i = 0; i < t->children_num; ++i) {
    total += number_of_branches(t->children[i]);
  }
  return total;
}
int maxChildren = 0;
int most_number_of_children(mpc_ast_t *t) {
  if (t->children_num == 0) {
    return 0;
  }
  if (maxChildren < t->children_num) {
    maxChildren = t->children_num;
  }
  for (int i = 0; i < t->children_num; ++i) {
    most_number_of_children(t->children[i]);
  }

  return maxChildren;
}
/*Use operator string to see which operation to perform*/

long eval_op(long x, char *op, long y) {
  if (strcmp(op, "+") == 0) {
    return x + y;
  }
  if (strcmp(op, "-") == 0) {
    return x - y;
  }
  if (strcmp(op, "*") == 0) {
    return x * y;
  }
  if (strcmp(op, "/") == 0) {
    return x / y;
  }
  if (strcmp(op, "%") == 0) {
    return x % y;
  }
  if (strcmp(op, "min") == 0) {
    return x < y ? x : y;
  }
  if (strcmp(op, "max") == 0) {
    return x > y ? x : y;
  }
  if (strcmp(op, "^") == 0) {
    return pow(x, y);
  }
  return 0;
}

long eval(mpc_ast_t *t) {
  /*if tagged as number return it directly*/
  if (strstr(t->tag, "number")) {
    return atoi(t->contents);
  }

  /* The operator is always second child.*/
  char *op = t->children[1]->contents;

  /*We store the third child in 'x'*/
  long x = eval(t->children[2]);
  if (!strstr(t->children[3]->tag, "expr") && (strcmp(op, "-") == 0)) {

    x = eval_op(0, op, x);
  } else {
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
      x = eval_op(x, op, eval(t->children[i]));
      i++;
    }
  }
  /* iterate the remaining children and combining*/
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

  /* Create Some Parsers */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Operator = mpc_new("operator");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT,
            "                                                     \
      number   : /-?[0-9]+/ ;                             \
      operator : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\" ;                  \
      expr     : <number> | '(' <operator> <expr>+ ')' ;  \
      lispy    : /^/ <operator> <expr>+ /$/ ;             \
    ",
            Number, Operator, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.2");
  puts("Press Ctrl+c to Exit\n");

  while (1) {
    char *input = readline("lispy> ");
    add_history(input);

    /* Attempt to parse the user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* On success print and delete the AST */
      /* Load AST From output*/
      mpc_ast_t *a = r.output;
      /*printf("Number of nodes %i\n", number_of_nodes(a));
      printf("Number of leaves %i\n", number_of_leaves(a));
      printf("Number of branches %i\n", number_of_branches(a));
      printf("Max number of children %i\n", most_number_of_children(a));
      printf("Number of expr nodes %i\n", number_of_expr_nodes(a));
      printf("Number of ( nodes %i\n", number_of_bracket_nodes(a));
*/
      long result = eval(r.output);
      printf("%li\n", result);
      /*printf("Tag: %s\n", a->tag);
      printf("Contents: %s\n", a->contents);
      printf("Number of childre: %i\n", a->children_num);
*/
      /* Get First Child*/
      /*mpc_ast_t* c0 = a->children[0];
      printf("Frist child Tag: %s\n", c0->tag);
      printf("Frist Child Contents: %s\n", c0->contents);
      printf("First child Number of childre: %i\n", c0->children_num);
      */
      /* Get Second Child*/
      /*mpc_ast_t* c1 = a->children[1];
      printf("Second child Tag: %s\n", c1->tag);
      printf("Second Child Contents: %s\n", c1->contents);
      printf("Second child Number of childre: %i\n", c1->children_num);

      mpc_ast_t* c3 = a->children[3];
      printf("Forth child Tag: %s\n", c3->tag);
      printf("Forth Child Contents: %s\n", c3->contents);
      printf("Forth child Number of childre: %i\n", c3->children_num);
      mpc_ast_t** cc0 = a->children[3]->children;
      printf("Fourth Children child Tag: %s\n", cc0[0]->tag);
      */
      mpc_ast_print(r.output);

      mpc_ast_delete(r.output);
    } else {
      /* Otherwise print and delete the Error */
      printf("hasf\n");
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  /* Undefine and delete our parsers */
  mpc_cleanup(4, Number, Operator, Expr, Lispy);

  return 0;
}

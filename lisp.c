#include "mpc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\0';
  return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

//Foward declaration
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv; 
// Type of lval types
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN };

typedef lval*(*lbuiltin)(lenv*,lval*);
// Declare new lval structure
struct lval {
  int type;
  long num;
  // Error and symbol
  char* err;
  char* sym;
  lbuiltin fun;
  // Count and pointer to a list of lval
  int count;
  struct lval** cell;
};

struct lenv{
  int count;
  char** syms;
  lval** vals;
};
// Create a new number type lval
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

// Create a new error type
lval* lval_err(char* fmt,...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  va_list va;
  va_start(va,fmt);

  v->err = malloc(512);

  vsnprintf(v->err,511,fmt,va);

  v->err =realloc(v->err,strlen(v->err)+1);

  va_end(va);
  return v;
}

lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_fun(lbuiltin func){
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun =func;
  return v;
}

lenv* lenv_new(void){
  lenv* e = malloc(sizeof(lenv));
  e->count = 0;
  e->syms =NULL;
  e->vals =NULL;
  return e;
}

void lval_del(lval* v) {
  switch (v->type) {
    // Do nothing special for number type
    case LVAL_NUM: break;

    // For err or sym, free the string data
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    case LVAL_FUN: break;

    // If S-expr or Q-expr, delete all elements inside
    case LVAL_QEXPR:
    case LVAL_SEXPR:
      for (int i = 0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }
      free(v->cell);
      break;
  }
  // Free the memory allocated to the lval itself
  free(v);
}

void lenv_del(lenv* e){
  for(int i=0;i<e->count;i++){
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}
lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

lval* lval_pop(lval* v, int i) {
  lval* x = v->cell[i];

  memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval*) * (v->count - i - 1));

  // Decrease the count of items in the list
  v->count--;

  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval* lval_join(lval* x, lval* y) {
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  lval_del(y);
  return x;
}

lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

void lval_print(lval* v);
lval* lval_eval(lenv* e,lval* v);
lval* lval_read(mpc_ast_t* t);

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);
    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM: printf("%li", v->num); break;
    case LVAL_ERR: printf("Error: %s", v->err); break;
    case LVAL_SYM: printf("%s", v->sym); break;
    case LVAL_FUN: printf("<function>"); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
  }
}

//Copying an lval for puttting things into the environment
lval* lval_copy(lval* v){
  lval* x = malloc(sizeof(lval));
  x->type = v->type;

  switch(v->type){
    //Copy functions and numbers directly
    case LVAL_FUN: x->fun = v->fun; break;
    case LVAL_NUM: x->num = v->num; break;

    //Copy strings using malloc
    case LVAL_ERR:
    x->err = malloc(strlen(v->err)+1);
    strcpy(x->err, v->err); break;

    case LVAL_SYM:
    x->sym = malloc(strlen(v->sym)+1);
    strcpy(x->sym,v->sym); break;

    //Copy lists
    case LVAL_SEXPR:
    case LVAL_QEXPR:
    x->count = v->count;
    x->cell = malloc(sizeof(lval*)* x->count);
    for(int i=0 ; i< x->count; i++){
      x->cell[i] = lval_copy(v->cell[i]);
    }
    break;
  }  
  return x;
}
void lval_println(lval* v) {
  lval_print(v);
  putchar('\n');
}

lval* lenv_get(lenv* e,lval* k){
  for(int i = 0 ;i<e->count;i++){
    //Check if the stored string matched the symbol string
    //if it does return a copy
    if(strcmp(e->syms[i],k->sym)==0){
      return lval_copy(e->vals[i]);
    }
  }
  return lval_err("Unbound symbol %s",k->sym);
}

void lenv_put(lenv* e,lval* k , lval* v){
  //iterate over all items in the environment 
  for(int i=0;i<e->count;i++){
    if(strcmp(e->syms[i],k->sym)==0){
      lval_del(e->vals[i]);
      e->vals[i]=lval_copy(v);
      return ;
    }
  }
  //if no existing entry found allocate space for new entry

  e->count++;
  e->vals = realloc(e->vals,sizeof(lval*)* e->count);
  e->syms = realloc(e->syms,sizeof(char*)* e->count);

  //copy contents of lval and symbol
  e->vals[e->count-1] = lval_copy(v);
  e->syms[e->count-1] = malloc(strlen(k->sym)+1);
  strcpy(e->syms[e->count-1],k->sym);
}
//Fowarc declaration
lval* builtin_op(lenv* e,lval* a, char* op);

lval* builtin_max(lenv* e,lval* a) {
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-number!");
    }
  }

  lval* max_value = lval_pop(a, 0);

  while (a->count > 0) {
    lval* next_value = lval_pop(a, 0);
    if (next_value->num > max_value->num) {
      lval_del(max_value);
      max_value = next_value;
    } else {
      lval_del(next_value);
    }
  }
  lval_del(a);
  return max_value;
}

lval* builtin_min(lenv* e,lval* a) {
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-numbers");
    }
  }

  lval* min_value = lval_pop(a, 0);

  while (a->count > 0) {
    lval* next_value = lval_pop(a, 0);
    if (next_value->num < min_value->num) {
      lval_del(min_value);
      min_value = next_value;
    } else {
      lval_del(next_value);
    }
  }
  lval_del(a);
  return min_value;
}

lval* builtin_avg(lenv* e,lval* a) {
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-numbers!");
    }
  }

  long total = 0;
  for (int i = 0; i < a->count; i++) {
    total += a->cell[i]->num;
  }

  long avg = total / a->count;

  lval_del(a);
  return lval_num(avg);
}

#define LASSERT(args, cond, fmt, ...) \
  if (!(cond)) { lval* err = lval_err(fmt, ##__VA_ARGS__); lval_del(args); return err; }

#define LASSERT_TYPE(func, args, index, expect) \
  LASSERT(args, args->cell[index]->type == expect, \
    "Function '%s' passed incorrect type for argument %i. Got %s, Expected %s.", \
    func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NUM(func, args, num) \
  LASSERT(args, args->count == num, \
    "Function '%s' passed incorrect number of arguments. Got %i, Expected %i.", \
    func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index) \
  LASSERT(args, args->cell[index]->count != 0, \
    "Function '%s' passed {} for argument %i.", func, index);


char* ltype_name(int t){
  switch(t){
    case LVAL_FUN: return "Function";
    case LVAL_NUM: return "Number ";
    case LVAL_ERR: return "Error";
    case LVAL_SYM: return "Symbol";
    case LVAL_SEXPR: return "S-Expression";
    case LVAL_QEXPR: return "Q-Expression";
    default: return "Unknown";
  }
}
lval* builtin_addition(lenv* e, lval* a){
  return builtin_op(e,a,"+");
}

lval* builtin_sub(lenv* e, lval* a){
  return builtin_op(e,a,"-");
}

lval* builtin_mul(lenv* e,lval* a){
  return builtin_op(e,a,"*");
}

lval* builtin_div(lenv* e,lval* a){
  return builtin_op(e,a,"/");
}

lval* builtin_mod(lenv* e,lval* a){
  return builtin_op(e,a,"%");
}
lval* builtin_list(lenv* e,lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_len(lval* a){
  LASSERT(a, a->count == 1, "Function 'len' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'len' passed incorrect types!");
  LASSERT(a, a->cell[0]->count !=0 ,"Function len passed {}");

  long len = a->cell[0]->count;
  lval_del(a);
  return lval_num(len);
}

lval* builtin_cons(lval*a){
  LASSERT(a, a->count == 2, "Function 'cons' passed too many arguments!");
  LASSERT(a, a->cell[1]->type == LVAL_QEXPR, "Function 'cons' passed incorrect types!");
  //LASSERT(a, a->cell[1]->type!= LVAL_QEXPR, "Function 'cons' passed incorrect types!");

  lval* x = lval_pop(a,0);//Extract the value to be appended
  lval* y = lval_pop(a,0);//Extract q expression

  lval* new_qexpr = lval_qexpr();
  new_qexpr = lval_add(new_qexpr,x);

  while(y->count > 0){
    new_qexpr = lval_add(new_qexpr,lval_pop(y,0));
  }

  lval_del(y);
  lval_del(a);
  return new_qexpr;

}
lval* builtin_head(lenv* e,lval* a) {
  LASSERT_NUM("head",a,1);
  LASSERT_TYPE("head",a,0,LVAL_QEXPR);
  LASSERT_NOT_EMPTY("head",a,0);

  lval* v = lval_take(a, 0);
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

lval* builtin_tail(lenv* e,lval* a) {
  LASSERT_NUM("tail",a,1);
  LASSERT_TYPE("tail",a,0,LVAL_QEXPR);
  LASSERT_NOT_EMPTY("tail",a,0);

  lval* v = lval_take(a, 0);
  lval_del(lval_pop(v, 0));
  return v;
}

lval* builtin_init(lval* a){
  LASSERT_NUM("init",a,1);
  LASSERT_TYPE("init",a,0,LVAL_QEXPR);
  LASSERT_NOT_EMPTY("init",a,0);

  lval* v =lval_take(a,0);
  lval_del(lval_pop(v,v->count-1));
  return v;

}
lval* builtin_eval(lenv*e,lval* a) {
  LASSERT_NUM("eval",a,1);
  LASSERT_TYPE("eval",a,0,LVAL_QEXPR);

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e,x);
}

lval* builtin_join(lenv* e,lval* a) {
  for (int i = 0; i < a->count; i++) {
    LASSERT_TYPE("join",a,i,LVAL_QEXPR);
  }

  lval* x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval* builtin_def(lenv* e,lval* a){
  LASSERT_TYPE("def",a,0,LVAL_QEXPR);

  lval* syms = a->cell[0];

 for (int i = 0; i < syms->count; i++) {
    LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
      "Function 'def' cannot define non-symbol. "
      "Got %s, Expected %s.",
      ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
  }

  LASSERT(a, syms->count == a->count-1, "Function def cannot define incorrect number of values to symbols");

  for(int i=0;i<syms->count;i++){
    lenv_put(e,syms->cell[i],a->cell[i+1]);
  }

  lval_del(a);
  return lval_sexpr();
}

void lenv_add_builtin(lenv* e,char* name,lbuiltin func){
  lval* k= lval_sym(name);
  lval* v= lval_fun(func);
  lenv_put(e,k,v);
  lval_del(k);
  lval_del(v);

}

lval* builtin_op(lenv* e,lval* a, char* op) {
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-number!");
    }
  }

  if (strcmp(op, "max") == 0) { return builtin_max(e,a); }
  if (strcmp(op, "min") == 0) { return builtin_min(e,a); }
  if (strcmp(op, "avg") == 0) { return builtin_avg(e,a); }

  lval* x = lval_pop(a, 0);

  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  while (a->count > 0) {
    lval* y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lval_del(x);
        lval_del(y);
        x = lval_err("Division by zero!");
        break;
      }
      x->num /= y->num;
    }
    if (strcmp(op, "%") == 0) { x->num %= y->num; }
    if (strcmp(op, "^") == 0) { x->num = pow(x->num, y->num); }

    lval_del(y);
  }

  lval_del(a);
  return x;
}

void lenv_add_builtins(lenv* e){
  //List Functions
  lenv_add_builtin(e,"list",builtin_list);
  lenv_add_builtin(e,"head",builtin_head);
  lenv_add_builtin(e,"tail",builtin_tail);
  lenv_add_builtin(e,"join",builtin_join);
  lenv_add_builtin(e,"eval",builtin_eval);

  lenv_add_builtin(e,"max",builtin_max);
  lenv_add_builtin(e,"min",builtin_min);
  lenv_add_builtin(e,"avg",builtin_avg);
  lenv_add_builtin(e,"def",builtin_def);

  //Arithmetic Functions
  lenv_add_builtin(e,"+", builtin_addition);
  lenv_add_builtin(e,"-", builtin_sub);
  lenv_add_builtin(e,"*", builtin_mul);
  lenv_add_builtin(e,"/", builtin_div);
  lenv_add_builtin(e,"%", builtin_mod);
  //lenv_add_builtin(e,"^", builtin_pow);
}
/*lval* builtin(lval* a, char* func) {
  if (strcmp("cons",func)==0) {return builtin_cons(a);}
  if (strcmp("init",func) ==0 ) {return builtin_init(a);}
  if (strcmp("len",func) ==0 ) {return builtin_len(a);}
  if (strcmp("list", func) == 0) { return builtin_list(a); }
  if (strcmp("head", func) == 0) { return builtin_head(a); }
  if (strcmp("tail", func) == 0) { return builtin_tail(a); }
  if (strcmp("join", func) == 0) { return builtin_join(a); }
  if (strcmp("eval", func) == 0) { return builtin_eval(a); }
  if (strcmp("+", func) == 0 || strcmp("-", func) == 0 || strcmp("*", func) == 0 ||
      strcmp("/", func) == 0 || strcmp("%", func) == 0 || strcmp("^", func) == 0) {
    return builtin_op(a, func);
  }
  lval_del(a);
  return lval_err("Unknown function");
}*/

lval* lval_eval_sexpr(lenv* e,lval* v) {
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e,v->cell[i]);
  }

  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  if (v->count == 0) { return v; }

  if (v->count == 1) { return lval_take(v, 0); }

  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_FUN) {
    lval_del(f);
    lval_del(v);
    return lval_err("S-expression does not start with symbol!");
  }

  lval* result = f->fun(e, v);
  lval_del(f);
  return result;
}

lval* lval_eval(lenv* e,lval* v) {
    if(v->type == LVAL_SYM){
      lval* x = lenv_get(e,v);
      lval_del(v);
      return x;
    }
    if(v->type == LVAL_SEXPR){return lval_eval_sexpr(e,v);}
    return v;
}

lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("Invalid number");
}

lval* lval_read(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }
  if (strstr(t->tag, "qexpr")) { x = lval_qexpr(); }

  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

int main(int argc, char** argv) {

  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Qexpr = mpc_new("qexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number   : /-?[0-9]+/ ;                             \
      symbol :  /[a-zA-Z0-9_^%+\\-*\\/\\\\=<>!&]+/; \
      sexpr    : '(' <expr>* ')' ;                        \
      qexpr    : '{' <expr>* '}' ;                        \
      expr     : <number> | <symbol> | <sexpr> | <qexpr>;  \
      lispy    : /^/ <expr>* /$/ ;                          \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.9");
  puts("Press Ctrl+c to Exit\n");

  lenv* e=lenv_new();
  lenv_add_builtins(e);

  while (1) {
    char* input = readline("lispy> ");
    add_history(input);

    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval* x = lval_eval(e, lval_read(r.output));
      lval_println(x);
      lval_del(x);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);

  }

  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  return 0;
}

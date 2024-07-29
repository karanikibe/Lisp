#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

/*Windows Compile instructions*/
#ifdef __WIN32
#include<string.h>

static char buffer[2048];

/*Fake readline function*/
char* readline(char* prompt)
{
    fputs(prompt,stdout);
    fgets(buffer,2048,stdin);
    char* cpy = malloc(strlen(buffer)+1);
    strcpy(cpy,buffer);
    cpy[strlen(cpy)-1]= '\0';
    return cpy;
}

void add_history(char* unused)
{}
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

typedef struct{
    int type;
    long num;

    //eRROR AND SYMBOL TYPES HAVE SOME STRING DATA
    char* err;
    char* sym;

    //Count and Pointer to a list of lval
    int count;
    struct lval** cell;

}lval;

//long eval(mpc_ast_t* t);

/*lval eval_op(long x,char* op, long y)
{
    if(strcmp(op,"+")==0) {return x+y;}
    if(strcmp(op,"-")==0) {return x-y;}
    if(strcmp(op,"*")==0) {return x*y;}
    if(strcmp(op,"/")==0) {return x/y;}
    return 0;
}*/

enum{LVAL_ERR,LVAL_NUM,LVAL_SYM,LVAL_SEXPR};
enum{LERR_DIV_ZERO,LERR_BAD_OP,LERR_BAD_NUM};

//Foward Declarations
lval* lval_err(char* m);
lval* lval_num(long x);
lval* lval_sym(char *s);
lval* lval_sexpr(void);
lval eval_op(lval x ,char* op,lval y);
lval eval(mpc_ast_t* t);
lval* lval_read_num(mpc_ast_t* t);
void lval_print(lval* v);
void lval_expr_print(lval* v,char open,char close);
void lval_println(lval* v);
void lval_del(lval* v);




lval* lval_err(char* m)
{
   lval* v=malloc(sizeof(lval));
   v->type =LVAL_ERR;
   v->err=malloc(strlen(m)+1);
   strcpy(v->err,m);
   return v;
}
//Structure for error handling
//lval eval_op(long x, char* op,long y);
lval* lval_num(long x)
{
    lval* v=malloc(sizeof(lval));
    v->type= LVAL_NUM;
    v->num =x;
    return v;
}

lval* lval_sym(char *s)
{
    lval* v= malloc(sizeof(lval()));
    v->type=LVAL_SYM;
    v->sym=malloc(strlen(s)+1);
    strcpy(v->sym,s);
    return v;
}

lval* lval_sexpr(void)
{
    lval* v=malloc(sizeof(lval));
    v->type=LVAL_SEXPR;
    v->count=0;
    v->cell=NULL;
    return v;
}

void lval_del(lval* v)
{
    switch(v->type)
    {
        case LVAL_NUM: break;

        case LVAL_ERR:free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        case LVAL_SEXPR:
        for(int i=0;i<v->count;i++)
        {
            lval_del((lval*)v->cell[i]);
        }

        free(v->cell);
        break;
    }
    free(v);
}
//lval eval_op(long x, char* op,long y);
lval eval_op(lval x ,char* op,lval y)
{
    //If either value is an error return it
    if(x.type==LVAL_ERR)
    {
        return x;
    }

    if(y.type==LVAL_ERR)
    {
        return y;
    }

    //Otherwise do maths on the values
    if(strcmp(op,"+")==0){return *lval_num(x.num +y.num);}
    if(strcmp(op,"-")==0){return *lval_num(x.num -y.num);}
    if(strcmp(op,"*")==0){return *lval_num(x.num*y.num);}
    if(strcmp(op,"/")==0)
    {
        //return y.num==0 ?* lval_err(LERR_DIV_ZERO): *lval_num(x.num/y.num);
         return y.num == 0 ? *lval_err("Division By Zero!") : *lval_num(x.num / y.num);
    }
    return *lval_err("Invalid Operation: ");

}

lval eval(mpc_ast_t* t)
{
    if(strstr(t->tag,"number"))
    {
        errno =0;
        long x =strtol(t->contents,NULL,10);
        return errno != ERANGE ? *lval_num(x):*lval_err("Invalid Number");
    }

    char* op =t->children[1]->contents;
    lval x =eval(t->children[2]);

    int i=3;
    while(strstr(t->children[i]->tag,"expr"))
    {
        x=eval_op(x,op,eval(t->children[i]));
        i++;
    }
    return x;
}

lval* lval_read_num(mpc_ast_t* t)
{
    errno=0;
    long x= strtol(t->contents,NULL,10);
    return errno !=ERANGE? 
    lval_num(x): lval_err("invalid number");
}

lval* lval_add(lval* v, lval* x)
{
    v->count++;
    v->cell=realloc(v->cell,sizeof(lval*) * v->count);
    v->cell[v->count - 1] = (struct lval*)x;
    return v;
}

lval* lval_read(mpc_ast_t* t)
{
    if(strstr(t->tag,"number")) {return lval_read_num(t);}
    if(strstr(t->tag,"symbol")) {return lval_sym(t->contents);}


    lval* x =NULL;
    if(strcmp(t->tag,">")==0) {x=lval_sexpr();}
    if(strstr(t->tag,"sexpr")) {x=lval_sexpr();}

    for(int i=0; i< t->children_num; i++)
    {
        if(strcmp(t->children[i]->contents,"(")==0) {continue;}
        if(strcmp(t->children[i]->contents,"(")==0) {continue;}
        if(strcmp(t->children[i]->tag,"regex")==0) {continue;}

        x= lval_add(x,lval_read(t->children[i]));
    }
    return x;
}


//Print an lval

void lval_print(lval* v)
{
    switch(v->type)
    {
        case LVAL_NUM: printf("%li",v->num); break;
        case LVAL_ERR: printf("%li",v->err); break;
        case LVAL_SYM: printf("%li",v->sym); break;
        case LVAL_SEXPR: lval_expr_print(v,'(',')'); break;
    }
}

void lval_expr_print(lval* v,char open,char close)
{
    putchar(open);

    for(int i=0;i<v->count;i++)
    {
        lval_print((lval*)v->cell[i]);

        if(i !=(v->count -1))
        {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_println(lval* v)
{
    lval_print(v);
    putchar('\n');
}

//static char input[2048];

int main(int argc, char **argv)
{
    //Create Parsers
    mpc_parser_t* Number = mpc_new("number");
    //mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t *Symbol=mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lipsy = mpc_new("lipsy");

    //Define them with the language
    mpca_lang(MPCA_LANG_DEFAULT,\
    "                          \
        number   : /-?[0-9]+/;  \
        symbol : '+' | '-' | '*'|'/'; \
        sexpr   : '(' <expr>* ')' \
        expr     : <number> | <symbol> | <sexpr>; \
        lipsy    : /^/ <expr>* /$/; \
    ",
    Number, Symbol,Sexpr, Expr, Lipsy
    );


    puts("Lispy Version 0.0.0.1");
    puts("Ctrl +c to exit\n");

    while(1)
    {
        char* input =readline("lipsy> ");
        add_history(input);

        mpc_result_t r;

        if(mpc_parse("<stdin>",input,Lipsy,&r))
        {
            /*On Success Print the AST*/
            mpc_ast_print(r.output);
            mpc_ast_delete(r.output);
        }else
        {
            /*Otherwise Print the error*/
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        /*Load AST from output*/
        mpc_ast_t* a =r.output;
        printf("Tag:%s\n",a->tag);
        printf("Contents:%s\n",a->contents);
        printf("Number of children : %i\n ",a->children_num);

        /*Get The first Child*/
        mpc_ast_t* c0=a->children[0];
        printf("First Child Tag:%s\n",c0->tag);
        printf("First Child Contents:%s\n",c0->contents);
        printf("First Child Number of children : %i\n", c0->children_num);

        /*Perform Operations*/
        

        /*Use operator string to see which peration is to be perfomed*/
        lval* x=lval_read(r.output);
        lval_println(x);
        lval_del(x);
        //printf("%li\n", result);
        mpc_ast_delete(r.output);
        
        //printf("No, You are a %s ",input);
        free(input);
    }

    mpc_cleanup(5,Number,Symbol,Sexpr,Expr,Lipsy);
    return 0;
}
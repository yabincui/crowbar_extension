#ifndef PRIVATE_CROWBAR_H_INCLUDED
#define PRIVATE_CROWBAR_H_INCLUDED
#include <stdio.h>
#include "MEM.h"
#include "CRB.h"
#include "CRB_dev.h"

#define smaller(a, b) ((a) < (b) ? (a) : (b))
#define larger(a, b) ((a) > (b) ? (a) : (b))

#define MESSAGE_ARGUMENT_MAX    (256)
#define LINE_BUF_SIZE           (1024)
#define STACK_ALLOC_SIZE		(256)
#define HEAP_THRESHOLD_SIZE		(1024*256)
#define ARRAY_ALLOC_SIZE		(1024)

typedef enum {
    PARSE_ERR = 1,
    CHARACTER_INVALID_ERR,
    FUNCTION_MULTIPLE_DEFINE_ERR,
    COMPILE_ERROR_COUNT_PLUS_1
} CompileError;

typedef enum {
    VARIABLE_NOT_FOUND_ERR = 1,
    FUNCTION_NOT_FOUND_ERR,
    ARGUMENT_TOO_MANY_ERR,
    ARGUMENT_TOO_FEW_ERR,
    NOT_BOOLEAN_TYPE_ERR,
    MINUS_OPERAND_TYPE_ERR,
    BAD_OPERAND_TYPE_ERR,
    NOT_BOOLEAN_OPERATOR_ERR,
    FOPEN_ARGUMENT_TYPE_ERR,
    FCLOSE_ARGUMENT_TYPE_ERR,
    FGETS_ARGUMENT_TYPE_ERR,
    FPUTS_ARGUMENT_TYPE_ERR,
    NOT_NULL_OPERATOR_ERR,
    DIVISION_BY_ZERO_ERR,
    GLOBAL_VARIABLE_NOT_FOUND_ERR,
    GLOBAL_STATEMENT_IN_TOPLEVEL_ERR,
    BAD_OPERATOR_FOR_STRING_ERR,
	NOT_LVALUE_ERR,
	NO_SUCH_METHOD_ERR,
	ARRAY_RESIZE_ARGUMENT_ERR,
	INDEX_OPERAND_NOT_ARRAY_ERR,
	INDEX_OPERAND_NOT_INT_ERR,
	ARRAY_INDEX_OUT_OF_BOUND_ERR,
	NEW_ARRAY_ARGUMENT_TYPE_ERR,
	INC_DEC_OPERAND_TYPE_ERR,
    RUNTIME_ERROR_COUNT_PLUS_1
} RuntimeError;

typedef enum {
    INT_MESSAGE_ARGUMENT = 1,
    DOUBLE_MESSAGE_ARGUMENT,
    STRING_MESSAGE_ARGUMENT,
    CHARACTER_MESSAGE_ARGUMENT,
    POINTER_MESSAGE_ARGUMENT,
    MESSAGE_ARGUMENT_END
} MessageArgumentType;

typedef struct {
    char *format;
} MessageFormat;

typedef struct Expression_tag Expression;

typedef enum {
    BOOLEAN_EXPRESSION = 1,
    INT_EXPRESSION,
    DOUBLE_EXPRESSION,
    STRING_EXPRESSION,
    IDENTIFIER_EXPRESSION,
    ASSIGN_EXPRESSION,
    ADD_EXPRESSION,
    SUB_EXPRESSION,
    MUL_EXPRESSION,
    DIV_EXPRESSION,
    MOD_EXPRESSION,
    EQ_EXPRESSION,
    NE_EXPRESSION,
    GT_EXPRESSION,
    GE_EXPRESSION,
    LT_EXPRESSION,
    LE_EXPRESSION,
    LOGICAL_AND_EXPRESSION,
    LOGICAL_OR_EXPRESSION,
    MINUS_EXPRESSION,
    FUNCTION_CALL_EXPRESSION,
    NULL_EXPRESSION,
	METHOD_CALL_EXPRESSION,
	ARRAY_EXPRESSION,
	INDEX_EXPRESSION,
	POST_INCREMENT_EXPRESSION,
	PREV_INCREMENT_EXPRESSION,
	POST_DECREMENT_EXPRESSION,
	PREV_DECREMENT_EXPRESSION,
    EXPRESSION_TYPE_COUNT_PLUS_1
} ExpressionType;

#define dkc_is_math_operator(operator) \
  ((operator) == ADD_EXPRESSION || (operator) == SUB_EXPRESSION\
   || (operator) == MUL_EXPRESSION || (operator) == DIV_EXPRESSION\
   || (operator) == MOD_EXPRESSION)

#define dkc_is_compare_operator(operator) \
  ((operator) == EQ_EXPRESSION || (operator) == NE_EXPRESSION\
   || (operator) == GT_EXPRESSION || (operator) == GE_EXPRESSION\
   || (operator) == LT_EXPRESSION || (operator) == LE_EXPRESSION)

#define dkc_is_logical_operator(operator) \
  ((operator) == LOGICAL_AND_EXPRESSION || (operator) == LOGICAL_OR_EXPRESSION)

typedef struct ArgumentList_tag {
    Expression *expression;
    struct ArgumentList_tag *next;
} ArgumentList;

typedef struct {
    Expression  *left;
    Expression  *operand;
} AssignExpression;

typedef struct {
    Expression  *left;
    Expression  *right;
} BinaryExpression;

typedef struct {
    char                *identifier;
    ArgumentList        *argument;
} FunctionCallExpression;

typedef struct {
	Expression *expression;
	char *identifier;
	ArgumentList *argument;
} MethodCallExpression;

typedef struct {
	Expression *array;
	Expression *index;
} IndexExpression;

typedef struct ExpressionList_tag {
	Expression *expression;
	struct ExpressionList_tag *next;
} ExpressionList;

typedef struct {
	Expression *operand;
} IncrementOrDecrement;

struct Expression_tag {
    ExpressionType type;
    int line_number;
    union {
        CRB_Boolean             boolean_value;
        int                     int_value;
        double                  double_value;
        char                    *string_value;
        char                    *identifier;
        AssignExpression        assign_expression;
        BinaryExpression        binary_expression;
        Expression              *minus_expression;
        FunctionCallExpression  function_call_expression;
		MethodCallExpression	method_call_expression;
		IndexExpression			index_expression;
		ExpressionList			*array_expression;
		IncrementOrDecrement	inc_dec;
    } u;
};

typedef struct Statement_tag Statement;

typedef struct StatementList_tag {
    Statement   *statement;
    struct StatementList_tag    *next;
} StatementList;

typedef struct {
    StatementList       *statement_list;
} Block;

typedef struct IdentifierList_tag {
    char        *name;
    struct IdentifierList_tag   *next;
} IdentifierList;

typedef struct {
    IdentifierList      *identifier_list;
} GlobalStatement;

typedef struct Elsif_tag {
    Expression  *condition;
    Statement   *statement;
    struct Elsif_tag    *next;
} Elsif;

typedef struct {
    Expression  *condition;
    Statement   *then_statement;
    Elsif       *elsif_list;
    Statement   *else_statement;
} IfStatement;

typedef struct {
    Expression  *condition;
    Statement   *statement;
} WhileStatement;

typedef struct {
    Expression  *init;
    Expression  *condition;
    Expression  *post;
    Statement   *statement;
} ForStatement;

typedef struct {
    Expression *return_value;
} ReturnStatement;

typedef struct {
	Block *block;
} BlockStatement;

typedef enum {
    EXPRESSION_STATEMENT = 1,
    GLOBAL_STATEMENT,
    IF_STATEMENT,
    WHILE_STATEMENT,
    FOR_STATEMENT,
    RETURN_STATEMENT,
    BREAK_STATEMENT,
    CONTINUE_STATEMENT,
	BLOCK_STATEMENT,
    STATEMENT_TYPE_COUNT_PLUS_1
} StatementType;

struct Statement_tag {
    StatementType       type;
    int                 line_number;
    union {
        Expression      *expression_s;
        GlobalStatement global_s;
        IfStatement     if_s;
        WhileStatement  while_s;
        ForStatement    for_s;
        ReturnStatement return_s;
		BlockStatement  block_s;
    } u;
};

typedef struct ParameterList_tag {
    char        *name;
    struct ParameterList_tag *next;
} ParameterList;

typedef enum {
    CROWBAR_FUNCTION_DEFINITION = 1,
    NATIVE_FUNCTION_DEFINITION
} FunctionDefinitionType;

typedef struct FunctionDefinition_tag {
    char                *name;
    FunctionDefinitionType      type;
    union {
        struct {
            ParameterList       *parameter;
            Block               *block;
        } crowbar_f;
        struct {
            CRB_NativeFunctionProc      *proc;
        } native_f;
    } u;
    struct FunctionDefinition_tag       *next;
} FunctionDefinition;

struct Variable_tag {
    char        *name;
    CRB_Value   value;
    struct Variable_tag *next;
};

typedef enum {
    NORMAL_STATEMENT_RESULT = 1,
    RETURN_STATEMENT_RESULT,
    BREAK_STATEMENT_RESULT,
    CONTINUE_STATEMENT_RESULT,
    STATEMENT_RESULT_TYPE_COUNT_PLUS_1
} StatementResultType;

typedef struct {
    StatementResultType type;
    union {
        CRB_Value       return_value;
    } u;
} StatementResult;

typedef struct GlobalVariableRef_tag {
    Variable    *variable;
    struct GlobalVariableRef_tag *next;
} GlobalVariableRef;

struct CRB_LocalEnvironment_tag {
    Variable    *variable;
    GlobalVariableRef   *global_variable;
	struct CRB_LocalEnvironment_tag *parent_env;
};

struct CRB_String_tag {
    char        *string;
    CRB_Boolean is_literal;
};

typedef struct CRB_Array_tag {
	int alloc_size;
	int length;
	CRB_Value *array;
} CRB_Array;

typedef struct {
	int stack_alloc_size;
	int stack_pointer;
	CRB_Value *stack;
} Stack;

typedef struct {
	int current_heap_size;
	int current_threshold;
	CRB_Object *header;
	int gc_enabled;
} Heap;

struct CRB_Interpreter_tag {
    MEM_Storage         interpreter_storage;
    MEM_Storage         execute_storage;
    Variable            *variable;
    FunctionDefinition  *function_list;
    StatementList       *statement_list;
    int                 current_line_number;
	Stack				stack;
	Heap				heap;
	CRB_LocalEnvironment *top_env;
};


typedef enum {
	STRING_OBJECT = 1,
	ARRAY_OBJECT,
	OBJECT_TYPE_COUNT_PLUS_1
} ObjectType;

#define dkc_is_object_value(type) \
	((type) == CRB_STRING_VALUE || (type) == CRB_ARRAY_VALUE)

struct CRB_Object_tag {
	ObjectType type;
	unsigned int marked:1;
	union {
		CRB_String string;
		CRB_Array array;
	} u;
	struct CRB_Object_tag *prev;
	struct CRB_Object_tag *next;
};


typedef struct {
	char *string;
} VString;


/* create.c */
void crb_function_define(char *identifier, ParameterList *parameter_list,
                         Block *block);
ParameterList *crb_create_parameter(char *identifier);
ParameterList *crb_chain_parameter(ParameterList *list,
                                   char *identifier);
ArgumentList *crb_create_argument_list(Expression *expression);
ArgumentList *crb_chain_argument_list(ArgumentList *list, Expression *expr);
StatementList *crb_create_statement_list(Statement *statement);
StatementList *crb_chain_statement_list(StatementList *list,
                                        Statement *statement);
Expression *crb_alloc_expression(ExpressionType type);
Expression *crb_create_assign_expression(Expression *left,
                                             Expression *operand);
Expression *crb_create_binary_expression(ExpressionType operator,
                                         Expression *left,
                                         Expression *right);
Expression *crb_create_minus_expression(Expression *operand);
Expression *crb_create_identifier_expression(char *identifier);
Expression *crb_create_function_call_expression(char *func_name,
                                                ArgumentList *argument);
Expression *crb_create_boolean_expression(CRB_Boolean value);
Expression *crb_create_null_expression(void);
Statement *crb_create_global_statement(IdentifierList *identifier_list);
IdentifierList *crb_create_global_identifier(char *identifier);
IdentifierList *crb_chain_identifier(IdentifierList *list, char *identifier);
Statement *crb_create_if_statement(Expression *condition,
                                    Statement *then_statement, 
									Elsif *elsif_list,
                                    Statement *else_statement);
Elsif *crb_chain_elsif_list(Elsif *list, Elsif *add);
Elsif *crb_create_elsif(Expression *expr, Statement *statement);
Statement *crb_create_while_statement(Expression *condition, 
									Statement *statement);
Statement *crb_create_for_statement(Expression *init, Expression *cond,
                                    Expression *post,
									Statement *statement);
Block *crb_create_block(StatementList *statement_list);
Statement *crb_create_expression_statement(Expression *expression);
Statement *crb_create_return_statement(Expression *expression);
Statement *crb_create_break_statement(void);
Statement *crb_create_continue_statement(void);
Statement *crb_create_block_statement(Block *block);

ExpressionList* crb_create_expression_list(Expression *expr);
ExpressionList* crb_chain_expression_list(ExpressionList *list,
											Expression *expr);
Expression* crb_create_array_expression(ExpressionList *list);
Expression* crb_create_index_expression(Expression *array_expr,
										Expression *index_expr);
Expression* crb_create_method_call_expression(Expression *obj_expr,
											char *identifier,
											ArgumentList *argument);
Expression* crb_create_incdec_expression(ExpressionType type,
											Expression *operand);

/* string.c */
char *crb_create_identifier(char *str);
void crb_open_string_literal(void);
void crb_add_string_literal(int letter);
void crb_reset_string_literal_buffer(void);
char *crb_close_string_literal(void);

/* execute.c */
StatementResult
crb_execute_statement_list(CRB_Interpreter *inter,
                           CRB_LocalEnvironment *env, StatementList *list);

/* eval.c */
CRB_Value crb_eval_binary_expression(CRB_Interpreter *inter,
                                 CRB_LocalEnvironment *env,
                                 ExpressionType operator,
                                 Expression *left, Expression *right);
CRB_Value crb_eval_minus_expression(CRB_Interpreter *inter,
                                CRB_LocalEnvironment *env, 
								Expression *operand);
CRB_Value crb_eval_expression(CRB_Interpreter *inter,
                          CRB_LocalEnvironment *env, Expression *expr);

void crb_stack_push_value(CRB_Interpreter *inter, CRB_Value *value);
CRB_Value crb_stack_pop_value(CRB_Interpreter *inter);
CRB_Value* crb_stack_peek_value(CRB_Interpreter *inter, int index);
void crb_stack_shrink_size(CRB_Interpreter *inter, int shrink_size);

/* heap.c */
CRB_Object *crb_literal_to_crb_string(CRB_Interpreter *inter, char *str);
//void crb_refer_string(CRB_Object *obj);
//void crb_release_string(CRB_Interpreter *inter, CRB_Object *obj);
CRB_Object *crb_create_crowbar_string(CRB_Interpreter *inter, char *str);
CRB_Object* crb_create_array(CRB_Interpreter *inter, int array_size);
void crb_array_add(CRB_Interpreter *inter, CRB_Object *obj, CRB_Value *val);
int crb_array_size(CRB_Interpreter *inter, CRB_Object *obj);
void crb_array_resize(CRB_Interpreter *inter, CRB_Object *obj, int new_size);


/* util.c */
CRB_Interpreter *crb_get_current_interpreter(void);
void crb_set_current_interpreter(CRB_Interpreter *inter);
void *crb_malloc(size_t size);
void *crb_execute_malloc(CRB_Interpreter *inter, size_t size);
Variable *crb_search_local_variable(CRB_LocalEnvironment *env,
                                    char *identifier);
Variable *
crb_search_global_variable(CRB_Interpreter *inter, char *identifier);
Variable* crb_add_local_variable(CRB_LocalEnvironment *env,
                            char *identifier, CRB_Value *value);
Variable* CRB_add_global_variable(CRB_Interpreter *inter, char *identifier,
								CRB_Value *value);
CRB_NativeFunctionProc *
crb_search_native_function(CRB_Interpreter *inter, char *name);
FunctionDefinition *crb_search_function(char *name);
char *crb_get_operator_string(ExpressionType type);


void crb_vstr_clear(VString *v);
void crb_vstr_append_string(VString *v, char *str);
void crb_vstr_append_character(VString *v, char ch);
char* CRB_value_to_string(CRB_Value *value);


/* error.c */
void crb_compile_error(CompileError id, ...);
void crb_runtime_error(int line_number, RuntimeError id, ...);

/* native.c */
void crb_nv_print_proc(CRB_Interpreter *interpreter,
							CRB_LocalEnvironment *env,
                            int arg_count);
void crb_nv_fopen_proc(CRB_Interpreter *interpreter,
							CRB_LocalEnvironment *env,
                            int arg_count);
void crb_nv_fclose_proc(CRB_Interpreter *interpreter,
							CRB_LocalEnvironment *env,
                             int arg_count);
void crb_nv_fgets_proc(CRB_Interpreter *interpreter,
							CRB_LocalEnvironment *env,
                            int arg_count);
void crb_nv_fputs_proc(CRB_Interpreter *interpreter,
							CRB_LocalEnvironment *env,
                            int arg_count);
void crb_add_std_fp(CRB_Interpreter *inter);
void crb_nv_new_array_proc(CRB_Interpreter *inter,
							CRB_LocalEnvironment *env,
							int arg_count);

/* gc.c */
void crb_gc_enable(CRB_Interpreter *inter);
void crb_gc_disable(CRB_Interpreter *inter);
void crb_garbage_collect(CRB_Interpreter *inter);
void crb_check_gc(CRB_Interpreter *inter);

#endif /* PRIVATE_CROWBAR_H_INCLUDED */

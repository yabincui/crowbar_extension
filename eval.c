#include <math.h>
#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "crowbar.h"


static void push_value(CRB_Interpreter *inter, CRB_Value *value)
{
	//printf("push_value stack_pointer..%d\n", inter->stack.stack_pointer);

	DBG_assert(inter->stack.stack_pointer <= inter->stack.stack_alloc_size,
				("stack_pointer..%d,stack_alloc_size..%d\n",
				 inter->stack.stack_pointer, 
				 inter->stack.stack_alloc_size));
	
	if (inter->stack.stack_pointer == inter->stack.stack_alloc_size) {
		inter->stack.stack_alloc_size += STACK_ALLOC_SIZE;
		inter->stack.stack = MEM_realloc(inter->stack.stack,
						sizeof(CRB_Value)*inter->stack.stack_alloc_size);
	}
	inter->stack.stack[inter->stack.stack_pointer++] = *value;
	
}


static CRB_Value pop_value(CRB_Interpreter *inter)
{
	//printf("pop_value stack_pointer..%d\n", inter->stack.stack_pointer);
	DBG_assert(inter->stack.stack_pointer>0,
			("pop_value when stack_pointer..%d\n",
			 inter->stack.stack_pointer));
	CRB_Value ret = inter->stack.stack[--inter->stack.stack_pointer];
	return ret;
}


static CRB_Value* peek_stack(CRB_Interpreter *inter, int index)
{
	DBG_assert(inter->stack.stack_pointer > index && index >= 0,
			("peek_stack with stack_pointer..%d, index..%d\n",
			 inter->stack.stack_pointer, index));

	return &inter->stack.stack[inter->stack.stack_pointer - index - 1];

}


static void shrink_stack(CRB_Interpreter *inter, int shrink_size)
{
	//printf("shrink_stack stack_pointer..%d, shrink_size..%d\n",
	//		inter->stack.stack_pointer, shrink_size);
	DBG_assert(inter->stack.stack_pointer >= shrink_size && shrink_size>=0,
			("shrink_stack with stack_pointer..%d, shrink_size..%d\n",
			 inter->stack.stack_pointer, shrink_size));
	inter->stack.stack_pointer -= shrink_size;
}





static void
eval_boolean_expression(CRB_Interpreter *inter, CRB_Boolean boolean_value)
{
    CRB_Value   v;

    v.type = CRB_BOOLEAN_VALUE;
    v.u.boolean_value = boolean_value;

    push_value(inter, &v);
}

static void
eval_int_expression(CRB_Interpreter *inter, int int_value)
{
    CRB_Value   v;

    v.type = CRB_INT_VALUE;
    v.u.int_value = int_value;

    push_value(inter, &v);
}

static void
eval_double_expression(CRB_Interpreter *inter, double double_value)
{
    CRB_Value   v;

    v.type = CRB_DOUBLE_VALUE;
    v.u.double_value = double_value;

    push_value(inter, &v);
}

static void
eval_string_expression(CRB_Interpreter *inter, CRB_CHAR *string_value)
{
    CRB_Value   v;

    v.type = CRB_STRING_VALUE;
    v.u.object_value = crb_literal_to_crb_string(inter, string_value);


    push_value(inter, &v);
}

static void eval_regexp_expression(CRB_Interpreter *inter, 
									CRB_Regexp *regexp)
{
	CRB_Value v;
	v.type = CRB_NATIVE_POINTER_VALUE;
	v.u.native_pointer.info = crb_get_regexp_info();
	v.u.native_pointer.pointer = (void*)regexp;

	push_value(inter, &v);
}

static void
eval_null_expression(CRB_Interpreter *inter)
{
    CRB_Value   v;

    v.type = CRB_NULL_VALUE;

    push_value(inter, &v);
}


/*
static Variable *
search_global_variable_from_env(CRB_Interpreter *inter,
                                CRB_LocalEnvironment *env, char *name)
{
    GlobalVariableRef *pos;

    if (env == NULL) {
        return crb_search_global_variable(inter, name);
    }

    for (pos = env->environ_scope->u.scope_chain.global_ref; 
			pos; pos = pos->next) {
        if (!strcmp(pos->variable->name, name)) {
            return pos->variable;
        }
    }

    return NULL;
}
*/

static void
eval_identifier_expression(CRB_Interpreter *inter,
                           CRB_LocalEnvironment *env, Expression *expr)
{
    Variable   *variable;

	variable = crb_search_local_variable(inter,
			env, expr->u.identifier, CRB_FALSE);

	if (variable == NULL) {
    	crb_runtime_error(expr->filename, expr->line_number, 
							VARIABLE_NOT_FOUND_ERR,
                              STRING_MESSAGE_ARGUMENT,
                              "name", expr->u.identifier,
                              MESSAGE_ARGUMENT_END);
        
    }

    push_value(inter, &(variable->value));
}

static void eval_expression(CRB_Interpreter *inter, 
							CRB_LocalEnvironment *env,
                                 Expression *expr);

static CRB_Value* get_identifier_lvalue(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										char *identifier)
{

	Variable *variable;

	variable = crb_search_local_variable(inter,
			env, identifier, CRB_TRUE);

	return &(variable->value);
}



static CRB_Value* get_array_element_lvalue(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										Expression *expr)
{
	CRB_Value *array_val;
	CRB_Value *index_val;

	eval_expression(inter, env, expr->u.index_expression.array);
	eval_expression(inter, env, expr->u.index_expression.index);

	array_val = peek_stack(inter, 1);
	index_val = peek_stack(inter, 0);

	if (array_val->type != CRB_ARRAY_VALUE)
		crb_runtime_error(expr->filename, expr->line_number, 
				INDEX_OPERAND_NOT_ARRAY_ERR,
				MESSAGE_ARGUMENT_END);
	if (index_val->type != CRB_INT_VALUE)
		crb_runtime_error(expr->filename, expr->line_number, 
				INDEX_OPERAND_NOT_INT_ERR,
				MESSAGE_ARGUMENT_END);
	if (index_val->u.int_value < 0 ||
			index_val->u.int_value >= 
				array_val->u.object_value->u.array.length)
		crb_runtime_error(expr->filename, expr->line_number, 
				ARRAY_INDEX_OUT_OF_BOUND_ERR,
				INT_MESSAGE_ARGUMENT, "size",
				array_val->u.object_value->u.array.length,
				INT_MESSAGE_ARGUMENT, "index",
				index_val->u.int_value);

	// we need to keep array_val on the stack to make sure it 
	// will not be garbage collected!!!
	pop_value(inter);
	
	return &array_val->u.object_value->u.array.array[
				index_val->u.int_value];

}


static CRB_Value* get_member_expression_lvalue(CRB_Interpreter *inter,
											CRB_LocalEnvironment *env,
											Expression *expr,
											CRB_Boolean can_create)
{
	CRB_Value *ret_val = NULL;
	

	eval_expression(inter, env, expr->u.member_expression.expression);
	CRB_Value *left_val = peek_stack(inter, 0);

	if (left_val->type != CRB_ASSOC_VALUE)
		crb_runtime_error(expr->filename, expr->line_number, 
				MEMBER_OPERATION_NOT_ASSOC_ERR,
				MESSAGE_ARGUMENT_END);

	if (left_val->type == CRB_ASSOC_VALUE) {
		Variable *variable = crb_search_assoc_variable(inter, 
							left_val->u.object_value, 
							expr->u.member_expression.member_name,
							can_create);
		if (variable != NULL)
			ret_val = &(variable->value);
		else {
			crb_runtime_error(expr->filename, expr->line_number, 
					NO_SUCH_MEMBER_ERR,
					STRING_MESSAGE_ARGUMENT, "member_name",
					expr->u.member_expression.member_name,
					MESSAGE_ARGUMENT_END);
		}
	}

	// we have to put assoc on the stack to retain it!!!
	return ret_val;
}


static CRB_Value* get_lvalue(CRB_Interpreter *inter,
							CRB_LocalEnvironment *env,
							Expression *expr)
{
	CRB_Value *dest;

	if (expr->type == IDENTIFIER_EXPRESSION)
		dest = get_identifier_lvalue(inter, env, expr->u.identifier);
	else if (expr->type == INDEX_EXPRESSION)
		dest = get_array_element_lvalue(inter, env, expr);
	else if (expr->type == MEMBER_EXPRESSION)
		dest = get_member_expression_lvalue(inter, env, expr, CRB_TRUE);
	else
		crb_runtime_error(expr->filename, expr->line_number, 
						NOT_LVALUE_ERR,
						MESSAGE_ARGUMENT_END);

	return dest;
}



static void
eval_binary_expression(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                           ExpressionType operator,
                           Expression *left, Expression *right);

static void get_temp_variable_name(CRB_Interpreter *inter,
									CRB_LocalEnvironment *env,
									char *tmp_name)
{
	int id = 0;
	Variable *var = NULL;

	do {
		id++;
		sprintf(tmp_name, "crowbar.tmp_name_%d", id);
		var = crb_search_scope_variable(inter, env->environ_scope,
										tmp_name, CRB_FALSE);
	} while (var != NULL);
	
}


static void 
eval_assign_expression(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                       	AssignType type,
						Expression *left, Expression *expression)
{
    CRB_Value  *src;
    CRB_Value  *dest;

	dest = get_lvalue(inter, env, left);
    
	eval_expression(inter, env, expression);
	src = peek_stack(inter, 0);


	if (type == ASSIGN_TYPE)
		*dest = *src;
	else {
		// we should not repeat executing left expr

		ExpressionType expr_type;

		if (type == ADD_ASSIGN_TYPE) expr_type = ADD_EXPRESSION;
		else if (type == SUB_ASSIGN_TYPE) expr_type = SUB_EXPRESSION;
		else if (type == MUL_ASSIGN_TYPE) expr_type = MUL_EXPRESSION;
		else if (type == DIV_ASSIGN_TYPE) expr_type = DIV_EXPRESSION;
		else if (type == MOD_ASSIGN_TYPE) expr_type = MOD_EXPRESSION;
		else DBG_panic(("unexpected assign_type: %d\n", type));

		char tmp_name1[100];
		char tmp_name2[100];
		get_temp_variable_name(inter, env, tmp_name1);
		Variable *var = crb_search_scope_variable(inter, 
							env->environ_scope, tmp_name1, CRB_TRUE);
		var->value = *dest;

		get_temp_variable_name(inter, env, tmp_name2);
		Variable *var2 = crb_search_scope_variable(inter,
							env->environ_scope, tmp_name2, CRB_TRUE);
		var2->value = *src;
		shrink_stack(inter, 1);

		
		Expression id_expr1;
		crb_init_identifier_expression(&id_expr1, tmp_name1,
										__FILE__, __LINE__);

		Expression id_expr2;
		crb_init_identifier_expression(&id_expr2, tmp_name2,
										__FILE__, __LINE__);

		eval_binary_expression(inter, env, expr_type, &id_expr1,
								&id_expr2);

		crb_remove_scope_variable(inter, env->environ_scope, tmp_name1);
		crb_remove_scope_variable(inter, env->environ_scope, tmp_name2);

		src = peek_stack(inter, 0);	
	}


	*dest = *src;
	if (left->type == INDEX_EXPRESSION 
			|| left->type == MEMBER_EXPRESSION) {
		// pop the array/assoc value after assignment
		CRB_Value *ret_pos = peek_stack(inter, 1);
		*ret_pos = *src;
		pop_value(inter);
	}

}

static CRB_Boolean
eval_binary_boolean(CRB_Interpreter *inter, ExpressionType operator,
                    CRB_Boolean left, CRB_Boolean right, 
					char *filename, int line_number)
{
    CRB_Boolean result;

    if (operator == EQ_EXPRESSION) {
        result = left == right;
    } else if (operator == NE_EXPRESSION) {
        result = left != right;
    } else {
        char *op_str = crb_get_operator_string(operator);
        crb_runtime_error(filename, line_number, NOT_BOOLEAN_OPERATOR_ERR,
                          STRING_MESSAGE_ARGUMENT, "operator", op_str,
                          MESSAGE_ARGUMENT_END);
    }

    return result;
}

static void
eval_binary_int(CRB_Interpreter *inter, ExpressionType operator,
                int left, int right,
                CRB_Value *result, char *filename, int line_number)
{
    if (dkc_is_math_operator(operator)) {
        result->type = CRB_INT_VALUE;
    } else if (dkc_is_compare_operator(operator)) {
        result->type = CRB_BOOLEAN_VALUE;
    } else {
        DBG_panic(("operator..%d\n", operator));
    }

    switch (operator) {
    case BOOLEAN_EXPRESSION:    /* FALLTHRU */
    case INT_EXPRESSION:        /* FALLTHRU */
    case DOUBLE_EXPRESSION:     /* FALLTHRU */
    case STRING_EXPRESSION:     /* FALLTHRU */
    case IDENTIFIER_EXPRESSION: /* FALLTHRU */
    case ASSIGN_EXPRESSION:
        DBG_panic(("bad case...%d", operator));
        break;
    case ADD_EXPRESSION:
        result->u.int_value = left + right;
        break;
    case SUB_EXPRESSION:
        result->u.int_value = left - right;
        break;
    case MUL_EXPRESSION:
        result->u.int_value = left * right;
        break;
    case DIV_EXPRESSION:
        result->u.int_value = left / right;
        break;
    case MOD_EXPRESSION:
        result->u.int_value = left % right;
        break;
    case LOGICAL_AND_EXPRESSION:        /* FALLTHRU */
    case LOGICAL_OR_EXPRESSION:
        DBG_panic(("bad case...%d", operator));
        break;
    case EQ_EXPRESSION:
        result->u.boolean_value = left == right;
        break;
    case NE_EXPRESSION:
        result->u.boolean_value = left != right;
        break;
    case GT_EXPRESSION:
        result->u.boolean_value = left > right;
        break;
    case GE_EXPRESSION:
        result->u.boolean_value = left >= right;
        break;
    case LT_EXPRESSION:
        result->u.boolean_value = left < right;
        break;
    case LE_EXPRESSION:
        result->u.boolean_value = left <= right;
        break;
    case MINUS_EXPRESSION:              /* FALLTHRU */
    case FUNCTION_CALL_EXPRESSION:      /* FALLTHRU */
    case NULL_EXPRESSION:               /* FALLTHRU */
    case EXPRESSION_TYPE_COUNT_PLUS_1:  /* FALLTHRU */
    default:
        DBG_panic(("bad case...%d", operator));
    }
}

static void
eval_binary_double(CRB_Interpreter *inter, ExpressionType operator,
                   double left, double right,
                   CRB_Value *result, char *filename, int line_number)
{
    if (dkc_is_math_operator(operator)) {
        result->type = CRB_DOUBLE_VALUE;
    } else if (dkc_is_compare_operator(operator)) {
        result->type = CRB_BOOLEAN_VALUE;
    } else {
        DBG_panic(("operator..%d\n", operator));
    }

    switch (operator) {
    case BOOLEAN_EXPRESSION:    /* FALLTHRU */
    case INT_EXPRESSION:        /* FALLTHRU */
    case DOUBLE_EXPRESSION:     /* FALLTHRU */
    case STRING_EXPRESSION:     /* FALLTHRU */
    case IDENTIFIER_EXPRESSION: /* FALLTHRU */
    case ASSIGN_EXPRESSION:
        DBG_panic(("bad case...%d", operator));
        break;
    case ADD_EXPRESSION:
        result->u.double_value = left + right;
        break;
    case SUB_EXPRESSION:
        result->u.double_value = left - right;
        break;
    case MUL_EXPRESSION:
        result->u.double_value = left * right;
        break;
    case DIV_EXPRESSION:
        result->u.double_value = left / right;
        break;
    case MOD_EXPRESSION:
        result->u.double_value = fmod(left, right);
        break;
    case LOGICAL_AND_EXPRESSION:        /* FALLTHRU */
    case LOGICAL_OR_EXPRESSION:
        DBG_panic(("bad case...%d", operator));
        break;
    case EQ_EXPRESSION:
        result->u.int_value = left == right;
        break;
    case NE_EXPRESSION:
        result->u.int_value = left != right;
        break;
    case GT_EXPRESSION:
        result->u.int_value = left > right;
        break;
    case GE_EXPRESSION:
        result->u.int_value = left >= right;
        break;
    case LT_EXPRESSION:
        result->u.int_value = left < right;
        break;
    case LE_EXPRESSION:
        result->u.int_value = left <= right;
        break;
    case MINUS_EXPRESSION:              /* FALLTHRU */
    case FUNCTION_CALL_EXPRESSION:      /* FALLTHRU */
    case NULL_EXPRESSION:               /* FALLTHRU */
    case EXPRESSION_TYPE_COUNT_PLUS_1:  /* FALLTHRU */
    default:
        DBG_panic(("bad default...%d", operator));
    }
}

static CRB_Boolean
eval_compare_string(ExpressionType operator,
                    CRB_Value *left, CRB_Value *right, 
					char *filename, int line_number)
{
    CRB_Boolean result;
    int cmp;

    cmp = CRB_wcscmp(left->u.object_value->u.string.string, 
				right->u.object_value->u.string.string);

    if (operator == EQ_EXPRESSION) {
        result = (cmp == 0);
    } else if (operator == NE_EXPRESSION) {
        result = (cmp != 0);
    } else if (operator == GT_EXPRESSION) {
        result = (cmp > 0);
    } else if (operator == GE_EXPRESSION) {
        result = (cmp >= 0);
    } else if (operator == LT_EXPRESSION) {
        result = (cmp < 0);
    } else if (operator == LE_EXPRESSION) {
        result = (cmp <= 0);
    } else {
        char *op_str = crb_get_operator_string(operator);
        crb_runtime_error(filename, line_number, 
							BAD_OPERATOR_FOR_STRING_ERR,
                          STRING_MESSAGE_ARGUMENT, "operator", op_str,
                          MESSAGE_ARGUMENT_END);
    }

    return result;
}

static CRB_Boolean
eval_binary_null(CRB_Interpreter *inter, ExpressionType operator,
                 CRB_Value *left, CRB_Value *right, 
				 char *filename, int line_number)
{
    CRB_Boolean result;

    if (operator == EQ_EXPRESSION) {
        result = left->type == CRB_NULL_VALUE && right->type == CRB_NULL_VALUE;
    } else if (operator == NE_EXPRESSION) {
        result =  !(left->type == CRB_NULL_VALUE
                    && right->type == CRB_NULL_VALUE);
    } else {
        char *op_str = crb_get_operator_string(operator);
        crb_runtime_error(filename, line_number, 
							NOT_NULL_OPERATOR_ERR,
                          STRING_MESSAGE_ARGUMENT, "operator", op_str,
                          MESSAGE_ARGUMENT_END);
    }

    return result;
}

void
chain_string(CRB_Interpreter *inter, CRB_Value *left, CRB_Value *right,
				CRB_Value *result)
{
	CRB_CHAR *right_str;
    int len;
    CRB_CHAR *str;

	right_str = CRB_value_to_string(right);

	result->type = CRB_STRING_VALUE;
	len = CRB_wcslen(left->u.object_value->u.string.string) + 
			CRB_wcslen(right_str);

	str = (CRB_CHAR*)MEM_malloc((len+1)*sizeof(CRB_CHAR));
	CRB_wcscpy(str, left->u.object_value->u.string.string);
	CRB_wcscat(str, right_str);
	result->u.object_value = crb_create_crowbar_string(inter, str);

	MEM_free(right_str);


}

static void
eval_binary_expression(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                           ExpressionType operator,
                           Expression *left, Expression *right)
{
    CRB_Value   *left_val;
    CRB_Value   *right_val;
    CRB_Value   result;

    eval_expression(inter, env, left);
    eval_expression(inter, env, right);
	left_val = peek_stack(inter, 1);
	right_val = peek_stack(inter, 0);

    if (left_val->type == CRB_INT_VALUE
        && right_val->type == CRB_INT_VALUE) {
        eval_binary_int(inter, operator,
                        left_val->u.int_value, right_val->u.int_value,
                        &result, left->filename, left->line_number);
    } else if (left_val->type == CRB_DOUBLE_VALUE
               && right_val->type == CRB_DOUBLE_VALUE) {
        eval_binary_double(inter, operator,
                           left_val->u.double_value, 
						   right_val->u.double_value,
                           &result, left->filename, left->line_number);
    } else if (left_val->type == CRB_INT_VALUE
               && right_val->type == CRB_DOUBLE_VALUE) {
        left_val->u.double_value = left_val->u.int_value;
        eval_binary_double(inter, operator,
                           left_val->u.double_value, 
						   right_val->u.double_value,
                           &result, left->filename, left->line_number);
    } else if (left_val->type == CRB_DOUBLE_VALUE
               && right_val->type == CRB_INT_VALUE) {
        right_val->u.double_value = right_val->u.int_value;
        eval_binary_double(inter, operator,
                           left_val->u.double_value, 
						   right_val->u.double_value,
                           &result, left->filename, left->line_number);
    } else if (left_val->type == CRB_BOOLEAN_VALUE
               && right_val->type == CRB_BOOLEAN_VALUE) {
        result.type = CRB_BOOLEAN_VALUE;
        result.u.boolean_value
            = eval_binary_boolean(inter, operator,
                                  left_val->u.boolean_value,
                                  right_val->u.boolean_value,
								  left->filename,
                                  left->line_number);
    } else if (left_val->type == CRB_STRING_VALUE
               && operator == ADD_EXPRESSION) {
		chain_string(inter, left_val, right_val, &result);
    
	} else if (left_val->type == CRB_STRING_VALUE
               && right_val->type == CRB_STRING_VALUE) {
        result.type = CRB_BOOLEAN_VALUE;
        result.u.boolean_value
            = eval_compare_string(operator, left_val, right_val,
                                  left->filename, left->line_number);
    } else if (left_val->type == CRB_NULL_VALUE
               || right_val->type == CRB_NULL_VALUE) {
        result.type = CRB_BOOLEAN_VALUE;
        result.u.boolean_value
            = eval_binary_null(inter, operator, left_val, right_val,
                                left->filename,
								left->line_number);
    } else {
        char *op_str = crb_get_operator_string(operator);
		printf("operator=%d, %s, left_type=%d, right_type=%d\n",
				operator, op_str, left_val->type, right_val->type);
        crb_runtime_error(left->filename, left->line_number, 
						BAD_OPERAND_TYPE_ERR,
                          STRING_MESSAGE_ARGUMENT, "operator", op_str,
                          MESSAGE_ARGUMENT_END);
    }

	pop_value(inter);
	pop_value(inter);
	push_value(inter, &result);
}


CRB_Value crb_eval_binary_expression(CRB_Interpreter *inter,
							CRB_LocalEnvironment *env,
							ExpressionType operator,
							Expression *left, Expression *right)
{
	eval_binary_expression(inter, env, operator, left, right);
	return pop_value(inter);

}

static void eval_not_expression(CRB_Interpreter *inter,
								CRB_LocalEnvironment *env,
								Expression *sub_expr)
{
	eval_expression(inter, env, sub_expr);
	CRB_Value *pv = peek_stack(inter, 0);

	if (pv->type != CRB_BOOLEAN_VALUE) {
		crb_runtime_error(sub_expr->filename, sub_expr->line_number, 
						NOT_BOOLEAN_FOR_NOT_EXPRESSION,
						MESSAGE_ARGUMENT_END);
	}
	if (pv->u.boolean_value == CRB_TRUE)
		pv->u.boolean_value = CRB_FALSE;
	else
		pv->u.boolean_value = CRB_TRUE;
}

static void
eval_logical_and_or_expression(CRB_Interpreter *inter,
                               CRB_LocalEnvironment *env,
                               ExpressionType operator,
                               Expression *left, Expression *right)
{
    CRB_Value   left_val;
    CRB_Value   right_val;
    CRB_Value   result;

    result.type = CRB_BOOLEAN_VALUE;
    eval_expression(inter, env, left);
	left_val = pop_value(inter);

    if (left_val.type != CRB_BOOLEAN_VALUE) {
        crb_runtime_error(left->filename, left->line_number, 
							NOT_BOOLEAN_TYPE_ERR,
                          MESSAGE_ARGUMENT_END);
    }
    if (operator == LOGICAL_AND_EXPRESSION) {
        if (!left_val.u.boolean_value) {
            result.u.boolean_value = CRB_FALSE;
            goto FUNC_END;
        }
    } else if (operator == LOGICAL_OR_EXPRESSION) {
        if (left_val.u.boolean_value) {
            result.u.boolean_value = CRB_TRUE;
            goto FUNC_END;
        }
    } else {
        DBG_panic(("bad operator..%d\n", operator));
    }

    eval_expression(inter, env, right);
    right_val = pop_value(inter);
	if (right_val.type != CRB_BOOLEAN_VALUE) {
        crb_runtime_error(right->filename, right->line_number, 
							NOT_BOOLEAN_TYPE_ERR,
                          MESSAGE_ARGUMENT_END);
    }
    result.u.boolean_value = right_val.u.boolean_value;

FUNC_END:
    push_value(inter, &result);
}






static void
eval_minus_expression(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                          Expression *operand)
{
    CRB_Value   operand_val;
    CRB_Value   result;

    eval_expression(inter, env, operand);
    operand_val = pop_value(inter);

	if (operand_val.type == CRB_INT_VALUE) {
        result.type = CRB_INT_VALUE;
        result.u.int_value = -operand_val.u.int_value;
    } else if (operand_val.type == CRB_DOUBLE_VALUE) {
        result.type = CRB_DOUBLE_VALUE;
        result.u.double_value = -operand_val.u.double_value;
    } else {
        crb_runtime_error(operand->filename, operand->line_number, 
							MINUS_OPERAND_TYPE_ERR,
                          MESSAGE_ARGUMENT_END);
    }
    push_value(inter, &result);
}


CRB_Value crb_eval_minus_expression(CRB_Interpreter *inter,
						CRB_LocalEnvironment *env,
						Expression *operand)
{
	eval_minus_expression(inter, env, operand);
	CRB_Value ret = pop_value(inter);
	return ret;
}


static CRB_LocalEnvironment *
alloc_local_environment(CRB_Interpreter *inter, CRB_Object *prev_scope,
						CRB_Boolean is_closure,
						int line_number, char *func_name)
{
    CRB_LocalEnvironment *ret;
    ret = MEM_malloc(sizeof(CRB_LocalEnvironment));

	if (prev_scope == NULL)
		prev_scope = inter->top_env->environ_scope;

    ret->environ_scope = crb_create_scope_chain(inter, prev_scope,
												is_closure);
	ret->caller_line_number = line_number;
	ret->func_name = func_name;
	ret->parent_env = inter->top_env;
	inter->top_env = ret;

    return ret;
}

static void
dispose_local_environment(CRB_Interpreter *inter)
{
	DBG_assert(inter->top_env!=NULL, 
			("dispose_local_environment with top_env\n"));

	CRB_LocalEnvironment *env = inter->top_env;
	inter->top_env = env->parent_env;


    MEM_free(env);
}

static void
call_native_function(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
					 CRB_LocalEnvironment *caller_env,
                     Expression *expr, CRB_NativeFunctionProc *proc)
{
    int         arg_count;
    ArgumentList        *arg_p;

   	//printf("call_native_function, before argument\n"); 
    
    for (arg_p = expr->u.function_call_expression.argument,  arg_count = 0;
         arg_p; arg_p = arg_p->next, arg_count++) {
        eval_expression(inter, caller_env, arg_p->expression);
    }

	//printf("call_native_function, after argument\n");

	// use the stack to pass arguments and get result
	crb_gc_disable(inter);
    proc(inter, env, arg_count, expr->filename, expr->line_number);

	//printf("after proc\n");

	crb_gc_enable(inter);

}

static void
call_crowbar_function(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
						CRB_LocalEnvironment *caller_env,
                      Expression *expr, FunctionDefinition *func)
{
    CRB_Value   value;
    StatementResult     result;
    ArgumentList        *arg_p;
    ParameterList       *param_p;



    for (arg_p = expr->u.function_call_expression.argument,
             param_p = func->u.crowbar_f.parameter;
         arg_p;
         arg_p = arg_p->next, param_p = param_p->next) {
        CRB_Value *arg_val;

        if (param_p == NULL) {
            crb_runtime_error(expr->filename, expr->line_number, 
								ARGUMENT_TOO_MANY_ERR,
                              MESSAGE_ARGUMENT_END);
        }
        eval_expression(inter, caller_env, arg_p->expression);
        arg_val = peek_stack(inter, 0);
		Variable *variable = crb_search_scope_variable(inter,
								env->environ_scope, param_p->name, 
								CRB_TRUE);
		variable->value = *arg_val;
		pop_value(inter);
    }
    if (param_p) {
        crb_runtime_error(expr->filename, expr->line_number, 
							ARGUMENT_TOO_FEW_ERR,
                          MESSAGE_ARGUMENT_END);
    }
    result = crb_execute_statement_list(inter, env,
                                        func->u.crowbar_f.block
                                        ->statement_list);
    if (result.type == RETURN_STATEMENT_RESULT) {
        value = result.u.return_value;
    } else {
        value.type = CRB_NULL_VALUE;
    }

    push_value(inter, &value);
}

static void
eval_function_call_expression(CRB_Interpreter *inter,
								CRB_LocalEnvironment *env,
                              	Expression *expr)
{
    FunctionDefinition  *func = NULL;
	CRB_Boolean is_closure = CRB_FALSE;
	CRB_Value value;
	CRB_Object *prev_scope = NULL;
	FunctionDefinition fake_function;
	CRB_Boolean is_fake_method = CRB_FALSE;

	CRB_LocalEnvironment *local_env;
	

	Expression *function_name_expr = expr->u.function_call_expression.expr;

	if (function_name_expr->type == IDENTIFIER_EXPRESSION) {
    	char *identifier = function_name_expr->u.identifier;

    	func = crb_search_function(identifier);
	}

	if (func == NULL) {
		eval_expression(inter, env, function_name_expr);
		CRB_Value *pv;

		pv = peek_stack(inter, 0);

		if (pv->type == CRB_CLOSURE_VALUE) {
			is_closure = CRB_TRUE;
			value = *pv;
			func = pv->u.closure_value.closure_expr->u.closure_definition.function;
			prev_scope = pv->u.closure_value.scope_obj;
		}
		else if (pv->type == CRB_FAKE_METHOD_VALUE) {
			fake_function = crb_get_fake_method_definition(*pv);
			func = &fake_function;
			is_fake_method = CRB_TRUE;
			value = *pv;
		}
		pop_value(inter);
	}

	if (func == NULL) {
		crb_runtime_error(expr->filename, expr->line_number, 
							FUNCTION_NOT_FOUND_ERR,
                          STRING_MESSAGE_ARGUMENT, "name", "",
                          MESSAGE_ARGUMENT_END);
	}

	//printf("call function %d, name_expr->type=%d\n", func->type,
	//		function_name_expr->type);

	//printf("before alloc_local_environment\n");
	local_env = alloc_local_environment(inter, prev_scope, is_closure,
										expr->line_number, func->name);
	//printf("after alloc_local_environment\n");

	//printf("call function %s\n", func->name ? func->name : "unknown");

	if (is_closure) {
		if (func->name != NULL) {
			// add the closure name to scope
			Variable *variable = crb_search_scope_variable(
					inter, local_env->environ_scope, func->name,
					CRB_TRUE);
			variable->value = value;
		}
	}
	else if (is_fake_method) {
		Variable *variable = crb_search_scope_variable(
				inter, local_env->environ_scope, "this",
				CRB_TRUE);
		variable->value.type = crb_object_type_to_value_type(
								value.u.fake_method.object->type);
		variable->value.u.object_value = value.u.fake_method.object;
	}


    switch (func->type) {
    case CROWBAR_FUNCTION_DEFINITION:
        call_crowbar_function(inter, local_env, env, expr, func);
        break;
    case NATIVE_FUNCTION_DEFINITION:
        call_native_function(inter, local_env, env, expr, 
								func->u.native_f.proc);
        break;
    default:
        DBG_panic(("bad case..%d\n", func->type));
    }
	
	dispose_local_environment(inter);

}


static void eval_array_expression(CRB_Interpreter *inter,
									CRB_LocalEnvironment *env,
									Expression *expr)
{
	int array_size = 0;
	ExpressionList *list;

	for (list = expr->u.array_expression; list != NULL; list = list->next)
		array_size++;

	CRB_Value array_val;

	array_val.type = CRB_ARRAY_VALUE;
	array_val.u.object_value = crb_create_array(inter, array_size);
	push_value(inter, &array_val);

	int i = 0;
	for (list = expr->u.array_expression; list != NULL; 
			list = list->next) {
		eval_expression(inter, env, list->expression);
		CRB_Value *pv = peek_stack(inter, 0);
		array_val.u.object_value->u.array.array[i++] = *pv;
		pop_value(inter);
	}
	
}


static void eval_index_expression(CRB_Interpreter *inter,
									CRB_LocalEnvironment *env,
									Expression *expr)
{
	CRB_Value *left;

	left = get_array_element_lvalue(inter, env, expr);

	// replace array to array[] on the stack	
	CRB_Value *ret_pos = peek_stack(inter, 0);
	*ret_pos = *left;

}

static void eval_incdec_expression(CRB_Interpreter *inter,
									CRB_LocalEnvironment *env,
									Expression *expr)
{
	CRB_Value *dest;

	dest = get_lvalue(inter, env, expr->u.inc_dec.operand);
	if (dest->type != CRB_INT_VALUE)
		crb_runtime_error(expr->filename, expr->line_number, 
				INC_DEC_OPERAND_TYPE_ERR,
				MESSAGE_ARGUMENT_END);

	CRB_Value old_value = *dest;

	if (expr->type == PREV_INCREMENT_EXPRESSION) {
		dest->u.int_value++;
		push_value(inter, dest);
	}
	else if (expr->type == POST_INCREMENT_EXPRESSION) {
		dest->u.int_value++;
		push_value(inter, &old_value);
	}
	else if (expr->type == PREV_DECREMENT_EXPRESSION) {
		dest->u.int_value--;
		push_value(inter, dest);
	}
	else if (expr->type == POST_DECREMENT_EXPRESSION) {
		dest->u.int_value--;
		push_value(inter, &old_value);
	}

}


static void eval_member_expression(CRB_Interpreter *inter,
								CRB_LocalEnvironment *env,
								Expression *expr)
{
	CRB_Value *ret_val = NULL;
	CRB_Value fake_method_value;
	

	eval_expression(inter, env, expr->u.member_expression.expression);
	CRB_Value *left_val = peek_stack(inter, 0);


	if (left_val->type == CRB_ASSOC_VALUE) {
		Variable *variable = crb_search_assoc_variable(inter, 
							left_val->u.object_value, 
							expr->u.member_expression.member_name,
							CRB_FALSE);
		if (variable != NULL)
			ret_val = &(variable->value);
	}

	if (ret_val == NULL && dkc_is_object_value(left_val->type)) {
		fake_method_value.type = CRB_FAKE_METHOD_VALUE;
		fake_method_value.u.fake_method.method_name = 
				expr->u.member_expression.member_name;
		fake_method_value.u.fake_method.object = left_val->u.object_value;
		fake_method_value.u.fake_method.filename = expr->filename;
		fake_method_value.u.fake_method.line_number = expr->line_number;

		ret_val = &fake_method_value;
	}

	if (ret_val == NULL) {
		crb_runtime_error(expr->filename, expr->line_number, 
				MEMBER_OPERATION_NOT_ASSOC_ERR,
				MESSAGE_ARGUMENT_END);
		
	}

	// change the stack top value
	*left_val = *ret_val; 
}



static void eval_closure_definition(CRB_Interpreter *inter,
								CRB_LocalEnvironment *env,
								Expression *expr)
{
	CRB_Value value;
	
	value.type = CRB_CLOSURE_VALUE;
	value.u.closure_value.closure_expr = expr;
	value.u.closure_value.scope_obj = env->environ_scope;
	push_value(inter, &value);
}


static void
eval_expression(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                Expression *expr)
{
	//printf("before eval_expr %d\n", expr->type);
    switch (expr->type) {
    case BOOLEAN_EXPRESSION:
        eval_boolean_expression(inter, expr->u.boolean_value);
        break;
    case INT_EXPRESSION:
        eval_int_expression(inter, expr->u.int_value);
        break;
    case DOUBLE_EXPRESSION:
        eval_double_expression(inter, expr->u.double_value);
        break;
    case STRING_EXPRESSION:
        eval_string_expression(inter, expr->u.string_value);
        break;
	case REGEXP_EXPRESSION:
		eval_regexp_expression(inter, expr->u.regexp_value);
		break;
    case IDENTIFIER_EXPRESSION:
        eval_identifier_expression(inter, env, expr);
        break;
    case ASSIGN_EXPRESSION:
        eval_assign_expression(inter, env,
									expr->u.assign_expression.type,
                                   expr->u.assign_expression.left,
                                   expr->u.assign_expression.operand);
        break;
    case ADD_EXPRESSION:        /* FALLTHRU */
    case SUB_EXPRESSION:        /* FALLTHRU */
    case MUL_EXPRESSION:        /* FALLTHRU */
    case DIV_EXPRESSION:        /* FALLTHRU */
    case MOD_EXPRESSION:        /* FALLTHRU */
    case EQ_EXPRESSION: /* FALLTHRU */
    case NE_EXPRESSION: /* FALLTHRU */
    case GT_EXPRESSION: /* FALLTHRU */
    case GE_EXPRESSION: /* FALLTHRU */
    case LT_EXPRESSION: /* FALLTHRU */
    case LE_EXPRESSION:
        eval_binary_expression(inter, env,
                                       expr->type,
                                       expr->u.binary_expression.left,
                                       expr->u.binary_expression.right);
        break;
	case NOT_EXPRESSION:
		eval_not_expression(inter, env, expr->u.not_expression.sub_expr);
		break;
    case LOGICAL_AND_EXPRESSION:/* FALLTHRU */
    case LOGICAL_OR_EXPRESSION:
        eval_logical_and_or_expression(inter, env, expr->type,
                                           expr->u.binary_expression.left,
                                           expr->u.binary_expression.right);
        break;
    case MINUS_EXPRESSION:
        eval_minus_expression(inter, env, expr->u.minus_expression);
        break;
    case FUNCTION_CALL_EXPRESSION:
        eval_function_call_expression(inter, env, expr);
        break;
    case NULL_EXPRESSION:
        eval_null_expression(inter);
        break;
	case ARRAY_EXPRESSION:
		eval_array_expression(inter, env, expr);
		break;
	case INDEX_EXPRESSION:
		eval_index_expression(inter, env, expr);
		break;
	case PREV_INCREMENT_EXPRESSION:		/* FALLTHRU */
	case PREV_DECREMENT_EXPRESSION:		/* FALLTHRU */
	case POST_INCREMENT_EXPRESSION:		/* FALLTHRU */
	case POST_DECREMENT_EXPRESSION:
		eval_incdec_expression(inter, env, expr);
		break;
	case MEMBER_EXPRESSION:
		eval_member_expression(inter, env, expr);
		break;
	case CLOSURE_DEFINITION:
		eval_closure_definition(inter, env, expr);
		break;
    case EXPRESSION_TYPE_COUNT_PLUS_1:  /* FALLTHRU */
    default:
        DBG_panic(("bad case. type..%d\n", expr->type));
    }

	//printf("after eval_expr %d\n", expr->type);
}

CRB_Value
crb_eval_expression(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                    Expression *expr)
{
    eval_expression(inter, env, expr);
	return pop_value(inter);
}

CRB_Value* crb_eval_and_peek_expression(CRB_Interpreter *inter,
					CRB_LocalEnvironment *env, Expression *expr)
{
	eval_expression(inter, env, expr);
	return peek_stack(inter, 0);
}



void crb_stack_push_value(CRB_Interpreter *inter, CRB_Value *value)
{
	push_value(inter, value);
}

CRB_Value crb_stack_pop_value(CRB_Interpreter *inter)
{
	return pop_value(inter);
}


CRB_Value* crb_stack_peek_value(CRB_Interpreter *inter, int index)
{
	return peek_stack(inter, index);
}


void crb_stack_shrink_size(CRB_Interpreter *inter, int shrink_size)
{
	shrink_stack(inter, shrink_size);
}




void crb_eval_function_call_expression(CRB_Interpreter *inter,
								CRB_LocalEnvironment *env,
                              	Expression *expr)
{
	eval_function_call_expression(inter, env, expr);

}

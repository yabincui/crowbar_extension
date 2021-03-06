#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "crowbar.h"

typedef struct fake_method_struct {
	ObjectType obj_type;
	char *method_name;
	CRB_NativeFunctionProc *fake_method;
} fake_method_struct;

static void fake_method_string_length_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number);

static void fake_method_string_substr_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number);

static void fake_method_array_add_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number);

static void fake_method_array_size_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number);

static void fake_method_array_resize_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number);

static void fake_method_array_insert_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number);

static void fake_method_array_remove_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number);

static void fake_method_exception_print_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number);

static void fake_method_array_iterator_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number);


static fake_method_struct fake_method_array[] = {
	{ STRING_OBJECT, "length", fake_method_string_length_proc },
	{ STRING_OBJECT, "substr", fake_method_string_substr_proc },
	{ ARRAY_OBJECT, "add", fake_method_array_add_proc },
	{ ARRAY_OBJECT, "resize", fake_method_array_resize_proc },
	{ ARRAY_OBJECT, "size", fake_method_array_size_proc },
	{ ARRAY_OBJECT, "insert", fake_method_array_insert_proc },
	{ ARRAY_OBJECT, "remove", fake_method_array_remove_proc },
	{ ARRAY_OBJECT, "iterator", fake_method_array_iterator_proc },
	{ ASSOC_OBJECT, "print_stack_trace", fake_method_exception_print_proc },
	
	{ OBJECT_TYPE_COUNT_PLUS_1, NULL, NULL }
};


FunctionDefinition crb_get_fake_method_definition(CRB_Value fake_method_val)
{
	char *method_name = fake_method_val.u.fake_method.method_name;
	ObjectType type = fake_method_val.u.fake_method.object->type;
	int i;

	for (i=0; fake_method_array[i].fake_method != NULL; i++) {
		if (type == fake_method_array[i].obj_type &&
				strcmp(method_name, fake_method_array[i].method_name)==0) {
			break;
		}
	}

	if (fake_method_array[i].fake_method == NULL) {
		crb_runtime_error(
							fake_method_val.u.fake_method.filename,
							fake_method_val.u.fake_method.line_number,
							NO_SUCH_METHOD_ERR, STRING_MESSAGE_ARGUMENT,
							"method_name", method_name, 
							MESSAGE_ARGUMENT_END);
	}
	
	FunctionDefinition function;
	function.name = method_name;
	function.type = NATIVE_FUNCTION_DEFINITION;
	function.u.native_f.proc = fake_method_array[i].fake_method;
	function.next = NULL;

	return function;

}


static void check_argument_count(int arg_count, int true_count,
								char *filename, int line_number)
{
	if (arg_count < true_count)
		crb_runtime_error(filename, line_number, 
					ARGUMENT_TOO_FEW_ERR, MESSAGE_ARGUMENT_END);
	else if (arg_count > true_count)
		crb_runtime_error(filename, line_number, 
					ARGUMENT_TOO_MANY_ERR, MESSAGE_ARGUMENT_END);
}


static void fake_method_string_length_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number)
{
	check_argument_count(arg_count, 0, filename, line_number);

	Variable *this_variable = crb_search_local_variable(inter, env,
												"this", CRB_FALSE);
	CRB_Object *this_obj = this_variable->value.u.object_value;

	CRB_Value ret_val;

	ret_val.type = CRB_INT_VALUE;
	ret_val.u.int_value = CRB_wcslen(this_obj->u.string.string);

	crb_stack_push_value(inter, &ret_val);
}

static void fake_method_string_substr_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number)
{
	check_argument_count(arg_count, 2, filename, line_number);

	CRB_Value *args = crb_stack_peek_value(inter, 1);

	if (args[0].type != CRB_INT_VALUE || args[1].type != CRB_INT_VALUE) {
		crb_runtime_error(filename, line_number, ARGUMENT_TYPE_MISMATCH_ERR,
							STRING_MESSAGE_ARGUMENT, "func_name", 
							"string.substr", MESSAGE_ARGUMENT_END);
	}
	int begin, length, total_len;
	Variable *this_variable = crb_search_local_variable(inter, env,
												"this", CRB_FALSE);
	CRB_Object *this_obj = this_variable->value.u.object_value;

	begin = args[0].u.int_value;
	length = args[1].u.int_value;
	total_len = CRB_wcslen(this_obj->u.string.string);

	if (begin < 0 || begin >= total_len 
			|| length < 0 || begin + length > total_len) {
		DBG_panic(("string.substr argument value error"));
	}


	CRB_Value ret_val;

	ret_val.type = CRB_STRING_VALUE;
	ret_val.u.object_value = crb_string_substr(inter, this_obj,
								begin, length);
	crb_stack_push_value(inter, &ret_val);
}



static void fake_method_array_add_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number)
{

	check_argument_count(arg_count, 1, filename, line_number);

	Variable *this_variable = crb_search_local_variable(inter, env,
												"this", CRB_FALSE);
	CRB_Object *this_obj = this_variable->value.u.object_value;

	CRB_Value ret_val;

	CRB_Value *args = crb_stack_peek_value(inter, 0);

	crb_array_add(inter, this_obj, &args[0]);

	crb_stack_shrink_size(inter, 1);

	ret_val.type = CRB_NULL_VALUE;
	crb_stack_push_value(inter, &ret_val);
}



static void fake_method_array_size_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number)
{

	check_argument_count(arg_count, 0, filename, line_number);

	Variable *this_variable = crb_search_local_variable(inter, env,
												"this", CRB_FALSE);
	CRB_Object *this_obj = this_variable->value.u.object_value;

	CRB_Value ret_val;

	ret_val.type = CRB_INT_VALUE;
	ret_val.u.int_value = crb_array_size(inter, this_obj);
	crb_stack_push_value(inter, &ret_val);
}


static void fake_method_array_resize_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number)
{

	check_argument_count(arg_count, 1, filename, line_number);

	Variable *this_variable = crb_search_local_variable(inter, env,
												"this", CRB_FALSE);
	CRB_Object *this_obj = this_variable->value.u.object_value;

	CRB_Value ret_val;

	CRB_Value *args = crb_stack_peek_value(inter, 0);

	if (args[0].type != CRB_INT_VALUE) {
		crb_runtime_error(filename, line_number, ARRAY_RESIZE_ARGUMENT_ERR,
							MESSAGE_ARGUMENT_END);
	}

	crb_array_resize(inter, this_obj, args[0].u.int_value);

	crb_stack_shrink_size(inter, 1);

	ret_val.type = CRB_NULL_VALUE;
	crb_stack_push_value(inter, &ret_val);
}


static void fake_method_array_insert_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number)
{
	check_argument_count(arg_count, 2, filename, line_number);

	Variable *this_variable = crb_search_local_variable(inter, env,
												"this", CRB_FALSE);
	CRB_Object *this_obj = this_variable->value.u.object_value;

	CRB_Value ret_val;

	CRB_Value *args = crb_stack_peek_value(inter, 1);

	if (args[0].type != CRB_INT_VALUE) {
		crb_runtime_error(filename, line_number, ARRAY_RESIZE_ARGUMENT_ERR,
							MESSAGE_ARGUMENT_END);
	}

	crb_array_insert(inter, this_obj, args[0].u.int_value,
							&args[1]);

	crb_stack_shrink_size(inter, 2);

	ret_val.type = CRB_NULL_VALUE;
	crb_stack_push_value(inter, &ret_val);
	
}

static void fake_method_array_remove_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number)
{
	check_argument_count(arg_count, 1, filename, line_number);

	Variable *this_variable = crb_search_local_variable(inter, env,
												"this", CRB_FALSE);
	CRB_Object *this_obj = this_variable->value.u.object_value;

	CRB_Value ret_val;

	CRB_Value *args = crb_stack_peek_value(inter, 0);

	if (args[0].type != CRB_INT_VALUE) {
		crb_runtime_error(filename, line_number, ARRAY_RESIZE_ARGUMENT_ERR,
							MESSAGE_ARGUMENT_END);
	}

	crb_array_remove(inter, this_obj, args[0].u.int_value);

	crb_stack_shrink_size(inter, 1);

	ret_val.type = CRB_NULL_VALUE;
	crb_stack_push_value(inter, &ret_val);

}

static void fake_method_array_iterator_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number)
{
	check_argument_count(arg_count, 0, filename, line_number);

	// call crowbar.create_array_iterator in builtin/builtin.crb

	Expression assoc_expr;
	crb_init_identifier_expression(&assoc_expr, "crowbar",
									filename, line_number);

	Expression member_expr;
	crb_init_member_expression(&member_expr, &assoc_expr,
								"create_array_iterator",
								filename, line_number);

	Expression this_expr;
	crb_init_identifier_expression(&this_expr, "this",
									filename, line_number);

	ArgumentList argument;
	crb_init_argument_list(&argument, &this_expr);

	Expression func_call_expr;
	crb_init_function_call_expression(&func_call_expr,
									&member_expr, &argument,
									filename, line_number);

	crb_eval_function_call_expression(inter, env, &func_call_expr);

}

static void fake_method_exception_print_proc(CRB_Interpreter *inter,
										CRB_LocalEnvironment *env,
										int arg_count,
										char *filename, int line_number)
{
	check_argument_count(arg_count, 0, filename, line_number);

	Variable *this_variable = crb_search_local_variable(inter, env,
										"this", CRB_FALSE);
	CRB_Object *this_obj = this_variable->value.u.object_value;


	exception_print_stack_trace(inter, this_obj);

	CRB_Value ret_val;

	ret_val.type = CRB_NULL_VALUE;
	crb_stack_push_value(inter, &ret_val);

}



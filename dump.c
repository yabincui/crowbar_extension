#include <stdio.h>
#include "DBG.h"
#include "CRB.h"
#include "CRB_dev.h"
#include "crowbar.h"



static void dump_expression(Expression *expression, FILE *fpout,
														int space_num);

static void dump_statement(Statement *statement, FILE *fpout, 
														int space_num);

static void dump_line_number(char *filename, int line_number, FILE *fpout)
{
	fprintf(fpout, "#file: \"%s\"\n", filename);
	fprintf(fpout, "#line: %d\n", line_number);
}


static const char* space_num_string(int space_num)
{
	static char space_str[41];

	if (space_num>40) space_num=40;
	space_str[space_num]='\0';
	while (space_num--)
		space_str[space_num]=' ';
	return space_str;
}


static void dump_parameter(ParameterList *parameter, FILE *fpout,
							int space_num)
{
	while (parameter!=NULL) {
		fprintf(fpout, "%sPARAMETER %s\n", space_num_string(space_num),
				parameter->name);
		parameter = parameter->next;
	}
}

static void dump_block(Block *block, FILE *fpout, int space_num)
{
	StatementList *list;
	
	fprintf(fpout, "%sBLOCK\n", space_num_string(space_num));
	
	for (list = block->statement_list; list != NULL; list = list->next)
		dump_statement(list->statement, fpout, space_num+1);

	fprintf(fpout, "%sEND_BLOCK\n", space_num_string(space_num));
}


static void dump_function(FunctionDefinition *function_def, FILE *fpout,
							int space_num)
{
	while (function_def!=NULL) {
		if (function_def->type != CROWBAR_FUNCTION_DEFINITION) {
			function_def = function_def->next;
			continue;
		}
		dump_line_number(function_def->filename, function_def->line_number,
							fpout);
		fprintf(fpout, "%sFUNCTION %s\n", space_num_string(space_num),
				function_def->name);
		dump_parameter(function_def->u.crowbar_f.parameter, fpout,
						space_num+1);
		dump_block(function_def->u.crowbar_f.block, fpout, space_num+1);

		function_def = function_def->next;
	}
}


static void dump_boolean_expression(Expression *expression, FILE *fpout,
														int space_num)
{
	fprintf(fpout, "%sBOOLEAN_EXPRESSION %s\n", space_num_string(space_num),
				(expression->u.boolean_value==CRB_TRUE)? "true" : "false");

}

static void dump_int_expression(Expression *expression, FILE *fpout,
														int space_num)
{
	fprintf(fpout, "%sINT_EXPRESSION %d\n", space_num_string(space_num),
				expression->u.int_value);

}

static void dump_double_expression(Expression *expression, FILE *fpout,
														int space_num)
{
	fprintf(fpout, "%sDOUBLE_EXPRESSION %.12lf\n", 
			space_num_string(space_num), expression->u.double_value);

}


static void dump_string_expression(Expression *expression, FILE *fpout,
														int space_num)
{
	fprintf(fpout, "%sSTRING_EXPRESSION \"",
			space_num_string(space_num));
	CRB_print_wcs(fpout, expression->u.string_value);
	fprintf(fpout, "\"\n");

}

static void dump_regexp_expression(Expression *expression, FILE *fpout,
														int space_num)
{
	fprintf(fpout, "%sREGEXP_EXPRESSION %%r%c",
			space_num_string(space_num), 
			expression->u.regexp_value->protect_char);
	CRB_print_wcs(fpout, expression->u.regexp_value->pattern);
	fprintf(fpout, "%c\n", expression->u.regexp_value->protect_char);
}


static void dump_identifier_expression(Expression *expression, FILE *fpout,
														int space_num)
{
	fprintf(fpout, "%sIDENTIFIER_EXPRESSION %s\n",
			space_num_string(space_num), expression->u.identifier);

}

static void dump_assign_expression(Expression *expression, FILE *fpout,
														int space_num)
{
	fprintf(fpout, "%sASSIGN_EXPRESSION\n",
			space_num_string(space_num));
	dump_expression(expression->u.assign_expression.left, fpout,
					space_num+1);
	dump_expression(expression->u.assign_expression.operand, fpout,
					space_num+1);

}


static void dump_binary_expression(Expression *expression, FILE *fpout,
														int space_num)
{
	const char *type_str = "";
	switch (expression->type) {
		case ADD_EXPRESSION:
			type_str = "ADD_EXPRESSION";
			break;
		case SUB_EXPRESSION:
			type_str = "SUB_EXPRESSION";
			break;
		case MUL_EXPRESSION:
			type_str = "MUL_EXPRESSION";
			break;
		case DIV_EXPRESSION:
			type_str = "DIV_EXPRESSION";
			break;
		case MOD_EXPRESSION:
			type_str = "MOD_EXPRESSION";
			break;
		case EQ_EXPRESSION:
			type_str = "EQ_EXPRESSION";
			break;
		case NE_EXPRESSION:
			type_str = "NE_EXPRESSION";
			break;
		case GT_EXPRESSION:
			type_str = "GT_EXPRESSION";
			break;
		case GE_EXPRESSION:
			type_str = "GE_EXPRESSION";
			break;
		case LT_EXPRESSION:
			type_str = "LT_EXPRESSION";
			break;
		case LE_EXPRESSION:
			type_str = "LE_EXPRESSION";
			break;
		default:
			DBG_panic(("bad operator..%d\n", expression->type));
	};

	fprintf(fpout, "%s%s\n", space_num_string(space_num), type_str);
	dump_expression(expression->u.binary_expression.left, fpout, 
														space_num+1);
	dump_expression(expression->u.binary_expression.right, fpout,
														space_num+1);
}


static void dump_logical_and_or_expression(Expression *expression,
										FILE *fpout, int space_num)
{
	const char *type_str = "";
	if (expression->type == LOGICAL_AND_EXPRESSION)
		type_str = "LOGICAL_AND_EXPRESSION";
	else if (expression->type == LOGICAL_OR_EXPRESSION)
		type_str = "LOGICAL_OR_EXPRESSION";
	else
		DBG_panic(("bad operator..%d\n", expression->type));

	fprintf(fpout, "%s%s\n", space_num_string(space_num), type_str);
	dump_expression(expression->u.binary_expression.left, fpout,
														space_num+1);
	dump_expression(expression->u.binary_expression.right, fpout,
														space_num+1);

}


static void dump_minus_expression(Expression *expression, FILE *fpout,
														int space_num)
{
	const char *type_str = "";
	if (expression->type == MINUS_EXPRESSION)
		type_str = "MINUS_EXPRESSION";
	else
		DBG_panic(("bad operator..%d\n", expression->type));

	fprintf(fpout, "%s%s\n", space_num_string(space_num), type_str);
	dump_expression(expression->u.minus_expression, fpout, space_num+1);

}


static void dump_function_call_expression(Expression *expression,
											FILE *fpout, int space_num)
{
	int argument_num = 0;
	ArgumentList *list = expression->u.function_call_expression.argument;
	while (list != NULL) {
		argument_num++;
		list = list->next;
	}

	fprintf(fpout, "%sFUNCTION_CALL_EXPRESSION %d\n",
				space_num_string(space_num), 
				argument_num);
	fprintf(fpout, "%sFUNCTION_NAME\n", space_num_string(space_num+1));
	dump_expression(expression->u.function_call_expression.expr,
					fpout, space_num+2);

	list = expression->u.function_call_expression.argument;
	while (list!=NULL) {
		fprintf(fpout, "%sARGUMENT\n", space_num_string(space_num+1));
		dump_expression(list->expression, fpout, space_num+2);
		list = list->next;
	}
}


static void dump_null_expression(Expression *expression, FILE *fpout,
														int space_num)
{
	fprintf(fpout, "%sNULL_EXPRESSION\n", space_num_string(space_num));

}


static void dump_method_call_expression(Expression *expression, 
										FILE *fpout,
										int space_num)
{
	int argument_num = 0;
	ArgumentList *list;
	for (list = expression->u.method_call_expression.argument;
			list != NULL; list = list->next)
		argument_num++;

	fprintf(fpout, "%sMETHOD_CALL_EXPRESSION %s %d\n",
			space_num_string(space_num), 
			expression->u.method_call_expression.identifier,
			argument_num);
	
	dump_expression(expression->u.method_call_expression.expression,
			fpout, space_num+1);
	for (list = expression->u.method_call_expression.argument;
			list != NULL; list = list->next) {
		fprintf(fpout, "%sARGUMENT\n", space_num_string(space_num+1));
		dump_expression(list->expression, fpout, space_num+2);
	}
	
}

static void dump_array_expression(Expression *expression,
									FILE *fpout, int space_num)
{
	int expr_num = 0;
	ExpressionList *list;
	for (list = expression->u.array_expression; list != NULL;
			list = list->next)
		expr_num++;

	fprintf(fpout, "%sARRAY_EXPRESSION %d\n",
			space_num_string(space_num), expr_num);
	for (list = expression->u.array_expression; list != NULL;
			list = list->next) {
		dump_expression(list->expression, fpout, space_num+1);
	}
}

static void dump_index_expression(Expression *expression,
								FILE *fpout, int space_num)
{
	fprintf(fpout, "%sINDEX_EXPRESSION\n", space_num_string(space_num));
	dump_expression(expression->u.index_expression.array, fpout, 
			space_num+1);
	dump_expression(expression->u.index_expression.index, fpout,
			space_num+1);
}


static void dump_incdec_expression(Expression *expression,
								FILE *fpout, int space_num)
{
	char *expr_name = NULL; 
	if (expression->type == PREV_INCREMENT_EXPRESSION)
		expr_name = "PREV_INCREMENT_EXPRESSION";
	else if (expression->type == POST_INCREMENT_EXPRESSION)
		expr_name = "POST_INCREMENT_EXPRESSION";
	else if (expression->type == PREV_DECREMENT_EXPRESSION)
		expr_name = "PREV_DECREMENT_EXPRESSION";
	else if (expression->type == POST_DECREMENT_EXPRESSION)
		expr_name = "POST_DECREMENT_EXPRESSION";

	fprintf(fpout, "%s%s\n", space_num_string(space_num),
				expr_name);
	dump_expression(expression->u.inc_dec.operand, fpout, space_num+1);
}


static void dump_member_expression(Expression *expression,
								FILE *fpout, int space_num)
{
	fprintf(fpout, "%sMEMBER_EXPRESSION %s\n", space_num_string(space_num),
								expression->u.member_expression.member_name);
	dump_expression(expression->u.member_expression.expression, fpout, space_num+1);
}

static void dump_closure_definition(Expression *expression,
								FILE *fpout, int space_num)
{
	FunctionDefinition *func = expression->u.closure_definition.function;

	fprintf(fpout, "%sCLOSURE_DEFINITION %s\n", space_num_string(space_num),
			func->name ? func->name : "");
	dump_parameter(func->u.crowbar_f.parameter, fpout,
						space_num+1);
	dump_block(func->u.crowbar_f.block, fpout, 
						space_num+1);

}


static void dump_expression(Expression *expression, FILE *fpout,
														int space_num)
{
	dump_line_number(expression->filename, expression->line_number, fpout);
	switch (expression->type) {
		case BOOLEAN_EXPRESSION:
			dump_boolean_expression(expression, fpout, space_num);
			break;
		case INT_EXPRESSION:
			dump_int_expression(expression, fpout, space_num);
			break;
		case DOUBLE_EXPRESSION:
			dump_double_expression(expression, fpout, space_num);
			break;
		case STRING_EXPRESSION:
			dump_string_expression(expression, fpout, space_num);
			break;
		case REGEXP_EXPRESSION:
			dump_regexp_expression(expression, fpout, space_num);
			break;
		case IDENTIFIER_EXPRESSION:
			dump_identifier_expression(expression, fpout, space_num);
			break;
		case ASSIGN_EXPRESSION:
			dump_assign_expression(expression, fpout, space_num);
			break;
		case ADD_EXPRESSION:            // fall through
		case SUB_EXPRESSION:			// fall through
		case MUL_EXPRESSION:			// fall through
		case DIV_EXPRESSION:			// fall through
		case MOD_EXPRESSION:			// fall through
		case EQ_EXPRESSION:				// fall through
		case NE_EXPRESSION:				// fall through
		case GT_EXPRESSION:				// fall through
		case GE_EXPRESSION:				// fall through
		case LT_EXPRESSION:				// fall through
		case LE_EXPRESSION:				// fall through
			dump_binary_expression(expression, fpout, space_num);
			break;
		case LOGICAL_AND_EXPRESSION:	// fall through
		case LOGICAL_OR_EXPRESSION:
			dump_logical_and_or_expression(expression, fpout, space_num);
			break;
		case MINUS_EXPRESSION:
			dump_minus_expression(expression, fpout, space_num);
			break;
		case FUNCTION_CALL_EXPRESSION:
			dump_function_call_expression(expression, fpout, space_num);
			break;
		case NULL_EXPRESSION:
			dump_null_expression(expression, fpout, space_num);
			break;
		case METHOD_CALL_EXPRESSION:
			dump_method_call_expression(expression, fpout, space_num);
			break;
		case ARRAY_EXPRESSION:
			dump_array_expression(expression, fpout, space_num);
			break;
		case INDEX_EXPRESSION:
			dump_index_expression(expression, fpout, space_num);
			break;
		case PREV_INCREMENT_EXPRESSION:		// fall through
		case POST_INCREMENT_EXPRESSION:		// fall through
		case PREV_DECREMENT_EXPRESSION:		// fall through
		case POST_DECREMENT_EXPRESSION:
			dump_incdec_expression(expression, fpout, space_num);
			break;
		case MEMBER_EXPRESSION:
			dump_member_expression(expression, fpout, space_num);
			break;
		case CLOSURE_DEFINITION:
			dump_closure_definition(expression, fpout, space_num);
			break;
		case EXPRESSION_TYPE_COUNT_PLUS_1:	// fall through
		default:
			DBG_panic(("bad case. type..%d\n", expression->type));

	}

}


static void dump_expression_statement(Statement *statement, FILE *fpout,
															int space_num)
{
	fprintf(fpout, "%sEXPRESSION_STATEMENT\n", space_num_string(space_num));
	dump_expression(statement->u.expression_s, fpout, space_num+1);
}

static void dump_global_statement(Statement *statement, FILE *fpout,
															int space_num)
{
	fprintf(fpout, "%sGLOBAL_STATEMENT", space_num_string(space_num));
	IdentifierList *list = statement->u.global_s.identifier_list;
	while (list!=NULL) {
		fprintf(fpout, " %s", list->name);
		list = list->next;
	}
	fprintf(fpout, "\n");

}


static void dump_if_statement(Statement *statement, FILE *fpout,
															int space_num)
{
	Elsif *elsif_list = statement->u.if_s.elsif_list;
	Statement *else_stat = statement->u.if_s.else_statement;
	
	fprintf(fpout, "%sIF_STATEMENT %s\n", space_num_string(space_num),
			(elsif_list || else_stat) ? "HAVE_NEXT" : "NOT_HAVE_NEXT");

	dump_expression(statement->u.if_s.condition, fpout, space_num+1);
	dump_statement(statement->u.if_s.then_statement, fpout, space_num+1);

	while (elsif_list != NULL) {

		fprintf(fpout, "%sELSIF %s\n", space_num_string(space_num+1),
		  (elsif_list->next || else_stat) ? "HAVE_NEXT" : "NOT_HAVE_NEXT");
		dump_expression(elsif_list->condition, fpout, space_num+2);
		dump_statement(elsif_list->statement, fpout, space_num+2);

		elsif_list = elsif_list->next;
	}

	if (else_stat != NULL) {
		fprintf(fpout, "%sELSE\n", space_num_string(space_num+1));
		dump_statement(else_stat, fpout, space_num+2);
	}
}


static void dump_while_statement(Statement *statement, FILE *fpout,
															int space_num)
{
	fprintf(fpout, "%sWHILE_STATEMENT\n", space_num_string(space_num));
	dump_expression(statement->u.while_s.condition, fpout, space_num+1);
	dump_statement(statement->u.while_s.statement, fpout, space_num+1);

}


static void dump_for_statement(Statement *statement, FILE *fpout,
															int space_num)
{
	fprintf(fpout, "%sFOR_STATEMENT\n", space_num_string(space_num));
	if (statement->u.for_s.init) {
		fprintf(fpout, "%sINIT\n", space_num_string(space_num+1));
		dump_expression(statement->u.for_s.init, fpout, space_num+2);
	}
	if (statement->u.for_s.condition) {
		fprintf(fpout, "%sCONDITION\n", space_num_string(space_num+1));
		dump_expression(statement->u.for_s.condition, fpout, space_num+2);
	}
	if (statement->u.for_s.post) {
		fprintf(fpout, "%sPOST\n", space_num_string(space_num+1));
		dump_expression(statement->u.for_s.post, fpout, space_num+2);
	}
	dump_statement(statement->u.for_s.statement, fpout, space_num+1);

}


static void dump_return_statement(Statement *statement, FILE *fpout,
															int space_num)
{
	fprintf(fpout, "%sRETURN_STATEMENT %s\n", space_num_string(space_num),
	  (statement->u.return_s.return_value) ? "HAVE_EXPR" : "NOT_HAVE_EXPR");
	if (statement->u.return_s.return_value)
		dump_expression(statement->u.return_s.return_value, fpout,
															space_num+1);

}


static void dump_break_statement(Statement *statement, FILE *fpout,
															int space_num)
{
	fprintf(fpout, "%sBREAK_STATEMENT\n", space_num_string(space_num));

}

static void dump_continue_statement(Statement *statement, FILE *fpout,
															int space_num)
{
	fprintf(fpout, "%sCONTINUE_STATEMENT\n", space_num_string(space_num));
}


static void dump_block_statement(Statement *statement, FILE *fpout,
															int space_num)
{
	fprintf(fpout, "%sBLOCK_STATEMENT\n", space_num_string(space_num));
	dump_block(statement->u.block_s.block, fpout, space_num+1);
}


static void dump_try_statement(Statement *statement, FILE *fpout,
										int space_num)
{
	fprintf(fpout, "%sTRY_STATEMENT", space_num_string(space_num));
	if (statement->u.try_s.identifier)
		fprintf(fpout, " %s", statement->u.try_s.identifier);
	if (statement->u.try_s.catch_st != NULL)
		fprintf(fpout, " WITH_CATCH");
	if (statement->u.try_s.final_st != NULL)
		fprintf(fpout, " WITH_FINALLY");
	fprintf(fpout, "\n");

	dump_statement(statement->u.try_s.run_st, fpout, space_num+1);
	if (statement->u.try_s.catch_st != NULL)
		dump_statement(statement->u.try_s.catch_st, fpout, space_num+1);
	if (statement->u.try_s.final_st != NULL)
		dump_statement(statement->u.try_s.final_st, fpout, space_num+1);
}


static void dump_throw_statement(Statement *statement, FILE *fpout,
										int space_num)
{
	fprintf(fpout, "%sTHROW_STATEMENT\n", space_num_string(space_num));
	dump_expression(statement->u.throw_s.throw_expr, fpout, space_num+1);
}

static void dump_foreach_statement(Statement *statement, FILE *fpout,
										int space_num)
{
	fprintf(fpout, "%sFOREACH_STATEMENT %s\n", space_num_string(space_num),
					statement->u.foreach_s.identifier);
	dump_expression(statement->u.foreach_s.array_expr, fpout, space_num+1);
	dump_statement(statement->u.foreach_s.sub_st, fpout, space_num+1);
}


static void dump_statement(Statement *statement, FILE *fpout, int space_num)
{
	dump_line_number(statement->filename, statement->line_number, fpout);
	switch (statement->type) {
		case EXPRESSION_STATEMENT:
			dump_expression_statement(statement, fpout, space_num);
			break;
		case GLOBAL_STATEMENT:
			dump_global_statement(statement, fpout, space_num);
			break;
		case IF_STATEMENT:
			dump_if_statement(statement, fpout, space_num);
			break;
		case WHILE_STATEMENT:
			dump_while_statement(statement, fpout, space_num);
			break;
		case FOR_STATEMENT:
			dump_for_statement(statement, fpout, space_num);
			break;
		case RETURN_STATEMENT:
			dump_return_statement(statement, fpout, space_num);
			break;
		case BREAK_STATEMENT:
			dump_break_statement(statement, fpout, space_num);
			break;
		case CONTINUE_STATEMENT:
			dump_continue_statement(statement, fpout, space_num);
			break;
		case BLOCK_STATEMENT:
			dump_block_statement(statement, fpout, space_num);
			break;
		case TRY_STATEMENT:
			dump_try_statement(statement, fpout, space_num);
			break;
		case THROW_STATEMENT:
			dump_throw_statement(statement, fpout, space_num);
			break;
		case FOREACH_STATEMENT:
			dump_foreach_statement(statement, fpout, space_num);
			break;
		case STATEMENT_TYPE_COUNT_PLUS_1:		// fall through
		default:
			DBG_panic(("bad case...%d", statement->type));

	}
}


void CRB_dump_interpreter(CRB_Interpreter *interpreter, FILE *fpout)
{
	dump_function(interpreter->function_list, fpout, 0);
	
	StatementList *list;
	for (list = interpreter->statement_list; list != NULL; 
												list = list->next)
		dump_statement(list->statement, fpout, 0);
			
}

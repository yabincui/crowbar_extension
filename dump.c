#include <stdio.h>
#include "DBG.h"
#include "CRB.h"
#include "CRB_dev.h"
#include "crowbar.h"



static void dump_expression(Expression *expression, FILE *fpout,
														int space_num);

static void dump_statement(Statement *statement, FILE *fpout, 
														int space_num);

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
	fprintf(fpout, "%sSTRING_EXPRESSION \"%s\"\n",
			space_num_string(space_num), expression->u.string_value);

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
	fprintf(fpout, "%sASSIGN_EXPRESSION %s\n",
			space_num_string(space_num), 
			expression->u.assign_expression.variable);
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

	fprintf(fpout, "%sFUNCTION_CALL_EXPRESSION %s %d\n",
				space_num_string(space_num), 
				expression->u.function_call_expression.identifier,
				argument_num);
	list = expression->u.function_call_expression.argument;
	while (list!=NULL) {
		fprintf(fpout, "%sARGUMENT\n", space_num_string(space_num));
		dump_expression(list->expression, fpout, space_num);
		list = list->next;
	}
}


static void dump_null_expression(Expression *expression, FILE *fpout,
														int space_num)
{
	fprintf(fpout, "%sNULL_EXPRESSION\n", space_num_string(space_num));

}


static void dump_expression(Expression *expression, FILE *fpout,
														int space_num)
{
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



static void dump_statement(Statement *statement, FILE *fpout, int space_num)
{
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
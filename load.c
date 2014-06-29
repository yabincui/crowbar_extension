#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "CRB.h"
#include "CRB_dev.h"
#include "crowbar.h"

#define MAX_LINE_ELEMENT_NUM     10

typedef struct {
	char *str;
	char * argv[MAX_LINE_ELEMENT_NUM];
	int argc;
} LineElement;


static LineElement *line_buffer = NULL;
static int line_buffer_valid = 0;

static Expression* load_expression(FILE *fpin);

static Statement* load_statement(FILE *fpin);


static const char* str_find(const char *str, const char *to_find)
{
	int len = strlen(to_find);
	const char *pos;

	for (pos=str; *pos; pos++) {
		if (strncmp(pos, to_find, len)==0)
			return pos;
	}
	return NULL;
}


static int line_to_arguments(char *line, char **args, int max_arg_num)
{
	int i;
	char *p = line;
	int in_string = 0;

	for (i=0; i<max_arg_num-1; ) {
		while (isspace(*p)) p++;
		if (*p=='\0') break;
		args[i] = p;
		while ((!isspace(*p) || in_string) && *p!='\0') {
			if (*p == '\"') {
				if (p==line || *(p-1)!='\\')
					in_string = !in_string;
			}
			p++;
		}
		i++;
		if (*p=='\0') break;
		*p = '\0';
		p++;
	}
	if (*p!='\0')
		DBG_panic(("MAX_LINE_ELEMENT_NUM not enough!"));
	
	args[i] = NULL;
	return i;
}

#define get_next_line(fpin) \
		get_next_line_debug(fpin, __FILE__, __LINE__)

static LineElement* get_next_line_debug(FILE *fpin,
								char *filename, int file_line)
{
	if (line_buffer_valid) {
		line_buffer_valid = 0;
		LineElement *p = line_buffer;
		line_buffer = NULL;
		return p;
	}
	else {
		char *p = NULL;
		char buf[80];
		int size = 0;
		int in_string = 0;
	
		while (fgets(buf, sizeof(buf), fpin)!=NULL) {
			int len = strlen(buf);
			if (size==0) {
				p = MEM_malloc(len+1);
				strcpy(p, buf);
			}
			else {
				p = MEM_realloc(p, size+len+1);
				strcat(p, buf);
			}
			size += len;

			char *pos = p+size-len;
			while (*pos != '\0') {
				if (*pos == '\"') {
					if (pos == p || *(pos-1)!='\\')
						in_string = !in_string;
				}
				pos++;
			}

			if (!in_string && (*(p+size-1) == '\n'))
				break;
		}

		LineElement *line = MEM_malloc_debug(sizeof(LineElement),
											filename, file_line);

		line->str = p;
		if (line->str)
			line->argc = line_to_arguments(line->str, line->argv,
										MAX_LINE_ELEMENT_NUM);
		else
			line->argc = 0;

		return line;
	}
}


static void unget_line(LineElement *line)
{
	if (!line_buffer_valid) {
		line_buffer_valid = 1;
		line_buffer = line;
	}
	else {
		DBG_panic(("unbuffer line twice\n"));
	}

}


static void release_line(LineElement *line)
{
	if (line->str)
		MEM_free(line->str);
	MEM_free(line);
}



ParameterList* load_parameter_list(FILE *fpin)
{
	LineElement *line;
	ParameterList *list = NULL;

	while (1) {
		line = get_next_line(fpin);

		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (line->argc==2 && strcmp(line->argv[0], "PARAMETER")==0) {

			char *identifier = crb_create_identifier(line->argv[1]);

			if (list == NULL)
				list = crb_create_parameter(identifier);
			else
				list = crb_chain_parameter(list, identifier);
			
			release_line(line);
		}
		else {
			unget_line(line); break;
		}
	}
	return list;
}


static StatementList* load_statement_list(FILE *fpin)
{
	LineElement *line;
	StatementList *list = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (str_find(line->argv[0], "STATEMENT")) {
			unget_line(line);
			Statement *stat = load_statement(fpin);
			if (list==NULL)
				list = crb_create_statement_list(stat);
			else
				list = crb_chain_statement_list(list, stat);
		}
		else {
			unget_line(line); break;
		}
	}
	return list;

}


static Block* load_block(FILE *fpin)
{
	LineElement *line;
	Block *block = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (line->argc==1 && strcmp(line->argv[0], "BLOCK")==0) {
			release_line(line);	
			
			StatementList *list = load_statement_list(fpin);
			
			block = crb_create_block(list);

			while (1) {
				line = get_next_line(fpin);
				if (!line->str) {
					release_line(line); break;
				}
				if (line->argc==0) {
					release_line(line); continue;
				}

				if (strcmp(line->argv[0], "END_BLOCK")==0) {
					release_line(line); break;
				}
				else {
					DBG_panic(("unexpected END_BLOCK:%s\n", line->argv[0]));
				}

			}

			break;
		}
		else {
			unget_line(line); break;
		}
	}
	return block;
}


static void load_function(FILE *fpin)
{
	LineElement *line;
	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}


		if (line->argc==2 && strcmp(line->argv[0], "FUNCTION")==0) {

			char *identifier = crb_create_identifier(line->argv[1]);

			release_line(line);

			ParameterList *parameter_list = load_parameter_list(fpin);
			Block *block = load_block(fpin);

			crb_function_define(identifier, parameter_list, block);
		}
		else {
			unget_line(line);
			break;
		}
	}
}


static Expression* load_boolean_expression(FILE *fpin)
{
	LineElement *line;
	Expression *expr = NULL;
	
	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "BOOLEAN_EXPRESSION")==0) {
			CRB_Boolean value = CRB_FALSE;
			if (strcmp(line->argv[1], "true")==0)
				value = CRB_TRUE;

			expr = crb_create_boolean_expression(value);
			release_line(line);
			break;
		}
		else {
			DBG_panic(("unexpected boolean expression:%s\n", line->argv[0]));
		}

	}

	return expr;

}


static Expression* load_int_expression(FILE *fpin)
{
	LineElement *line;
	Expression *expr = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}
		if (strcmp(line->argv[0], "INT_EXPRESSION")==0) {
			int value;
			sscanf(line->argv[1], "%d", &value);
			expr = crb_alloc_expression(INT_EXPRESSION);
			expr->u.int_value = value;
			release_line(line);
			break;
		}
		else {
			DBG_panic(("unexpected int expression:%s\n", line->argv[0]));
		}
	}
	return expr;
}


static Expression* load_double_expression(FILE *fpin)
{
	LineElement *line;
	Expression *expr = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "DOUBLE_EXPRESSION")==0) {
			double value;
			sscanf(line->argv[1], "%lf", &value);
			expr = crb_alloc_expression(DOUBLE_EXPRESSION);
			expr->u.double_value = value;
			release_line(line);
			break;
		}
		else {
			DBG_panic(("unexpected double expression:%s\n", line->argv[0]));
		}
	}
	return expr;

}

static Expression* load_string_expression(FILE *fpin)
{
	LineElement *line;
	Expression *expr = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "STRING_EXPRESSION")==0) {
			expr = crb_alloc_expression(STRING_EXPRESSION);
			int len = strlen(line->argv[1])-1; // \"abc\"
			char *str = crb_malloc(len);
			int i;
			for (i=0; i<len-1; i++) {
				str[i] = line->argv[1][i+1];
			}
			str[i] = '\0';
			expr->u.string_value = str;
			release_line(line);
			break;
		}
		else {
			DBG_panic(("unexpected string expression:%s\n", line->argv[0]));

		}
	}

	return expr;
}


static Expression* load_identifier_expression(FILE *fpin)
{
	LineElement *line;
	Expression *expr = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "IDENTIFIER_EXPRESSION")==0) {
			char *identifier = crb_create_identifier(line->argv[1]);
			expr = crb_create_identifier_expression(identifier);

			release_line(line);
			break;
		}
		else {
			DBG_panic(("unexpected identifier expression:%s\n",
						line->argv[0]));
		}
	}

	return expr;
}


static Expression* load_assign_expression(FILE *fpin)
{
	LineElement *line;
	Expression *expr = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "ASSIGN_EXPRESSION")==0) {
			char *identifier = crb_create_identifier(line->argv[1]);
			Expression *right_expr;
			
			release_line(line);
			right_expr = load_expression(fpin);
			expr = crb_create_assign_expression(identifier, right_expr);
			break;
		}
		else {
			DBG_panic(("unexpected assign expression:%s\n", line->argv[0]));

		}

	}
	return expr;
}


static Expression* load_binary_expression(FILE *fpin)
{
	LineElement *line;
	Expression *expr = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}
		
		ExpressionType type;

		if (strcmp(line->argv[0], "ADD_EXPRESSION")==0)
			type = ADD_EXPRESSION;
		else if (strcmp(line->argv[0], "SUB_EXPRESSION")==0)
			type = SUB_EXPRESSION;
		else if (strcmp(line->argv[0], "MUL_EXPRESSION")==0)
			type = MUL_EXPRESSION;
		else if (strcmp(line->argv[0], "DIV_EXPRESSION")==0)
			type = DIV_EXPRESSION;
		else if (strcmp(line->argv[0], "MOD_EXPRESSION")==0)
			type = MOD_EXPRESSION;
		else if (strcmp(line->argv[0], "EQ_EXPRESSION")==0)
			type = EQ_EXPRESSION;
		else if (strcmp(line->argv[0], "NE_EXPRESSION")==0)
			type = NE_EXPRESSION;
		else if (strcmp(line->argv[0], "GT_EXPRESSION")==0)
			type = GT_EXPRESSION;
		else if (strcmp(line->argv[0], "GE_EXPRESSION")==0)
			type = GE_EXPRESSION;
		else if (strcmp(line->argv[0], "LT_EXPRESSION")==0)
			type = LT_EXPRESSION;
		else if (strcmp(line->argv[0], "LE_EXPRESSION")==0)
			type = LE_EXPRESSION;
		else
			DBG_panic(("unexpected binary expression:%s\n", line->argv[0]));

		release_line(line);

		Expression *left = load_expression(fpin);
		Expression *right = load_expression(fpin);

		expr = crb_create_binary_expression(type, left, right);

		break;
	}
	return expr;
}



static Expression* load_logical_and_or_expression(FILE *fpin)
{
	LineElement *line;
	Expression *expr = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		ExpressionType type;

		if (strcmp(line->argv[0], "LOGICAL_AND_EXPRESSION")==0) {
			type = LOGICAL_AND_EXPRESSION;
		}
		else if (strcmp(line->argv[0], "LOGICAL_OR_EXPRESSION")==0) {
			type = LOGICAL_OR_EXPRESSION;
		}
		else {
			DBG_panic(("unexpected logical and/or expression:%s\n",
						line->argv[0]));

		}

		release_line(line);

		Expression *left = load_expression(fpin);
		Expression *right = load_expression(fpin);

		expr = crb_create_binary_expression(type, left, right);

		break;
	}
	return expr;
}


static Expression* load_minus_expression(FILE *fpin)
{
	LineElement *line;
	Expression *expr = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "MINUS_EXPRESSION")==0) {
			release_line(line);
			Expression *sub_expr = load_expression(fpin);
			expr = crb_create_minus_expression(sub_expr);
			break;
		}
		else {
			DBG_panic(("unexpected minus expression:%s\n",
						line->argv[0]));
		}

	}
	return expr;
}


static Expression* load_function_call_expression(FILE *fpin)
{
	LineElement *line;
	Expression *expr = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "FUNCTION_CALL_EXPRESSION")==0) {
			char *identifier = crb_create_identifier(line->argv[1]);
			int argument_num;
			sscanf(line->argv[2], "%d", &argument_num);
			
			release_line(line);

			ArgumentList *list = NULL;
			int i;

			for (i=0; i<argument_num; i++) {
				Expression *arg_expr = NULL;

				while (1) {
					line = get_next_line(fpin);
					if (!line->str) {
						release_line(line); break;
					}

					if (line->argc == 0) {
						release_line(line); continue;
					}

					if (strcmp(line->argv[0], "ARGUMENT")==0) {
						release_line(line);
						arg_expr = load_expression(fpin);
						break;
					}
					else {
						DBG_panic(("unexpected ARGUMENT:%s\n", line->argv[0]));
					}

				}

				if (list == NULL)
					list = crb_create_argument_list(arg_expr);
				else
					list = crb_chain_argument_list(list, arg_expr);
			}

			expr = crb_create_function_call_expression(identifier, list);
			break;
		}
		else {
			DBG_panic(("unexpected function call expression:%s\n",
						line->argv[0]));
		}
	}

	return expr;
}


static Expression* load_null_expression(FILE *fpin)
{
	LineElement *line;
	Expression *expr = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}
		
		if (strcmp(line->argv[0], "NULL_EXPRESSION")==0) {
			release_line(line);
			expr = crb_create_null_expression();
			break;
		}
		else
			DBG_panic(("unexpected null expression:%s\n", line->argv[0]));

	}

	return expr;
}



static Expression* load_expression(FILE *fpin)
{
	LineElement *line;
	Expression *expr = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "BOOLEAN_EXPRESSION")==0) {
			unget_line(line);
			expr = load_boolean_expression(fpin);
		}
		else if (strcmp(line->argv[0], "INT_EXPRESSION")==0) {
			unget_line(line);
			expr = load_int_expression(fpin);
		}
		else if (strcmp(line->argv[0], "DOUBLE_EXPRESSION")==0) {
			unget_line(line);
			expr = load_double_expression(fpin);
		}
		else if (strcmp(line->argv[0], "STRING_EXPRESSION")==0) {
			unget_line(line);
			expr = load_string_expression(fpin);
		}
		else if (strcmp(line->argv[0], "IDENTIFIER_EXPRESSION")==0) {
			unget_line(line);
			expr = load_identifier_expression(fpin);
		}
		else if (strcmp(line->argv[0], "ASSIGN_EXPRESSION")==0) {
			unget_line(line);
			expr = load_assign_expression(fpin);
		}
		else if (strcmp(line->argv[0], "ADD_EXPRESSION")==0 ||
				strcmp(line->argv[0], "SUB_EXPRESSION")==0  ||
				strcmp(line->argv[0], "MUL_EXPRESSION")==0  ||
				strcmp(line->argv[0], "DIV_EXPRESSION")==0  ||
				strcmp(line->argv[0], "MOD_EXPRESSION")==0  ||
				strcmp(line->argv[0], "EQ_EXPRESSION")==0   ||
				strcmp(line->argv[0], "NE_EXPRESSION")==0   ||
				strcmp(line->argv[0], "GT_EXPRESSION")==0   ||
				strcmp(line->argv[0], "GE_EXPRESSION")==0   ||
				strcmp(line->argv[0], "LT_EXPRESSION")==0   ||
				strcmp(line->argv[0], "LE_EXPRESSION")==0) {
			unget_line(line);
			expr = load_binary_expression(fpin);
		}
		else if (strcmp(line->argv[0], "LOGICAL_AND_EXPRESSION")==0 ||
				strcmp(line->argv[0], "LOGICAL_OR_EXPRESSION")==0) {
			unget_line(line);
			expr = load_logical_and_or_expression(fpin);
		}
		else if (strcmp(line->argv[0], "MINUS_EXPRESSION")==0) {
			unget_line(line);
			expr = load_minus_expression(fpin);
		}
		else if (strcmp(line->argv[0], "FUNCTION_CALL_EXPRESSION")==0) {
			unget_line(line);
			expr = load_function_call_expression(fpin);
		}
		else if (strcmp(line->argv[0], "NULL_EXPRESSION")==0) {
			unget_line(line);
			expr = load_null_expression(fpin);
		}
		else {
			DBG_panic(("unexpected expression: %s\n", line->argv[0]));
		}

		break;

	}
	return expr;
}

static Statement* load_expression_statement(FILE *fpin)
{
	LineElement *line;	
	Statement *stat = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "EXPRESSION_STATEMENT")==0) {
			release_line(line);
			Expression *expr = load_expression(fpin);
			stat = crb_create_expression_statement(expr);
			break;
		}
		else {
			DBG_panic(("unexpected expression_statement: %s\n", 
						line->argv[0]));
		}

	}
	
	return stat;
}


static Statement* load_global_statement(FILE *fpin)
{
	LineElement *line;
	Statement *stat = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "GLOBAL_STATEMENT")==0) {
			IdentifierList *list = NULL;
			int i;

			for (i=1; i<line->argc; i++) {
				char *identifier = crb_create_identifier(line->argv[i]);
				if (list == NULL)
					list = crb_create_global_identifier(identifier);
				else
					list = crb_chain_identifier(list, identifier);
			}

			release_line(line);
			stat = crb_create_global_statement(list);
			break;
		}
		else {
			DBG_panic(("unexpected global statement:%s\n", line->argv[0]));
		}

	}

	return stat;
}


static Statement* load_if_statement(FILE *fpin)
{
	LineElement *line;
	Statement *stat = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}
		if (strcmp(line->argv[0], "IF_STATEMENT")==0) {
			int have_next = 0;
			if (strcmp(line->argv[1], "HAVE_NEXT")==0)
				have_next = 1;
			
			release_line(line);
			Expression *if_cond = load_expression(fpin);
			Statement *then_stat = load_statement(fpin);
			Elsif *elsif_list = NULL;
			Statement *else_stat = NULL;

			while (have_next) {
				line = get_next_line(fpin);
				if (!line->str) {
					release_line(line); break;
				}

				if (line->argc == 0) {
					release_line(line); continue;
				}

				if (strcmp(line->argv[0], "ELSIF")==0) {
					have_next = 0;
					if (strcmp(line->argv[1], "HAVE_NEXT")==0)
						have_next = 1;
					release_line(line);
					Expression *elsif_cond = load_expression(fpin);
					Statement *elsif_stat = load_statement(fpin);
					
					Elsif *elsif_node = crb_create_elsif(elsif_cond,
														elsif_stat);
					if (elsif_list == NULL)
						elsif_list = elsif_node;
					else
						elsif_list = crb_chain_elsif_list(elsif_list,
														elsif_node);

				}
				else if (strcmp(line->argv[0], "ELSE")==0) {
					release_line(line);
					have_next = 0;
					else_stat = load_statement(fpin);

				}
				else {
					DBG_panic(("unexpected elsif/else: %s\n", line->argv[0]));

				}

			}

			stat = crb_create_if_statement(if_cond, then_stat, elsif_list, 
												else_stat);

			break;
		}
		else {
			DBG_panic(("unexpected if statement: %s\n", line->argv[0]));
		}

	}

	return stat;
}



static Statement* load_while_statement(FILE *fpin)
{
	LineElement *line;
	Statement *stat = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "WHILE_STATEMENT")==0) {
			release_line(line);
			Expression *while_cond = load_expression(fpin);
			Statement *while_stat = load_statement(fpin);

			stat = crb_create_while_statement(while_cond, while_stat);
			break;
		}
		else {
			DBG_panic(("unexpected while statement:%s\n", line->argv[0]));
		}

	}
	return stat;
}



static Statement* load_for_statement(FILE *fpin)
{
	LineElement *line;
	Statement *stat = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "FOR_STATEMENT")==0) {
			release_line(line);

			Expression *init_expr = NULL;
			Expression *cond_expr = NULL;
			Expression *post_expr = NULL;
			Statement *for_stat = NULL;
			while (1) {
				line = get_next_line(fpin);
				if (!line->str) {
					release_line(line); break;
				}

				if (line->argc == 0) {
					release_line(line); continue;
				}

				if (strcmp(line->argv[0], "INIT")==0) {
					release_line(line);
					init_expr = load_expression(fpin);
				}
				else if (strcmp(line->argv[0], "CONDITION")==0) {
					release_line(line);
					cond_expr = load_expression(fpin);
				}
				else if (strcmp(line->argv[0], "POST")==0) {
					release_line(line);
					post_expr = load_expression(fpin);
				}
				else {
					unget_line(line);
					for_stat = load_statement(fpin);
					break;
				}
			}

			stat = crb_create_for_statement(init_expr, cond_expr, post_expr,
					for_stat);
			break;
		}
		else {
			DBG_panic(("unexpected for statement:%s\n", line->argv[0]));
		}

	}
	return stat;
}


static Statement* load_return_statement(FILE *fpin)
{
	LineElement *line;
	Statement *stat = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "RETURN_STATEMENT")==0) {
			int have_expr = 0;
			if (strcmp(line->argv[1], "HAVE_EXPR")==0)
				have_expr = 1;
			release_line(line);

			Expression *expr = NULL;
			if (have_expr) {
				expr = load_expression(fpin);
			}
			stat = crb_create_return_statement(expr);
			break;
		}
		else {
			DBG_panic(("unexpected return statement:%s\n", line->argv[0]));
		}

	}
	
	return stat;
}


static Statement* load_break_statement(FILE *fpin)
{
	LineElement *line;
	Statement *stat = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "BREAK_STATEMENT")==0) {
			release_line(line);
			stat = crb_create_break_statement();
			break;
		}
		else {
			DBG_panic(("unexpected break statement:%s\n", line->argv[0]));
		}
	}
	return stat;

}


static Statement* load_continue_statement(FILE *fpin)
{
	LineElement *line;
	Statement *stat = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}
		if (strcmp(line->argv[0], "CONTINUE_STATEMENT")==0) {
			release_line(line);
			stat = crb_create_continue_statement();
			break;
		}
		else {
			DBG_panic(("unexpected continue statement:%s\n", line->argv[0]));
		}
	}
	return stat;
}


Statement* load_block_statement(FILE *fpin)
{
	LineElement *line;
	Statement *stat = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}
		if (strcmp(line->argv[0], "BLOCK_STATEMENT")==0) {
			release_line(line);
			Block *block;
			block = load_block(fpin);
			stat = crb_create_block_statement(block);
			break;
		}
		else {
			DBG_panic(("unexpected block statement:%s\n", line->argv[0]));
		}
	}
	return stat;
}


static Statement* load_statement(FILE *fpin)
{
	LineElement *line;
	Statement *stat = NULL;

	while (1) {
		line = get_next_line(fpin);
		
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "EXPRESSION_STATEMENT")==0) {
			unget_line(line);
			stat = load_expression_statement(fpin);
		}
		else if (strcmp(line->argv[0], "GLOBAL_STATEMENT")==0) {
			unget_line(line);
			stat = load_global_statement(fpin);
		}
		else if (strcmp(line->argv[0], "IF_STATEMENT")==0) {
			unget_line(line);
			stat = load_if_statement(fpin);
		}
		else if (strcmp(line->argv[0], "WHILE_STATEMENT")==0) {
			unget_line(line);
			stat = load_while_statement(fpin);
		}
		else if (strcmp(line->argv[0], "FOR_STATEMENT")==0) {
			unget_line(line);
			stat = load_for_statement(fpin);
		}
		else if (strcmp(line->argv[0], "RETURN_STATEMENT")==0) {
			unget_line(line);
			stat = load_return_statement(fpin);
		}
		else if (strcmp(line->argv[0], "BREAK_STATEMENT")==0) {
			unget_line(line);
			stat = load_break_statement(fpin);
		}
		else if (strcmp(line->argv[0], "CONTINUE_STATEMENT")==0) {
			unget_line(line);
			stat = load_continue_statement(fpin);
		}
		else if (strcmp(line->argv[0], "BLOCK_STATEMENT")==0) {
			unget_line(line);
			stat = load_block_statement(fpin);
		}
		else {
			DBG_panic(("unexpected statement: %s\n", line->argv[0]));
		}

		break;
	}
	return stat;
}


void CRB_load_interpreter(CRB_Interpreter *interpreter, FILE *fpin)
{

	LineElement *line;
	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}


		if (strcmp(line->argv[0], "FUNCTION")==0) {
			unget_line(line);
			load_function(fpin);
		}
		else if (str_find(line->argv[0], "STATEMENT")) {
			unget_line(line);
			Statement *stat = load_statement(fpin);
			if (interpreter->statement_list == NULL)
				interpreter->statement_list = 
					crb_create_statement_list(stat);
			else
				interpreter->statement_list =
					crb_chain_statement_list(interpreter->statement_list,
												stat);
		}
		else {
			DBG_panic(("unexpected dump: %s\n", line->argv[0]));
		}

	}


}




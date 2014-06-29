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
	int in_string = 0, in_regexp = 0;
	char regexp_protect_char = 0;

	for (i=0; i<max_arg_num-1; ) {
		while (isspace(*p)) p++;
		if (*p=='\0') break;
		args[i] = p;
		while ((!isspace(*p) || in_string || in_regexp) && *p!='\0') {
			if (*p == '\"') {
				if (p==line || *(p-1)!='\\')
					in_string = !in_string;
			}

			if (!in_string && !in_regexp && p!=line && p!=(line+1) 
					&& (*(p-2)=='%') && (*(p-1)=='r')) {
				in_regexp = 1;
				regexp_protect_char = *p;
			}
			else if (in_regexp && regexp_protect_char == *p) {
				in_regexp = 0;
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
		if (line->str && line->str[0]!='#')
			line->argc = line_to_arguments(line->str, line->argv,
										MAX_LINE_ELEMENT_NUM);
		else {
			line->argc = 0;
			if (line->str) {	
				int line_number;
				if (sscanf(line->str, "#line: %d", &line_number)==1) {
					crb_get_current_interpreter()->current_line_number = 
														line_number;
				}
				else if (strncmp(line->str, "#file:", 
									strlen("#file:"))==0) {
					char name_buf[512];
					char *p = line->str + strlen("#file:");
					int i = 0;
					while (*p != '\"') p++;
					p++;
					while (*p != '\"') {
						name_buf[i++] = *p++;
					}
					name_buf[i] = '\0';
					crb_get_current_interpreter()->current_file_name = 
								crb_create_identifier(name_buf);
				}
			}

		}

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
			char *mb_str = line->argv[1]+1;
			int mb_len = strlen(mb_str);
			mb_str[mb_len-1] = '\0';  // omit "" in "abc"
			
			int wc_len = CRB_mbstowcs_len(mb_str);
			DBG_assert(wc_len >= 0, ("wc_len..%d\n", wc_len));
			CRB_CHAR *str = (CRB_CHAR*)crb_malloc(
					sizeof(CRB_CHAR)*(wc_len+1));
			CRB_mbstowcs(str, mb_str);

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

static Expression* load_regexp_expression(FILE *fpin)
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

		if (strcmp(line->argv[0], "REGEXP_EXPRESSION")==0) {
			expr = crb_alloc_expression(REGEXP_EXPRESSION);

			char protect_char = line->argv[1][2];  // %r!
			char *mb_str = line->argv[1]+3;
			int mb_len = strlen(mb_str);
			mb_str[mb_len-1] = '\0';  // omit ! in %r!abc!
			
			int wc_len = CRB_mbstowcs_len(mb_str);
			DBG_assert(wc_len >= 0, ("wc_len..%d\n", wc_len));
			CRB_CHAR *str = (CRB_CHAR*)crb_malloc(
					sizeof(CRB_CHAR)*(wc_len+1));
			CRB_mbstowcs(str, mb_str);

			expr->u.regexp_value = crb_create_regexp_in_compile(str, 
										protect_char);

			release_line(line);
			break;
		}
		else {
			DBG_panic(("unexpected regexp expression:%s\n", line->argv[0]));

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
			AssignType type;
			char *type_str;
			Expression *left_expr;
			Expression *right_expr;
			
			DBG_assert(line->argc == 2, ("ASSIGN_EXPRESSION without assign_type"));
			type_str = line->argv[1];
			if (!strcmp(type_str, "ASSIGN"))
				type = ASSIGN_TYPE;
			else if (!strcmp(type_str, "ADD_ASSIGN"))
				type = ADD_ASSIGN_TYPE;
			else if (!strcmp(type_str, "SUB_ASSIGN"))
				type = SUB_ASSIGN_TYPE;
			else if (!strcmp(type_str, "MUL_ASSIGN"))
				type = MUL_ASSIGN_TYPE;
			else if (!strcmp(type_str, "DIV_ASSIGN"))
				type = DIV_ASSIGN_TYPE;
			else if (!strcmp(type_str, "MOD_ASSIGN"))
				type = MOD_ASSIGN_TYPE;
			else
				DBG_panic(("unexpected assign_type: %s\n", type_str));

			release_line(line);
			left_expr = load_expression(fpin);
			right_expr = load_expression(fpin);
			expr = crb_create_assign_expression(type, left_expr, 
												right_expr);
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


static Expression* load_not_expression(FILE *fpin)
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


		if (strcmp(line->argv[0], "NOT_EXPRESSION")==0) {

			release_line(line);

			Expression *sub_expr = load_expression(fpin);

			expr = crb_create_not_expression(sub_expr);

			break;
		}
		else {
			DBG_panic(("unexpected not_expression: %s\n", line->argv[0]));
		}
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
			int argument_num;
			sscanf(line->argv[1], "%d", &argument_num);
			
			release_line(line);

			Expression *name_expr = NULL;
			while (1) {
				line = get_next_line(fpin);
				if (!line->str) {
					release_line(line); break;
				}
				if (line->argc == 0) {
					release_line(line); continue;
				}
				if (strcmp(line->argv[0], "FUNCTION_NAME")==0) {
					release_line(line);
					name_expr = load_expression(fpin);
					break;
				}
				else {
					DBG_panic(("unexpected function_name:%s\n",
								line->argv[0]));
				}
			}

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

			expr = crb_create_function_call_expression(name_expr, list);
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




Expression* load_array_expression(FILE *fpin)
{
	LineElement *line;
	Expression *expr = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}
		if (line->argc==0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "ARRAY_EXPRESSION")==0) {
			DBG_assert(line->argc==2, 
					("unexpected array_expression with argc..%d\n",
					 line->argc));

			int expr_num;
			sscanf(line->argv[1], "%d", &expr_num);
			
			release_line(line);
			int i;
			ExpressionList *list = NULL;

			for (i=0; i<expr_num; i++) {
				Expression *sub_expr = load_expression(fpin);
				if (list == NULL)
					list = crb_create_expression_list(sub_expr);
				else
					list = crb_chain_expression_list(list, sub_expr);
			}

			expr = crb_create_array_expression(list);
			break;
		}
		else {
			DBG_panic(("unexpected array_expression:%s\n",
						line->argv[0]));
		}

	}

	return expr;
}


static Expression* load_index_expression(FILE *fpin)
{
	LineElement *line;
	Expression *expr = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}
		if (line->argc==0) {
			release_line(line); continue;
		}

		if (strcmp(line->argv[0], "INDEX_EXPRESSION")==0) {
			release_line(line);

			Expression *array_expr = load_expression(fpin);
			Expression *index_expr = load_expression(fpin);

			expr = crb_create_index_expression(array_expr, index_expr);
			break;

		}
		else {
			DBG_panic(("unexpected index_expression:%s\n",
						line->argv[0]));
		}

	}

	return expr;
}


Expression* load_incdec_expression(FILE *fpin)
{
	LineElement *line;
	Expression *expr = NULL;

	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}
		if (line->argc==0) {
			release_line(line); continue;
		}

		ExpressionType type;
		if (strcmp(line->argv[0], "PREV_INCREMENT_EXPRESSION")==0)
			type = PREV_INCREMENT_EXPRESSION;
		else if (strcmp(line->argv[0], "POST_INCREMENT_EXPRESSION")==0)
			type = POST_INCREMENT_EXPRESSION;
		else if (strcmp(line->argv[0], "PREV_DECREMENT_EXPRESSION")==0)
			type = PREV_DECREMENT_EXPRESSION;
		else if (strcmp(line->argv[0], "POST_DECREMENT_EXPRESSION")==0)
			type = POST_DECREMENT_EXPRESSION;
		else 
			DBG_panic(("unexpected incdec_expression:%s\n",
						line->argv[0]));

		release_line(line);
		Expression *sub_expr = load_expression(fpin);

		expr = crb_create_incdec_expression(type, sub_expr);
		break;

	}
	return expr;
}

Expression* load_member_expression(FILE *fpin)
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

		if (strcmp(line->argv[0], "MEMBER_EXPRESSION")==0) {
			DBG_assert(line->argc==2, ("unexpected MEMBER_EXPRESSION"));
			
			char *name = crb_create_identifier(line->argv[1]);	
			release_line(line);
			Expression *assoc_expr = load_expression(fpin);

			expr = crb_create_member_expression(assoc_expr, name);
			break;
		}
		else {
			DBG_panic(("unexpected member_expression:%s\n",
						line->argv[0]));
		}
	}
	return expr;
}

static Expression* load_closure_definition(FILE *fpin)
{
	Expression *expr = NULL;
	LineElement *line;
	while (1) {
		line = get_next_line(fpin);
		if (!line->str) {
			release_line(line); break;
		}

		if (line->argc == 0) {
			release_line(line); continue;
		}


		if (strcmp(line->argv[0], "CLOSURE_DEFINITION")==0) {

			char *identifier = NULL;

			if (line->argc == 2) {
				identifier = crb_create_identifier(line->argv[1]);
			}

			release_line(line);

			ParameterList *parameter_list = load_parameter_list(fpin);
			Block *block = load_block(fpin);


			expr = crb_create_closure_definition(identifier, 
								parameter_list, block);
			break;
		}
		else {
			DBG_panic(("unexpected closure_definition:%s\n", line->argv[0]));
		}
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
		else if (strcmp(line->argv[0], "REGEXP_EXPRESSION")==0) {
			unget_line(line);
			expr = load_regexp_expression(fpin);
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
		else if (strcmp(line->argv[0], "NOT_EXPRESSION")==0) {
			unget_line(line);
			expr = load_not_expression(fpin);
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
		else if (strcmp(line->argv[0], "ARRAY_EXPRESSION")==0) {
			unget_line(line);
			expr = load_array_expression(fpin);
		}
		else if (strcmp(line->argv[0], "INDEX_EXPRESSION")==0) {
			unget_line(line);
			expr = load_index_expression(fpin);
		}
		else if (strcmp(line->argv[0], "PREV_INCREMENT_EXPRESSION")==0
				|| strcmp(line->argv[0], "POST_INCREMENT_EXPRESSION")==0
				|| strcmp(line->argv[0], "PREV_DECREMENT_EXPRESSION")==0
				|| strcmp(line->argv[0], "POST_DECREMENT_EXPRESSION")==0) {
			unget_line(line);
			expr = load_incdec_expression(fpin);
		}
		else if (strcmp(line->argv[0], "MEMBER_EXPRESSION")==0) {
			unget_line(line);
			expr = load_member_expression(fpin);
		}
		else if (strcmp(line->argv[0], "CLOSURE_DEFINITION")==0) {
			unget_line(line);
			expr = load_closure_definition(fpin);
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

Statement* load_try_statement(FILE *fpin)
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
		if (strcmp(line->argv[0], "TRY_STATEMENT")==0) {
			int i;
			CRB_Boolean with_catch = CRB_FALSE;
			CRB_Boolean with_finally = CRB_FALSE;
			char *identifier = NULL;
			Statement *run_st = NULL;
			Statement *catch_st = NULL;
			Statement *finally_st = NULL;
			for (i=1; i<line->argc; i++) {
				if (strcmp(line->argv[i], "WITH_CATCH")==0)
					with_catch = CRB_TRUE;
				else if (strcmp(line->argv[i], "WITH_FINALLY")==0)
					with_finally = CRB_TRUE;
				else
					identifier = crb_create_identifier(line->argv[i]);
			}

			release_line(line);
			run_st = load_statement(fpin);
			if (with_catch)
				catch_st = load_statement(fpin);
			if (with_finally)
				finally_st = load_statement(fpin);

			stat = crb_create_try_statement(run_st, identifier, 
								catch_st, finally_st);
			break;
		}
		else {
			DBG_panic(("unexpected try statement:%s\n", line->argv[0]));
		}
	}
	return stat;
}


Statement* load_throw_statement(FILE *fpin)
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
		if (strcmp(line->argv[0], "THROW_STATEMENT")==0) {
			release_line(line);

			Expression *throw_expr = load_expression(fpin);

			stat = crb_create_throw_statement(throw_expr);
			break;
		}
		else {
			DBG_panic(("unexpected throw statement:%s\n", line->argv[0]));
		}
	}
	return stat;
}


Statement* load_foreach_statement(FILE *fpin)
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
		if (strcmp(line->argv[0], "FOREACH_STATEMENT")==0) {

			DBG_assert(line->argc == 2, (""));

			char *identifier = crb_create_identifier(line->argv[1]);

			release_line(line);

			Expression *array_expr = load_expression(fpin);
			Statement *sub_st = load_statement(fpin);

			stat = crb_create_foreach_statement(identifier, array_expr,
												sub_st);
			break;
		}
		else {
			DBG_panic(("unexpected foreach statement:%s\n", line->argv[0]));
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
		else if (strcmp(line->argv[0], "TRY_STATEMENT")==0) {
			unget_line(line);
			stat = load_try_statement(fpin);
		}
		else if (strcmp(line->argv[0], "THROW_STATEMENT")==0) {
			unget_line(line);
			stat = load_throw_statement(fpin);
		}
		else if (strcmp(line->argv[0], "FOREACH_STATEMENT")==0) {
			unget_line(line);
			stat = load_foreach_statement(fpin);
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




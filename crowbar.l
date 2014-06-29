%{
#include <stdio.h>
#include <string.h>
#include "DBG.h"
#include "crowbar.h"
#include "y.tab.h"

struct bufstack {
	struct bufstack *prev;
	YY_BUFFER_STATE bs;
	int lineno;
	char *filename;
	FILE *fp;
} *curbs = NULL;
	

int
yywrap(void)
{
    return 1;
}

static void set_current_filename(char *filename)
{
	crb_get_current_interpreter()->current_file_name = filename;	

}

static char* get_current_filename()
{
	return crb_get_current_interpreter()->current_file_name;
}

static void increment_line_number(void)
{
    crb_get_current_interpreter()->current_line_number++;
}

static int get_line_number(void)
{
	return crb_get_current_interpreter()->current_line_number;
}

static void set_line_number(int line_number)
{
	crb_get_current_interpreter()->current_line_number = line_number;
}

static void set_compile_state(int state)
{
	crb_get_current_interpreter()->compile_state = state;
}

%}
%start COMMENT STRING_LITERAL_STATE GBK_2ND_CHAR IFILE_STATE REGEXP_LITERAL_STATE

%%
<INITIAL>"function"     return FUNCTION;
<INITIAL>"if"           return IF;
<INITIAL>"else"         return ELSE;
<INITIAL>"elsif"        return ELSIF;
<INITIAL>"while"        return WHILE;
<INITIAL>"for"          return FOR;
<INITIAL>"return"       return RETURN_T;
<INITIAL>"break"        return BREAK;
<INITIAL>"continue"     return CONTINUE;
<INITIAL>"null"         return NULL_T;
<INITIAL>"true"         return TRUE_T;
<INITIAL>"false"        return FALSE_T;
<INITIAL>"foreach"		return FOREACH;
<INITIAL>"global"       return GLOBAL_T;
<INITIAL>"closure"		return CLOSURE;
<INITIAL>"try"			return TRY;
<INITIAL>"catch"		return CATCH;
<INITIAL>"finally"		return FINALLY;
<INITIAL>"throw"		return THROW;
<INITIAL>"("            return LP;
<INITIAL>")"            return RP;
<INITIAL>"{"            return LC;
<INITIAL>"}"            return RC;
<INITIAL>";"            return SEMICOLON;
<INITIAL>","            return COMMA;
<INITIAL>":"			return COLON;
<INITIAL>"&&"           return LOGICAL_AND;
<INITIAL>"||"           return LOGICAL_OR;
<INITIAL>"="            return ASSIGN;
<INITIAL>"=="           return EQ;
<INITIAL>"!="           return NE;
<INITIAL>">"            return GT;
<INITIAL>">="           return GE;
<INITIAL>"<"            return LT;
<INITIAL>"<="           return LE;
<INITIAL>"+"            return ADD;
<INITIAL>"-"            return SUB;
<INITIAL>"*"            return MUL;
<INITIAL>"/"            return DIV;
<INITIAL>"%"            return MOD;
<INITIAL>"["			return LB;
<INITIAL>"]"			return RB;
<INITIAL>"."			return DOT;
<INITIAL>"++"			return INCREMENT;
<INITIAL>"--"			return DECREMENT;
<INITIAL>[A-Za-z_][A-Za-z_0-9]* {
    yylval.identifier = crb_create_identifier(yytext);
    return IDENTIFIER;
}
<INITIAL>([1-9][0-9]*)|"0" {
    Expression  *expression = crb_alloc_expression(INT_EXPRESSION);
    sscanf(yytext, "%d", &expression->u.int_value);
    yylval.expression = expression;
    return INT_LITERAL;
}
<INITIAL>[0-9]+\.[0-9]+ {
    Expression  *expression = crb_alloc_expression(DOUBLE_EXPRESSION);
    sscanf(yytext, "%lf", &expression->u.double_value);
    yylval.expression = expression;
    return DOUBLE_LITERAL;
}
<INITIAL>\" {
    crb_open_string_literal();
    BEGIN STRING_LITERAL_STATE;
}
<INITIAL>[ \t\r] ;
<INITIAL>\n {increment_line_number();}
<INITIAL>#     BEGIN COMMENT;

<INITIAL><<EOF>> 			{
								if (crb_pop_parse_file())
									yyterminate();
							}
<INITIAL>^[\t ]*import[ \t\"]+   {  BEGIN IFILE_STATE; }
<INITIAL>%r.                {
								crb_set_regexp_start_char(yytext[2]);
								crb_open_string_literal();
								BEGIN REGEXP_LITERAL_STATE;
							}

<INITIAL>.      {
    char buf[LINE_BUF_SIZE];

    if (isprint(yytext[0])) {
        buf[0] = yytext[0];
        buf[1] = '\0';
    } else {
        sprintf(buf, "0x%02x", (unsigned char)yytext[0]);
    }

    crb_compile_error(CHARACTER_INVALID_ERR,
                      STRING_MESSAGE_ARGUMENT, "bad_char", buf,
                      MESSAGE_ARGUMENT_END);
}

<IFILE_STATE>[^ \t\"\n]+		{
									int c;
									while ((c=input()) &&  c!='\n')
										;
									increment_line_number();
									if (crb_push_parse_file(yytext)) {
										set_compile_state(1);
										yyterminate();
									}
									BEGIN INITIAL;
								}

<IFILE_STATE>.|\n				{
									fprintf(stderr, 
											"%s:%4d bad import line\n",
											get_current_filename(),
											get_line_number());	
									yyterminate();
									set_compile_state(1);
								}

<REGEXP_LITERAL_STATE>\n        {
									crb_compile_error(CR_IN_REGEXP_ERR,
											MESSAGE_ARGUMENT_END);
								}
<REGEXP_LITERAL_STATE>.			{
									if (yytext[0] != crb_regexp_start_char()) {
										crb_add_string_literal(yytext[0]);
									}
									else {
										Expression *expression = crb_alloc_expression(REGEXP_EXPRESSION);
										CRB_CHAR *string_literal = crb_close_string_literal();
										expression->u.regexp_value = crb_create_regexp_in_compile(string_literal, crb_regexp_start_char());
										yylval.expression = expression;
										BEGIN INITIAL;
										return REGEXP_LITERAL;
									}
								}


<COMMENT>\n     {
    increment_line_number();
    BEGIN INITIAL;
}
<COMMENT>.      ;
<STRING_LITERAL_STATE>\"        {
    Expression *expression = crb_alloc_expression(STRING_EXPRESSION);
    expression->u.string_value = crb_close_string_literal();
    yylval.expression = expression;
    BEGIN INITIAL;
    return STRING_LITERAL;
}
<STRING_LITERAL_STATE>\n        {
    crb_add_string_literal('\n');
    increment_line_number();
}
<STRING_LITERAL_STATE>\\\"      crb_add_string_literal('"');
<STRING_LITERAL_STATE>\\n       crb_add_string_literal('\n');
<STRING_LITERAL_STATE>\\t       crb_add_string_literal('\t');
<STRING_LITERAL_STATE>\\\\      crb_add_string_literal('\\');
<STRING_LITERAL_STATE>.         {
		crb_add_string_literal(yytext[0]);
		Encoding enc = crb_get_current_interpreter()->source_encoding;
		if (enc == GBK_ENCODING && yytext[0] >= 0x80)
			BEGIN GBK_2ND_CHAR;

}

<GBK_2ND_CHAR>.                 {
		crb_add_string_literal(yytext[0]);
		BEGIN STRING_LITERAL_STATE;

}

%%


int check_repeat_import(char *filename)
{
	struct bufstack *bs;
	char *cur_filename = get_current_filename();
	if (cur_filename == NULL) return 0;

	if (strcmp(cur_filename, filename)==0)
		return 1;

	if (curbs==NULL) return 0;

	for (bs=curbs->prev; bs!=NULL; bs=bs->prev) {
		if (strcmp(bs->filename, filename)==0)
			return 1;
	}
	return 0;
}


int crb_push_parse_file(char *filename)
{
	if (check_repeat_import(filename)) {
		fprintf(stderr, "warning: recursive import file %s\n", filename);
		return 1;
	}


	FILE *fp = fopen(filename, "r");
	if (fp==NULL) {
		perror(filename); return 1;
	}

	struct bufstack *bs = crb_malloc(sizeof(struct bufstack));
	if (bs==NULL) {
		perror("crb_malloc"); return 1;
	}



	if (curbs) {
		curbs->lineno = get_line_number();
		curbs->filename = get_current_filename();
	}
	bs->prev = curbs;

	bs->bs = yy_create_buffer(fp, YY_BUF_SIZE);
	bs->fp = fp;

	curbs = bs;

	yy_switch_to_buffer(curbs->bs);

	set_current_filename(crb_create_identifier(filename));
	set_line_number(1);

	return 0;
}



int crb_pop_parse_file()
{
	struct bufstack *bs = curbs;
	struct bufstack *prevbs;

	DBG_assert(bs!=NULL, ("crb_pop_parse_file error"));

	fclose(bs->fp);
	yy_delete_buffer(bs->bs);

	curbs = bs->prev;

	if (!curbs) return 1;
	yy_switch_to_buffer(curbs->bs);
	set_current_filename(curbs->filename);
	set_line_number(curbs->lineno);
	return 0;
}



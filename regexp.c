#include <limits.h>
#include <oniguruma.h>
#include "DBG.h"
#include "MEM.h"
#include "crowbar.h"

static UChar* encode_utf16_be(CRB_CHAR *src)
{
	UChar *dest = NULL;
	int dest_size;
	int src_idx, dest_idx;

	dest_size = CRB_wcslen(src)*2+2;
	dest = MEM_malloc(dest_size);

	for (src_idx = dest_idx = 0; ; src_idx++) {
		if (dest_idx + 1 >= dest_size) {
			MEM_free(dest);
			return NULL;
		}
		dest[dest_idx] = (src[src_idx]>>8) & 0xff;
		dest[dest_idx+1] = src[src_idx] & 0xff;
		dest_idx += 2;
		if (src[src_idx] == L'\0')
			break;
	}

	return dest;

}

static CRB_Regexp* alloc_crb_regexp(regex_t *reg, CRB_Boolean is_literal)
{
	CRB_Regexp *crb_reg;

	crb_reg = MEM_malloc(sizeof(CRB_Regexp));
	crb_reg->is_literal = CRB_TRUE;
	crb_reg->regexp = reg;
	crb_reg->next = NULL;

	return crb_reg;
}


CRB_Regexp *crb_create_regexp_in_compile(CRB_CHAR *str, char protect_char)
{
	int r;
	regex_t *reg;
	OnigErrorInfo einfo;
	CRB_Regexp *regexp;
	CRB_Interpreter *inter;
	UChar *pattern;

	pattern = encode_utf16_be(str);
	if (pattern == NULL) {
		crb_compile_error(UNEXPECTED_WIDE_STRING_IN_COMPILE_ERR,
						MESSAGE_ARGUMENT_END);
	}

	r = onig_new(&reg, pattern, 
			pattern + onigenc_str_bytelen_null(ONIG_ENCODING_UTF16_BE, pattern),
			ONIG_OPTION_NONE, ONIG_ENCODING_UTF16_BE, ONIG_SYNTAX_PERL, 
			&einfo);
	if (r != ONIG_NORMAL) {
		char s[ONIG_MAX_ERROR_MESSAGE_LEN];
		onig_error_code_to_str(s, r, &einfo);
		crb_compile_error(CAN_NOT_CREATE_REGEXP_IN_COMPILE_ERR,
				STRING_MESSAGE_ARGUMENT, "message", s,
				MESSAGE_ARGUMENT_END);

	}
	
	regexp = alloc_crb_regexp(reg, CRB_TRUE);
	regexp->pattern = str;
	regexp->protect_char = protect_char;
	inter = crb_get_current_interpreter();


	regexp->next = inter->regexp_literals;
	inter->regexp_literals = regexp;

	MEM_free(pattern);


	return regexp;
}

void crb_dispose_regexp_literals(CRB_Interpreter *inter)
{
	CRB_Regexp *tmp;
	
	while (inter->regexp_literals) {
		tmp = inter->regexp_literals;
		inter->regexp_literals = inter->regexp_literals->next;
		onig_free(tmp->regexp);
		MEM_free(tmp);
	}
}

static CRB_NativePointerInfo st_regexp_type_info = {
	"crowbar.lang.regexp"
};

CRB_NativePointerInfo* crb_get_regexp_info(void)
{
	return &st_regexp_type_info;
}

static void check_argument_count(int arg_count, int true_low_count,
									int true_up_count,
									char *filename, int line_number)
{
	if (arg_count < true_low_count)
		crb_runtime_error(filename, line_number, 
						ARGUMENT_TOO_FEW_ERR, MESSAGE_ARGUMENT_END);
	else if (arg_count > true_up_count)
		crb_runtime_error(filename, line_number, 
						ARGUMENT_TOO_MANY_ERR, MESSAGE_ARGUMENT_END);
}


static CRB_Boolean search_sub(CRB_Interpreter *inter,
							CRB_LocalEnvironment *env,
							regex_t *regexp, 
							UChar *subject, UChar *end_p,
							UChar *at_p, UChar **next_at,
							OnigRegion *region_org)
{
	int r;
	OnigRegion *region = NULL;
	if (region_org == NULL && next_at != NULL)
		region = onig_region_new();
	else
		region = region_org;

	r = onig_search(regexp, subject, end_p, at_p, end_p,
					region, ONIG_OPTION_NONE);
	if (r < 0 && r != ONIG_MISMATCH) {
		char s[ONIG_MAX_ERROR_MESSAGE_LEN];
		onig_region_free(region, 1);
		MEM_free(subject);
		onig_error_code_to_str(s, r);
		crb_runtime_error(0, ONIG_SEARCH_FAIL_ERR,
						STRING_MESSAGE_ARGUMENT, "message",
						s, MESSAGE_ARGUMENT_END);
	}
	if (next_at != NULL) {
		*next_at = subject + region->end[0];
		if (region_org == NULL) {
			onig_region_free(region, 1);
		}
	}
	return r >= 0;
}

static CRB_Boolean match_sub(CRB_Interpreter *inter,
							CRB_LocalEnvironment *env,
							regex_t *regexp, 
							UChar *subject, UChar *end_p,
							OnigRegion *region_org)
{
	int r;
	OnigRegion *region = NULL;
	if (region_org == NULL)
		region = onig_region_new();
	else
		region = region_org;
/* we change the definition of reg_match here to match whole string */
	r = onig_match(regexp, subject, end_p, subject,
					region, ONIG_OPTION_NONE);
	if (r < 0 && r != ONIG_MISMATCH) {
		char s[ONIG_MAX_ERROR_MESSAGE_LEN];
		onig_region_free(region, 1);
		MEM_free(subject);
		onig_error_code_to_str(s, r);
		crb_runtime_error(0, ONIG_SEARCH_FAIL_ERR,
						STRING_MESSAGE_ARGUMENT, "message",
						s, MESSAGE_ARGUMENT_END);
	}

	if (region->end[0] != (end_p - subject))
		r = ONIG_MISMATCH;

	if (region_org == NULL) {
		onig_region_free(region, 1);
	}
	return r >= 0;
}


static void onig_region_to_crb_region(CRB_Interpreter *inter,
								CRB_LocalEnvironment *env,
								OnigRegion *onig_region,
								CRB_Object *crb_region,
								CRB_Object *crb_subject)
{
	int i;
	CRB_Value begin_val, end_val, string_val;
	CRB_Value tmp_val;

	begin_val.type = CRB_ARRAY_VALUE;
	begin_val.u.object_value = crb_create_array(inter, onig_region->num_regs);
	crb_set_assoc_variable(inter, crb_region, "begin", begin_val);

	end_val.type = CRB_ARRAY_VALUE;
	end_val.u.object_value = crb_create_array(inter, onig_region->num_regs);
	crb_set_assoc_variable(inter, crb_region, "end", end_val);

	string_val.type = CRB_ARRAY_VALUE;
	string_val.u.object_value = crb_create_array(inter, onig_region->num_regs);
	crb_set_assoc_variable(inter, crb_region, "string", string_val);

	for (i = 0; i < onig_region->num_regs; i++) {
		tmp_val.type = CRB_INT_VALUE;
		tmp_val.u.int_value = onig_region->beg[i]/2;
		crb_array_set(inter, begin_val.u.object_value, i, &tmp_val);
		
		tmp_val.type = CRB_INT_VALUE;
		tmp_val.u.int_value = onig_region->end[i]/2;
		crb_array_set(inter, end_val.u.object_value, i, &tmp_val);

		tmp_val.type = CRB_STRING_VALUE;
		tmp_val.u.object_value = crb_string_substr(inter, crb_subject,
									onig_region->beg[i]/2,
									(onig_region->end[i] - onig_region->beg[i])/2);
		crb_array_set(inter, string_val.u.object_value, i, &tmp_val);

	}


}


static CRB_Boolean search_crb_if(CRB_Interpreter *inter,
								CRB_LocalEnvironment *env,
								CRB_Regexp *crb_reg,
								CRB_Object *crb_subject,
								CRB_Object *crb_region)
{
	UChar *subject;
	UChar *end_p;
	OnigRegion *onig_region = NULL;
	CRB_Boolean matched;

	subject = encode_utf16_be(crb_subject->u.string.string);
	if (subject == NULL) {
		crb_runtime_error(0, UNEXPECTED_WIDE_STRING_ERR,
							MESSAGE_ARGUMENT_END);
	}
	if (crb_region) {
		onig_region = onig_region_new();
	}
	end_p = subject + onigenc_str_bytelen_null(ONIG_ENCODING_UTF16_BE, subject);
	matched = search_sub(inter, env, crb_reg->regexp, subject, end_p,
					subject, NULL, onig_region);

	MEM_free(subject);

	if (crb_region) {
		onig_region_to_crb_region(inter, env, onig_region, crb_region,
									crb_subject);
		onig_region_free(onig_region, 1);
	}
	return matched;

}

static CRB_Boolean match_crb_if(CRB_Interpreter *inter,
								CRB_LocalEnvironment *env,
								CRB_Regexp *crb_reg,
								CRB_Object *crb_subject,
								CRB_Object *crb_region)
{
	UChar *subject;
	UChar *end_p;
	OnigRegion *onig_region = NULL;
	CRB_Boolean matched;

	subject = encode_utf16_be(crb_subject->u.string.string);
	if (subject == NULL) {
		crb_runtime_error(0, UNEXPECTED_WIDE_STRING_ERR,
							MESSAGE_ARGUMENT_END);
	}
	if (crb_region) {
		onig_region = onig_region_new();
	}
	end_p = subject + onigenc_str_bytelen_null(ONIG_ENCODING_UTF16_BE, subject);
	matched = match_sub(inter, env, crb_reg->regexp, subject, end_p,
						onig_region);

	MEM_free(subject);

	if (crb_region) {
		onig_region_to_crb_region(inter, env, onig_region, crb_region,
									crb_subject);
		onig_region_free(onig_region, 1);
	}
	return matched;

}

#define REGEXP_GROUP_INDEX_MAX_COLUMN 3

static void replace_matched_place(CRB_Interpreter *inter,
								CRB_LocalEnvironment *env,
								CRB_CHAR *replacement, 
								UChar *subject,
								OnigRegion *region,
								VString *vs)
{
	int i;
	char g_idx_str[REGEXP_GROUP_INDEX_MAX_COLUMN+1];
	int g_idx_col;
	int scanf_result;
	int g_idx;
	int g_pos;

	for (i=0; replacement[i] != L'\0'; i++) {
		if (replacement[i] != L'\\') {
			crb_vstr_append_character(vs, replacement[i]);
			continue;
		}
		if (replacement[i+1] == L'\\') {
			crb_vstr_append_character(vs, replacement[i]);
			continue;
		}
		i++;
		for (g_idx_col = 0; g_idx_col < REGEXP_GROUP_INDEX_MAX_COLUMN; 
											g_idx_col++) {
			if (replacement[i] >= L'0' && replacement[i] <= L'9') {
				if (g_idx_col >= REGEXP_GROUP_INDEX_MAX_COLUMN) {
					MEM_free(subject);
					MEM_free(vs->string);
					onig_region_free(region, 1);
					crb_runtime_error(0, GROUP_INDEX_OVERFLOW_ERR,
							MESSAGE_ARGUMENT_END);
				}
				g_idx_str[g_idx_col] = '0' + (replacement[i] - L'0');
				i++;
			} else {
				i--;
				break;
			}

		}
		if (g_idx_col == 0) {
			crb_vstr_append_character(vs, L'\\');
			continue;
		}
		g_idx_str[g_idx_col] = '\0';

		scanf_result = sscanf(g_idx_str, "%d", &g_idx);
		DBG_assert(scanf_result==1, (""));

		if (g_idx <= 0 || g_idx >= region->num_regs) {
			MEM_free(subject);
			MEM_free(vs->string);
			onig_region_free(region, 1);
			crb_runtime_error(0, NO_SUCH_GROUP_INDEX_ERR,
					INT_MESSAGE_ARGUMENT, "g_idx", g_idx,
					MESSAGE_ARGUMENT_END);

		}
		for (g_pos = region->beg[g_idx]; g_pos < region->end[g_idx];
				g_pos += 2) {
			crb_vstr_append_character(vs, (subject[g_pos] << 8) 
								+ subject[g_pos+1]);
		}

	}
	
}


static CRB_Object* replace_crb_if(CRB_Interpreter *inter,
								CRB_LocalEnvironment *env,
								CRB_Regexp *crb_reg,
								CRB_Object *replacement,
								CRB_Object *crb_subject,
								int match_limit)
{
	UChar *subject;
	UChar *end_p;
	UChar *at_p;
	UChar *next_at;
	CRB_Boolean matched;
	OnigRegion *region;
	int match_count = 0;
	int i;

	VString vs;
	CRB_Object *result;

	subject = encode_utf16_be(crb_subject->u.string.string);
	if (subject == NULL) {
		crb_runtime_error(0, UNEXPECTED_WIDE_STRING_ERR,
							MESSAGE_ARGUMENT_END);
	}
	end_p = subject + onigenc_str_bytelen_null(ONIG_ENCODING_UTF16_BE,
								subject);
	at_p = subject;

	region = onig_region_new();
	crb_vstr_clear(&vs);

	while (match_count < match_limit) {
		matched = search_sub(inter, env, crb_reg->regexp, subject,
						end_p, at_p, &next_at, region);
		if (!matched)
			break;
		for (i = at_p - subject; i < region->beg[0]; i += 2) {
			crb_vstr_append_character(&vs, (subject[i] << 8) + 
					subject[i+1]);
		}
		replace_matched_place(inter, env, replacement->u.string.string,
					subject, region, &vs);
		match_count++;
		at_p = next_at;
	}
	if (match_count == 0) {
		MEM_free(subject);
		onig_region_free(region, 1);
		return crb_subject;
	}
	for (i = at_p - subject; i < end_p - subject; i += 2) {
		crb_vstr_append_character(&vs, (subject[i] << 8) + subject[i+1]);
	}
	result = crb_create_crowbar_string(inter, vs.string);
	MEM_free(subject);
	onig_region_free(region, 1);
	return result;

}

void add_splitted_string(CRB_Interpreter *inter,
						CRB_Object *array_obj,
						VString *vs)
{
	CRB_Value value;

	value.type = CRB_STRING_VALUE;
	value.u.object_value = crb_create_crowbar_string(inter, vs->string);
	crb_array_add(inter, array_obj, &value);
}


static CRB_Object* split_crb_if(CRB_Interpreter *inter,
						CRB_LocalEnvironment *env,
						CRB_Regexp *crb_reg,
						CRB_Object *crb_subject)
{
	UChar *subject;
	UChar *end_p;
	UChar *at_p;
	UChar *next_at;
	OnigRegion *region;
	int i;
	VString vs;
	CRB_Object *array_obj;

	array_obj = crb_create_array(inter, 0);

	subject = encode_utf16_be(crb_subject->u.string.string);
	if (subject == NULL) {
		crb_runtime_error(0, UNEXPECTED_WIDE_STRING_ERR,
							MESSAGE_ARGUMENT_END);
	}
	end_p = subject + onigenc_str_bytelen_null(ONIG_ENCODING_UTF16_BE,
						subject);
	at_p = subject;

	region = onig_region_new();

	while (search_sub(inter, env, crb_reg->regexp, subject,
						end_p, at_p, &next_at, region)) {
		crb_vstr_clear(&vs);
		for (i = at_p - subject; i < region->beg[0]; i += 2) {
			crb_vstr_append_character(&vs, (subject[i] << 8) + subject[i+1]);
		}
		add_splitted_string(inter, 	array_obj, &vs);
		at_p = next_at;
	}
	crb_vstr_clear(&vs);
	for (i = at_p - subject; i < end_p - subject; i += 2) {
		crb_vstr_append_character(&vs, (subject[i] << 8) + subject[i+1]);
	}
	add_splitted_string(inter, array_obj, &vs);

	MEM_free(subject);
	onig_region_free(region, 1);

	return array_obj;

}





/* implement re_search(pattern, subject [, region]) */
static void nv_search_proc(CRB_Interpreter *inter,
							CRB_LocalEnvironment *env,
							int arg_count,
							char *filename, int line_number)
{
	crb_gc_disable(inter);

	char *FUNC_NAME = "re_search";

	check_argument_count(arg_count, 2, 3, filename, line_number);

	CRB_Object *region = NULL;
	CRB_Value *args;

	args = crb_stack_peek_value(inter, arg_count-1);

	if (args[0].type != CRB_NATIVE_POINTER_VALUE ||
			args[0].u.native_pointer.info != crb_get_regexp_info()) {
		crb_runtime_error(filename, line_number, 
							ARGUMENT_TYPE_MISMATCH_ERR, 
							STRING_MESSAGE_ARGUMENT,
							"func_name", FUNC_NAME,
							MESSAGE_ARGUMENT_END);
	}
	if (args[1].type != CRB_STRING_VALUE) {
		crb_runtime_error(filename, line_number, 
							ARGUMENT_TYPE_MISMATCH_ERR,
							STRING_MESSAGE_ARGUMENT,
							"func_name", FUNC_NAME,
							MESSAGE_ARGUMENT_END);
	}

	if (arg_count==3) {
		if (args[2].type != CRB_ASSOC_VALUE) {
			crb_runtime_error(filename, line_number, 
							ARGUMENT_TYPE_MISMATCH_ERR,
							STRING_MESSAGE_ARGUMENT,
							"func_name", FUNC_NAME,
							MESSAGE_ARGUMENT_END);
		}
		region = args[2].u.object_value;
	}

	CRB_Regexp *crb_reg = args[0].u.native_pointer.pointer;
	CRB_Value result;

	result.type = CRB_BOOLEAN_VALUE;
	result.u.boolean_value = search_crb_if(inter, env, 
								crb_reg,
								args[1].u.object_value,
								region);

	crb_stack_shrink_size(inter, arg_count);
	crb_stack_push_value(inter, &result);

	crb_gc_enable(inter);
}


/* implement re_match(pattern, subject [, region]) */
static void nv_match_proc(CRB_Interpreter *inter,
							CRB_LocalEnvironment *env,
							int arg_count,
							char *filename, int line_number)
{
	crb_gc_disable(inter);

	char *FUNC_NAME = "re_match";

	check_argument_count(arg_count, 2, 3, filename, line_number);

	CRB_Object *region = NULL;
	CRB_Value *args;

	args = crb_stack_peek_value(inter, arg_count-1);

	if (args[0].type != CRB_NATIVE_POINTER_VALUE ||
			args[0].u.native_pointer.info != crb_get_regexp_info()) {
		crb_runtime_error(filename, line_number, 
							ARGUMENT_TYPE_MISMATCH_ERR, 
							STRING_MESSAGE_ARGUMENT,
							"func_name", FUNC_NAME,
							MESSAGE_ARGUMENT_END);
	}
	if (args[1].type != CRB_STRING_VALUE) {
		crb_runtime_error(filename, line_number, 
							ARGUMENT_TYPE_MISMATCH_ERR,
							STRING_MESSAGE_ARGUMENT,
							"func_name", FUNC_NAME,
							MESSAGE_ARGUMENT_END);
	}

	if (arg_count==3) {
		if (args[2].type != CRB_ASSOC_VALUE) {
			crb_runtime_error(filename, line_number, 
							ARGUMENT_TYPE_MISMATCH_ERR,
							STRING_MESSAGE_ARGUMENT,
							"func_name", FUNC_NAME,
							MESSAGE_ARGUMENT_END);
		}
		region = args[2].u.object_value;
	}

	CRB_Regexp *crb_reg = args[0].u.native_pointer.pointer;
	CRB_Value result;

	result.type = CRB_BOOLEAN_VALUE;
	result.u.boolean_value = match_crb_if(inter, env, 
								crb_reg,
								args[1].u.object_value,
								region);

	crb_stack_shrink_size(inter, arg_count);
	crb_stack_push_value(inter, &result);

	crb_gc_enable(inter);
}


/* implement re_replace(pattern, replacement, subject) */
static void nv_replace_proc(CRB_Interpreter *inter,
						CRB_LocalEnvironment *env,
						int arg_count,
						char *filename, int line_number)
{
	crb_gc_disable(inter);

	char *FUNC_NAME = "reg_replace";

	check_argument_count(arg_count, 3, 3, filename, line_number);

	CRB_Value *args = crb_stack_peek_value(inter, arg_count-1);

	if (args[0].type != CRB_NATIVE_POINTER_VALUE 
			|| args[0].u.native_pointer.info != crb_get_regexp_info()
			|| args[1].type != CRB_STRING_VALUE
			|| args[2].type != CRB_STRING_VALUE) {
			
		crb_runtime_error(filename, line_number, 
							ARGUMENT_TYPE_MISMATCH_ERR, 
							STRING_MESSAGE_ARGUMENT,
							"func_name", FUNC_NAME,
							MESSAGE_ARGUMENT_END);
	}

	CRB_Regexp *crb_reg;
	CRB_Value result;

	crb_reg = args[0].u.native_pointer.pointer;
	
	result.type = CRB_STRING_VALUE;
	result.u.object_value = replace_crb_if(inter, env, crb_reg,
							args[1].u.object_value,
							args[2].u.object_value, 1);

	crb_stack_shrink_size(inter, arg_count);
	crb_stack_push_value(inter, &result);

	crb_gc_enable(inter);

}

/* implement re_replace_all(pattern, replacement, subject) */
static void nv_replace_all_proc(CRB_Interpreter *inter,
						CRB_LocalEnvironment *env,
						int arg_count,
						char *filename, int line_number)
{
	crb_gc_disable(inter);

	char *FUNC_NAME = "reg_replace_all";

	check_argument_count(arg_count, 3, 3, filename, line_number);

	CRB_Value *args = crb_stack_peek_value(inter, arg_count-1);

	if (args[0].type != CRB_NATIVE_POINTER_VALUE 
			|| args[0].u.native_pointer.info != crb_get_regexp_info()
			|| args[1].type != CRB_STRING_VALUE
			|| args[2].type != CRB_STRING_VALUE) {
			
		crb_runtime_error(filename, line_number, 
							ARGUMENT_TYPE_MISMATCH_ERR, 
							STRING_MESSAGE_ARGUMENT,
							"func_name", FUNC_NAME,
							MESSAGE_ARGUMENT_END);
	}

	CRB_Regexp *crb_reg;
	CRB_Value result;

	crb_reg = args[0].u.native_pointer.pointer;
	
	result.type = CRB_STRING_VALUE;
	result.u.object_value = replace_crb_if(inter, env, crb_reg,
							args[1].u.object_value,
							args[2].u.object_value, INT_MAX);

	crb_stack_shrink_size(inter, arg_count);
	crb_stack_push_value(inter, &result);

	crb_gc_enable(inter);

}

/* implement re_split(pattern, subject) */
static void nv_split_proc(CRB_Interpreter *inter,
							CRB_LocalEnvironment *env,
							int arg_count,
							char *filename, int line_number)
{
	crb_gc_disable(inter);

	char *FUNC_NAME = "reg_split";

	check_argument_count(2, 2, 2, filename, line_number);
	
	CRB_Value *args = crb_stack_peek_value(inter, 1);

	if (args[0].type != CRB_NATIVE_POINTER_VALUE
			|| args[0].u.native_pointer.info != crb_get_regexp_info()
			|| args[1].type != CRB_STRING_VALUE) {
		crb_runtime_error(filename, line_number, 
							ARGUMENT_TYPE_MISMATCH_ERR, 
							STRING_MESSAGE_ARGUMENT,
							"func_name", FUNC_NAME,
							MESSAGE_ARGUMENT_END);
	}

	CRB_Regexp *crb_reg;
	CRB_Value result;

	crb_reg = args[0].u.native_pointer.pointer;

	result.type = CRB_ARRAY_VALUE;
	result.u.object_value = split_crb_if(inter, env,
							crb_reg, args[1].u.object_value);

	crb_stack_shrink_size(inter, arg_count);
	crb_stack_push_value(inter, &result);

	crb_gc_enable(inter);
}



void crb_add_regexp_functions(CRB_Interpreter *inter)
{
	CRB_add_native_function(inter, "reg_search", nv_search_proc);
	CRB_add_native_function(inter, "reg_match", nv_match_proc);
	CRB_add_native_function(inter, "reg_replace", nv_replace_proc);
	CRB_add_native_function(inter, "reg_replace_all", nv_replace_all_proc);
	CRB_add_native_function(inter, "reg_split", nv_split_proc);
}

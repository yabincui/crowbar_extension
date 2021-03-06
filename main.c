#include <stdio.h>
#include <string.h>
#include "CRB.h"
#include "MEM.h"
#include "DBG.h"

#define NAME_BUF_MAX 512

static void to_crb_code_name(const char *crb_name, char *buf, int max_len)
{
	const char *suffix = ".crb_code";
	int max_base_len = max_len - strlen(suffix) - 1;
	int i, j;

	for (i=0; i<max_base_len && crb_name[i]!='.' && crb_name[i]!='\0'; i++)
		buf[i] = crb_name[i];
	for (j=0; suffix[j]!='\0'; j++, i++)
		buf[i] = suffix[j];
	buf[i] = '\0';
}


static void dump_crb_code(CRB_Interpreter *inter, const char *crb_filename)
{

	char dump_file_name[NAME_BUF_MAX];
	to_crb_code_name(crb_filename, dump_file_name, sizeof(dump_file_name));
	

	FILE *fpdump;
	fpdump = fopen(dump_file_name, "w");
	if (fpdump == NULL) {
		fprintf(stderr, "can not open %s file to dump\n", dump_file_name);
		exit(1);
	}


	CRB_dump_interpreter(inter, fpdump);

	fclose(fpdump);
}


static void load_crb_code(CRB_Interpreter *inter, const char *crb_filename)
{

	char load_file_name[NAME_BUF_MAX];
	to_crb_code_name(crb_filename, load_file_name, sizeof(load_file_name));


	FILE *fpload;
	fpload = fopen(load_file_name, "r");
	if (fpload == NULL) {
		fprintf(stderr, "can not open %s file to load\n", load_file_name);
		exit(1);
	}


	CRB_load_interpreter(inter, fpload);

	fclose(fpload);
}

Encoding string_to_encoding(const char *type_name)
{
	Encoding type = NO_ENCODING;

	if (strcmp(type_name, "en")==0)
		type = EN_ENCODING;
	else if (strcmp(type_name, "utf8")==0)
		type = UTF8_ENCODING;
	else if (strcmp(type_name, "gbk")==0)
		type = GBK_ENCODING;
	
	return type;
}

static int file_exist(char *filepath)
{
	FILE *fp = fopen(filepath, "r");
	if (fp == NULL)
		return 0;
	fclose(fp);
	return 1;
}

static void find_builtin_filepath(char *exec_path, char *builtin_filepath)
{
	char *end_p;


	// try basename(argv[0]) + "/builtin/builtin.crb"
	end_p = strrchr(exec_path, '/');
	if (end_p != NULL) {
		int len = end_p - exec_path;
		strncpy(builtin_filepath, exec_path, len+1);
		builtin_filepath[len+1] = '\0';
	}
	else {
		builtin_filepath[0] = '\0';
	}
	strcat(builtin_filepath, "builtin/builtin.crb");
	if (file_exist(builtin_filepath))
		return;

	DBG_panic(("cannot find builtin.crb in %s\n", builtin_filepath));

}


void usage()
{
	printf("crowbar v0.3 usage:\n");
	printf("  crowbar [options] source_file_name\n");
	printf("options:\n");
	printf("  --source_encoding string  -- set the source file encoding\n");
	printf("  --env_encoding string     -- set the environment encoding\n");
	printf("supported encoding: en, utf8, gbk\n");
	printf("\n\n");
}

int
main(int argc, char **argv)
{
    CRB_Interpreter     *interpreter;

	int include_builtin_code = 1;
	int dump_crb_code_flag = 1;
	int load_crb_code_flag = 1;
	int dump2_crb_code_flag = 1;
	int exec_flag = 1;

	char *source_name = NULL;
	Encoding source_encoding = UTF8_ENCODING;
	Encoding env_encoding = UTF8_ENCODING;
	int i;

	for (i=1; i<argc; i++) {
		if (strcmp(argv[i], "--source_encoding")==0) {
			i++;
			if (i==argc) {
				usage();
				exit(1);
			}
			source_encoding = string_to_encoding(argv[i]);
		}
		else if (strcmp(argv[i], "--env_encoding")==0) {
			i++;
			if (i==argc) {
				usage();
				exit(1);
			}
			env_encoding = string_to_encoding(argv[i]);
		}
		else {
			if (source_name == NULL)
				source_name = argv[i];
			else {
				usage();
				exit(1);
			}
		}
	
	}

    if (source_name == NULL || source_encoding == NO_ENCODING
			|| env_encoding == NO_ENCODING) {
        usage();
		exit(1);
    }



    interpreter = CRB_create_interpreter(source_encoding, env_encoding);
	//printf("CRB_compile\n");

	if (include_builtin_code) {

		char builtin_filename[NAME_BUF_MAX];
		find_builtin_filepath(argv[0], builtin_filename);

		CRB_compile(interpreter, builtin_filename);
	}

    int compile_result = CRB_compile(interpreter, source_name);

	if (compile_result) {
		exit(1);
	}

	if (dump_crb_code_flag)
		dump_crb_code(interpreter, source_name);
	

	if (load_crb_code_flag) {
		CRB_reset_interpreter(&interpreter);
		load_crb_code(interpreter, source_name);

		if (dump2_crb_code_flag) {
			dump_crb_code(interpreter, "temp.crb");
		}
	}

	//printf("exec_flag=%d\n", exec_flag);
	if (exec_flag) {
		//printf("CRB_interpret\n");
		CRB_interpret(interpreter);
	}

	CRB_dispose_interpreter(interpreter);

	//interpreter = CRB_create_interpreter();
	//CRB_load_interpreter(interpreter, fpdump);
    //CRB_interpret(interpreter);
    //CRB_dispose_interpreter(interpreter);

    MEM_dump_blocks(stdout);


    return 0;
}

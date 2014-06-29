#include <stdio.h>
#include <string.h>
#include "CRB.h"
#include "MEM.h"

#define NAME_BUF_MAX 100

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

int
main(int argc, char **argv)
{
    CRB_Interpreter     *interpreter;
    FILE *fp;

	int dump_crb_code_flag = 1;
	int load_crb_code_flag = 1;
	int dump2_crb_code_flag = 1;
	int exec_flag = 1;

    if (argc != 2) {
        fprintf(stderr, "usage:%s filename", argv[0]);
        exit(1);
    }

    fp = fopen(argv[1], "r");
    if (fp == NULL) {
        fprintf(stderr, "%s not found.\n", argv[1]);
        exit(1);
    }


    interpreter = CRB_create_interpreter();
    CRB_compile(interpreter, fp);
	if (dump_crb_code_flag)
		dump_crb_code(interpreter, argv[1]);

	if (load_crb_code_flag) {
		CRB_reset_interpreter(&interpreter);
		load_crb_code(interpreter, argv[1]);

		if (dump2_crb_code_flag) {
			dump_crb_code(interpreter, "temp.crb");
		}
	}

	if (exec_flag) {
		CRB_interpret(interpreter);
	}

	CRB_dispose_interpreter(interpreter);

	//interpreter = CRB_create_interpreter();
	//CRB_load_interpreter(interpreter, fpdump);
    //CRB_interpret(interpreter);
    //CRB_dispose_interpreter(interpreter);

    MEM_dump_blocks(stdout);

	fclose(fp);

    return 0;
}

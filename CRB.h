#ifndef PUBLIC_CRB_H_INCLUDED
#define PUBLIC_CRB_H_INCLUDED
#include <stdio.h>

typedef struct CRB_Interpreter_tag CRB_Interpreter;

CRB_Interpreter *CRB_create_interpreter(void);
void CRB_compile(CRB_Interpreter *interpreter, FILE *fp);
void CRB_interpret(CRB_Interpreter *interpreter);
void CRB_dispose_interpreter(CRB_Interpreter *interpreter);
void CRB_reset_interpreter(CRB_Interpreter **pinter);

void CRB_dump_interpreter(CRB_Interpreter *interpreter, FILE *fpout);
void CRB_load_interpreter(CRB_Interpreter *interpreter, FILE *fpin);

#endif /* PUBLIC_CRB_H_INCLUDED */

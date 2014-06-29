TARGET = crowbar
CC=gcc
OBJS = \
  lex.yy.o\
  y.tab.o\
  main.o\
  interface.o\
  create.o\
  execute.o\
  eval.o\
  string.o\
  util.o\
  native.o\
  error.o\
  error_message.o\
  dump.o  \
  load.o \
  heap.o \
  gc.o \
  wchar.o \
  fake_method.o \
  exception.o \
  regexp.o \
  ./memory/mem.o\
  ./debug/dbg.o
CFLAGS = -c -g -Wall -DDEBUG #-Wswitch-enum -DDEBUG #-ansi -pedantic -DDEBUG
INCLUDES = \

$(TARGET):$(OBJS)
	cd ./memory; $(MAKE);
	cd ./debug; $(MAKE);
	$(CC) $(OBJS) -o $@ -lm -lonig
clean:
	rm -f *.o lex.yy.c y.tab.c y.tab.h y.output *~

	
y.tab.h : crowbar.y
	bison --yacc -dv crowbar.y
y.tab.c : crowbar.y
	bison --yacc -dv crowbar.y
lex.yy.c : crowbar.l crowbar.y y.tab.h
	flex crowbar.l
y.tab.o: y.tab.c crowbar.h MEM.h
	$(CC) -c -g $*.c $(INCLUDES)
lex.yy.o: lex.yy.c crowbar.h MEM.h
	$(CC) -c -g $*.c $(INCLUDES)
.c.o:
	$(CC) $(CFLAGS) $*.c $(INCLUDES)
./memory/mem.o:
	cd ./memory; $(MAKE);
./debug/dbg.o:
	cd ./debug; $(MAKE);
############################################################
create.o: create.c MEM.h DBG.h crowbar.h CRB.h CRB_dev.h
error.o: error.c MEM.h crowbar.h CRB.h CRB_dev.h
error_message.o: error_message.c crowbar.h MEM.h CRB.h CRB_dev.h
eval.o: eval.c MEM.h DBG.h crowbar.h CRB.h CRB_dev.h
execute.o: execute.c MEM.h DBG.h crowbar.h CRB.h CRB_dev.h
interface.o: interface.c MEM.h DBG.h crowbar.h CRB.h CRB_dev.h
main.o: main.c CRB.h MEM.h
native.o: native.c MEM.h DBG.h crowbar.h CRB.h CRB_dev.h
string.o: string.c MEM.h crowbar.h CRB.h CRB_dev.h
heap.o: heap.c MEM.h DBG.h crowbar.h CRB.h CRB_dev.h
util.o: util.c MEM.h DBG.h crowbar.h CRB.h CRB_dev.h
dump_load.o : dump_load.c MEM.h DBG.h crowbar.h CRB.h CRB_dev.h
gc.o: gc.c MEM.h DBG.h crowbar.h CRB.h CRB_dev.h
wchar.o: MEM.h DBG.h crowbar.h
fake_method.o : fake_method.c MEM.h DBG.h crowbar.h CRB.h CRB_dev.h
exception.o : exception.c MEM.h DBG.h crowbar.h CRB.h CRB_dev.h
regexp.o : regexp.c MEM.h DBG.h crowbar.h CRB.h CRB_dev.h

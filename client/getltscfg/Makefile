
CC ?= cc
LEX=flex
YACC=bison

LIBS=-lfl -lpopt
##LIBS=-lfl -ll -lpopt
YFLAGS=-v -d
CCFLAGS=-W -Wall $(CFLAGS) $(CPPFLAGS)

getltscfg:	getltscfg.o		\
		parser.tab.o		\
		lexer.o
	$(CC) -o getltscfg getltscfg.o	\
			parser.tab.o	\
			lexer.o		\
			$(LIBS)		\
			$(LDFLAGS)

getltscfg.o:	getltscfg.c		\
		getltscfg.h
	$(CC) -c getltscfg.c $(CCFLAGS) -o getltscfg.o

parser.tab.o:	parser.tab.c
	$(CC) -c parser.tab.c $(CCFLAGS) -o parser.tab.o

lexer.o:	lexer.c parser.tab.c
	$(CC) -c lexer.c $(CCFLAGS) -o lexer.o

parser.tab.c:	parser.y		\
		getltscfg.h
	$(YACC) $(YFLAGS) parser.y

lexer.c:	lexer.l			\
		getltscfg.h
	$(LEX) lexer.l
	mv lex.yy.c lexer.c

clean:
	-@rm -f lexer.c
	-@rm -f lexer.o
	-@rm -f parser.tab.c
	-@rm -f parser.tab.o
	-@rm -f y.output
	-@rm -f parser.tab.h
	-@rm -f parser.output
	-@rm -f getltscfg.o
	-@rm -f getltscfg

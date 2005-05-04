%{
#include	<stdio.h>
#include	<malloc.h>
#include	"getltscfg.h"
void yyerror(char *);
int yylex(void);

%}

%union	{
	char	*string;
	int	num;
}


%term		<string> 	INC
%term   EQUAL 
%term 	LB 
%term 	RB
%nonassoc	<string>	STRING


%%
configuration
	:	sections
	;

includes
	:	include
	|	sections
	|	includes include
	;

include
	:	INC  {
			process_include($1);
	}
	;

sections
	:	section
	|	includes
	|	sections section
	;

section
	:	sectiontag declarations
	;

declarations
	:	declaration
	|	declarations declaration
	;

declaration
	:	STRING EQUAL STRING {
			process_tuple($1,$3);
	}
	;

sectiontag
	:	LB STRING RB {
			process_section($2);
		}
	;



%%

//-----------------------------------------------------------------------------

void yyerror(char *s)
{
        (void)fprintf(stderr,"\n%s in %s, line=%d\n\n", s, curConfigFile, lineno);
        fSyntaxError = TRUE;
}

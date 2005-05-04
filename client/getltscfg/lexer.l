/* Modifications made to accept includes much in the same way bind does.
   Any real lex/bison hacker is welcome to fix the obviously cheesy bits.
   in particular, it seems hacky to make the include token be the keyword
   'include' plus the rest of the file path making it necessary to parse 
   out the identifier.  I did this becase the regex for 'string' is
   relatively greedy and comes up with a longer match string than the 
   include token.  What i really need is some sort of 'break' syntax for 
   lex that would read the first definition matching an include line and 
   STOP processing any other tokens.  By default it returns the 
   matching expression with the largest number of matched characters.
*/
%{
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	"getltscfg.h"
#include	"parser.tab.h"

void trim(char *);
int validateinclude(char *);
%}

lb		\[
rb		\]

inc		^[ \t]*include[ \t]+((\"[^\n]*\")|([^=\n\[\]"'#]*))[ \t]*
qstring		\"[^\n]*\"
string		[^=\n\[\]"'#]*

equal		\=

whitespace	[ \t]+
comment		#.*\n


%%

{equal}		{	return(EQUAL);		}
{lb}		{	return(LB);		}
{rb}		{	return(RB);		}
{inc}		{	
			if(validateinclude(yytext)) {
				// the file is valid and readable
				return(INC);
			}
			else {
				// simply returning a string type makes the whole syntax invalid.
				// a real lex hacker is welcome to make this (and any of the other cheesy bits) better
				return(STRING);
			}
		}

{whitespace}	{	/* do nothing */ }


{string}	{
			yylval.string = (char *)malloc(strlen(yytext)+1);
			strcpy(yylval.string,yytext);
			trim(yylval.string);
			return(STRING);
		}

{qstring}	{
                        int len = strlen(yytext);

			//
			// result is 2 bytes smaller than qstring, but don't
			// forget room for the trailing NULL.
			//
			yylval.string = (char *)malloc( len - 1 );

			//
			// Copy string, Skipping leading and trailing quote
			//
                        memcpy(yylval.string,yytext+1,len-2);
                        yylval.string[len-2] = '\0';	// Terminate that string
			return(STRING);
		}

{comment}	{	lineno++;		}
\n		{	lineno++;		}

%%

void trim(char *str)
{
    char *p;

    // Trim leading spaces

    while( isspace(*str) ){
        p = str;
        while(*p){
            *p = *(p+1);
            p++;
        }
    }

    // Look for hash '#' characters to denote comments
    // If we find one, and it is NOT inside of a quoted string
    // then replace it with a NULL to terminate the string.

    p = str;
    while(*p)
            p++;

    // Coming out of the loop, p points to the terminating NULL

    // Trim trailing spaces

    p--;                    // Get off of the trailing NULL

    while( p >= str && isspace(*p) ){
        *p = '\0';          // Replace the space with a NULL
        p--;
    }
}
// this function takes a line that appears to be for an include
// and parses the string to retrieve only the full path to the file and
// attempts to open the file.  if a failure occurs, zero is returned.
int validateinclude(char *fPath) 
{
	int len;
	char *tmp;
	FILE *incFile;
	
	tmp = (char *)malloc(strlen(fPath)+1);
	strcpy(tmp,fPath);
	trim(tmp);		

	sscanf(tmp,"include %n",&len);
	if(len > 0) {
		// fix fPath by removing the leading include tag
    		while(*tmp && len > 0) {
            		tmp++;
			len--;
		}
		trim(tmp);
		// if there are quotes, remove them
		len = strlen(tmp);
		if(len > 0) {
			if(tmp[0] == '"' && tmp[len - 1] == '"') {
				tmp[len-1] = '\0';
				tmp++;
				trim(tmp);
			}
			// try and validate this is indeed a file
			incFile = fopen (tmp,"r");
			if(incFile == NULL) {
				fclose(incFile);
				return 0;
			}
			else {
				fclose(incFile);
				// setup the filename in the yyval string
				yylval.string = (char *)malloc(strlen(tmp)+1);
				strcpy(yylval.string,tmp);
				return 1;
			}
		}
	}
	return 0;
}


#ifdef MAIN
#define EXTERN
#else
#define EXTERN extern
#endif

#define TRUE    1
#define FALSE   0

EXTERN int fDebuggingOn;
EXTERN int fSyntaxError;
EXTERN int lineno;
EXTERN char curConfigFile[1024];

void process_section(char *);
void process_include(char *);
void process_tuple(char*,char *);
int parseConfig(char *);

#define MAIN
#undef input

#include  <stdio.h>
#include  <ctype.h>
#include  <stdlib.h>
#include  <errno.h>
#include  <string.h>
#include  <popt.h>
#include  <sys/utsname.h>
#include  <sys/socket.h>
#include  <sys/ioctl.h>
#include  <sys/types.h>
#include  <sys/stat.h>
#include  <unistd.h>
#include  <net/if.h>
#include  <netinet/in.h>
#include  <arpa/inet.h>
#include  "getltscfg.h"

extern FILE *yyin;

struct TUPLE {
    char *keyword;
    char *value;
    struct TUPLE *next;
};

struct SECT {
    char *name;
    struct TUPLE *tuple_list;
    struct SECT *next;
};

struct CHAIN {
    char *name;
    struct SECT *sect;
    struct CHAIN *next;
    struct CHAIN *prev;
};

typedef struct TUPLE TUPLETYPE;
typedef struct SECT  SECTTYPE;
typedef struct CHAIN CHAINTYPE;

SECTTYPE *headsect;
SECTTYPE *cursect;

TUPLETYPE *curtuple;
TUPLETYPE *headtuple;
TUPLETYPE *tailtuple;

CHAINTYPE *headchain;
CHAINTYPE *tailchain;
CHAINTYPE *curchain;

#define NUM_WORKSTATION_ID 4
char *aWorkstationId[NUM_WORKSTATION_ID];

char sIPAddr[64];
char sMACAddr[64];

int	fDumpAll;

static char *sConfigFile = "/etc/lts.conf";
static char *sHostName   = "";

#define MAX_FILE_INCLUDES 10
struct stat *includeFiles[MAX_FILE_INCLUDES];
int includeFileCount = 0;

struct utsname unamebuf;

FILE *CFGFILE;

char *pOptionName;
int verbose = 0;
//
// Function prototypes
//
void DecodeCommandLine(int, const char **);
int yyparse(void);

//------------------------------------------------------------------------------
//
// Creating a new section involves allocating a section structure, filling in
// the fields, and then linking the structure into the linked list that starts
// with headsect.
//
void process_section( char *sectname )
{
    if(verbose) {
        fprintf(stderr, "getlts::process_section [%s]\n",sectname);
    }
    //
    // Allocate a section structure
    //
    SECTTYPE *sectptr = (SECTTYPE *)malloc(sizeof(SECTTYPE));

    //
    // Allocate some memory to hold the name of the section
    //
    sectptr->name = (char *)malloc(strlen(sectname)+1);

    //
    // Copy the name in
    //
    strcpy(sectptr->name,sectname);

    //
    // set the 'next' pointer to zero
    //
    sectptr->next = 0;

    if( cursect )
        cursect->next = sectptr;
    else
        headsect = sectptr;

    cursect = sectptr;
    cursect->tuple_list = 0;  // Each section starts with an empty tuple list
    curtuple = 0;
}



//------------------------------------------------------------------------------
// This procedure takes a filename as argument (which has already been verified
// as readable) process it as if the contents were part of the original config file
// In other words, this code supports 'include' syntax for the config files
void process_include( char *incfile )
{
   if(verbose) {
       fprintf(stderr, "getlts::process_include [%s]\n",incfile);
   }
   // attempt to parse the new config file
   parseConfig(incfile);
}

//------------------------------------------------------------------------------


void process_tuple(char *kwd, char *val)
{
    if(verbose) {
        fprintf(stderr, "getlts::process_tuple [%s,%s]\n",kwd,val);
    }
    //
    // Allocate a TUPLE structure
    //
    TUPLETYPE *tupleptr = (TUPLETYPE *)malloc(sizeof(TUPLETYPE));

    //
    // Allocate some memory to hold the keyword
    //
    tupleptr->keyword = (char *)malloc(strlen(kwd)+1);
    strcpy(tupleptr->keyword,kwd);

    //
    // Allocate some memory to hold the value
    //
    tupleptr->value = (char *)malloc(strlen(val)+1);
    strcpy(tupleptr->value,val);

    //
    // Set the 'next' pointer to zero
    //
    tupleptr->next = 0;

    if( curtuple )
        curtuple->next = tupleptr;
    else
        cursect->tuple_list = tupleptr;

    curtuple = tupleptr;
}

//------------------------------------------------------------------------------
//
// find_chain_entry(), returns TRUE if it found the entry, and FALSE otherwise
//
int find_chain_entry(char *name)
{
    int	fFound = FALSE;
    SECTTYPE *worksect;
    cursect = headsect;
    while( cursect && !fFound ){
        if(strcasecmp(cursect->name,name) == 0){
            CHAINTYPE *chainptr = (CHAINTYPE *)malloc(sizeof(CHAINTYPE));
            worksect = cursect;
            fFound = TRUE;
            if(curchain)
                curchain->next = chainptr;
            else
                headchain = chainptr;

            chainptr->name = (char *)malloc(strlen(cursect->name)+1);
            strcpy(chainptr->name,cursect->name);
            chainptr->sect = cursect;
            chainptr->next = 0;
            chainptr->prev = curchain;

            curchain = chainptr;
            tailchain = chainptr;
        }
        cursect = cursect->next;
    }
    return(fFound);
}

//------------------------------------------------------------------------------

void add_tuple(TUPLETYPE *tuple)
{
    int fFound = FALSE;
    TUPLETYPE *t;

    t = headtuple;
    while(t && !fFound){
        if(strcasecmp(t->keyword,tuple->keyword) == 0)
            fFound = TRUE;
        else
            t = t->next;
    }

    if(fFound){
        // replace it
        free((char *)t->value);
        t->value = (char *)malloc(strlen(tuple->value)+1);
        strcpy(t->value,tuple->value);
    }
    else{

        // Allocate the space for the tuple
        t = (TUPLETYPE *)malloc(sizeof(TUPLETYPE));

	t->next = 0;

        // Copy the keyword
        t->keyword = (char *)malloc(strlen(tuple->keyword)+1);
        strcpy(t->keyword,tuple->keyword);

        // Copy the value
        t->value = (char *)malloc(strlen(tuple->value)+1);
        strcpy(t->value,tuple->value);

        if(tailtuple)
            tailtuple->next = t;
        else
            headtuple = t;

        tailtuple = t;
    }
}

//------------------------------------------------------------------------------
//
// void GetAddrs()
//
// This routine obtains the IP and MAC addresses of the first
// appropriately configured network interface.
//

void GetAddrs()
{
    int numreqs = 10;
    struct ifconf ifc;
    struct ifreq *ifr;	/* netdevice(7) */
    struct ifreq info;
    struct sockaddr_in *sa;

    char *pIPAddr = &sIPAddr[0];
    char *pMACAddr = &sMACAddr[0];

    int skfd, n;

    *pIPAddr  = '\0';
    *pMACAddr = '\0';

    skfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if(skfd < 0){
        perror("socket");
	return;
    }

    //
    // Get a list of all the interfaces.
    //
    ifc.ifc_buf = NULL;
    for(;;){
	ifc.ifc_len = sizeof(struct ifreq) * numreqs;
	ifc.ifc_buf = (char *)realloc(ifc.ifc_buf, ifc.ifc_len);
	if(ifc.ifc_buf == NULL) {
	    fprintf(stderr, "Out of memory\n");
	    return;
	}

	if(ioctl(skfd, SIOCGIFCONF, &ifc) < 0){
	    perror("SIOCGIFCONF");
	    goto out;
	}

	if(ifc.ifc_len == sizeof(struct ifreq) * numreqs){
	    // assume it overflowed and try again
	    numreqs += 10;
	    continue;
	}
	break;
    }

    //
    // Look for the first interface that has an IP address, is not
    // loopback, and is up.
    //
    ifr = ifc.ifc_req;
    for(n = 0; n < ifc.ifc_len; n += sizeof(struct ifreq)){
	if(ifr->ifr_addr.sa_family != AF_INET)
	    continue;

	strcpy(info.ifr_name, ifr->ifr_name);
	if(ioctl(skfd, SIOCGIFFLAGS, &info) < 0){
	    perror("SIOCGIFFLAGS");
	    goto out;
	}
	if(!(info.ifr_flags & IFF_LOOPBACK) && (info.ifr_flags & IFF_UP)){
	    sa = (struct sockaddr_in *) &ifr->ifr_addr;
	    sprintf(pIPAddr, "%s", inet_ntoa(sa->sin_addr));
	    break;
	}
	ifr++;
    }

    if(n == ifc.ifc_len){
	fprintf(stderr, "No configured interface found\n");
	goto out;
    }

    if(ioctl(skfd, SIOCGIFHWADDR, &info) < 0){
	perror("SIOCGIFHWADDR");
	goto out;
    }

    for(n = 0; n < IFHWADDRLEN; n++)
	pMACAddr += sprintf(pMACAddr, "%02x%s",
	    info.ifr_hwaddr.sa_data[n] & 0xff, n < IFHWADDRLEN-1 ? ":" : "");

out:
    free(ifc.ifc_buf);
}

//------------------------------------------------------------------------------

int main( int argc, const char **argv )
{
    int    i;
    int    status;
    int    fFound;
    CHAINTYPE *chain;

    fDumpAll   = FALSE;
   
    DecodeCommandLine( argc, argv );

    fSyntaxError = FALSE;

    //
    // Initialize some of our list  pointers
    //
    cursect   = headsect  = 0;
    curchain  = headchain = tailchain = 0;
    tailtuple = headtuple = 0;

    for( i = 0; i < NUM_WORKSTATION_ID; i++ ){
        aWorkstationId[i] = 0L;
    }

    if( sHostName[0] ){
        aWorkstationId[0] = (char *)malloc(strlen(sHostName)+1);
	strcpy(aWorkstationId[0],sHostName);
    }
    else{
        // Get the hostname from the system
        uname(&unamebuf);
	aWorkstationId[0] = (char *)malloc(strlen(unamebuf.nodename)+1);
	strcpy(aWorkstationId[0],unamebuf.nodename);

	// Get the
        GetAddrs();
	aWorkstationId[1] = (char *)malloc(strlen(sIPAddr)+1);
	strcpy(aWorkstationId[1],sIPAddr);

	aWorkstationId[2] = (char *)malloc(strlen(sMACAddr)+1);
	strcpy(aWorkstationId[2],sMACAddr);
    }


    // treat the main config file just like any other include
    if(parseConfig(sConfigFile) != 0) {
        char buf[255];
	sprintf(buf,"Error opening config file [%s]", sConfigFile);
        perror(buf);
        exit(2);
    }


    lineno = 1;
    yyparse();

    
    //
    // When the parsing is complete, we have a tree in memory
    // that contains ALL of the sections with all of the values.
    //

    //
    // The next step is to find the section that matches the workstation
    // that we are interested in.  Then, walk through the linked list
    // looking for any entries that indicate we want to "inherit" entries
    // from another section.  We'll build a new linked list of those
    // sections that we want to inherit from. Then, we'll tack the 'Default'
    // section on the end of that.  Once we have the inheritance list, we
    // need to walk that list backwards, building the list of
    // tuples (keyword/value pairs).
    // Finally, when we are all done, we should have all of the values that
    // should go with the workstation we are interested in.  So, we
    // either dump all of the entries out, or we dump out a single entry.
    // This is dependent on whether this program was called with the
    // '-a' option or not.
    //

    //
    // Find the entry that exactly matches our workstation.
    //

    i = 0;
    fFound = FALSE;
    while( aWorkstationId[i] && !fFound ){
        if(find_chain_entry(aWorkstationId[i])){
	    fFound = TRUE;
        }
        i++;
    }

    chain = headchain;
    while(chain){
        curtuple = chain->sect->tuple_list;
        while(curtuple){
            if(strcasecmp(curtuple->keyword,"LIKE") == 0)
                status = find_chain_entry(curtuple->value);

            curtuple = curtuple->next;
        }
        chain = chain->next;
    }

    //
    // the last entry in the chain is the '[Default]' entry
    //
    status = find_chain_entry("default");


    curchain = tailchain;
    while(curchain){
        curtuple = curchain->sect->tuple_list;
        while(curtuple){
            if(strcasecmp(curtuple->keyword,"LIKE") != 0)
                add_tuple(curtuple);

            curtuple = curtuple->next;
        }
        curchain = curchain->prev;
    }

    if(fDumpAll){
        //
        // Spit out all of the variables for this workstation
        //
        curtuple = headtuple;
        while(curtuple){
            char *p = curtuple->keyword;
            while(*p){
                *p = toupper(*p);
                p++;
            }
            if(strcasecmp(curtuple->value,"NONE") != 0){
                printf("%s=\"%s\"\n", curtuple->keyword, curtuple->value);
            }
            curtuple = curtuple->next;
        }
        curtuple = headtuple;
        while(curtuple){
            if(strcasecmp(curtuple->value,"NONE") != 0){
                printf("export %s\n", curtuple->keyword);
            }
            curtuple = curtuple->next;
        }
    }
    else{
        //
        // Dump just one var
        //
        int fFound = FALSE;
        curtuple = headtuple;
        while(curtuple && !fFound ){
            if(strcasecmp(curtuple->keyword,pOptionName) == 0){
                if(strcasecmp(curtuple->value,"NONE") != 0){
                    printf("%s\n", curtuple->value);
                }
                fFound = TRUE;
             }
             curtuple = curtuple->next;
        }
    }

    return(0);
}

//------------------------------------------------------------------------------

void usage(int argc, const char **argv) {

    fprintf(stderr, "\nUsage: %s  [options] [keyword]\n", argv[0]);
    fprintf(stderr, "\n  Options:    [{-c|--configfile} <configfile>]\n");
    fprintf(stderr, "              [{-n|--hostname} <hostname>]\n");
    fprintf(stderr, "              [{-a|--all}]\n");
    fprintf(stderr, "              [{-v|--verbose}]\n");
    fprintf(stderr, "              [{-h|--help}]\n");
    fprintf(stderr, "              [keyword]\n\n");
    
    exit(1);
}

//------------------------------------------------------------------------------

void DecodeCommandLine(int argc, const char **argv)
{
    signed int c;

    poptContext optCon;           // context for parsing command-line options

    struct poptOption optionsTable[] = {
        { "configfile", 'c', POPT_ARG_STRING, &sConfigFile,  0,  NULL, NULL },
        { "hostname",   'n', POPT_ARG_STRING, &sHostName,    0,  NULL, NULL },
        { "all",        'a', 0,               0,            'a', NULL, NULL },
        { "verbose",    'v', 0,               0,            'v', NULL, NULL },
        { "help",       'h', 0,               0,            'h', NULL, NULL },
        { NULL,         0,   0,               NULL,          0,  NULL, NULL }
    };

    if( argc < 2 ){
        usage(argc,argv);
    }

    optCon = poptGetContext((const char *)NULL,
		            argc,
			    (const char **)argv,
			    (const struct poptOption *)optionsTable,
			    0 );

    while( ( c = poptGetNextOpt(optCon) ) >= 0 ) {
        switch(c){
            case 'a': fDumpAll = TRUE;
                      break;
            case 'h': usage(argc,argv);
                      break;
            case 'v': verbose = TRUE;
                      break;
        }
    }

    //
    // Make sure they supplied the name of a configuration key
    //
    if( ! fDumpAll ){
        if( ( pOptionName = (char *)poptGetArg(optCon) ) == NULL ){
            usage(argc,argv);
        }
    }

    poptFreeContext(optCon);
}
int parseConfig(char *config) 
{
    FILE *INCFILE;
    int i, found = 0;
    struct stat cstat;
    char buf[255];
    
    // get a stat on the file
    if(stat(config, &cstat) != 0) {
	sprintf(buf,"Error retrieving file stats for file [%s]",config);
        perror(buf);
        exit(2);
    }
    // verify that we haven't already opened this file..
    for(i = 0; i < includeFileCount && i < MAX_FILE_INCLUDES; i++) {
        if(verbose) {
            fprintf(stderr, "parseConfig::looping over includeFiles, trying to match (dev: [%d], inode: [%d]) against (dev: [%d], inode: [%d])\n",(int)cstat.st_dev,(int)cstat.st_ino,(int)((struct stat)*includeFiles[i]).st_dev,(int)((struct stat)*includeFiles[i]).st_ino);
        }
        if(cstat.st_dev == ((struct stat)*includeFiles[i]).st_dev && cstat.st_ino == ((struct stat)*includeFiles[i]).st_ino) {
            fprintf(stderr, "Warning:  ignoring previously seen include file: %s\n",config);
            found = 1;
        }
    }
    if(i < MAX_FILE_INCLUDES && found == 0) {
        if(verbose) {
            fprintf(stderr, "Adding file to include list..\n");
        }
        includeFiles[i] = (struct stat *)malloc(sizeof(cstat));
        memcpy(includeFiles[i],&cstat,sizeof(cstat));
        includeFileCount++;
        
        if((INCFILE = fopen(config,"r")) == NULL ) {
            sprintf(buf,"Error opening config file [%s]", config);
            perror(buf);
            exit(2);
        }
	// I'm not certain if closing yyin is necessary or appropriate.  I couldn't get it to work correctly.
/*	if(fileno(yyin) > 0) {
		fclose(yyin);
	}
*/
	lineno = 0;
	strcpy(curConfigFile,config);
        yyin = INCFILE;

        return 0;
    }
    else if(i >= MAX_FILE_INCLUDES) {
        fprintf( stderr, "Warning: MAX_FILE_INCLUDES (%d) exceeded.  Ignoring include file %s\n",MAX_FILE_INCLUDES,config);
        return 1;
    }
    else {
        fprintf( stderr, "Warning: Include file %s has already been included.  Skipping to avoid infinite loop.\n",config);
        return 2;
    }
}

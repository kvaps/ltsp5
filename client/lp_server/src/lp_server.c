/*
	lp_server [-n port] [-w] [-d device] [-t stty_cmds]

    Export a printer by simulating an HP JetDirect interface.
    -n connection port (default 9100)
    -w open output device write only (default r/w)
    -d device (default /dev/lp)
    -t stty_cmds - stty commands to apply if device is serial port

    The lp_server will bind to the connection port, and wait for
    an incoming connection.  After accepting a  connection, it 
	opens the output device, (rw or write only as specified),
    applys the stty options,  and then copies the connection output
    to the device input and vice versa.


    Copyright 1994-1997 Patrick Powell, San Diego, CA <papowell@sdsu.edu>

 $Id: lp_server.c,v 1.1 2004/07/08 05:03:36 jam Exp $
*/


#define EXTERN
#include "portable.h"
#include "common.h"
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <version.h>


char *device = "/dev/lp";
char *stty_opts;
char *port = "9100";
int write_only;
void usage(void);
int sock, conn, dev = -1;
int pid;

char *restrict;

char buffer[1024];

int tcp_open( char * portname );
int Check_restriction(char *restrict, struct sockaddr_in *sin);

extern char *optarg;
extern int optind;

int main( int argc, char *argv[] )
{
	int c, n, maxfd, mask;
	struct sockaddr_in sin;
	fd_set readfds;

	name = argv[0];
	while( ((c = getopt(argc, argv, "D:n:wd:t:r:")) != EOF) ){
		switch(c){
		case 'D': debug = atoi(optarg); break;
		case 'n': port = optarg; break;
		case 'w': write_only = 1; break;
		case 'r': restrict = optarg; break;
		case 'd': device = optarg; break;
		case 't': stty_opts = optarg; break;
		default: usage();
		}
	}
	if( optind != argc ) usage();

	/* step 1 - bind to the port */
	sock = tcp_open( port );
	if( sock < 0 ) exit(1);

	if( debug ) logmsg(1,"socket %d", sock );

	while(1) {
		/* accept a connection */
		if( dev >= 0 ) close(dev); dev = -1;
		if( debug ) logmsg( 1, "waiting" );
		n = sizeof(sin);
		conn = accept( sock,(void *) &sin, &n );
		if(debug) logmsg(1,"conn %d", conn );
		if( conn == -1 && errno != EINTR ){
			logerr_die(1, "accept failed" );
		}
		if( restrict && Check_restriction(restrict, &sin) ){
			if( debug ) logmsg( 1, "reject %s", inet_ntoa( sin.sin_addr ) );
			close( conn );
			continue;
		}
		/* open the output device */
		if( write_only ){
			dev = open( device, O_WRONLY|O_NONBLOCK );
		} else {
			dev = open( device, O_RDWR|O_NONBLOCK );
		}
		if( dev == -1 ){
			logerr_die(1, "open of %s failed", device );
		}
		if(debug) logmsg(1,"dev %d", dev );
		mask = fcntl( dev, F_GETFL, 0 );
		if( mask == -1 ){
			logerr_die(1,"fcntl F_GETFL of '%s' failed", device);
		}
		if( mask & O_NONBLOCK ){
			mask &= ~O_NONBLOCK;
			mask = fcntl( dev, F_SETFL, mask );
			if( mask == -1 && errno != ENODEV ){
				logerr_die( 1, "fcntl F_SETFL of '%s' failed",
					device );
			}
		}
		if( isatty( dev ) && stty_opts ){
			Do_stty( dev, stty_opts );
		}

		/* write/read-write loop */
		for(;;) {
			FD_ZERO( &readfds );
			
			FD_SET( conn, &readfds );
			if( !write_only ) {
				if(debug) logmsg(1, "read/write mode FD_SET");
				FD_SET( dev, &readfds );
			}
			
			if( conn > dev ) {
				maxfd = conn;
			} else {
				maxfd = dev;
			}
			
			maxfd++;
			
			n = select( maxfd, &readfds, ( fd_set * ) 0, ( fd_set * ) 0, 0 );
			if(debug) logmsg(1, "select returned %d", n);

			if( n == -1 ) {
				logerr_die(1, "an error occured with the select statement");
			}
			
			if( FD_ISSET( conn, &readfds ) ) {
				/* data on socket */
				if( (n = read( conn, buffer, sizeof( buffer ))) <= 0)
					break;
				if(debug) logmsg(1,"read %d bytes from socket", n);
				
				n = write( dev, buffer, n );
				if(debug) logmsg(1, "wrote %d bytes to device", n);
			}
			
			if( !write_only ) {
				if( FD_ISSET( dev, &readfds ) ) {
					/* data on device */
					n = read( dev, buffer, sizeof( buffer ));
					if(debug) logmsg(1,"read %d bytes from device", n);
					n = write( conn, buffer, n );
					if(debug) logmsg(1, "wrote %d bytes to socket", n);
				}
			}
		}			

		/* close connections */
		close( conn ); conn = -1;
		close( dev ); dev = -1;
	}
	return(0);
}


int tcp_open( char *portname )
{
	int port, i, fd, err;
	int option, len;
	struct sockaddr_in sin;
	struct servent *servent;

	port = atoi( portname );
	if( port <= 0 ){
		servent = getservbyname( portname, "tcp" );
		if( servent ){
			port = ntohs( servent->s_port );
		}
	}
	if( port <= 0 ){
		fprintf( stderr, "tcp_open: bad port number '%s'\n",portname );
		return( -1 );
	}
	if( debug ) logmsg(1, "port %d", port );
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons( port );

	fd = socket( AF_INET, SOCK_STREAM, 0 );
	err = errno;
	if( fd < 0 ){
		fprintf(stderr,"tcp_open: socket call failed - %s\n", Errormsg(err) );
		return( -1 );
	}
	i = bind( fd, (struct sockaddr *) & sin, sizeof (sin) );
	err = errno;

	if( i < 0 ){
		fprintf(stderr,"tcp_open: bind to '%s port %d' failed - %s\n",
			inet_ntoa( sin.sin_addr ), ntohs( sin.sin_port ),
			Errormsg(errno) );
		close(fd);
		return( -1 );
	}
/* #ifdef SO_REUSEADDR */
	len = sizeof( option );
	option = 1;
	if( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR,
			(char *)&option, sizeof(option) ) ){
		logerr_die( 1, "setsockopt failed" );
	}
/* #endif */

	i = listen( fd, 10 );
	err = errno;

	if( i < 0 ){
		fprintf(stderr,"tcp_open: listen to '%s port %d' failed - %s\n",
			inet_ntoa( sin.sin_addr ), ntohs( sin.sin_port ),
			Errormsg(errno) );
		close(fd);
		return( -1 );
	}
	return( fd );
}

char *msg[] = {
"lp_server [-n port] [-w] [-d device] [-t stty_cmds]",
"   Version " VERSION ,
"    Export a printer by simulating an HP JetDirect interface.",
"    -n connection port (default 9100)",
"    -w open output device write only (default r/w)",
"    -d device (default /dev/lp)",
"    -t stty_cmds - stty commands to apply if device is serial port",
"    -r remote_list - remote hosts allowed, in IP/netmask or hostname",
"           separated by commas. i.e. - 130.191.20.10/24,dickory would",
"           allow connections from subnet 130.191.20.0 (24 bit netmask)",
"           and host dickory",
"    -D level - set debug level",
0
};

void usage()
{
	char *s;
	int i;
	for( i = 0; (s = msg[i]); ++i ){
		fprintf( stderr, "%s\n", s );
	}
	exit(1);
}

/*
 * check restrictions on connecting host
 */

int Check_restriction(char *restrict, struct sockaddr_in *sin)
{
	char *s, *mask, *end, *b;
	struct hostent *h;
	struct sockaddr_in addr;
	unsigned long n, m, a;

	a = ntohl( sin->sin_addr.s_addr );
	if( debug ) logmsg(1,"remote 0x%x", a );
	b = strdup( restrict );
	for( s = b; s; s = end ){
		end = strchr( s, ',' );
		if( end ){
			*end++ = 0;
		}
		while( isspace( *s ) ) ++s;
		mask = strchr( s, '/' );
		if( mask ){
			*mask++ = 0;
		}
		if( ( h = gethostbyname( s ) ) ){
			memcpy( &addr.sin_addr, h->h_addr_list[0], sizeof(addr.sin_addr));
			n = ntohl( addr.sin_addr.s_addr );
		} else {
			n = inet_addr( s );
		}
		if( n == (unsigned long)-1 ){
			logmsg(1,"bad addr format '%s'", s );
			return(1);
		}
		m = -1;
		if( mask ){
			m = atoi( mask );
			if( m == 0 || m > 32 ){
				logmsg(1,"bad mask format '%s'", mask );
			}
			m =  ~((1 << (32-m)) - 1);
		}
		if( debug ) logmsg( 1, "checking 0x%x, mask 0x%x", n, m );
		n = (n ^ a) & m;
		if( debug ) logmsg( 1, "result 0x%x", n );
		if( n == 0 ) return( 0 );
	}
	return( 1 );
}

/*
 * get_ipaddr()
 * (C) James A. McQuillan, 2000
 *
 * This software is licensed under the GNU GLPv2.
 */

#include <sys/utsname.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <string.h>

#include "ldm.h"

void
get_ipaddr()
{
    int numreqs = 10;
    struct ifconf ifc;
    struct ifreq *ifr;	/* netdevice(7) */
    struct ifreq info;
    struct sockaddr_in *sa;

    char *ipaddr = NULL;

    int skfd, n;

    skfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (skfd < 0)
        die("socket");

    /*
     * Get a list of all the interfaces.
     */

    ifc.ifc_buf = NULL;

    for(;;) {
        ifc.ifc_len = sizeof(struct ifreq) * numreqs;
	    ifc.ifc_buf = (char *)realloc(ifc.ifc_buf, ifc.ifc_len);
	    if(ifc.ifc_buf == NULL) 
            die("Out of memory");

	    if (ioctl(skfd, SIOCGIFCONF, &ifc) < 0) {
            syslog(LOG_ERR, "SIOCGIFCONF");
	        goto out;
	    }

	    if(ifc.ifc_len == (int)sizeof(struct ifreq) * numreqs){
	        /* assume it overflowed and try again */
	        numreqs += 10;
	        continue;
	    }

	    break;
    }

    /*
     * Look for the first interface that has an IP address, is not
     * loopback, and is up.
     */

    ifr = ifc.ifc_req;
    for (n = 0; n < ifc.ifc_len; n += sizeof(struct ifreq)) {
	    if (ifr->ifr_addr.sa_family != AF_INET)
	        continue;

	    strcpy(info.ifr_name, ifr->ifr_name);
	    if (ioctl(skfd, SIOCGIFFLAGS, &info) < 0) {
            syslog(LOG_ERR, "SIOCGIFFLAGS");
	        goto out;
	    }

	    if (!(info.ifr_flags & IFF_LOOPBACK) && (info.ifr_flags & IFF_UP)) {
	        sa = (struct sockaddr_in *) &ifr->ifr_addr;
	        ldminfo.ipaddr = strdup(inet_ntoa(sa->sin_addr));
	        break;
	    }

	    ifr++;
    }

    if (n == ifc.ifc_len)
	    syslog(LOG_ERR, "No configured interface found");

out:
    if (ifc.ifc_buf)
        free(ifc.ifc_buf);
}

/* LTSP Graphical GTK Greeter
 * Copyright (C) 2007 Francis Giraldeau, <francis.giraldeau@revolutionlinux.com>
 *
 * - Queries servers to get information about them
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>
#include <fcntl.h>
#include <glib/gi18n.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netdb.h>

#include <greeter.h>

void
ldminfo_init(GHashTable ** ldminfo_hash, GList ** host_list,
             const char *ldm_server)
{
    char **hosts_char = NULL;
    ldminfo *ldm_host_info = NULL;
    int i;

    *ldminfo_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                          g_free, g_free);
    hosts_char = g_strsplit(ldm_server, " ", -1);

    for (i = 0; hosts_char != NULL && hosts_char[i] != NULL; i++) {
        /* Initialize to default values */
        ldm_host_info = g_new0(ldminfo, 1);
        ldm_host_info->languages = NULL;
        ldm_host_info->sessions = NULL;
        ldm_host_info->rating = 0;
        ldm_host_info->state = SRV_DOWN;

        /*
         * Insert into the hash table.
         */

        g_hash_table_insert(*ldminfo_hash, g_strdup(hosts_char[i]),
                            ldm_host_info);

        /*
         * Populate the ldminfo structure, and determine if the host
         * is up or down.
         */

        _ldminfo_query_one(hosts_char[i], ldm_host_info);

        /*
         * Add the host to the host list.
         */

        *host_list = g_list_append(*host_list, g_strdup(hosts_char[i]));
    }
    g_strfreev(hosts_char);
}

/*
 * Do the query for one host and fill ldminfo struct
 * Note: for right now, we're reading files in /tmp.  Francis would like
 * the host detection closer to the login and checking network availability
 * etc.  What should happen here, for gutsy+1, is to call out to an
 * external script.  This script will query ldminfo, perform ssh port testing,
 * etc. Things like nc -z hostname ssh, etc.
 */

void
_ldminfo_query_one(const char *hostname, ldminfo * ldm_host_info)
{
    int filedes, numbytes;
    char buf[MAXBUFSIZE];
    char hostfile[BUFSIZ];

    snprintf(hostfile, sizeof hostfile, "/tmp/ldm/%s", hostname);

    filedes = open(hostfile, O_RDONLY);

    if ((numbytes = read(filedes, buf, MAXBUFSIZE - 1)) == -1) {
        perror("read");
        goto error;
    }

    buf[numbytes] = '\0';

    close(filedes);
    ldm_host_info->state = SRV_UP;
    _ldminfo_parse_string(buf, ldm_host_info);
    return;

  error:
    close(filedes);
    ldm_host_info->state = SRV_DOWN;
}

/*
 * split string by line and then construct the ldm_host_info
 */

void
_ldminfo_parse_string(const char *s, ldminfo * ldm_host_info)
{

    char **lines = NULL;
    int i;

    lines = g_strsplit(s, "\n", -1);

    for (i = 0; lines != NULL && lines[i] != NULL; i++) {
        if (!g_ascii_strncasecmp(lines[i], "language:", 9)) {
            gchar **val;
            val = g_strsplit(lines[i], ":", 2);
            ldm_host_info->languages =
                g_list_append(ldm_host_info->languages, g_strdup(val[1]));
            g_strfreev(val);
        } else if (!g_ascii_strncasecmp(lines[i], "session:", 8)) {
            gchar **val;
            val = g_strsplit(lines[i], ":", 2);
            ldm_host_info->sessions =
                g_list_append(ldm_host_info->sessions, g_strdup(val[1]));
            g_strfreev(val);
        } else if (!g_ascii_strncasecmp(lines[i], "rating:", 7)) {
            gchar **val;
            val = g_strsplit(lines[i], ":", 2);
            ldm_host_info->rating = atoi(val[1]);
            g_strfreev(val);
        } else {
            /* Variable not supported */
        }
    }
    g_strfreev(lines);
}

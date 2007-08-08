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


#ifndef LDMINFO_H
#define LDMINFO_H

#define MAXSTRSZ 255
#define MAXBUFSIZE 4096

/*
 * Info about servers
 */

typedef struct {
	GList *languages;
	GList *sessions;
	int rating;
	int state;
} ldminfo;

/* 
 * state enum
 */

enum {
	SRV_UP,
	SRV_DOWN
};

/* 
 * Init the hash table : key=char hostnames, values=struct *ldminfo
 * ldm_server is the LDM_SERVER variable, a list of hostnames separated by space
 */

void ldminfo_hash_init(GHashTable **lsminfo_hash, const char *ldm_server);

/* update the best_srv_hostname */
void		ldminfo_get_best_server(GHashTable *ldminfo_hash, char **best_host);

void		_ldminfo_compute_best_srv(const char *hostname, ldminfo *ldm_host_info);

void		_populate_sorted_host_list(const char *hostname, ldminfo *ldm_host_info, GList **host_list);

void		ldminfo_get_sorted_host_list(GHashTable *ldminfo_hash, GList **sorted_host_list);

void		print_str_list(const char* elem);

/* Do the query for one host and fill ldminfo struct */
void		_ldminfo_query_one(const char *hostname, ldminfo *ldm_host_info);

/* Do the query for all hosts */
void		_ldminfo_query_all(GHashTable *ldminfo_hash);

/* split string by line and then construct the ldm_host_info */
void 		_ldminfo_parse_string(const char *s, ldminfo *ldm_host_info);

#endif /* LDMINFO_H */


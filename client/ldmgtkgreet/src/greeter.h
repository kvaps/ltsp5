/*
 * LTSP Graphical GTK Greeter
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


#ifndef GREETER_H
#define GREETER_H

#define MAXSTRSZ 255
#define MAXBUFSIZE 4096

/*
 * Info about servers
 */

typedef struct {
    GList *languages;
    GList *sessions;
    gint rating;
    gint state;
} ldminfo;

/* 
 * state enum
 */

enum {
    SRV_UP,
    SRV_DOWN
};

/*
 * hostwin.c
 */

extern gchar host[MAXSTRSZ];
extern gint current_host_id;
extern gint selected_host_id;

void update_selected_host();
void populate_host_combo_box(const char *hostname,
                             GtkWidget * host_combo_box);
void hostwin(GtkWidget * widget, GtkWindow * win);

/*
 * greeter.c
 */

extern GHashTable *ldminfo_hash;
extern GList *host_list;
extern gint current_host_id;
extern gint selected_host_id;

void destroy_popup(GtkWidget * widget, GtkWidget * popup);

/*
 * ldminfo.c
 */

/* 
 * Init the hash table : key=char hostnames, values=struct *ldminfo
 * ldm_server is the LDM_SERVER variable, a list of hostnames separated by space
 */

void ldminfo_init(GHashTable ** lsminfo_hash, GList ** host_list,
                  const char *ldm_server);

/* Do the query for one host and fill ldminfo struct */
void _ldminfo_query_one(const char *hostname, ldminfo * ldm_host_info);

/* split string by line and then construct the ldm_host_info */
void _ldminfo_parse_string(const char *s, ldminfo * ldm_host_info);

/*
 * langwin.c
 */

extern gchar language[MAXSTRSZ];
extern GtkWidget *lang_select;
extern gint lang_total;
extern gint lang_selected;
void update_selected_lang();
void populate_lang_combo_box(const char *lang, GtkWidget * lang_combo_box);
void langwin(GtkWidget * widget, GtkWindow * win);

/*
 * sesswin.c
 */

extern gchar session[MAXSTRSZ];
extern GtkWidget *sess_select;  /* session selection combo */
extern gint sess_total;
extern gint sess_selected;

void update_selected_sess();
void populate_sess_combo_box(const char *sess, GtkWidget * sess_combo_box);
void sesswin(GtkWidget * widget, GtkWindow * win);

#endif                          /* GREETER_H */

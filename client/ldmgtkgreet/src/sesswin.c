/*
 * LTSP Graphical GTK Greeter
 * Copyright (2007) Oliver Grawert <ogra@ubuntu.com>, Canonical Ltd.
 * Code Licensed under GPL v2
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>
#include <glib/gi18n.h>

#include <greeter.h>

gchar session[MAXSTRSZ] = "None";
GtkWidget *sess_select;                     /* session selection combo */
gint sess_total = 0;
gint sess_selected = 0;

/*
 * Local functions
 */

static void
sesswin_accept(GtkWidget *widget, GtkWidget *sesswin)
{
    sess_selected = gtk_combo_box_get_active( GTK_COMBO_BOX(sess_select) );
    gtk_widget_destroy(sesswin);
}

/*
 * External functions
 */

void
populate_sess_combo_box(const char *sess, GtkWidget *sess_combo_box)
{
	gtk_combo_box_append_text(GTK_COMBO_BOX(sess_combo_box),g_strdup(sess));
    sess_total++;
}

/*
 * Session window
 */

void
sesswin(GtkWidget *widget, GtkWindow *win)
{
	GtkWidget *sesswin, *label, *vbox, *buttonbox;
	GtkWidget *cancel, *accept, *frame;
    ldminfo *curr_host = NULL;
	
	sess_select = gtk_combo_box_new_text();

    /* 
     * Populate sess with default host hash
     */

    curr_host = g_hash_table_lookup(ldminfo_hash, 
                                    g_list_nth_data(sorted_host_list,
                                    current_host_id));

    gtk_combo_box_append_text(GTK_COMBO_BOX(sess_select),
                              g_strdup(_("Default")));
    gtk_combo_box_append_text(GTK_COMBO_BOX(sess_select),
                              g_strdup(_("Failsafe xterm")));
    g_list_foreach(curr_host->sessions, 
                   (GFunc)populate_sess_combo_box, sess_select);

    gtk_combo_box_set_active(GTK_COMBO_BOX(sess_select), sess_selected);

    /*
     * Build window
     */

	sesswin = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_position((GtkWindow *) sesswin, GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_modal((GtkWindow *) sesswin, TRUE);
	
	vbox = gtk_vbox_new(FALSE, 0);
	buttonbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

	cancel = gtk_button_new_from_stock("gtk-cancel");
	g_signal_connect (G_OBJECT (cancel), "clicked",
			G_CALLBACK (destroy_popup),
			sesswin);

	accept = gtk_button_new_with_mnemonic(_("Change _Session"));
    g_signal_connect (G_OBJECT (accept), "clicked",
            G_CALLBACK (sesswin_accept),
            sesswin);

	gtk_box_pack_end((GtkBox *)buttonbox, (GtkWidget *)accept, FALSE, FALSE, 0);
	gtk_box_pack_end((GtkBox *)buttonbox, (GtkWidget *)cancel, FALSE, FALSE, 0);

	label = gtk_label_new("");
	gtk_label_set_markup((GtkLabel *) label, 
			 _("Select your session manager:"));

	gtk_box_pack_start((GtkBox *)vbox, (GtkWidget *)label, FALSE, FALSE, 0);
	gtk_box_pack_start((GtkBox *)vbox, (GtkWidget *)sess_select, FALSE, FALSE, 5);
	gtk_box_pack_start((GtkBox *)vbox, (GtkWidget *)buttonbox, TRUE, TRUE, 5);

	frame = gtk_frame_new("");
	gtk_frame_set_shadow_type((GtkFrame *)frame, GTK_SHADOW_OUT);
	gtk_frame_set_label_align((GtkFrame *)frame, 1.0, 0.0);

	gtk_container_add (GTK_CONTAINER(frame), vbox);
	gtk_container_add (GTK_CONTAINER(sesswin), frame);

	gtk_widget_show_all(sesswin);

	return;
}

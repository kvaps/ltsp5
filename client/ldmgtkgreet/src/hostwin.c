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

gchar host[MAXSTRSZ] = "None";
gint current_host_id = 0;
gint selected_host_id = 0;

/*
 * Local functions
 */

static void
hostwin_update_host(GtkWidget *widget, gpointer data)
{
	current_host_id = gtk_combo_box_get_active( GTK_COMBO_BOX(widget) );
}

static void
hostwin_accept(GtkWidget *widget, GtkWidget *hostwin)
{
	selected_host_id = current_host_id;
	gtk_widget_destroy(hostwin);

    /*
     * We selected a new host, so we have to update the lang and session
     * windows.
     */

    lang_selected = 0;
    sess_selected = 0;
}

/*
 * External functions
 */

void
update_selected_host()
{
    g_strlcpy(host, g_list_nth_data(sorted_host_list, selected_host_id),
              MAXSTRSZ);
}

void
populate_host_combo_box(const char *hostname, GtkWidget *host_combo_box)
{
	gtk_combo_box_append_text(GTK_COMBO_BOX(host_combo_box),g_strdup(hostname));
}

void
hostwin(GtkWidget *widget, GtkWindow *win)
{
	GtkWidget *hostwin, *label, *vbox, *buttonbox, *combo_host;
	GtkWidget *cancel, *accept, *frame;
	GtkListStore *list_store;
	GtkTreeIter iter;
	
	hostwin = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_position((GtkWindow *) hostwin, GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_modal((GtkWindow *) hostwin, TRUE);
	
	vbox = gtk_vbox_new(FALSE, 0);
	buttonbox = gtk_hbox_new(FALSE, 5);

	combo_host = gtk_combo_box_new_text();
	g_list_foreach(sorted_host_list, (GFunc)populate_host_combo_box, combo_host);
	
	g_signal_connect (G_OBJECT (combo_host), "changed",
		G_CALLBACK (hostwin_update_host),
		NULL);

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo_host), selected_host_id);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	cancel = gtk_button_new_from_stock("gtk-cancel");
	g_signal_connect (G_OBJECT (cancel), "clicked",
			G_CALLBACK (destroy_popup),
			hostwin);

	accept =  gtk_button_new_from_stock("gtk-apply");
	g_signal_connect (G_OBJECT (accept), "clicked",
			G_CALLBACK (hostwin_accept),
			hostwin);

	gtk_box_pack_end((GtkBox *)buttonbox, (GtkWidget *)accept, FALSE, FALSE, 0);
	gtk_box_pack_end((GtkBox *)buttonbox, (GtkWidget *)cancel, FALSE, FALSE, 0);

	label = gtk_label_new("");
	gtk_label_set_markup((GtkLabel *) label, 
			 _("Select the host for your session to use:"));

	gtk_box_pack_start((GtkBox *)vbox, (GtkWidget *)label, FALSE, FALSE, 0);
	gtk_box_pack_start((GtkBox *)vbox, (GtkWidget *)combo_host, FALSE, FALSE, 5);
	gtk_box_pack_start((GtkBox *)vbox, (GtkWidget *)buttonbox, TRUE, TRUE, 5);

	frame = gtk_frame_new("");
	gtk_frame_set_shadow_type((GtkFrame *) frame, GTK_SHADOW_OUT);
	gtk_frame_set_label_align((GtkFrame *) frame, 1.0, 0.0);

	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_container_add (GTK_CONTAINER (hostwin), frame);

	gtk_widget_show_all(hostwin);
}

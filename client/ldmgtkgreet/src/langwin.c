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
#include <glib/gi18n.h>

#include <greeter.h>

gchar language[MAXSTRSZ] = "None";

#define EXPAND TRUE
#define DONTEXPAND FALSE
#define FILL TRUE
#define DONTFILL FALSE

GtkWidget *lang_select;                     /* language selection combo */
gint lang_total = 0;
gint lang_selected = 0;

/*
 * Local functions
 */

static void
langwin_accept(GtkWidget *widget, GtkWidget *langwin)
{
    lang_selected = gtk_combo_box_get_active( GTK_COMBO_BOX(lang_select) );
    gtk_widget_destroy(langwin);
}

/*
 * External functions
 */

void
populate_lang_combo_box(const char *lang, GtkWidget *lang_combo_box)
{
	gtk_combo_box_append_text(GTK_COMBO_BOX(lang_combo_box),g_strdup(lang));
    lang_total++;
}

void
langwin(GtkWidget *widget, GtkWindow *win)
{
	GtkWidget *langwin, *label, *vbox, *buttonbox;
	GtkWidget *cancel, *accept, *frame;
    ldminfo *curr_host = NULL;
	
	lang_select = gtk_combo_box_new_text();

    /* 
     * Populate lang with default host hash
     */

    curr_host = g_hash_table_lookup(ldminfo_hash, 
                                    g_list_nth_data(sorted_host_list,
                                    current_host_id));

    gtk_combo_box_append_text(GTK_COMBO_BOX(lang_select),
                              g_strdup(_("Default")));
    g_list_foreach(curr_host->languages, 
                   (GFunc)populate_lang_combo_box, lang_select);

    gtk_combo_box_set_active(GTK_COMBO_BOX(lang_select), lang_selected);

    /*
     * Build window
     */

	langwin = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_position((GtkWindow *) langwin, GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_modal((GtkWindow *) langwin, TRUE);
	
	vbox = gtk_vbox_new(FALSE, 0);
	buttonbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	cancel = gtk_button_new_from_stock("gtk-cancel");
	g_signal_connect (G_OBJECT (cancel), "clicked",
			G_CALLBACK (destroy_popup),
			langwin);

	accept = gtk_button_new_with_mnemonic(_("Change _Language"));
	g_signal_connect (G_OBJECT (accept), "clicked",
			G_CALLBACK (langwin_accept),
			langwin);

	gtk_box_pack_end((GtkBox *)buttonbox, (GtkWidget *)accept, FALSE, FALSE, 0);
	gtk_box_pack_end((GtkBox *)buttonbox, (GtkWidget *)cancel, FALSE, FALSE, 0);

	label = gtk_label_new("");
	gtk_label_set_markup((GtkLabel *) label, 
			 _("Select the language for your session to use:"));

	gtk_box_pack_start((GtkBox *)vbox, (GtkWidget *)label, FALSE, FALSE, 0);
	gtk_box_pack_start((GtkBox *)vbox, (GtkWidget *)lang_select, FALSE, FALSE, 5);
	gtk_box_pack_start((GtkBox *)vbox, (GtkWidget *)buttonbox, TRUE, TRUE, 5);

	frame = gtk_frame_new("");
	gtk_frame_set_shadow_type((GtkFrame *)frame, GTK_SHADOW_OUT);
	gtk_frame_set_label_align((GtkFrame *)frame, 1.0, 0.0);

	gtk_container_add (GTK_CONTAINER(frame), vbox);
	gtk_container_add (GTK_CONTAINER(langwin), frame);

	gtk_widget_show_all(langwin);

	return;
}

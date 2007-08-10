/*
LTSP Graphical GTK Greeter
Copyright (2007) Oliver Grawert <ogra@ubuntu.com>, Canonical Ltd.
Code Licensed under GPL v2

TODO:
fill lang popup window (with translated langs)
fill session list
configfile
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <net/if.h> 
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include <ldminfo.h>

char user[MAXSTRSZ];
char pass[MAXSTRSZ];
char language[MAXSTRSZ] = "None";
char session[MAXSTRSZ] = "None";
char host[MAXSTRSZ] = "None";

#define EXPAND TRUE
#define DONTEXPAND FALSE
#define FILL TRUE
#define DONTFILL FALSE


GtkWidget *UserPrompt;                      /* propmt area before the entry */
GtkWidget *StatusMessages;                  /* Status msg area below entry */
GtkWidget *entry;                           /* entry box */
GtkWidget *lang_select;                     /* language selection combo */
gint lang_total = 0;

GHashTable *ldminfo_hash = NULL;
GList *sorted_host_list = NULL;
gint current_host_id = 0;
gint selected_host_id = 0;

static void
destroy(GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
}

static void
halt(GtkWidget *widget, gpointer data)
{
	GError **error;
	g_spawn_command_line_async("/sbin/poweroff -fp", error);
}

static void
reboot(GtkWidget *widget, gpointer data)
{
	GError **error;
	g_spawn_command_line_async("/sbin/reboot -fp", error);
}

static void
populate_lang_combo_box(const char *lang, GtkWidget *lang_combo_box)
{
	gtk_combo_box_append_text(GTK_COMBO_BOX(lang_combo_box),g_strdup(lang));
    lang_total++;
}

gboolean
update_time(GtkWidget *label)
{	
	const char *timestring = 0;
	time_t timet;
	struct tm *timePtr;

	timet = time( NULL );
	timePtr = localtime( &timet );

	timestring = g_strdup_printf("%.2d.%.2d, %.2d:%.2d", timePtr->tm_mday, 
			timePtr->tm_mon+1, timePtr->tm_hour, timePtr->tm_min);

	gtk_label_set_markup((GtkLabel *) label, timestring);

	return TRUE;
}

static void
destroy_popup(GtkWidget *widget, GtkWidget *langwin)
{
	gtk_widget_destroy(langwin);
	return;
}

static void
hostwin_update_host(GtkWidget *widget, gpointer data)
{
	current_host_id = gtk_combo_box_get_active( GTK_COMBO_BOX(widget) );
	return;
}

static void
hostwin_accept(GtkWidget *widget, GtkWidget *hostwin)
{
    ldminfo *curr_host = NULL;
    gint i;

	selected_host_id = current_host_id;
	gtk_widget_destroy(hostwin);

    /*
     * We selected a new host, so we have to update the lang and session
     * windows.
     */

    for (i = 0; i < lang_total; i++)
        gtk_combo_box_remove_text (GTK_COMBO_BOX(lang_select), 1);

    lang_total = 0;
    curr_host = g_hash_table_lookup(ldminfo_hash, 
                                    g_list_nth_data(sorted_host_list,
                                    current_host_id));

    g_list_foreach(curr_host->languages, 
                   (GFunc)populate_lang_combo_box, lang_select);

	return;
}

static void
hostwin_cancel(GtkWidget *widget, GtkWidget *hostwin)
{
	gtk_widget_destroy(hostwin);
	return;
}

void update_selected_host()
{
    g_strlcpy(host, g_list_nth_data(sorted_host_list, selected_host_id),
              MAXSTRSZ);
}

gboolean
handle_command(GIOChannel *io_input)
{
    GString *buf;

    buf = g_string_new("");

    g_io_channel_read_line_string(io_input, buf, NULL, NULL);

    if (!g_strncasecmp(buf->str, "msg", 3)) {
        gchar **split_buf;
        split_buf = g_strsplit (buf->str, " ", 2);
        gtk_label_set_markup((GtkLabel *) StatusMessages, split_buf[1]);
        g_strfreev (split_buf);
    } else if (!g_strncasecmp(buf->str, "quit", 4)) {
	    GdkCursor *cursor;
	    cursor = gdk_cursor_new(GDK_WATCH);
	    gdk_window_set_cursor(gdk_get_default_root_window(), cursor);
	    gtk_main_quit ();
    } else if (!g_strncasecmp(buf->str, "prompt", 6)) {
        gchar **split_buf;
        split_buf = g_strsplit (buf->str, " ", 2);
        gtk_label_set_markup((GtkLabel *) UserPrompt, split_buf[1]);
        g_strfreev (split_buf);
    } else if (!g_strncasecmp(buf->str, "userid", 6)) {
        gtk_entry_set_visibility(GTK_ENTRY(entry), TRUE);
    } else if (!g_strncasecmp(buf->str, "passwd", 6)) {
        gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
    } else if (!g_strncasecmp(buf->str, "hostname", 8)) {
    	update_selected_host();
        printf("%s\n", host);
    }

    g_string_free(buf, TRUE);
    return TRUE;
}

/*
 * Session window
 */

static void
sesswin(GtkWidget *widget, GtkWindow *win)
{
	GtkWidget *sesswin, *s_vbox, *s_buttonbox;
	GtkWidget *s_cancel, *s_accept, *s_frame, *s_vbox2;
	GtkWidget *radio_button1;
	GtkWidget *radio_button2;
	/*GtkWidget *radio_button3;*/

	sesswin = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_position((GtkWindow *) sesswin, GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_modal((GtkWindow *) sesswin, TRUE);

	s_vbox = gtk_vbox_new(FALSE, 0);
	s_vbox2 = gtk_vbox_new(FALSE, 0);
	s_buttonbox = gtk_hbox_new(FALSE, 5);

	radio_button1 = gtk_radio_button_new_with_mnemonic( NULL, _("_1. Default") );
	radio_button2 = gtk_radio_button_new_with_mnemonic( gtk_radio_button_group( GTK_RADIO_BUTTON( radio_button1 )), 
				_("_2. Terminal (failsafe)") );
	/*radio_button3 = gtk_radio_button_new_with_mnemonic(
			gtk_radio_button_group( GTK_RADIO_BUTTON( radio_button1 )),
				_("_3. GNOME") );*/

	gtk_box_pack_start(GTK_BOX(s_vbox2), GTK_WIDGET(radio_button1),
                       FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(s_vbox2), GTK_WIDGET(radio_button2),
                       FALSE, FALSE, 0);
	/*gtk_box_pack_start(GTK_BOX(s_vbox2), GTK_WIDGET(radio_button3),
                         FALSE, FALSE, 0);*/

	gtk_container_set_border_width (GTK_CONTAINER (s_vbox2), 15);
	gtk_container_set_border_width (GTK_CONTAINER (s_vbox), 5);

	s_cancel = gtk_button_new_from_stock("gtk-cancel");
	g_signal_connect (G_OBJECT(s_cancel), "clicked", G_CALLBACK(destroy_popup),
			          sesswin);

	s_accept = gtk_button_new_with_mnemonic(_("Change _Session"));

	gtk_box_pack_end(GTK_BOX(s_buttonbox), GTK_WIDGET(s_accept),
                     FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(s_buttonbox), GTK_WIDGET(s_cancel),
                     FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(s_vbox), GTK_WIDGET(s_vbox2), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(s_vbox), GTK_WIDGET(s_buttonbox), TRUE, TRUE, 5);

	s_frame = gtk_frame_new("");
	gtk_label_set_markup(GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(s_frame))),
			_("<b>Sessions</b>"));
	gtk_frame_set_shadow_type(GTK_FRAME(s_frame), GTK_SHADOW_OUT);
	gtk_frame_set_label_align(GTK_FRAME(s_frame), 0.0, 0.0);

	gtk_container_add (GTK_CONTAINER (s_frame), s_vbox);
	gtk_container_add (GTK_CONTAINER (sesswin), s_frame);

	gtk_widget_show_all(sesswin);
	return;

}


static void
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

    gtk_combo_box_append_text(GTK_COMBO_BOX(lang_select), g_strdup(_("Default")));
    g_list_foreach(curr_host->languages, 
                   (GFunc)populate_lang_combo_box, lang_select);

    gtk_combo_box_set_active(GTK_COMBO_BOX(lang_select), 0);

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

	gtk_box_pack_end((GtkBox *) buttonbox, (GtkWidget *) accept, FALSE, FALSE, 0);
	gtk_box_pack_end((GtkBox *) buttonbox, (GtkWidget *) cancel, FALSE, FALSE, 0);

	label = gtk_label_new("");
	gtk_label_set_markup((GtkLabel *) label, 
			 _("Select the language for your session to use:"));

	gtk_box_pack_start((GtkBox *) vbox, (GtkWidget *) label, FALSE, FALSE, 0);
	gtk_box_pack_start((GtkBox *) vbox, (GtkWidget *) lang_select, FALSE, FALSE, 5);
	gtk_box_pack_start((GtkBox *) vbox, (GtkWidget *) buttonbox, TRUE, TRUE, 5);

	frame = gtk_frame_new("");
	gtk_frame_set_shadow_type((GtkFrame *) frame, GTK_SHADOW_OUT);
	gtk_frame_set_label_align((GtkFrame *) frame, 1.0, 0.0);

	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_container_add (GTK_CONTAINER (langwin), frame);

	gtk_widget_show_all(langwin);

	return;
}

static void
populate_host_combo_box(const char *hostname, GtkWidget *host_combo_box)
{
	gtk_combo_box_append_text(GTK_COMBO_BOX(host_combo_box),g_strdup(hostname));
}

static void
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


	/* I don't get the right behavior with a list store, the text never appears
	list_store = gtk_list_store_new(1, G_TYPE_STRING);
	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, "testing1", -1);
	combo_host = gtk_combo_box_new_with_model(GTK_TREE_MODEL(list_store));
	*/
	
	combo_host = gtk_combo_box_new_text();
	g_list_foreach(sorted_host_list, (GFunc)populate_host_combo_box, combo_host);
	
	g_signal_connect (G_OBJECT (combo_host), "changed",
		G_CALLBACK (hostwin_update_host),
		NULL);

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo_host), selected_host_id);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	cancel = gtk_button_new_from_stock("gtk-cancel");
	g_signal_connect (G_OBJECT (cancel), "clicked",
			G_CALLBACK (hostwin_cancel),
			hostwin);

	accept =  gtk_button_new_from_stock("gtk-apply");
	g_signal_connect (G_OBJECT (accept), "clicked",
			G_CALLBACK (hostwin_accept),
			hostwin);

	gtk_box_pack_end((GtkBox *) buttonbox, (GtkWidget *) accept, FALSE, FALSE, 0);
	gtk_box_pack_end((GtkBox *) buttonbox, (GtkWidget *) cancel, FALSE, FALSE, 0);

	label = gtk_label_new("");
	gtk_label_set_markup((GtkLabel *) label, 
			 _("Select the host for your session to use:"));

	gtk_box_pack_start((GtkBox *) vbox, (GtkWidget *) label, FALSE, FALSE, 0);
	gtk_box_pack_start((GtkBox *) vbox, (GtkWidget *) combo_host, FALSE, FALSE, 5);
	gtk_box_pack_start((GtkBox *) vbox, (GtkWidget *) buttonbox, TRUE, TRUE, 5);

	frame = gtk_frame_new("");
	gtk_frame_set_shadow_type((GtkFrame *) frame, GTK_SHADOW_OUT);
	gtk_frame_set_label_align((GtkFrame *) frame, 1.0, 0.0);

	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_container_add (GTK_CONTAINER (hostwin), frame);

	gtk_widget_show_all(hostwin);
	
	return;
}

char *
get_sysname(void)
{
	struct utsname name;
	char *node;

	if (uname(&name) == 0){
		node = strdup(name.nodename);
		return node;
	}
	return NULL;
}

char *
get_ip(void)
{
	struct ifreq ifa; 
	struct sockaddr_in *i;
	int fd;

	strcpy (ifa.ifr_name, "eth0");

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ioctl(fd, SIOCGIFADDR, &ifa);
	i = (struct sockaddr_in*)&ifa.ifr_addr;
	return inet_ntoa(i->sin_addr);
}

static void
handle_entry(GtkEntry *entry, GdkWindow *window)
{
	printf("%s\n", gtk_entry_get_text(entry));
    gtk_entry_set_text(entry, "");
}

static void
popup_menu(GtkWidget *widget, GtkWindow *window)
{
	GtkWidget *menu, *lang_item, *sess_item, *host_item, *quit_item,
              *reboot_item;
	GtkWidget *sep, *langico, *sessico, *hostico, *rebootico, *haltico;

	menu = gtk_menu_new (); 

	lang_item = gtk_image_menu_item_new_with_mnemonic(_("_Select language ..."));
	langico = gtk_image_new_from_file (PIXMAPS_DIR "/language.png");
	gtk_image_menu_item_set_image((GtkImageMenuItem *) lang_item, langico);

	sess_item = gtk_image_menu_item_new_with_mnemonic(_("_Select session ..."));
	sessico = gtk_image_new_from_file (PIXMAPS_DIR "/session.png");
	gtk_image_menu_item_set_image((GtkImageMenuItem *) sess_item, sessico);

	host_item = gtk_image_menu_item_new_with_mnemonic(_("_Select host ..."));
	hostico = gtk_image_new_from_file (PIXMAPS_DIR "/host.png");
	gtk_image_menu_item_set_image((GtkImageMenuItem *) host_item, hostico);

	sep = gtk_separator_menu_item_new();
	reboot_item =  gtk_image_menu_item_new_with_mnemonic (_("_Reboot"));
	rebootico = gtk_image_new_from_file (PIXMAPS_DIR "/reboot.png");
	gtk_image_menu_item_set_image((GtkImageMenuItem *) reboot_item, rebootico);
	quit_item = gtk_image_menu_item_new_with_mnemonic (_("_Shutdown"));
	haltico = gtk_image_new_from_file (PIXMAPS_DIR "/shutdown.png");
	gtk_image_menu_item_set_image((GtkImageMenuItem *) quit_item, haltico);
	
	g_signal_connect_swapped (G_OBJECT (quit_item), "activate",
			G_CALLBACK (halt), NULL);
	g_signal_connect_swapped (G_OBJECT (reboot_item), "activate",
			G_CALLBACK (reboot), NULL);
	g_signal_connect_swapped (G_OBJECT (lang_item), "activate",
			G_CALLBACK (langwin), window);
	g_signal_connect_swapped (G_OBJECT (sess_item), "activate",
			G_CALLBACK (sesswin), window);
	g_signal_connect_swapped (G_OBJECT (host_item), "activate",
			G_CALLBACK (hostwin), window);


	gtk_menu_shell_append (GTK_MENU_SHELL (menu), lang_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), sess_item);
    if (g_hash_table_size(ldminfo_hash) > 1)
	    gtk_menu_shell_append (GTK_MENU_SHELL (menu), host_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), sep);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), reboot_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), quit_item);

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time());

	gtk_widget_show_all(menu);
	gtk_menu_reposition(GTK_MENU (menu));

	return;	
}

int
main(int argc, char *argv[])
{
	int pw, ph, spheight, w, h;
	const char *hoststring = 0;

	gint lw, lh;

	GdkCursor *normcursor, *busycursor;
	GtkWidget *window, *syslabel, *logo, *EntryBox, *timelabel;
	GtkWidget *StatusBarBox, *spacer, *vbox, *vbox2, *hbox;
	GtkButton *optionbutton, *cancelbutton;
	GdkWindow *root;
	GdkPixbuf *rawpic, *pix;
	GdkPixmap *pic;
	GdkBitmap *mask;
	gint width, height;
    GIOChannel *g_stdin;

	gtk_init (&argc, &argv);

	gtk_rc_add_default_file (GTKRC_DIR "/greeter-gtkrc");

	/* Initialize information about hosts */
	ldminfo_hash_init(&ldminfo_hash, getenv("LDM_SERVER"));
	_ldminfo_query_all(ldminfo_hash);
	ldminfo_get_sorted_host_list(ldminfo_hash, &sorted_host_list);

	normcursor = gdk_cursor_new(GDK_LEFT_PTR);
	busycursor = gdk_cursor_new(GDK_WATCH);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK(destroy), NULL);

	root = gdk_get_default_root_window();

	gdk_window_set_cursor(root, busycursor);
	gdk_drawable_get_size(root, &width, &height);

	pw = width;
	ph = height;
	w = width;
	h = height;

	logo = gtk_image_new_from_file(PIXMAPS_DIR "/logo.png");

	pix = gtk_image_get_pixbuf ((GtkImage *) logo);
	lw = gdk_pixbuf_get_width (pix);
	lh = gdk_pixbuf_get_height (pix);

	spheight = (height / 4) - (lh / 2);

	rawpic = gdk_pixbuf_new_from_file_at_scale(PIXMAPS_DIR "/bg.png", 
			pw, ph, FALSE, NULL);
	gdk_pixbuf_render_pixmap_and_mask(rawpic, &pic, &mask, 0);
	gdk_pixbuf_unref(rawpic);

	gtk_widget_set_app_paintable(window, TRUE);
	gtk_widget_set_size_request(window, w, h);
	gtk_widget_realize(window);
	gdk_window_set_back_pixmap((GdkWindow *) window->window, pic, 0);
	gtk_window_set_decorated((GtkWindow *) window, FALSE);

	vbox = gtk_vbox_new(FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0); 
	EntryBox = gtk_hbox_new(FALSE, 5);
	hbox = gtk_hbox_new(FALSE, 0);

	StatusBarBox = gtk_hbox_new(FALSE, 0);

	optionbutton = (GtkButton *) gtk_button_new_from_stock("gtk-preferences");
	gtk_button_set_relief(optionbutton, GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click((GtkButton *) optionbutton, FALSE);
	
	g_signal_connect (G_OBJECT(optionbutton), "clicked",
			G_CALLBACK(popup_menu), window);

    /*
     * Cancel button 
     */

	cancelbutton = (GtkButton *) gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_button_set_relief(cancelbutton, GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click((GtkButton *) cancelbutton, FALSE);

	g_signal_connect (G_OBJECT(cancelbutton), "clicked",
			G_CALLBACK(destroy), window);

	syslabel = gtk_label_new("");
	timelabel = gtk_label_new("");
	hoststring = g_strdup_printf("<b>%s (%s) //</b>", get_sysname(), get_ip());
	gtk_label_set_markup((GtkLabel *) syslabel, hoststring);
	update_time(timelabel);

	g_timeout_add(30000, (GSourceFunc)update_time, timelabel);

	gtk_box_pack_start(GTK_BOX(StatusBarBox), 
                       GTK_WIDGET(optionbutton), FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(StatusBarBox), 
                       GTK_WIDGET(cancelbutton), FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(StatusBarBox), 
                     GTK_WIDGET(timelabel), FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(StatusBarBox), 
                     GTK_WIDGET(syslabel), FALSE, FALSE, 0);

	UserPrompt = gtk_label_new("");
	spacer = gtk_label_new("");

	if (lw < 180)
		lw=180;

	gtk_misc_set_alignment((GtkMisc *) UserPrompt, 1, 0.5);
	gtk_widget_set_size_request(UserPrompt, (lw / 2), 0);

    StatusMessages = gtk_label_new("");
	entry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 12);
	g_signal_connect(G_OBJECT(entry), "activate", 
			G_CALLBACK(handle_entry), root);

	gtk_box_pack_start(GTK_BOX(EntryBox), UserPrompt, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(EntryBox), entry, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), spacer, FALSE, FALSE, spheight);
	gtk_box_pack_start(GTK_BOX(vbox), logo, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), EntryBox, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), StatusMessages, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox2), StatusBarBox, FALSE, FALSE, 5);

	gtk_container_add (GTK_CONTAINER (window), vbox2);

	
	gtk_widget_show_all(window);

	gdk_window_set_cursor(root, normcursor);
	
    /*
     * Start listening to stdin
     */

    g_stdin = g_io_channel_unix_new(STDIN_FILENO);        /* listen to stdin */
    g_io_add_watch(g_stdin, G_IO_IN, (GIOFunc)handle_command, g_stdin);
    setbuf(stdout, NULL);

	gtk_main ();

	return 0;
}

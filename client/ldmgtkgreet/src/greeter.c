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
#include <sys/utsname.h>
#include <sys/socket.h>
#include <net/if.h> 
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include <greeter.h>

char user[MAXSTRSZ];
char pass[MAXSTRSZ];

GtkWidget *UserPrompt;                      /* propmt area before the entry */
GtkWidget *StatusMessages;                  /* Status msg area below entry */
GtkWidget *entry;                           /* entry box */

GHashTable *ldminfo_hash = NULL;
GList *sorted_host_list = NULL;
GIOChannel *g_stdout;                       /* stdout io channel */

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

gboolean
update_time(GtkWidget *label)
{	
	gchar *timestring = 0;
	time_t timet;
	struct tm *timePtr;

	timet = time( NULL );
	timePtr = localtime( &timet );

	timestring = g_strdup_printf("%.2d.%.2d, %.2d:%.2d", timePtr->tm_mday, 
			timePtr->tm_mon+1, timePtr->tm_hour, timePtr->tm_min);

	gtk_label_set_markup((GtkLabel *) label, timestring);

    g_free(timestring);

	return TRUE;
}

void
destroy_popup(GtkWidget *widget, GtkWidget *popup)
{
	gtk_widget_destroy(popup);
	return;
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
        gchar *hoststr;
    	update_selected_host();
	    hoststr = g_strdup_printf("%s\n", host);
        g_io_channel_write_chars(g_stdout, hoststr, -1, NULL, NULL);
        g_io_channel_flush(g_stdout, NULL);
        g_free(hoststr);
    } else if (!g_strncasecmp(buf->str, "language", 8)) {
        gchar *langstr;
    	update_selected_lang();
	    langstr = g_strdup_printf("%s\n", language);
        g_io_channel_write_chars(g_stdout, langstr, -1, NULL, NULL);
        g_io_channel_flush(g_stdout, NULL);
        g_free(langstr);
    } else if (!g_strncasecmp(buf->str, "session", 7)) {
        gchar *sessstr;
    	update_selected_sess();
	    sessstr = g_strdup_printf("%s\n", session);
        g_io_channel_write_chars(g_stdout, sessstr, -1, NULL, NULL);
        g_io_channel_flush(g_stdout, NULL);
        g_free(sessstr);
    }

    g_string_free(buf, TRUE);
    return TRUE;
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
    gchar *entrystr;
    
	entrystr = g_strdup_printf("%s\n", gtk_entry_get_text(entry));
    g_io_channel_write_chars(g_stdout, entrystr, -1, NULL, NULL);
    g_io_channel_flush(g_stdout, NULL);
    g_free(entrystr);
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
    g_stdout = g_io_channel_unix_new(STDOUT_FILENO); 
    g_io_add_watch(g_stdin, G_IO_IN, (GIOFunc)handle_command, g_stdin);

	gtk_main ();

	return 0;
}

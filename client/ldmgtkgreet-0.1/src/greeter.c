/*
LTSP Graphical GTK Greeter
Copyright (2007) Oliver Grawert <ogra@ubuntu.com>, Canonical Ltd.
Code Licensed under GPL v2

TODO:
fill lang popup window (with translated langs)
fill session list
configfile
add "start with error" function and error dialogs if LDM_ERR is set
*/

#define _GNU_SOURCE

#include <stdio.h>
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

char user[255];
char pass[255];
char language[255] = "None";
char session[255] = "None";

GtkWidget *label;

static void destroy(GtkWidget *widget,
		gpointer data)
{
	gtk_main_quit ();
}

static void halt(GtkWidget *widget,
		gpointer data)
{
	GError **error;
	g_spawn_command_line_async("/sbin/poweroff -fp", error);
}

static void reboot(GtkWidget *widget,
		gpointer data)
{
	GError **error;
	g_spawn_command_line_async("/sbin/reboot -fp", error);
}

gboolean update_time(GtkWidget *label)
{	
	const char *timestring = 0;
	time_t timet;
	struct tm *timePtr;

	timet = time( NULL );
	timePtr = localtime( &timet );

	timestring = g_strdup_printf("%.2d.%.2d, %.2d:%.2d", timePtr->tm_mday, \
			timePtr->tm_mon+1, timePtr->tm_hour, timePtr->tm_min);

	gtk_label_set_markup((GtkLabel *) label, timestring);

	return TRUE;
}

static void destroy_popup(GtkWidget *widget,
		GtkWidget *langwin)
{
	gtk_widget_destroy(langwin);
	return;
}

static void sesswin(GtkWidget *widget,
		GtkWindow *win)
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

	gtk_box_pack_start((GtkBox *) s_vbox2, (GtkWidget *) radio_button1, FALSE, FALSE, 0);
	gtk_box_pack_start((GtkBox *) s_vbox2, (GtkWidget *) radio_button2, FALSE, FALSE, 0);
	/*gtk_box_pack_start((GtkBox *) s_vbox2, (GtkWidget *) radio_button3, FALSE, FALSE, 0);*/

	gtk_container_set_border_width (GTK_CONTAINER (s_vbox2), 15);
	gtk_container_set_border_width (GTK_CONTAINER (s_vbox), 5);

	s_cancel = gtk_button_new_from_stock("gtk-cancel");
	g_signal_connect (G_OBJECT (s_cancel), "clicked",
			G_CALLBACK (destroy_popup),
			sesswin);

	s_accept = gtk_button_new_with_mnemonic(_("Change _Session"));

	gtk_box_pack_end((GtkBox *) s_buttonbox, (GtkWidget *) s_accept, FALSE, FALSE, 0);
	gtk_box_pack_end((GtkBox *) s_buttonbox, (GtkWidget *) s_cancel, FALSE, FALSE, 0);

	gtk_box_pack_start((GtkBox *) s_vbox, (GtkWidget *) s_vbox2, FALSE, FALSE, 0);
	gtk_box_pack_start((GtkBox *) s_vbox, (GtkWidget *) s_buttonbox, TRUE, TRUE, 5);

	s_frame = gtk_frame_new("");
	gtk_label_set_markup((GtkLabel *) gtk_frame_get_label_widget((GtkFrame *) s_frame),
			_("<b>Sessions</b>"));
	gtk_frame_set_shadow_type((GtkFrame *) s_frame, GTK_SHADOW_OUT);
	gtk_frame_set_label_align((GtkFrame *) s_frame, 0.0, 0.0);

	gtk_container_add (GTK_CONTAINER (s_frame), s_vbox);
	gtk_container_add (GTK_CONTAINER (sesswin), s_frame);

	gtk_widget_show_all(sesswin);
	return;

}

static void langwin(GtkWidget *widget,
		GtkWindow *win)
{
	GtkWidget *langwin, *label, *vbox, *buttonbox, *select;
	GtkWidget *cancel, *accept, *frame;
	
	langwin = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_position((GtkWindow *) langwin, GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_modal((GtkWindow *) langwin, TRUE);
	
	vbox = gtk_vbox_new(FALSE, 0);
	buttonbox = gtk_hbox_new(FALSE, 5);
	select = gtk_combo_box_new();

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
	gtk_box_pack_start((GtkBox *) vbox, (GtkWidget *) select, FALSE, FALSE, 5);
	gtk_box_pack_start((GtkBox *) vbox, (GtkWidget *) buttonbox, TRUE, TRUE, 5);

	frame = gtk_frame_new("");
	gtk_frame_set_shadow_type((GtkFrame *) frame, GTK_SHADOW_OUT);
	gtk_frame_set_label_align((GtkFrame *) frame, 1.0, 0.0);

	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_container_add (GTK_CONTAINER (langwin), frame);

	gtk_widget_show_all(langwin);

	return;
}

char* get_sysname(void)
{
	struct utsname name;
	char *node;

	if (uname(&name) == 0){
		node = strdup(name.nodename);
		return node;
	}
	return NULL;
}

char* get_ip(void)
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

static void switch_entry(GtkEntry *entry,
		GdkWindow *window)
{
	char text[255];
	GdkCursor *cursor;

	cursor = gdk_cursor_new(GDK_WATCH);

	strncpy(text, gtk_entry_get_text(entry), sizeof(text));
	if (strlen(text) < 1 ) {
		return;
	}
	if (strlen(user) > 0 ) {
		gdk_window_set_cursor(window, cursor);
		printf("%s\n%s\n%s\n%s\n", user, text, language, session);
		gtk_main_quit ();
	}
	strncpy(user, text, sizeof(user));
	gtk_entry_set_text(entry, "");
	gtk_entry_set_visibility(entry, FALSE);
	gtk_label_set_markup((GtkLabel *) label, \
			(_("<b>Password:</b>")));
}

static void popup_menu(GtkWidget *widget,
		GtkWindow *window)
{
	GtkWidget *menu, *lang_item, *sess_item, *quit_item, *reboot_item;
	GtkWidget *sep, *langico, *sessico, *rebootico, *haltico;

	menu = gtk_menu_new (); 

	lang_item = gtk_image_menu_item_new_with_mnemonic (_("_Select language ..."));
	langico = gtk_image_new_from_file (PIXMAPS_DIR "/language.png");
	gtk_image_menu_item_set_image((GtkImageMenuItem *) lang_item, langico);
	sess_item = gtk_image_menu_item_new_with_mnemonic (_("_Select session ..."));
	sessico = gtk_image_new_from_file (PIXMAPS_DIR "/session.png");
	gtk_image_menu_item_set_image((GtkImageMenuItem *) sess_item, sessico);
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


	gtk_menu_shell_append (GTK_MENU_SHELL (menu), lang_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), sess_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), sep);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), reboot_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), quit_item);

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time());

	gtk_widget_show_all(menu);
	gtk_menu_reposition(GTK_MENU (menu));

	return;	
}

int main( int argc,
		char *argv[] )
{
	int pw, ph, spheight, w, h;
	const char *hoststring = 0;

	gint lw, lh;

	GdkCursor *normcursor, *busycursor;
	GtkWidget *window, *entry, *syslabel, *logo, *hbox2, *timelabel;
	GtkWidget *bottombox, *spacer, *vbox, *vbox2, *hbox;
	GtkButton *optionbutton;
	GdkWindow *root;
	GdkPixbuf *rawpic, *pix;
	GdkPixmap *pic;
	GdkBitmap *mask;
	gint width, height;

	gtk_init (&argc, &argv);

	gtk_rc_add_default_file (GTKRC_DIR "/greeter-gtkrc");

	normcursor = gdk_cursor_new(GDK_LEFT_PTR);
	busycursor = gdk_cursor_new(GDK_WATCH);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect (G_OBJECT (window), "destroy",
			G_CALLBACK (destroy), NULL);

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

	if (!(rawpic = gdk_pixbuf_new_from_file_at_scale(PIXMAPS_DIR "/bg.png", 
			pw, ph, FALSE, NULL))) {
        fprintf(stderr, "Couldn't load background pixmap\n");
        exit(1);
    }
	gdk_pixbuf_render_pixmap_and_mask(rawpic, &pic, &mask, 0);
	gdk_pixbuf_unref(rawpic);

	gtk_widget_set_app_paintable(window, TRUE);
	gtk_widget_set_size_request(window, w, h);
	gtk_widget_realize(window);
	gdk_window_set_back_pixmap((GdkWindow *) window->window, 
			pic, 0);
	gtk_window_set_decorated((GtkWindow *) window, FALSE);

	vbox = gtk_vbox_new(FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 0);
	hbox2 = gtk_hbox_new(FALSE, 5);
	hbox = gtk_hbox_new(FALSE, 0);

	bottombox = gtk_hbox_new(FALSE, 0);

	optionbutton = (GtkButton *) gtk_button_new_from_stock("gtk-preferences");
	gtk_button_set_relief(optionbutton, GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click((GtkButton *) optionbutton, FALSE);
	
	g_signal_connect (G_OBJECT (optionbutton), "clicked",
			G_CALLBACK (popup_menu),
			window);

	syslabel = gtk_label_new("");
	timelabel = gtk_label_new("");
	hoststring = g_strdup_printf("<b>%s (%s) //</b>", get_sysname(), get_ip());
	gtk_label_set_markup((GtkLabel *) syslabel, hoststring);
	update_time(timelabel);

	g_timeout_add(30000, (GSourceFunc)update_time, timelabel);

	gtk_box_pack_start((GtkBox *) bottombox, (GtkWidget *) optionbutton, FALSE, FALSE, 5);
	gtk_box_pack_end((GtkBox *) bottombox, (GtkWidget *) timelabel, FALSE, FALSE, 5);
	gtk_box_pack_end((GtkBox *) bottombox, (GtkWidget *) syslabel, FALSE, FALSE, 0);

	label = gtk_label_new("");
	spacer = gtk_label_new("");

	if (lw < 180)
	{
		lw=180;
	}

	gtk_label_set_markup((GtkLabel *) label, \
			(_("<b>Username:</b>")));
	gtk_misc_set_alignment((GtkMisc *) label, 1, 0.5);
	gtk_widget_set_size_request(label, (lw / 2), 0);

	entry = gtk_entry_new();
	gtk_entry_set_width_chars((GtkEntry *) entry, 12);
	g_signal_connect (G_OBJECT (entry), "activate", 
			G_CALLBACK (switch_entry), root);

	gtk_box_pack_start((GtkBox *) hbox2, label, FALSE, FALSE, 0);
	gtk_box_pack_start((GtkBox *) hbox2, entry, FALSE, FALSE, 0);

	gtk_box_pack_start((GtkBox *) vbox, spacer, FALSE, FALSE, spheight);
	gtk_box_pack_start((GtkBox *) vbox, logo, FALSE, FALSE, 5);
	gtk_box_pack_start((GtkBox *) vbox, hbox2, TRUE, FALSE, 0);
	gtk_box_pack_start((GtkBox *) hbox, vbox, TRUE, FALSE, 0);
	gtk_box_pack_start((GtkBox *) vbox2, hbox, FALSE, FALSE, 0);
	gtk_box_pack_end((GtkBox *) vbox2, bottombox, FALSE, FALSE, 5);

	gtk_container_add (GTK_CONTAINER (window), vbox2);

	gtk_widget_show_all  (window);

	gdk_window_set_cursor(root, normcursor);

	gtk_main ();

	return 0;
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Matthias Clasen <mclasen@redhat.com>
 * Copyright (C) 2013 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include "gs-application.h"

#include <stdlib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libnotify/notify.h>

#include "gs-box.h"
#include "gs-shell.h"
#include "gs-plugin-loader.h"

struct _GsApplication {
	GtkApplication parent;

	GCancellable	    *cancellable;
	GtkApplication	  *application;
	GtkCssProvider	  *provider;
	GsPluginLoader	  *plugin_loader;
	gint		     pending_apps;
	GsShell		 *shell;
};

struct _GsApplicationClass {
	GtkApplicationClass parent_class;
};

G_DEFINE_TYPE (GsApplication, gs_application, GTK_TYPE_APPLICATION);

static void
gs_application_init (GsApplication *application)
{
}

static void
about_activated (GSimpleAction *action,
		 GVariant      *parameter,
		 gpointer       app)
{
	const gchar *authors[] = {
		"Richard Hughes",
		"Matthias Clasen",
		"Allan Day",
		"Ryan Lerch",
		"William Jon McCann",
		NULL
	};
	const gchar *copyright = "Copyright \xc2\xa9 2013 Richard Hughes, Matthias Clasen";
	GtkIconTheme *icon_theme;
	GdkPixbuf *logo;
	GList *windows;
	GtkWindow *parent = NULL;

	windows = gtk_application_get_windows (GTK_APPLICATION (app));
	if (windows)
		parent = windows->data;

	icon_theme = gtk_icon_theme_get_default ();
	logo = gtk_icon_theme_load_icon (icon_theme, "gnome-software", 256, 0, NULL);

	gtk_show_about_dialog (parent,
			       /* TRANSLATORS: this is the title of the about window */
			       "title", _("About Software"),
			       /* TRANSLATORS: this is the application name */
			       "program-name", _("Software"),
			       "authors", authors,
			       /* TRANSLATORS: well, we seem to think so, anyway */
			       "comments", _("A nice way to manage the software on your system."),
			       "copyright", copyright,
			       "license-type", GTK_LICENSE_GPL_2_0,
			       "logo", logo,
			       "translator-credits", _("translator-credits"),
			       "version", VERSION,
			       NULL);

	g_object_unref (logo);
}

static void
quit_activated (GSimpleAction *action,
		GVariant      *parameter,
		gpointer       app)
{
	g_application_quit (G_APPLICATION (app));
}

static void
set_mode_activated (GSimpleAction *action,
		    GVariant      *parameter,
		    gpointer       data)
{
	GsApplication *app = GS_APPLICATION (data);
	const gchar *mode;

	mode = g_variant_get_string (parameter, NULL);
	if (g_strcmp0 (mode, "updates") == 0) {
		gs_shell_set_mode (app->shell, GS_SHELL_MODE_UPDATES);
	} else if (g_strcmp0 (mode, "installed") == 0) {
		gs_shell_set_mode (app->shell, GS_SHELL_MODE_INSTALLED);
	} else if (g_strcmp0 (mode, "overview") == 0) {
		gs_shell_set_mode (app->shell, GS_SHELL_MODE_OVERVIEW);
	} else if (g_strcmp0 (mode, "updated") == 0) {
		gs_shell_set_mode (app->shell, GS_SHELL_MODE_UPDATED);
	} else {
		g_warning ("Mode '%s' not recognised", mode);
	}
}

static GActionEntry actions[] = {
	{ "about", about_activated, NULL, NULL, NULL },
	{ "quit", quit_activated, NULL, NULL, NULL },
	{ "set-mode", set_mode_activated, "s", NULL, NULL }
};

static void
gs_application_startup (GApplication *application)
{
	GsApplication *app = GS_APPLICATION (application);
	GtkBuilder *builder;
	GMenuModel *app_menu;
	GtkWindow *window;
	GFile *file;
	GError *error = NULL;
	gchar *theme;

	G_APPLICATION_CLASS (gs_application_parent_class)->startup (application);

	notify_init ("gnome-software");

	g_type_ensure (GS_TYPE_BOX);

	/* set up the app menu */
	g_action_map_add_action_entries (G_ACTION_MAP (app),
					 actions, G_N_ELEMENTS (actions),
					 application);
	builder = gtk_builder_new_from_resource ("/org/gnome/software/app-menu.ui");
	app_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "appmenu"));
	gtk_application_set_app_menu (GTK_APPLICATION (app), app_menu);
	g_object_unref (builder);

	/* get CSS */
	app->provider = gtk_css_provider_new ();
	gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
						   GTK_STYLE_PROVIDER (app->provider),
						   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_get (gtk_settings_get_default (), "gtk-theme-name", &theme, NULL);
	if (g_strcmp0 (theme, "HighContrast") == 0) {
		file = g_file_new_for_uri ("resource:///org/gnome/software/gtk-style-hc.css");
	} else {
		file = g_file_new_for_uri ("resource:///org/gnome/software/gtk-style.css");
	}
	gtk_css_provider_load_from_file (app->provider, file, NULL);
	g_object_unref (file);
	g_free (theme);

	/* setup plugins */
	app->plugin_loader = gs_plugin_loader_new ();
	gs_plugin_loader_set_location (app->plugin_loader, NULL);
	if (!gs_plugin_loader_setup (app->plugin_loader, &error)) {
		g_warning ("Failed to setup plugins: %s", error->message);
		exit (1);
	}
	gs_plugin_loader_set_enabled (app->plugin_loader, "hardcoded-featured", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "hardcoded-kind", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "hardcoded-popular", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "hardcoded-ratings", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "hardcoded-screenshots", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "menu-spec-categories", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "menu-spec-refine", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "local-ratings", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "packagekit", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "systemd-updates", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "packagekit-refine", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "packagekit-history", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "packagekit-offline", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "appdata", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "appstream", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "desktopdb", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "datadir-apps", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "datadir-filename", TRUE);
	gs_plugin_loader_set_enabled (app->plugin_loader, "datadir-filename-local", TRUE);

	/* show the priority of each plugin */
	gs_plugin_loader_dump_state (app->plugin_loader);

	/* setup UI */
	app->shell = gs_shell_new ();

	app->cancellable = g_cancellable_new ();

	window = gs_shell_setup (app->shell, app->plugin_loader, app->cancellable);
	gtk_application_add_window (GTK_APPLICATION (app), window);

        gtk_window_present (window);
}

static void
gs_application_activate (GApplication *application)
{
	gs_shell_activate (GS_APPLICATION (application)->shell);
}

static void
gs_application_finalize (GObject *object)
{
	GsApplication *app = GS_APPLICATION (object);

	g_clear_object (&app->plugin_loader);
	g_clear_object (&app->cancellable);
	g_clear_object (&app->shell);
	g_clear_object (&app->provider);
	G_OBJECT_CLASS (gs_application_parent_class)->finalize (object);
}

static void
gs_application_class_init (GsApplicationClass *class)
{
	G_OBJECT_CLASS (class)->finalize = gs_application_finalize;
	G_APPLICATION_CLASS (class)->startup = gs_application_startup;
	G_APPLICATION_CLASS (class)->activate = gs_application_activate;
}

GsApplication *
gs_application_new (void)
{
	return g_object_new (GS_APPLICATION_TYPE,
			     "application-id", "org.gnome.Software",
			     "flags", G_APPLICATION_IS_SERVICE,
			     NULL);
}

/* vim: set noexpandtab: */

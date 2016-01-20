/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
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

/* Notes:
 *
 * All GsApp's created have management-plugin set to XgdApp
 * Some GsApp's created have have XgdApp::type of app or runtime
 * The GsApp:origin is the remote name, e.g. test-repo
 *
 * Some outstanding notes:
 *
 * - How can I get the AppStream metadata using libxdgapp?
 * - Where is the privaledge elevation helper?
 */

#include <config.h>

#include <xdg-app.h>

#include <gs-plugin.h>

#include "gs-utils.h"

struct GsPluginPrivate {
	XdgAppInstallation	*installation;
	GFileMonitor		*monitor;
};

/**
 * gs_plugin_get_name:
 */
const gchar *
gs_plugin_get_name (void)
{
	return "xdg-app";
}

/**
 * gs_plugin_get_deps:
 */
const gchar **
gs_plugin_get_deps (GsPlugin *plugin)
{
	static const gchar *deps[] = {
		"appstream",
		NULL };
	return deps;
}

/**
 * gs_plugin_initialize:
 */
void
gs_plugin_initialize (GsPlugin *plugin)
{
	/* create private area */
	plugin->priv = GS_PLUGIN_GET_PRIVATE (GsPluginPrivate);
}

/**
 * gs_plugin_destroy:
 */
void
gs_plugin_destroy (GsPlugin *plugin)
{
	if (plugin->priv->installation != NULL)
		g_object_unref (plugin->priv->installation);
	if (plugin->priv->monitor != NULL)
		g_object_unref (plugin->priv->monitor);
}

/**
 * gs_plugin_xdg_app_changed_cb:
 */
static void
gs_plugin_xdg_app_changed_cb (GFileMonitor *monitor,
			      GFile *child,
			      GFile *other_file,
			      GFileMonitorEvent event_type,
			      GsPlugin *plugin)
{
	gs_plugin_updates_changed (plugin);
}

/**
 * gs_plugin_ensure_installation:
 */
static gboolean
gs_plugin_ensure_installation (GsPlugin *plugin,
			       GCancellable *cancellable,
			       GError **error)
{
	g_autoptr(AsProfileTask) ptask = NULL;
	if (plugin->priv->installation != NULL)
		return TRUE;

	/* FIXME: this should default to system-wide, but we need a permissions
	 * helper to elevate privs */
	ptask = as_profile_start_literal (plugin->profile, "xdg-app::ensure-origin");
	plugin->priv->installation = xdg_app_installation_new_user (cancellable, error);
	if (plugin->priv->installation == NULL)
		return FALSE;

	/* watch for changes */
	plugin->priv->monitor =
		xdg_app_installation_create_monitor (plugin->priv->installation,
						     cancellable,
						     error);
	if (plugin->priv->monitor == NULL)
		return FALSE;
	g_signal_connect (plugin->priv->monitor, "changed",
			  G_CALLBACK (gs_plugin_xdg_app_changed_cb), plugin);

	/* success */
	return TRUE;
}

/**
 * gs_plugin_xdg_app_set_metadata:
 */
static void
gs_plugin_xdg_app_set_metadata (GsApp *app, XdgAppRef *xref)
{
	gs_app_set_management_plugin (app, "XgdApp");
	gs_app_set_metadata (app, "XgdApp::branch",
			     xdg_app_ref_get_branch (xref));
	gs_app_set_metadata (app, "XgdApp::commit",
			     xdg_app_ref_get_commit (xref));
	gs_app_set_metadata (app, "XgdApp::name",
			     xdg_app_ref_get_name (xref));
	gs_app_set_metadata (app, "XgdApp::arch",
			     xdg_app_ref_get_arch (xref));

	switch (xdg_app_ref_get_kind (xref)) {
	case XDG_APP_REF_KIND_APP:
		gs_app_set_metadata (app, "XgdApp::type", "app");
		break;
	case XDG_APP_REF_KIND_RUNTIME:
		gs_app_set_metadata (app, "XgdApp::type", "runtime");
		break;
	default:
		break;
	}
}

/**
 * gs_plugin_xdg_app_set_metadata_installed:
 */
static void
gs_plugin_xdg_app_set_metadata_installed (GsApp *app, XdgAppInstalledRef *xref)
{
	guint64 mtime;
	guint64 size_installed;
	g_autofree gchar *metadata_fn = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GFileInfo) info = NULL;

	/* for all types */
	gs_plugin_xdg_app_set_metadata (app, XDG_APP_REF (xref));

	/* get the last time the app was updated */
	metadata_fn = g_build_filename (xdg_app_installed_ref_get_deploy_dir (xref),
					"..",
					"origin",
					NULL);
	file = g_file_new_for_path (metadata_fn);
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_TIME_MODIFIED,
				  G_FILE_QUERY_INFO_NONE,
				  NULL, NULL);
	if (info != NULL) {
		mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
		gs_app_set_install_date (app, mtime);
	}

	/* this is faster than resolving */
	gs_app_set_origin (app, xdg_app_installed_ref_get_origin (xref));

	/* this is faster than xdg_app_installation_fetch_remote_size_sync() */
	size_installed = xdg_app_installed_ref_get_installed_size (xref);
	if (size_installed != 0)
		gs_app_set_size (app, size_installed);
}

/**
 * gs_plugin_xdg_app_create_installed:
 */
static GsApp *
gs_plugin_xdg_app_create_installed (GsPlugin *plugin,
				    XdgAppInstalledRef *xref,
				    GError **error)
{	g_autofree gchar *id = NULL;
	g_autoptr(AsIcon) icon = NULL;
	g_autoptr(GsApp) app = NULL;

	g_return_val_if_fail (xref != NULL, NULL);

	/*
	 * Only show the current application in GNOME Software
	 *
	 * You can have multiple versions/branches of a particular app-id
	 * installed but only one of them is "current" where this means:
	 *  1) the default to launch unless you specify a version
	 *  2) The one that gets its exported files exported
	 */
	if (!xdg_app_installed_ref_get_is_current (xref) &&
	    xdg_app_ref_get_kind (XDG_APP_REF(xref)) == XDG_APP_REF_KIND_APP) {
		g_set_error (error,
			     GS_PLUGIN_ERROR,
			     GS_PLUGIN_ERROR_NOT_SUPPORTED,
			     "%s not current, ignoring",
			     xdg_app_ref_get_name (XDG_APP_REF (xref)));
		return NULL;
	}

	/* create new object */
	app = gs_app_new (NULL);
	gs_app_set_kind (app, GS_APP_KIND_NORMAL);
	gs_plugin_xdg_app_set_metadata_installed (app, xref);

	switch (xdg_app_ref_get_kind (XDG_APP_REF(xref))) {
	case XDG_APP_REF_KIND_APP:
		id = g_strdup_printf ("%s.desktop",
				      xdg_app_ref_get_name (XDG_APP_REF(xref)));
		gs_app_set_id_kind (app, AS_ID_KIND_DESKTOP);
		break;
	case XDG_APP_REF_KIND_RUNTIME:
		id = g_strdup_printf ("%s.runtime",
				      xdg_app_ref_get_name (XDG_APP_REF(xref)));
		gs_app_set_metadata (app, "XgdApp::type", "runtime");
		gs_app_set_id_kind (app, AS_ID_KIND_RUNTIME);
		gs_app_set_name (app, GS_APP_QUALITY_NORMAL,
				 xdg_app_ref_get_name (XDG_APP_REF (xref)));
		gs_app_set_summary (app, GS_APP_QUALITY_NORMAL,
				    "Framework for applications");
		icon = as_icon_new ();
		as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
		as_icon_set_name (icon, "system-run-symbolic");
		gs_app_set_icon (app, icon);
		break;
	default:
		g_set_error_literal (error,
				     GS_PLUGIN_ERROR,
				     GS_PLUGIN_ERROR_NOT_SUPPORTED,
				     "XdgAppRefKind not known");
		return NULL;
	}

	/* we know the full application ID now */
	gs_app_set_id (app, id);
	return g_object_ref (app);
}

typedef struct {
	GsApp		*app;
	GsPlugin	*plugin;
} GsPluginHelper;

/**
 * gs_plugin_xdg_app_progress_cb:
 */
static void
gs_plugin_xdg_app_progress_cb (const gchar *status,
			       guint progress,
			       gboolean estimating,
			       gpointer user_data)
{
	GsPluginHelper *helper = (GsPluginHelper *) user_data;
	if (helper->app == NULL)
		return;
	gs_plugin_progress_update (helper->plugin, helper->app, progress);
}

/**
 * gs_plugin_add_installed:
 */
gboolean
gs_plugin_add_installed (GsPlugin *plugin,
			 GList **list,
			 GCancellable *cancellable,
			 GError **error)
{
	g_autoptr(GPtrArray) xrefs = NULL;
	guint i;

	/* ensure we can set up the repo */
	if (!gs_plugin_ensure_installation (plugin, cancellable, error))
		return FALSE;

	/* get apps and runtimes */
	xrefs = xdg_app_installation_list_installed_refs (plugin->priv->installation,
							  cancellable, error);
	if (xrefs == NULL)
		return FALSE;
	for (i = 0; i < xrefs->len; i++) {
		XdgAppInstalledRef *xref = g_ptr_array_index (xrefs, i);
		g_autoptr(GError) error_local = NULL;
		g_autoptr(GsApp) app = NULL;
		app = gs_plugin_xdg_app_create_installed (plugin, xref, &error_local);
		if (app == NULL) {
			g_warning ("failed to add xdg-app: %s", error_local->message);
			continue;
		}
		gs_app_set_state (app, AS_APP_STATE_INSTALLED);
		gs_plugin_add_app (list, app);
	}

	return TRUE;
}

/**
 * gs_plugin_add_sources:
 */
gboolean
gs_plugin_add_sources (GsPlugin *plugin,
		       GList **list,
		       GCancellable *cancellable,
		       GError **error)
{
	g_autoptr(GPtrArray) xremotes = NULL;
	guint i;

	/* ensure we can set up the repo */
	if (!gs_plugin_ensure_installation (plugin, cancellable, error))
		return FALSE;

	xremotes = xdg_app_installation_list_remotes (plugin->priv->installation,
						      cancellable,
						      error);
	if (xremotes == NULL)
		return FALSE;
	for (i = 0; i < xremotes->len; i++) {
		XdgAppRemote *xremote = g_ptr_array_index (xremotes, i);
		g_autoptr(GsApp) app = NULL;
		app = gs_app_new (xdg_app_remote_get_name (xremote));
		gs_app_set_management_plugin (app, "XgdApp");
		gs_app_set_kind (app, GS_APP_KIND_SOURCE);
		gs_app_set_state (app, AS_APP_STATE_INSTALLED);
		gs_app_set_name (app,
				 GS_APP_QUALITY_LOWEST,
				 xdg_app_remote_get_name (xremote));
		gs_app_set_summary (app,
				    GS_APP_QUALITY_LOWEST,
				    xdg_app_remote_get_title (xremote));
		gs_app_set_url (app,
				AS_URL_KIND_HOMEPAGE,
				xdg_app_remote_get_url (xremote));
		gs_plugin_add_app (list, app);
	}
	return TRUE;
}

/**
 * gs_plugin_add_updates:
 */
gboolean
gs_plugin_add_updates (GsPlugin *plugin,
		       GList **list,
		       GCancellable *cancellable,
		       GError **error)
{
	guint i;
	g_autoptr(GPtrArray) xrefs = NULL;

	/* ensure we can set up the repo */
	if (!gs_plugin_ensure_installation (plugin, cancellable, error))
		return FALSE;

	/* get all the installed apps (no network I/O) */
	xrefs = xdg_app_installation_list_installed_refs (plugin->priv->installation,
							  cancellable,
							  error);
	if (xrefs == NULL)
		return FALSE;
	for (i = 0; i < xrefs->len; i++) {
		XdgAppInstalledRef *xref = g_ptr_array_index (xrefs, i);
		const gchar *commit;
		const gchar *latest_commit;
		g_autoptr(GsApp) app = NULL;
		g_autoptr(GError) error_local = NULL;

		/* check the application has already been downloaded */
		commit = xdg_app_ref_get_commit (XDG_APP_REF (xref));
		latest_commit = xdg_app_installed_ref_get_latest_commit (xref);
		if (g_strcmp0 (commit, latest_commit) == 0) {
			g_debug ("no downloaded update for %s",
				 xdg_app_ref_get_name (XDG_APP_REF (xref)));
			continue;
		}

		/* we have an update to show */
		g_debug ("%s has a downloaded update %s->%s",
			 xdg_app_ref_get_name (XDG_APP_REF (xref)),
			 commit, latest_commit);
		app = gs_plugin_xdg_app_create_installed (plugin, xref, &error_local);
		if (app == NULL) {
			g_warning ("failed to add xdg-app: %s", error_local->message);
			continue;
		}
		if (gs_app_get_state (app) == AS_APP_STATE_INSTALLED)
			gs_app_set_state (app, AS_APP_STATE_UNKNOWN);
		gs_app_set_state (app, AS_APP_STATE_UPDATABLE_LIVE);
		gs_plugin_add_app (list, app);
	}

	return TRUE;
}

/**
 * gs_plugin_refresh:
 */
gboolean
gs_plugin_refresh (GsPlugin *plugin,
		   guint cache_age,
		   GsPluginRefreshFlags flags,
		   GCancellable *cancellable,
		   GError **error)
{
	GsPluginHelper helper;
	guint i;
	gboolean ret;
	g_autoptr(GPtrArray) xrefs = NULL;
	g_autoptr(GPtrArray) xremotes = NULL;

	/* not us */
	if ((flags & GS_PLUGIN_REFRESH_FLAGS_UPDATES) == 0)
		return TRUE;

	/* ensure we can set up the repo */
	if (!gs_plugin_ensure_installation (plugin, cancellable, error))
		return FALSE;

	/* update AppStream metadata */
	xremotes = xdg_app_installation_list_remotes (plugin->priv->installation,
						      cancellable,
						      error);
	if (xremotes == NULL)
		return FALSE;
	for (i = 0; i < xremotes->len; i++) {
		guint tmp;
		g_autoptr(GError) error_local = NULL;
		g_autoptr(GFile) file = NULL;
		g_autoptr(GFile) file_timestamp = NULL;
		g_autofree gchar *appstream_fn = NULL;
		XdgAppRemote *xremote = g_ptr_array_index (xremotes, i);

		/* is the timestamp new enough */
		file_timestamp = xdg_app_remote_get_appstream_timestamp (xremote, NULL);
		tmp = gs_utils_get_file_age (file_timestamp);
		if (tmp < cache_age) {
			g_autofree gchar *fn = g_file_get_path (file_timestamp);
			g_debug ("%s is only %i seconds old, so ignoring refresh",
				 fn, tmp);
			continue;
		}

		/* download new data */
		ret = xdg_app_installation_update_appstream_sync (plugin->priv->installation,
								  xdg_app_remote_get_name (xremote),
								  NULL, /* arch */
								  NULL, /* out_changed */
								  cancellable,
								  &error_local);
		if (!ret) {
			if (g_error_matches (error_local,
					     G_IO_ERROR,
					     G_IO_ERROR_FAILED)) {
				g_debug ("Failed to get AppStream metadata: %s",
					 error_local->message);
				continue;
			}
			g_set_error (error,
				     GS_PLUGIN_ERROR,
				     GS_PLUGIN_ERROR_NOT_SUPPORTED,
				     "Failed to get AppStream metadata: %s",
				     error_local->message);
			return FALSE;
		}

		/* add the new AppStream repo to the shared store */
		file = xdg_app_remote_get_appstream_dir (xremote, NULL);
		appstream_fn = g_file_get_path (file);
		g_debug ("using AppStream metadata found at: %s", appstream_fn);
	}

	/* use helper: FIXME: new()&ref? */
	helper.plugin = plugin;

	/* get all the updates available from all remotes */
	xrefs = xdg_app_installation_list_installed_refs_for_update (plugin->priv->installation,
								     cancellable,
								     error);
	if (xrefs == NULL)
		return FALSE;
	for (i = 0; i < xrefs->len; i++) {
		XdgAppInstalledRef *xref = g_ptr_array_index (xrefs, i);
		g_autoptr(GsApp) app = NULL;
		g_autoptr(XdgAppInstalledRef) xref2 = NULL;

		/* try to create a GsApp so we can do progress reporting */
		app = gs_plugin_xdg_app_create_installed (plugin, xref, NULL);
		helper.app = app;

		/* fetch but do not deploy */
		g_debug ("pulling update for %s",
			 xdg_app_ref_get_name (XDG_APP_REF (xref)));
		xref2 = xdg_app_installation_update (plugin->priv->installation,
						     XDG_APP_UPDATE_FLAGS_NO_DEPLOY,
						     xdg_app_ref_get_kind (XDG_APP_REF (xref)),
						     xdg_app_ref_get_name (XDG_APP_REF (xref)),
						     xdg_app_ref_get_arch (XDG_APP_REF (xref)),
						     xdg_app_ref_get_branch (XDG_APP_REF (xref)),
						     gs_plugin_xdg_app_progress_cb, &helper,
						     cancellable, error);
		if (xref2 == NULL)
			return FALSE;
	}

	return TRUE;
}

/**
 * gs_plugin_refine_item_origin:
 */
static gboolean
gs_plugin_refine_item_origin (GsPlugin *plugin,
			      GsApp *app,
			      GCancellable *cancellable,
			      GError **error)
{
	const gchar *origin;
	guint i;
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GPtrArray) xremotes = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;

	/* already set */
	origin = gs_app_get_origin_ui (app);
	if (origin != NULL)
		return TRUE;

	/* ensure we can set up the repo */
	ptask = as_profile_start_literal (plugin->profile, "xdg-app::refine-origin");
	if (!gs_plugin_ensure_installation (plugin, cancellable, error))
		return FALSE;

	/* find list of remotes */
	xremotes = xdg_app_installation_list_remotes (plugin->priv->installation,
						      cancellable,
						      error);
	if (xremotes == NULL)
		return FALSE;
	for (i = 0; i < xremotes->len; i++) {
		XdgAppRemote *xremote = g_ptr_array_index (xremotes, i);
		if (g_strcmp0 (gs_app_get_origin (app),
		    xdg_app_remote_get_name (xremote)) == 0) {
			gs_app_set_origin_ui (app, xdg_app_remote_get_title (xremote));
			break;
		}
	}

	return TRUE;
}

/**
 * gs_plugin_refine_item_commit:
 */
static gboolean
gs_plugin_refine_item_commit (GsPlugin *plugin,
			      GsApp *app,
			      GCancellable *cancellable,
			      GError **error)
{
	XdgAppRefKind kind = XDG_APP_REF_KIND_APP;
	g_autoptr(AsProfileTask) ptask = NULL;
	g_autoptr(XdgAppRemoteRef) xref_remote = NULL;

	if (gs_app_get_metadata_item (app, "XgdApp::commit") != NULL)
		return TRUE;
	if (g_strcmp0 (gs_app_get_metadata_item (app, "XgdApp::type"), "runtime") == 0)
		kind = XDG_APP_REF_KIND_RUNTIME;

	ptask = as_profile_start_literal (plugin->profile, "xdg-app::fetch-remote-ref");
	xref_remote = xdg_app_installation_fetch_remote_ref_sync (plugin->priv->installation,
								  gs_app_get_origin (app),
								  kind,
								  gs_app_get_metadata_item (app, "XgdApp::name"),
								  gs_app_get_metadata_item (app, "XgdApp::arch"),
								  gs_app_get_metadata_item (app, "XgdApp::branch"),
								  cancellable,
								  error);
	if (xref_remote == NULL)
		return FALSE;
	gs_app_set_metadata (app, "XgdApp::commit",
			     xdg_app_ref_get_commit (XDG_APP_REF (xref_remote)));
	return TRUE;
}

/**
 * gs_plugin_refine_item_size:
 */
static gboolean
gs_plugin_refine_item_size (GsPlugin *plugin,
			    GsApp *app,
			    GCancellable *cancellable,
			    GError **error)
{
	gboolean ret;
	guint64 download_size;
	guint64 installed_size;
	g_autoptr(GError) error_local = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;

	if (gs_app_get_size (app) > 0)
		return TRUE;

	/* need commit */
	if (!gs_plugin_refine_item_commit (plugin, app, cancellable, error))
		return FALSE;

	ptask = as_profile_start_literal (plugin->profile, "xdg-app::refine-size");
	ret = xdg_app_installation_fetch_remote_size_sync (plugin->priv->installation,
							   gs_app_get_origin (app),
							   gs_app_get_metadata_item (app, "XgdApp::commit"),
							   &download_size,
							   &installed_size,
							   cancellable, &error_local);
	if (!ret) {
		g_warning ("libxdgapp failed to return application size: %s",
			   error_local->message);
		installed_size = GS_APP_SIZE_MISSING;
	}
	gs_app_set_size (app, installed_size);

	return TRUE;
}

/**
 * gs_plugin_refine_item_action:
 */
static gboolean
gs_plugin_refine_item_action (GsPlugin *plugin,
			      GsApp *app,
			      GCancellable *cancellable,
			      GError **error)
{
	guint i;
	g_autoptr(GPtrArray) xrefs = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;

	/* already found */
	if (gs_app_get_metadata_item (app, "XgdApp::type") != NULL)
		return TRUE;

	/* get apps and runtimes */
	ptask = as_profile_start_literal (plugin->profile, "xdg-app::refine-action");
	xrefs = xdg_app_installation_list_installed_refs (plugin->priv->installation,
							  cancellable, error);
	if (xrefs == NULL)
		return FALSE;
	for (i = 0; i < xrefs->len; i++) {
		g_autofree gchar *id = NULL;
		XdgAppInstalledRef *xref = g_ptr_array_index (xrefs, i);
		if (xdg_app_ref_get_kind (XDG_APP_REF(xref)) != XDG_APP_REF_KIND_APP)
			continue;
		id = g_strdup_printf ("%s.desktop",
				      xdg_app_ref_get_name (XDG_APP_REF(xref)));
		if (g_strcmp0 (id, gs_app_get_id (app)) != 0)
			continue;

		/* 'claim' this application for our own */
		g_debug ("claiming %s for xdg-app", gs_app_get_id (app));
		gs_plugin_xdg_app_set_metadata_installed (app, xref);
		if (gs_app_get_state (app) == AS_APP_STATE_UNKNOWN)
			gs_app_set_state (app, AS_APP_STATE_INSTALLED);
	}

	/* since xdg-app can't 'disable' a repo, if we have an AppStream entry
	 * with the correct bundle type that's not installed we can assume it
	 * is available for install */
	if (gs_app_get_state (app) == AS_APP_STATE_UNKNOWN) {
		g_autoptr(XdgAppRef) xref = NULL;
		xref = xdg_app_ref_parse (gs_app_get_source_default (app), error);
		if (xref == NULL)
			return FALSE;
		gs_plugin_xdg_app_set_metadata (app, xref);
		gs_app_set_state (app, AS_APP_STATE_AVAILABLE);
	}

	return TRUE;
}

/**
 * gs_plugin_refine_item:
 */
static gboolean
gs_plugin_refine_item (GsPlugin *plugin,
		       GsApp *app,
		       GsPluginRefineFlags flags,
		       GCancellable *cancellable,
		       GError **error)
{
	g_autoptr(AsProfileTask) ptask = NULL;

	/* only process this app if was created by this plugin */
	if (g_strcmp0 (gs_app_get_management_plugin (app), "XgdApp") != 0)
		return TRUE;

	/* profile */
	ptask = as_profile_start (plugin->profile,
				  "xdg-app::refine{%s}",
				  gs_app_get_id (app));

	/* check if this desktop ID can be handled by the plugin */
	if (flags & GS_PLUGIN_REFINE_FLAGS_REQUIRE_SETUP_ACTION) {
		if (!gs_plugin_refine_item_action (plugin, app, cancellable, error))
			return FALSE;
	}

	/* size */
	if (flags & GS_PLUGIN_REFINE_FLAGS_REQUIRE_SIZE) {
		if (!gs_plugin_refine_item_size (plugin, app, cancellable, error))
			return FALSE;
	}

	/* origin */
	if (flags & GS_PLUGIN_REFINE_FLAGS_REQUIRE_ORIGIN) {
		if (!gs_plugin_refine_item_origin (plugin, app, cancellable, error))
			return FALSE;
	}

	return TRUE;
}

/**
 * gs_plugin_refine:
 */
gboolean
gs_plugin_refine (GsPlugin *plugin,
		  GList **list,
		  GsPluginRefineFlags flags,
		  GCancellable *cancellable,
		  GError **error)
{
	GList *l;
	GsApp *app;

	/* ensure we can set up the repo */
	if (!gs_plugin_ensure_installation (plugin, cancellable, error))
		return FALSE;

	for (l = *list; l != NULL; l = l->next) {
		app = GS_APP (l->data);
		if (!gs_plugin_refine_item (plugin, app, flags, cancellable, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * gs_plugin_launch:
 */
gboolean
gs_plugin_launch (GsPlugin *plugin,
		  GsApp *app,
		  GCancellable *cancellable,
		  GError **error)
{
	const gchar *branch = NULL;

	/* only process this app if was created by this plugin */
	if (g_strcmp0 (gs_app_get_management_plugin (app), "XgdApp") != 0)
		return TRUE;

	/* ensure we can set up the repo */
	if (!gs_plugin_ensure_installation (plugin, cancellable, error))
		return FALSE;

	branch = gs_app_get_metadata_item (app, "XgdApp::branch");
	if (branch == NULL)
		branch = "master";
	return xdg_app_installation_launch (plugin->priv->installation,
					    gs_app_get_metadata_item (app, "XgdApp::name"),
					    NULL,
					    branch,
					    NULL,
					    cancellable,
					    error);
}

/**
 * gs_plugin_app_remove:
 */
gboolean
gs_plugin_app_remove (GsPlugin *plugin,
		      GsApp *app,
		      GCancellable *cancellable,
		      GError **error)
{
	GsPluginHelper helper;

	/* only process this app if was created by this plugin */
	if (g_strcmp0 (gs_app_get_management_plugin (app), "XgdApp") != 0)
		return TRUE;

	/* ensure we can set up the repo */
	if (!gs_plugin_ensure_installation (plugin, cancellable, error))
		return FALSE;

	/* use helper: FIXME: new()&ref? */
	helper.app = app;
	helper.plugin = plugin;

	/* remove */
	gs_app_set_state (app, AS_APP_STATE_REMOVING);
	return xdg_app_installation_uninstall (plugin->priv->installation,
					       XDG_APP_REF_KIND_APP,
					       gs_app_get_metadata_item (app, "XgdApp::name"),
					       gs_app_get_metadata_item (app, "XgdApp::arch"),
					       gs_app_get_metadata_item (app, "XgdApp::branch"),
					       gs_plugin_xdg_app_progress_cb, &helper,
					       cancellable, error);
}

/**
 * gs_plugin_app_install:
 */
gboolean
gs_plugin_app_install (GsPlugin *plugin,
		       GsApp *app,
		       GCancellable *cancellable,
		       GError **error)
{
	GsPluginHelper helper;
	g_autoptr(XdgAppInstalledRef) xref = NULL;

	/* only process this app if was created by this plugin */
	if (g_strcmp0 (gs_app_get_management_plugin (app), "XgdApp") != 0)
		return TRUE;

	/* ensure we can set up the repo */
	if (!gs_plugin_ensure_installation (plugin, cancellable, error))
		return FALSE;

	/* use helper: FIXME: new()&ref? */
	helper.app = app;
	helper.plugin = plugin;

	/* install */
	gs_app_set_state (app, AS_APP_STATE_INSTALLING);
	xref = xdg_app_installation_install (plugin->priv->installation,
					     gs_app_get_origin (app),
					     XDG_APP_REF_KIND_APP,
					     gs_app_get_metadata_item (app, "XgdApp::name"),
					     gs_app_get_metadata_item (app, "XgdApp::arch"),
					     gs_app_get_metadata_item (app, "XgdApp::branch"),
					     gs_plugin_xdg_app_progress_cb, &helper,
					     cancellable, error);
	return xref != NULL;
}

/**
 * gs_plugin_app_update:
 *
 * This is only called when updating device firmware live.
 */
gboolean
gs_plugin_app_update (GsPlugin *plugin,
		      GsApp *app,
		      GCancellable *cancellable,
		      GError **error)
{
	GsPluginHelper helper;
	XdgAppRefKind kind = XDG_APP_REF_KIND_APP;
	g_autoptr(XdgAppInstalledRef) xref = NULL;

	/* only process this app if was created by this plugin */
	if (g_strcmp0 (gs_app_get_management_plugin (app), "XgdApp") != 0)
		return TRUE;

	/* ensure we can set up the repo */
	if (!gs_plugin_ensure_installation (plugin, cancellable, error))
		return FALSE;

	/* use helper: FIXME: new()&ref? */
	helper.app = app;
	helper.plugin = plugin;

	/* special case */
	if (g_strcmp0 (gs_app_get_metadata_item (app, "XgdApp::type"), "runtime") == 0)
		kind = XDG_APP_REF_KIND_RUNTIME;

	/* install */
	gs_app_set_state (app, AS_APP_STATE_INSTALLING);
	xref = xdg_app_installation_update (plugin->priv->installation,
					    XDG_APP_UPDATE_FLAGS_NONE,
					    kind,
					    gs_app_get_metadata_item (app, "XgdApp::name"),
					    gs_app_get_metadata_item (app, "XgdApp::arch"),
					    gs_app_get_metadata_item (app, "XgdApp::branch"),
					    gs_plugin_xdg_app_progress_cb, &helper,
					    cancellable, error);
	return xref != NULL;
}

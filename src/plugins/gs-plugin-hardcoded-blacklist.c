/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
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

#include <config.h>
#include <fnmatch.h>
#include <gs-plugin.h>

/*
 * SECTION:
 * Blacklists some applications based on a hardcoded list.
 */

/**
 * gs_plugin_get_name:
 */
const gchar *
gs_plugin_get_name (void)
{
	return "hardcoded-blacklist";
}

/**
 * gs_plugin_get_deps:
 */
const gchar **
gs_plugin_get_deps (GsPlugin *plugin)
{
	static const gchar *deps[] = {
		"appstream",		/* need ID */
		NULL };
	return deps;
}

/**
 * gs_plugin_refine_app:
 */
static gboolean
gs_plugin_refine_app (GsPlugin *plugin, GsApp *app, GError **error)
{
	guint i;
	const gchar *app_globs[] = {
		"freeciv-server.desktop",
		"nm-connection-editor.desktop",
		"plank.desktop",
		"*release-notes*.desktop",
		"*Release_Notes*.desktop",
		"remote-viewer.desktop",
		"Rodent-*.desktop",
		"rygel-preferences.desktop",
		"system-config-keyboard.desktop",
		"tracker-preferences.desktop",
		"Uninstall*.desktop",
		NULL };

	/* not set yet */
	if (gs_app_get_id (app) == NULL)
		return TRUE;

	/* search */
	for (i = 0; app_globs[i] != NULL; i++) {
		if (fnmatch (app_globs[i], gs_app_get_id (app), 0) == 0) {
			gs_app_add_category (app, "Blacklisted");
			break;
		}
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
	g_autoptr(AsProfileTask) ptask = NULL;

	/* are any of the packages on the blacklist? */
	ptask = as_profile_start_literal (plugin->profile, "hardcoded-blacklist");
	for (l = *list; l != NULL; l = l->next) {
		app = GS_APP (l->data);
		if (!gs_plugin_refine_app (plugin, app, error))
			return FALSE;
	}
	return TRUE;
}

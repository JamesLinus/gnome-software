/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Canonical Ltd.
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

#include <math.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#include <gs-plugin.h>
#include <gs-utils.h>

struct GsPluginPrivate {
	SoupSession		*session;
};

const gchar *
gs_plugin_get_name (void)
{
	return "ubuntu-reviews";
}

#define UBUNTU_REVIEWS_SERVER		"https://reviews.ubuntu.com/reviews"

void
gs_plugin_initialize (GsPlugin *plugin)
{
	/* create private area */
	plugin->priv = GS_PLUGIN_GET_PRIVATE (GsPluginPrivate);

	/* check that we are running on Ubuntu */
	if (!gs_plugin_check_distro_id (plugin, "ubuntu")) {
		gs_plugin_set_enabled (plugin, FALSE);
		g_debug ("disabling '%s' as we're not Ubuntu", plugin->name);
		return;
	}
}

const gchar **
gs_plugin_get_deps (GsPlugin *plugin)
{
	static const gchar *deps[] = { NULL };
	return deps;
}

void
gs_plugin_destroy (GsPlugin *plugin)
{
	if (plugin->priv->session != NULL)
		g_object_unref (plugin->priv->session);
}

static gboolean
setup_networking (GsPlugin *plugin, GError **error)
{
	/* already set up */
	if (plugin->priv->session != NULL)
		return TRUE;

	/* set up a session */
	plugin->priv->session = soup_session_new_with_options (SOUP_SESSION_USER_AGENT,
	                                                       "gnome-software",
	                                                       NULL);
	if (plugin->priv->session == NULL) {
		g_set_error (error,
			     GS_PLUGIN_ERROR,
			     GS_PLUGIN_ERROR_FAILED,
			     "%s: failed to setup networking",
			     plugin->name);
		return FALSE;
	}
	return TRUE;
}

static GDateTime *
parse_date_time (const gchar *text)
{
	const gchar *format = "YYYY-MM-DD HH:MM:SS";
	int i, value_index, values[6] = { 0, 0, 0, 0, 0, 0 };

	if (!text)
		return NULL;

	/* Extract the numbers as shown in the format */
	for (i = 0, value_index = 0; text[i] && format[i] && value_index < 6; i++) {
		char c = text[i];

		if (c == '-' || c == ' ' || c == ':') {
			if (format[i] != c)
				return NULL;
			value_index++;
		} else {
			int d = c - '0';
			if (d < 0 || d > 9)
				return NULL;
			values[value_index] = values[value_index] * 10 + d;
		}
	}

	/* We didn't match the format */
	if (format[i] != '\0' || text[i] != '\0' || value_index != 5)
		return NULL;

	/* Use the numbers to create a GDateTime object */
	return g_date_time_new_utc (values[0], values[1], values[2], values[3], values[4], values[5]);
}

static GsAppReview *
parse_review (JsonNode *node)
{
	GsAppReview *review;
	JsonObject *object;
	gint64 star_rating;

	if (!JSON_NODE_HOLDS_OBJECT (node))
		return NULL;

	object = json_node_get_object (node);

	review = gs_app_review_new ();
	gs_app_review_set_reviewer (review, json_object_get_string_member (object, "reviewer_displayname"));
	gs_app_review_set_summary (review, json_object_get_string_member (object, "summary"));
	gs_app_review_set_text (review, json_object_get_string_member (object, "review_text"));
	gs_app_review_set_version (review, json_object_get_string_member (object, "version"));
	star_rating = json_object_get_int_member (object, "rating");
	if (star_rating > 0)
		gs_app_review_set_rating (review, star_rating * 20);
	gs_app_review_set_date (review, parse_date_time (json_object_get_string_member (object, "date_created")));

	return review;
}

static gboolean
parse_reviews (GsPlugin *plugin, const gchar *text, GsApp *app, GError **error)
{
	JsonParser *parser = NULL;
	JsonArray *array;
	gint i;
	gboolean result = FALSE;

	parser = json_parser_new ();
	if (!json_parser_load_from_data (parser, text, -1, error))
		goto out;
	if (!JSON_NODE_HOLDS_ARRAY (json_parser_get_root (parser)))
		goto out;
	array = json_node_get_array (json_parser_get_root (parser));
	for (i = 0; i < json_array_get_length (array); i++) {
		GsAppReview *review;

		/* Read in from JSON... (skip bad entries) */
		review = parse_review (json_array_get_element (array, i));
		if (!review)
			continue;

		gs_app_add_review (app, review);
	}
	result = TRUE;

out:
	g_clear_object (&parser);

	return result;
}

static gboolean
download_reviews (GsPlugin *plugin, GsApp *app, const gchar *package_name, GError **error)
{
	guint status_code;
	g_autofree gchar *uri = NULL;
	g_autoptr(SoupMessage) msg = NULL;
	g_auto(GStrv) split = NULL;

	/* Get the review stats using HTTP */
	// FIXME: This will only get the first page of reviews
	uri = g_strdup_printf ("%s/api/1.0/reviews/filter/any/any/any/any/%s/",
			       UBUNTU_REVIEWS_SERVER, package_name);
	msg = soup_message_new (SOUP_METHOD_GET, uri);
	if (!setup_networking (plugin, error))
		return FALSE;
	status_code = soup_session_send_message (plugin->priv->session, msg);
	if (status_code != SOUP_STATUS_OK) {
		g_set_error (error,
			     GS_PLUGIN_ERROR,
			     GS_PLUGIN_ERROR_FAILED,
			     "Failed to download Ubuntu reviews for %s: %s",
			     package_name, soup_status_get_phrase (status_code));
		return FALSE;
	}

	/* Extract the stats from the data */
	if (!parse_reviews (plugin, msg->response_body->data, app, error))
		return FALSE;

	return TRUE;
}

gboolean
gs_plugin_refine (GsPlugin *plugin,
		  GList **list,
		  GsPluginRefineFlags flags,
		  GCancellable *cancellable,
		  GError **error)
{
	GList *l;

	/* We only update reviews */
	if ((flags & GS_PLUGIN_REFINE_FLAGS_REQUIRE_REVIEWS) == 0)
		return TRUE;

	for (l = *list; l != NULL; l = l->next) {
		GsApp *app = GS_APP (l->data);
		GPtrArray *reviews, *sources;
		guint i;

		reviews = gs_app_get_reviews (app);
		if (reviews->len > 0)
			continue;

		sources = gs_app_get_sources (app);
		for (i = 0; i < sources->len; i++) {
			const gchar *package_name;
			gboolean ret;

			package_name = g_ptr_array_index (sources, i);
			ret = download_reviews (plugin, app, package_name, error);
			if (!ret)
				return FALSE;
		}
	}

	return TRUE;
}

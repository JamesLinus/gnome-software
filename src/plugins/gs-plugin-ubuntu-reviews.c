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
#include <oauth.h>

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

static void
add_string_member (JsonBuilder *builder, const gchar *name, const gchar *value)
{
	json_builder_set_member_name (builder, name);
	json_builder_add_string_value (builder, value);
}

static void
add_int_member (JsonBuilder *builder, const gchar *name, gint64 value)
{
	json_builder_set_member_name (builder, name);
	json_builder_add_int_value (builder, value);
}

static void
sign_message (SoupMessage *message, OAuthMethod method,
              const gchar *oauth_consumer_key, const gchar *oauth_consumer_secret,
              const gchar *oauth_token, const gchar *oauth_token_secret)
{
	g_autofree gchar *url = NULL, *oauth_authorization_parameters = NULL, *authorization_text = NULL;
	gchar **url_parameters = NULL;
	int url_parameters_length;

	url = soup_uri_to_string (soup_message_get_uri (message), FALSE);

	url_parameters_length = oauth_split_url_parameters(url, &url_parameters);
	oauth_sign_array2_process (&url_parameters_length, &url_parameters,
	                           NULL,
	                           method,
	                           message->method,
	                           oauth_consumer_key, oauth_consumer_secret,
	                           oauth_token, oauth_token_secret);
	oauth_authorization_parameters = oauth_serialize_url_sep (url_parameters_length, 1, url_parameters, ", ", 6);
	oauth_free_array (&url_parameters_length, &url_parameters);
	authorization_text = g_strdup_printf ("OAuth realm=\"Ratings and Reviews\", %s", oauth_authorization_parameters);
	soup_message_headers_append (message->request_headers, "Authorization", authorization_text);
}

static void
set_request (SoupMessage *message, JsonBuilder *builder)
{
	JsonGenerator *generator = json_generator_new ();
	json_generator_set_root (generator, json_builder_get_root (builder));
	gsize length;
	gchar *data = json_generator_to_data (generator, &length);
	soup_message_set_request (message, "application/json", SOUP_MEMORY_TAKE, data, length);
	g_object_unref (generator);
}

static gboolean
set_package_review (GsPlugin *plugin,
                    GsAppReview *review,
                    const gchar *package_name,
                    GError **error)
{
	g_autofree gchar *uri = NULL, *path = NULL;
	g_autofree gchar *oauth_consumer_key = NULL, *oauth_consumer_secret = NULL, *oauth_token = NULL, *oauth_token_secret = NULL;
	g_autoptr(GKeyFile) config = NULL;
	gint rating, n_stars = 0;
	g_autoptr(SoupMessage) msg = NULL;
	JsonBuilder *builder;
	guint status_code;

	/* Ubuntu reviews require a summary and description - just make one up for now */
	rating = gs_app_review_get_rating (review);
	if (rating > 80)
		n_stars = 5;
	else if (rating > 60)
		n_stars = 4;
	else if (rating > 40)
		n_stars = 3;
	else if (rating > 20)
		n_stars = 2;
	else
		n_stars = 1;

	/* Load OAuth token */
	// FIXME needs to integrate with GNOME Online Accounts / libaccounts
	config = g_key_file_new ();
	path = g_build_filename (g_get_user_config_dir (), "gnome-software", "ubuntu-one-credentials", NULL);
	g_key_file_load_from_file (config, path, G_KEY_FILE_NONE, NULL);
	oauth_consumer_key = g_key_file_get_string (config, "gnome-software", "consumer-key", NULL);
	oauth_consumer_secret = g_key_file_get_string (config, "gnome-software", "consumer-secret", NULL);
	oauth_token = g_key_file_get_string (config, "gnome-software", "token", NULL);
	oauth_token_secret = g_key_file_get_string (config, "gnome-software", "token-secret", NULL);
	if (!oauth_consumer_key || !oauth_consumer_secret || !oauth_token || !oauth_token_secret) {
		g_set_error (error,
			     GS_PLUGIN_ERROR,
			     GS_PLUGIN_ERROR_FAILED,
			     "%s", "No Ubuntu One OAuth tokens");
		return FALSE;
	}

	uri = g_strdup_printf ("%s/api/1.0/reviews/", UBUNTU_REVIEWS_SERVER);
	msg = soup_message_new (SOUP_METHOD_POST, uri);
	builder = json_builder_new ();
	json_builder_begin_object (builder);
	add_string_member (builder, "package_name", package_name);
	add_string_member (builder, "summary", gs_app_review_get_summary (review));
	add_string_member (builder, "review_text", gs_app_review_get_text (review));
	add_string_member (builder, "language", "en"); // FIXME
	add_string_member (builder, "origin", "ubuntu"); // FIXME gs_app_get_origin (app));
	add_string_member (builder, "distroseries", "xenial"); // FIXME
	add_string_member (builder, "version", gs_app_review_get_version (review));
	add_int_member (builder, "rating", n_stars);
	add_string_member (builder, "arch_tag", "amd64"); // FIXME
	json_builder_end_object (builder);
	set_request (msg, builder);
	g_object_unref (builder);
	sign_message (msg, OA_HMAC, oauth_consumer_key, oauth_consumer_secret, oauth_token, oauth_token_secret);

	status_code = soup_session_send_message (plugin->priv->session, msg);
	if (status_code != SOUP_STATUS_OK) {
		g_set_error (error,
			     GS_PLUGIN_ERROR,
			     GS_PLUGIN_ERROR_FAILED,
			     "Failed to post review: %s",
			     soup_status_get_phrase (status_code));
		return FALSE;
	}

	return TRUE;
}

gboolean
gs_plugin_app_set_review (GsPlugin *plugin,
			  GsApp *app,
			  GCancellable *cancellable,
			  GError **error)
{
	GsAppReview *review;
	GPtrArray *sources;
	const gchar *package_name;
	gboolean ret;
	guint i;
	g_autoptr(SoupMessage) msg = NULL;

	review = gs_app_get_self_review (app);
	g_return_val_if_fail (review != NULL, FALSE);

	/* get the package name */
	sources = gs_app_get_sources (app);
	if (sources->len == 0) {
		g_warning ("no package name for %s", gs_app_get_id (app));
		return TRUE;
	}

	if (!setup_networking (plugin, error))
		return FALSE;

	/* set rating for each package */
	for (i = 0; i < sources->len; i++) {
		package_name = g_ptr_array_index (sources, i);
		ret = set_package_review (plugin,
		                          review,
		                          package_name,
		                          error);
		if (!ret)
			return FALSE;
	}

	return TRUE;
}

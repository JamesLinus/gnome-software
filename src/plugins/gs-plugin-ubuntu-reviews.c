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
#include <sqlite3.h>

#include <gs-plugin.h>
#include <gs-utils.h>

#include "ubuntu-sso-credentials-proxy.h"

struct GsPluginPrivate {
	gchar                   *db_path;
	sqlite3                 *db;
	gsize			 db_loaded;
	SoupSession		*session;
	UbuntuSSOCredentials    *credentials;
};

typedef struct {
	gint64		 one_star_count;
	gint64		 two_star_count;
	gint64		 three_star_count;
	gint64		 four_star_count;
	gint64		 five_star_count;
} Histogram;

const gchar *
gs_plugin_get_name (void)
{
	return "ubuntu-reviews";
}

#define UBUNTU_REVIEWS_SERVER		"https://reviews.ubuntu.com/reviews"

/* Download new stats every three months */
// FIXME: Much shorter time?
#define REVIEW_STATS_AGE_MAX		(60 * 60 * 24 * 7 * 4 * 3)

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

	plugin->priv->db_path = g_build_filename (g_get_user_data_dir (),
						  "gnome-software",
						  "ubuntu-reviews.db",
						  NULL);
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
	g_clear_object (&plugin->priv->credentials);

	if (plugin->priv->db != NULL)
		sqlite3_close (plugin->priv->db);
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

static gint
get_timestamp_sqlite_cb (void *data, gint argc,
			 gchar **argv, gchar **col_name)
{
	gint64 *timestamp = (gint64 *) data;
	*timestamp = g_ascii_strtoll (argv[0], NULL, 10);
	return 0;
}

static gboolean
set_package_stats (GsPlugin *plugin,
		   const gchar *package_name,
		   Histogram *histogram,
		   GError **error)
{
	char *error_msg = NULL;
	gint result;
	g_autofree gchar *statement = NULL;

	statement = g_strdup_printf ("INSERT OR REPLACE INTO review_stats (package_name, "
				     "one_star_count, two_star_count, three_star_count, "
                                     "four_star_count, five_star_count) "
				     "VALUES ('%s', '%" G_GINT64_FORMAT "', '%" G_GINT64_FORMAT"', '%" G_GINT64_FORMAT "', '%" G_GINT64_FORMAT "', '%" G_GINT64_FORMAT "');",
				     package_name, histogram->one_star_count, histogram->two_star_count,
				     histogram->three_star_count, histogram->four_star_count, histogram->five_star_count);
	result = sqlite3_exec (plugin->priv->db, statement, NULL, NULL, &error_msg);
	if (result != SQLITE_OK) {
		g_set_error (error,
			     GS_PLUGIN_ERROR,
			     GS_PLUGIN_ERROR_FAILED,
			     "SQL error: %s", error_msg);
		sqlite3_free (error_msg);
		return FALSE;
	}

	return TRUE;
}

static gboolean
set_timestamp (GsPlugin *plugin,
	       const gchar *type,
	       GError **error)
{
	char *error_msg = NULL;
	gint result;
	g_autofree gchar *statement = NULL;

	statement = g_strdup_printf ("INSERT OR REPLACE INTO timestamps (key, value) "
				     "VALUES ('%s', '%" G_GINT64_FORMAT "');",
				     type,
				     g_get_real_time () / G_USEC_PER_SEC);
	result = sqlite3_exec (plugin->priv->db, statement, NULL, NULL, &error_msg);
	if (result != SQLITE_OK) {
		g_set_error (error,
			     GS_PLUGIN_ERROR,
			     GS_PLUGIN_ERROR_FAILED,
			     "SQL error: %s", error_msg);
		sqlite3_free (error_msg);
		return FALSE;
	}
	return TRUE;
}

static gint
get_rating_sqlite_cb (void *data,
		      gint argc,
		      gchar **argv,
		      gchar **col_name)
{
	Histogram *histogram = (Histogram *) data;
	histogram->one_star_count = g_ascii_strtoll (argv[0], NULL, 10);
	histogram->two_star_count = g_ascii_strtoll (argv[1], NULL, 10);
	histogram->three_star_count = g_ascii_strtoll (argv[2], NULL, 10);
	histogram->four_star_count = g_ascii_strtoll (argv[3], NULL, 10);
	histogram->five_star_count = g_ascii_strtoll (argv[4], NULL, 10);
	return 0;
}

static gboolean
get_rating (GsPlugin *plugin,
	    const gchar *package_name,
	    gint *rating,
	    GError **error)
{
	Histogram histogram = { 0, 0, 0, 0, 0 };
	gchar *error_msg = NULL;
	gint result, n_ratings;
	g_autofree gchar *statement = NULL;

	/* Get histogram from the database */
	statement = g_strdup_printf ("SELECT one_star_count, two_star_count, three_star_count, four_star_count, five_star_count FROM review_stats "
				     "WHERE package_name = '%s'", package_name);
	result = sqlite3_exec (plugin->priv->db,
			       statement,
			       get_rating_sqlite_cb,
			       &histogram,
			       &error_msg);
	if (result != SQLITE_OK) {
		g_set_error (error,
			     GS_PLUGIN_ERROR,
			     GS_PLUGIN_ERROR_FAILED,
			     "SQL error: %s", error_msg);
		sqlite3_free (error_msg);
		return FALSE;
	}

	/* Convert to a rating */
	// FIXME: Convert to a Wilson score
	n_ratings = histogram.one_star_count + histogram.two_star_count + histogram.three_star_count + histogram.four_star_count + histogram.five_star_count;
	if (n_ratings == 0)
		*rating = -1;
	else
		*rating = ((histogram.one_star_count * 20) + (histogram.two_star_count * 40) + (histogram.three_star_count * 60) + (histogram.four_star_count * 80) + (histogram.five_star_count * 100)) / n_ratings;
g_warning ("%s %zi %zi %zi %zi %zi / %d -> %d", package_name, histogram.one_star_count, histogram.two_star_count, histogram.three_star_count, histogram.four_star_count, histogram.five_star_count, n_ratings, *rating);

	return TRUE;
}

static gboolean
parse_histogram (const gchar *text, Histogram *histogram)
{
	JsonParser *parser = NULL;
	JsonArray *array;
	gboolean result = FALSE;

	/* Histogram is a five element JSON array, e.g. "[1, 3, 5, 8, 4]" */
	parser = json_parser_new ();
	if (!json_parser_load_from_data (parser, text, -1, NULL))
		goto out;
	if (!JSON_NODE_HOLDS_ARRAY (json_parser_get_root (parser)))
		goto out;
	array = json_node_get_array (json_parser_get_root (parser));
	if (json_array_get_length (array) != 5)
		goto out;
	histogram->one_star_count = json_array_get_int_element (array, 0);
	histogram->two_star_count = json_array_get_int_element (array, 1);
	histogram->three_star_count = json_array_get_int_element (array, 2);
	histogram->four_star_count = json_array_get_int_element (array, 3);
	histogram->five_star_count = json_array_get_int_element (array, 4);
	result = TRUE;

out:
	g_clear_object (&parser);

	return result;
}

static gboolean
parse_review_entry (JsonNode *node, const gchar **package_name, Histogram *histogram)
{
	JsonObject *object;
	const gchar *name = NULL, *histogram_text = NULL;

	if (!JSON_NODE_HOLDS_OBJECT (node))
		return FALSE;

	object = json_node_get_object (node);

	name = json_object_get_string_member (object, "package_name");
	histogram_text = json_object_get_string_member (object, "histogram");
	if (!name || !histogram_text)
		return FALSE;

	if (!parse_histogram (histogram_text, histogram))
		return FALSE;
	*package_name = name;

	return TRUE;
}

static gboolean
parse_review_entries (GsPlugin *plugin, const gchar *text, GError **error)
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
		const gchar *package_name;
		Histogram histogram;

		/* Read in from JSON... (skip bad entries) */
		if (!parse_review_entry (json_array_get_element (array, i), &package_name, &histogram))
			continue;

		/* ...write into the database (abort everything if can't write) */
		if (!set_package_stats (plugin, package_name, &histogram, error))
			goto out;
	}
	result = TRUE;

out:
	g_clear_object (&parser);

	return result;
}

static gboolean
download_review_stats (GsPlugin *plugin, GError **error)
{
	guint status_code;
	g_autofree gchar *uri = NULL;
	g_autoptr(SoupMessage) msg = NULL;
	g_auto(GStrv) split = NULL;

	/* Get the review stats using HTTP */
	uri = g_strdup_printf ("%s/api/1.0/review-stats/any/any/",
			       UBUNTU_REVIEWS_SERVER);
	msg = soup_message_new (SOUP_METHOD_GET, uri);
	if (!setup_networking (plugin, error))
		return FALSE;
	status_code = soup_session_send_message (plugin->priv->session, msg);
	if (status_code != SOUP_STATUS_OK) {
		g_set_error (error,
			     GS_PLUGIN_ERROR,
			     GS_PLUGIN_ERROR_FAILED,
			     "Failed to download Ubuntu reviews dump: %s",
			     soup_status_get_phrase (status_code));
		return FALSE;
	}

	/* Extract the stats from the data */
	if (!parse_review_entries (plugin, msg->response_body->data, error))
		return FALSE;

	/* Record the time we downloaded it */
	return set_timestamp (plugin, "stats_mtime", error);
}

static gboolean
load_database (GsPlugin *plugin, GError **error)
{
	const gchar *statement;
	gboolean rebuild_ratings = FALSE;
	char *error_msg = NULL;
	gint result;
	gint64 stats_mtime = 0;
	gint64 now;
	g_autoptr(GError) error_local = NULL;

	g_debug ("trying to open database '%s'", plugin->priv->db_path);
	if (!gs_mkdir_parent (plugin->priv->db_path, error))
		return FALSE;
	result = sqlite3_open (plugin->priv->db_path, &plugin->priv->db);
	if (result != SQLITE_OK) {
		g_set_error (error,
			     GS_PLUGIN_ERROR,
			     GS_PLUGIN_ERROR_FAILED,
			     "Can't open Ubuntu review statistics database: %s",
			     sqlite3_errmsg (plugin->priv->db));
		return FALSE;
	}

	/* We don't need to keep doing fsync */
	sqlite3_exec (plugin->priv->db, "PRAGMA synchronous=OFF",
		      NULL, NULL, NULL);

	/* Create a table to store the stats */
	result = sqlite3_exec (plugin->priv->db, "SELECT * FROM review_stats LIMIT 1", NULL, NULL, &error_msg);
	if (result != SQLITE_OK) {
		g_debug ("creating table to repair: %s", error_msg);
		sqlite3_free (error_msg);
		statement = "CREATE TABLE review_stats ("
			    "package_name TEXT PRIMARY KEY,"
			    "one_star_count INTEGER DEFAULT 0,"
			    "two_star_count INTEGER DEFAULT 0,"
			    "three_star_count INTEGER DEFAULT 0,"
			    "four_star_count INTEGER DEFAULT 0,"
			    "five_star_count INTEGER DEFAULT 0);";
		sqlite3_exec (plugin->priv->db, statement, NULL, NULL, NULL);
		rebuild_ratings = TRUE;
	}

	/* Create a table to store local reviews */
	result = sqlite3_exec (plugin->priv->db, "SELECT * FROM reviews LIMIT 1", NULL, NULL, &error_msg);
	if (result != SQLITE_OK) {
		g_debug ("creating table to repair: %s", error_msg);
		sqlite3_free (error_msg);
		statement = "CREATE TABLE reviews ("
			    "package_name TEXT PRIMARY KEY,"
			    "id TEXT,"
			    "version TEXT,"
			    "date TEXT,"
			    "rating INTEGER,"
                            "summary TEXT,"
                            "text TEXT);";
		sqlite3_exec (plugin->priv->db, statement, NULL, NULL, NULL);
		rebuild_ratings = TRUE;
	}

	/* Create a table to store timestamps */
	result = sqlite3_exec (plugin->priv->db,
			       "SELECT value FROM timestamps WHERE key = 'stats_mtime' LIMIT 1",
			       get_timestamp_sqlite_cb, &stats_mtime,
			       &error_msg);
	if (result != SQLITE_OK) {
		g_debug ("creating table to repair: %s", error_msg);
		sqlite3_free (error_msg);
		statement = "CREATE TABLE timestamps ("
			    "key TEXT PRIMARY KEY,"
			    "value INTEGER DEFAULT 0);";
		sqlite3_exec (plugin->priv->db, statement, NULL, NULL, NULL);

		/* Set the time of database creation */
		if (!set_timestamp (plugin, "stats_ctime", error))
			return FALSE;
	}

	/* Download data if we have none or it is out of date */
	now = g_get_real_time () / G_USEC_PER_SEC;
	if (stats_mtime == 0 || rebuild_ratings) {
		g_debug ("No Ubuntu review statistics");
		if (!download_review_stats (plugin, &error_local)) {
			g_warning ("Failed to get Ubuntu review statistics: %s",
				   error_local->message);
			return TRUE;
		}
	} else if (now - stats_mtime > REVIEW_STATS_AGE_MAX) {
		g_debug ("Ubuntu review statistics was %" G_GINT64_FORMAT
			 " days old, so regetting",
			 (now - stats_mtime) / ( 60 * 60 * 24));
		if (!download_review_stats (plugin, error))
			return FALSE;
	} else {
		g_debug ("Ubuntu review statistics %" G_GINT64_FORMAT
			 " days old, so no need to redownload",
			 (now - stats_mtime) / ( 60 * 60 * 24));
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

static gboolean
refine_rating (GsPlugin *plugin, GsApp *app, GError **error)
{
	GPtrArray *sources;
	guint i;

	/* Load database once */
	if (g_once_init_enter (&plugin->priv->db_loaded)) {
		gboolean ret = load_database (plugin, error);
		g_once_init_leave (&plugin->priv->db_loaded, TRUE);
		if (!ret)
			return FALSE;
	}

	/* Skip if already has a rating */
	if (gs_app_get_rating (app) != -1)
		return TRUE;

	sources = gs_app_get_sources (app);
	for (i = 0; i < sources->len; i++) {
		const gchar *package_name;
		gint rating;
		gboolean ret;

		/* If we have a local review, use that as the rating */
		// FIXME

		/* Otherwise use the statistics */
		package_name = g_ptr_array_index (sources, i);
		ret = get_rating (plugin, package_name, &rating, error);
		if (!ret)
			return FALSE;
		if (rating != -1) {
			g_debug ("ubuntu-reviews setting rating on %s to %i%%",
				 package_name, rating);
			gs_app_set_rating (app, rating);
			gs_app_set_rating_confidence (app, 100);
			gs_app_set_rating_kind (app, GS_APP_RATING_KIND_SYSTEM);
			if (rating > 80)
				gs_app_add_kudo (app, GS_APP_KUDO_POPULAR);
		}
	}

	return TRUE;
}

static gboolean
refine_reviews (GsPlugin *plugin, GsApp *app, GError **error)
{
	GPtrArray *sources;
	guint i;

	/* Skip if already has reviews */
	if (gs_app_get_reviews (app)->len > 0)
		return TRUE;

	sources = gs_app_get_sources (app);
	for (i = 0; i < sources->len; i++) {
		const gchar *package_name;
		gboolean ret;

		package_name = g_ptr_array_index (sources, i);
		ret = download_reviews (plugin, app, package_name, error);
		if (!ret)
			return FALSE;
	}

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
	gboolean ret = TRUE;

	for (l = *list; l != NULL; l = l->next) {
		GsApp *app = GS_APP (l->data);

		if ((flags & GS_PLUGIN_REFINE_FLAGS_REQUIRE_RATING) != 0) {
			if (!refine_rating (plugin, app, error))
				return FALSE;
		}
		if ((flags & GS_PLUGIN_REFINE_FLAGS_REQUIRE_REVIEWS) != 0) {
			if (!refine_reviews (plugin, app, error))
				return FALSE;
		}
	}

	return ret;
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

gboolean
gs_plugin_get_supports_reviews (GsPlugin *plugin)
{
	return TRUE;
}

const gchar *
gs_plugin_get_review_auth (GsPlugin *plugin)
{
	return "ubuntuone";
}

static gboolean
send_review (GsPlugin    *plugin,
             GsAppReview *review,
             const gchar *package_name,
             const gchar *consumer_key,
             const gchar *consumer_secret,
             const gchar *token,
             const gchar *token_secret,
             GError **error)
{
	gint rating;
	gint n_stars;
	g_autofree gchar *uri;
	g_autoptr(SoupMessage) msg;
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

	/* Create message for reviews.ubuntu.com */
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
	sign_message (msg, OA_HMAC, consumer_key, consumer_secret, token, token_secret);

	/* Send to the server */
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

typedef struct CredentialsFoundInfo {
	GsPlugin    *plugin;
	GsAppReview *review;
	gchar       *package_name;
} CredentialsFoundInfo;

static void
credentials_found (CredentialsFoundInfo *info,
                   const gchar          *app_name,
                   GVariant             *credentials,
                   UbuntuSSOCredentials *proxy)
{
	const gchar *consumer_key;
	const gchar *consumer_secret;
	const gchar *token;
	const gchar *token_secret;

	g_signal_handlers_disconnect_by_func (proxy, credentials_found, info);

	if (credentials &&
	    g_variant_lookup (credentials, "consumer_key", "&s", &consumer_key) &&
	    g_variant_lookup (credentials, "consumer_secret", "&s", &consumer_secret) &&
	    g_variant_lookup (credentials, "token", "&s", &token) &&
	    g_variant_lookup (credentials, "token_secret", "&s", &token_secret))
		send_review (info->plugin,
		             info->review,
		             info->package_name,
		             consumer_key,
		             consumer_secret,
		             token,
		             token_secret,
		             NULL);

	g_free (info->package_name);
	g_object_unref (info->review);
	g_object_unref (info->plugin);
	g_free (info);
}

static gboolean
set_package_review (GsPlugin *plugin,
                    GsAppReview *review,
                    const gchar *package_name,
                    GError **error)
{
	GVariant *credentials;
	const gchar *consumer_key;
	const gchar *consumer_secret;
	const gchar *token;
	const gchar *token_secret;
	CredentialsFoundInfo *info;

	/* Write review into database so we can easily access it */
	// FIXME

	/* Load OAuth token */
	if (!plugin->priv->credentials) {
		plugin->priv->credentials = ubuntu_sso_credentials_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
		                                                                           G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
		                                                                           "com.ubuntu.sso",
		                                                                           "/com/ubuntu/sso/credentials",
		                                                                           NULL,
		                                                                           error);

		if (!plugin->priv->credentials)
			return FALSE;
	}

	if (!ubuntu_sso_credentials_call_find_credentials_and_wait_sync (plugin->priv->credentials,
	                                                                 "Ubuntu One",
	                                                                 g_variant_new_array (G_VARIANT_TYPE ("{ss}"), NULL, 0),
	                                                                 &credentials,
	                                                                 NULL,
	                                                                 error) ||
	    !credentials ||
	    !g_variant_lookup (credentials, "consumer_key", "&s", &consumer_key) ||
	    !g_variant_lookup (credentials, "consumer_secret", "&s", &consumer_secret) ||
	    !g_variant_lookup (credentials, "token", "&s", &token) ||
	    !g_variant_lookup (credentials, "token_secret", "&s", &token_secret)) {
		if (credentials)
			g_variant_unref (credentials);

		info = g_new (CredentialsFoundInfo, 1);
		info->plugin = g_object_ref (plugin);
		info->review = g_object_ref (review);
		info->package_name = g_strdup (package_name);

		g_signal_connect_swapped (plugin->priv->credentials,
		                          "credentials-found",
		                          G_CALLBACK (credentials_found),
		                          info);

		return ubuntu_sso_credentials_call_login_sync (plugin->priv->credentials,
		                                               "Ubuntu One",
		                                               g_variant_new_array (G_VARIANT_TYPE ("{ss}"), NULL, 0),
		                                               NULL,
		                                               error);
	}

	g_variant_unref (credentials);

	return send_review (plugin,
	                    review,
	                    package_name,
	                    consumer_key,
	                    consumer_secret,
	                    token,
	                    token_secret,
	                    error);
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

	/* Load database once */
	if (g_once_init_enter (&plugin->priv->db_loaded)) {
		gboolean ret = load_database (plugin, error);
		g_once_init_leave (&plugin->priv->db_loaded, TRUE);
		if (!ret)
			return FALSE;
	}

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

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

#include "config.h"

#include "gs-app-review.h"

struct _GsAppReview
{
	GObject			 parent_instance;

	gchar			*summary;
	gchar			*text;
	gint			 rating;
	gchar			*version;
	gchar			*reviewer;
	GDateTime		*date;  
};

enum {
	PROP_0,
	PROP_SUMMARY,
	PROP_TEXT,
	PROP_RATING,
	PROP_VERSION,
	PROP_REVIEWER,
	PROP_DATE,
	PROP_LAST
};

G_DEFINE_TYPE (GsAppReview, gs_app_review, G_TYPE_OBJECT)

/**
 * gs_app_review_get_summary:
 **/
const gchar *
gs_app_review_get_summary (GsAppReview *review)
{
	g_return_val_if_fail (GS_IS_APP_REVIEW (review), NULL);
	return review->summary;
}

/**
 * gs_app_review_set_summary:
 */
void
gs_app_review_set_summary (GsAppReview *review, const gchar *summary)
{
	g_return_if_fail (GS_IS_APP_REVIEW (review));
	g_free (review->summary);
	review->summary = g_strdup (summary);
}

/**
 * gs_app_review_get_text:
 **/
const gchar *
gs_app_review_get_text (GsAppReview *review)
{
	g_return_val_if_fail (GS_IS_APP_REVIEW (review), NULL);
	return review->text;
}

/**
 * gs_app_review_set_text:
 */
void
gs_app_review_set_text (GsAppReview *review, const gchar *text)
{
	g_return_if_fail (GS_IS_APP_REVIEW (review));
	g_free (review->text);
	review->text = g_strdup (text);
}

/**
 * gs_app_review_get_rating:
 **/
gint
gs_app_review_get_rating (GsAppReview *review)
{
	g_return_val_if_fail (GS_IS_APP_REVIEW (review), -1);
	return review->rating;
}

/**
 * gs_app_review_set_rating:
 */
void
gs_app_review_set_rating (GsAppReview *review, gint rating)
{
	g_return_if_fail (GS_IS_APP_REVIEW (review));
	review->rating = rating;
}

/**
 * gs_app_review_get_reviewer:
 **/
const gchar *
gs_app_review_get_reviewer (GsAppReview *review)
{
	g_return_val_if_fail (GS_IS_APP_REVIEW (review), NULL);
	return review->reviewer;
}

/**
 * gs_app_review_set_version:
 */
void
gs_app_review_set_version (GsAppReview *review, const gchar *version)
{
	g_return_if_fail (GS_IS_APP_REVIEW (review));
	g_free (review->version);
	review->version = g_strdup (version);
}

/**
 * gs_app_review_get_version:
 **/
const gchar *
gs_app_review_get_version (GsAppReview *review)
{
	g_return_val_if_fail (GS_IS_APP_REVIEW (review), NULL);
	return review->version;
}

/**
 * gs_app_review_set_reviewer:
 */
void
gs_app_review_set_reviewer (GsAppReview *review, const gchar *reviewer)
{
	g_return_if_fail (GS_IS_APP_REVIEW (review));
	g_free (review->reviewer);
	review->reviewer = g_strdup (reviewer);
}

/**
 * gs_app_review_get_date:
 **/
GDateTime *
gs_app_review_get_date (GsAppReview *review)
{
	g_return_val_if_fail (GS_IS_APP_REVIEW (review), 0);
	return review->date;
}

/**
 * gs_app_review_set_date:
 */
void
gs_app_review_set_date (GsAppReview *review, GDateTime *date)
{
	g_return_if_fail (GS_IS_APP_REVIEW (review));
	g_clear_pointer (&review->date, g_date_time_unref);
	review->date = g_date_time_ref (date);
}

/**
 * gs_app_review_get_property:
 */
static void
gs_app_review_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GsAppReview *review = GS_APP_REVIEW (object);

	switch (prop_id) {
	case PROP_SUMMARY:
		g_value_set_string (value, review->summary);
		break;
	case PROP_TEXT:
		g_value_set_string (value, review->text);
		break;
	case PROP_RATING:
		g_value_set_int (value, review->rating);
		break;
	case PROP_VERSION:
		g_value_set_string (value, review->version);
		break;
	case PROP_REVIEWER:
		g_value_set_string (value, review->reviewer);
		break;
	case PROP_DATE:
		g_value_set_object (value, review->date);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gs_app_review_set_property:
 */
static void
gs_app_review_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GsAppReview *review = GS_APP_REVIEW (object);

	switch (prop_id) {
	case PROP_SUMMARY:
		gs_app_review_set_summary (review, g_value_get_string (value));
		break;
	case PROP_TEXT:
		gs_app_review_set_text (review, g_value_get_string (value));
		break;
	case PROP_RATING:
		gs_app_review_set_rating (review, g_value_get_int (value));
		break;
	case PROP_VERSION:
		gs_app_review_set_version (review, g_value_get_string (value));
		break;
	case PROP_REVIEWER:
		gs_app_review_set_reviewer (review, g_value_get_string (value));
		break;
	case PROP_DATE:
		gs_app_review_set_date (review, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gs_app_review_dispose (GObject *object)
{
	GsAppReview *review = GS_APP_REVIEW (object);

	g_clear_pointer (&review->date, g_date_time_unref);

	G_OBJECT_CLASS (gs_app_review_parent_class)->dispose (object);
}

static void
gs_app_review_finalize (GObject *object)
{
	GsAppReview *review = GS_APP_REVIEW (object);

	g_free (review->summary);
	g_free (review->text);
	g_free (review->reviewer);

	G_OBJECT_CLASS (gs_app_review_parent_class)->finalize (object);
}

static void
gs_app_review_class_init (GsAppReviewClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = gs_app_review_dispose;
	object_class->finalize = gs_app_review_finalize;
	object_class->get_property = gs_app_review_get_property;
	object_class->set_property = gs_app_review_set_property;

	/**
	 * GsApp:summary:
	 */
	pspec = g_param_spec_string ("summary", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_SUMMARY, pspec);

  	/**
	 * GsApp:text:
	 */
	pspec = g_param_spec_string ("text", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_TEXT, pspec);

  	/**
	 * GsApp:rating:
	 */
	pspec = g_param_spec_int ("rating", NULL, NULL,
				  -1, 100, -1,
				  G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_RATING, pspec);

  	/**
	 * GsApp:version:
	 */
	pspec = g_param_spec_string ("version", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_VERSION, pspec);

  	/**
	 * GsApp:reviewer:
	 */
	pspec = g_param_spec_string ("reviewer", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_REVIEWER, pspec);


  	/**
	 * GsApp:date:
	 */
	pspec = g_param_spec_object ("date", NULL, NULL,
				     GS_TYPE_APP_REVIEW,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_DATE, pspec);
}

/**
 * gs_app_review_init:
 **/
static void
gs_app_review_init (GsAppReview *review)
{
	review->rating = -1;
}

/**
 * gs_app_review_new:
 *
 * Return value: a new GsAppReview object.
 **/
GsAppReview *
gs_app_review_new (void)
{
	GsAppReview *review;
	review = g_object_new (GS_TYPE_APP_REVIEW, NULL);
	return GS_APP_REVIEW (review);
}

/* vim: set noexpandtab: */

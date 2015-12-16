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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gs-app-review-row.h"
#include "gs-star-widget.h"

struct _GsAppReviewRow
{
	GtkListBoxRow	 parent_instance;

	GsAppReview	*review;
	GtkWidget	*stars;
	GtkWidget	*summary_label;
	GtkWidget	*author_label;
	GtkWidget	*text_label;
};

G_DEFINE_TYPE (GsAppReviewRow, gs_app_review_row, GTK_TYPE_LIST_BOX_ROW)

static void
gs_app_review_row_refresh (GsAppReviewRow *row)
{
	const gchar *reviewer;
	GDateTime *date;
	gchar *text;

	gs_star_widget_set_rating (GS_STAR_WIDGET (row->stars), GS_APP_RATING_KIND_SYSTEM, gs_app_review_get_rating (row->review));
	reviewer = gs_app_review_get_reviewer (row->review);
	date = gs_app_review_get_date (row->review);
	if (reviewer && date) {
		gchar *date_text = g_date_time_format (date, "%e %B %Y");
		text = g_strdup_printf ("%s, %s", reviewer, date_text);
		g_free (date_text);
	}
	else if (reviewer)
		text = g_strdup (reviewer);
	else if (date)
		text = g_date_time_format (date, "%e %B %Y");
	else
		text = g_strdup ("");
	gtk_label_set_text (GTK_LABEL (row->author_label), text);
	g_free (text);
	gtk_label_set_text (GTK_LABEL (row->summary_label), gs_app_review_get_summary (row->review));
	gtk_label_set_text (GTK_LABEL (row->text_label), gs_app_review_get_text (row->review));
}

static gboolean
gs_app_review_row_refresh_idle (gpointer user_data)
{
	GsAppReviewRow *row = GS_APP_REVIEW_ROW (user_data);

	gs_app_review_row_refresh (row);

	g_object_unref (row);
	return G_SOURCE_REMOVE;
}

static void
gs_app_review_row_notify_props_changed_cb (GsApp *app,
					   GParamSpec *pspec,
					   GsAppReviewRow *row)
{
	g_idle_add (gs_app_review_row_refresh_idle, g_object_ref (row));
}

static void
gs_app_review_row_set_review (GsAppReviewRow *row, GsAppReview *review)
{
	row->review = g_object_ref (review);

	g_signal_connect_object (row->review, "notify::state",
				 G_CALLBACK (gs_app_review_row_notify_props_changed_cb),
				 row, 0);
	gs_app_review_row_refresh (row);
}

static void
gs_app_review_row_init (GsAppReviewRow *row)
{
	gtk_widget_set_has_window (GTK_WIDGET (row), FALSE);
	gtk_widget_init_template (GTK_WIDGET (row));
}

static void
gs_app_review_row_dispose (GObject *object)
{
	GsAppReviewRow *row = GS_APP_REVIEW_ROW (object);

	g_clear_object (&row->review);

	G_OBJECT_CLASS (gs_app_review_row_parent_class)->dispose (object);
}

static void
gs_app_review_row_class_init (GsAppReviewRowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = gs_app_review_row_dispose;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Software/gs-app-review-row.ui");

	gtk_widget_class_bind_template_child (widget_class, GsAppReviewRow, stars);
	gtk_widget_class_bind_template_child (widget_class, GsAppReviewRow, summary_label);
	gtk_widget_class_bind_template_child (widget_class, GsAppReviewRow, author_label);
	gtk_widget_class_bind_template_child (widget_class, GsAppReviewRow, text_label);
}

/**
 * gs_app_review_row_new:
 * @review: The review to show
 *
 * Create a widget suitable for showing an application review.
 *
 * Return value: A new @GsAppReviewRow.
 **/
GtkWidget *
gs_app_review_row_new (GsAppReview *review)
{
	GtkWidget *row;

	g_return_val_if_fail (GS_IS_APP_REVIEW (review), NULL);

	row = g_object_new (GS_TYPE_APP_REVIEW_ROW, NULL);
	gs_app_review_row_set_review (GS_APP_REVIEW_ROW (row), review);

	return row;
}

/* vim: set noexpandtab: */

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

#include "gs-review-widget.h"
#include "gs-star-widget.h"

typedef struct
{
	GtkWidget	*stars;
	GtkWidget	*summary_label;
	GtkWidget	*author_label;
	GtkWidget	*text_label;
	gchar		*author;
	GDateTime	*date;
} GsReviewWidgetPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GsReviewWidget, gs_review_widget, GTK_TYPE_BIN)

/**
 * gs_review_widget_set_rating:
 * @review: The review to update.
 * @rating: The rating, from 0-100.
 *
 * Set the rating given in this review.
 **/
void
gs_review_widget_set_rating (GsReviewWidget *review, gint rating)
{
	GsReviewWidgetPrivate *priv;
	g_return_if_fail (GS_IS_REVIEW_WIDGET (review));
	priv = gs_review_widget_get_instance_private (review);
	gs_star_widget_set_rating (GS_STAR_WIDGET (priv->stars), GS_APP_RATING_KIND_SYSTEM, rating);
}

static void
update_author_label (GsReviewWidget *review)
{
	GsReviewWidgetPrivate *priv = gs_review_widget_get_instance_private (review);
	gchar *text;

	if (priv->author && priv->date) {
		gchar *date_text = g_date_time_format (priv->date, "%e %B %Y");
		text = g_strdup_printf ("%s, %s", priv->author, date_text);
		g_free (date_text);
	}
	else if (priv->author)
		text = g_strdup (priv->author);
	else if (priv->date)
		text = g_date_time_format (priv->date, "%e %B %Y");
	else
		text = g_strdup ("");
	gtk_label_set_text (GTK_LABEL (priv->author_label), text);
	g_free (text);
}

/**
 * gs_review_widget_set_author:
 * @review: The review to update.
 * @author: The author name.
 *
 * Set the author who wrote the review.
 **/
void
gs_review_widget_set_author (GsReviewWidget *review, const gchar *author)
{
	GsReviewWidgetPrivate *priv;
	g_return_if_fail (GS_IS_REVIEW_WIDGET (review));
	priv = gs_review_widget_get_instance_private (review);
	g_free (priv->author);
	priv->author = g_strdup (author);
	update_author_label (review);
}

/**
 * gs_review_widget_set_date:
 * @review: The review to update.
 * @date: The review date.
 *
 * Set the date the review was created.
 **/
void
gs_review_widget_set_date (GsReviewWidget *review, GDateTime *date)
{
	GsReviewWidgetPrivate *priv;
	g_return_if_fail (GS_IS_REVIEW_WIDGET (review));
	priv = gs_review_widget_get_instance_private (review);
	g_clear_pointer (&priv->date, g_date_time_unref);
	priv->date = g_date_time_ref (date);
	update_author_label (review);
}

/**
 * gs_review_widget_set_summary:
 * @review: The review to update.
 * @summary: Review summary.
 *
 * Set the summary for the review, usually a short line, e.g. "This application is great".
 **/
void
gs_review_widget_set_summary (GsReviewWidget *review, const gchar *summary)
{
	GsReviewWidgetPrivate *priv;
	g_return_if_fail (GS_IS_REVIEW_WIDGET (review));
	priv = gs_review_widget_get_instance_private (review);
	gtk_label_set_text (GTK_LABEL (priv->summary_label), summary);
}

/**
 * gs_review_widget_set_text:
 * @review: The review to update.
 * @text: The review text.
 *
 * Set the text of the review, usually a paragraph or more describing this application. e.g.
 * "This application is really useful for the problem I had.
 * It has a number of great features that make it useful.
 * I would recommend this to all my friends".
 **/
void
gs_review_widget_set_text (GsReviewWidget *review, const gchar *text)
{
	GsReviewWidgetPrivate *priv;
	g_return_if_fail (GS_IS_REVIEW_WIDGET (review));
	priv = gs_review_widget_get_instance_private (review);
	gtk_label_set_text (GTK_LABEL (priv->text_label), text);
}

static void
gs_review_widget_init (GsReviewWidget *review)
{
	gtk_widget_set_has_window (GTK_WIDGET (review), FALSE);
	gtk_widget_init_template (GTK_WIDGET (review));
}

static void
gs_review_widget_dispose (GObject *object)
{
	GsReviewWidget *review = GS_REVIEW_WIDGET (object);
	GsReviewWidgetPrivate *priv = gs_review_widget_get_instance_private (review);

	g_clear_pointer (&priv->date, g_date_time_unref);

	G_OBJECT_CLASS (gs_review_widget_parent_class)->dispose (object);
}

static void
gs_review_widget_finalize (GObject *object)
{
	GsReviewWidget *review = GS_REVIEW_WIDGET (object);
	GsReviewWidgetPrivate *priv = gs_review_widget_get_instance_private (review);

	g_free (priv->author);

	G_OBJECT_CLASS (gs_review_widget_parent_class)->finalize (object);
}

static void
gs_review_widget_class_init (GsReviewWidgetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = gs_review_widget_dispose;
	object_class->finalize = gs_review_widget_finalize;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Software/gs-review-widget.ui");

	gtk_widget_class_bind_template_child_private (widget_class, GsReviewWidget, stars);
	gtk_widget_class_bind_template_child_private (widget_class, GsReviewWidget, summary_label);
	gtk_widget_class_bind_template_child_private (widget_class, GsReviewWidget, author_label);
	gtk_widget_class_bind_template_child_private (widget_class, GsReviewWidget, text_label);
}

/**
 * gs_review_widget_new:
 *
 * Create a widget suitable for showing an application review.
 *
 * Return value: A new @GsReviewWidget.
 **/
GtkWidget *
gs_review_widget_new (void)
{
	GsReviewWidget *review;
	review = g_object_new (GS_TYPE_REVIEW_WIDGET, NULL);
	return GTK_WIDGET (review);
}

/* vim: set noexpandtab: */

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
} GsReviewWidgetPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GsReviewWidget, gs_review_widget, GTK_TYPE_BIN)

/**
 * gs_review_widget_set_rating:
 **/
void
gs_review_widget_set_rating (GsReviewWidget *review, gint rating)
{
	GsReviewWidgetPrivate *priv;
	g_return_if_fail (GS_IS_REVIEW_WIDGET (review));
	priv = gs_review_widget_get_instance_private (review);
	gs_star_widget_set_rating (GS_STAR_WIDGET (priv->stars), GS_APP_RATING_KIND_USER, rating);
}

/**
 * gs_review_widget_set_summary:
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
 **/
void
gs_review_widget_set_text (GsReviewWidget *review, const gchar *text)
{
	GsReviewWidgetPrivate *priv;
	g_return_if_fail (GS_IS_REVIEW_WIDGET (review));
	priv = gs_review_widget_get_instance_private (review);
	gtk_label_set_text (GTK_LABEL (priv->text_label), text);
}

/**
 * gs_review_widget_init:
 **/
static void
gs_review_widget_init (GsReviewWidget *review)
{
	gtk_widget_set_has_window (GTK_WIDGET (review), FALSE);
	gtk_widget_init_template (GTK_WIDGET (review));
}

/**
 * gs_review_widget_class_init:
 **/
static void
gs_review_widget_class_init (GsReviewWidgetClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Software/gs-review-widget.ui");

	gtk_widget_class_bind_template_child_private (widget_class, GsReviewWidget, stars);
	gtk_widget_class_bind_template_child_private (widget_class, GsReviewWidget, summary_label);
	gtk_widget_class_bind_template_child_private (widget_class, GsReviewWidget, author_label);
	gtk_widget_class_bind_template_child_private (widget_class, GsReviewWidget, text_label);
}

/**
 * gs_review_widget_new:
 **/
GtkWidget *
gs_review_widget_new (void)
{
	GsReviewWidget *review;
	review = g_object_new (GS_TYPE_REVIEW_WIDGET, NULL);
	return GTK_WIDGET (review);
}

/* vim: set noexpandtab: */

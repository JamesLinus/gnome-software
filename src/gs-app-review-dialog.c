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

#include "gs-app-review-dialog.h"
#include "gs-star-widget.h"

struct _GsAppReviewDialog
{
	GtkDialog	 parent_instance;

	GtkWidget	*star;
	GtkWidget	*summary_entry;
	GtkWidget	*text_view;
	GtkWidget	*post_button;
};

G_DEFINE_TYPE (GsAppReviewDialog, gs_app_review_dialog, GTK_TYPE_DIALOG)

gint
gs_app_review_dialog_get_rating (GsAppReviewDialog *dialog)
{
	return gs_star_widget_get_rating (GS_STAR_WIDGET (dialog->star));
}

void
gs_app_review_dialog_set_rating	(GsAppReviewDialog *dialog, gint rating)
{
	gs_star_widget_set_rating (GS_STAR_WIDGET (dialog->star), GS_APP_RATING_KIND_USER, rating);
}

const gchar *
gs_app_review_dialog_get_summary (GsAppReviewDialog *dialog)
{
	return gtk_entry_get_text (GTK_ENTRY (dialog->summary_entry));
}

gchar *
gs_app_review_dialog_get_text (GsAppReviewDialog *dialog)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->text_view));
	gtk_text_buffer_get_start_iter (buffer, &start);
	gtk_text_buffer_get_end_iter (buffer, &end);
	return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

static void
gs_app_review_dialog_init (GsAppReviewDialog *dialog)
{
	gtk_widget_init_template (GTK_WIDGET (dialog));
}

static void
gs_app_review_dialog_class_init (GsAppReviewDialogClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Software/gs-app-review-dialog.ui");

	gtk_widget_class_bind_template_child (widget_class, GsAppReviewDialog, star);
	gtk_widget_class_bind_template_child (widget_class, GsAppReviewDialog, summary_entry);
	gtk_widget_class_bind_template_child (widget_class, GsAppReviewDialog, text_view);
	gtk_widget_class_bind_template_child (widget_class, GsAppReviewDialog, post_button);
}

GtkWidget *
gs_app_review_dialog_new (void)
{
	return GTK_WIDGET (g_object_new (GS_TYPE_APP_REVIEW_DIALOG,
					 "use-header-bar", TRUE,
					 NULL));
}

/* vim: set noexpandtab: */

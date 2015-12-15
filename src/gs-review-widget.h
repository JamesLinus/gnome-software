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

#ifndef GS_REVIEW_WIDGET_H
#define GS_REVIEW_WIDGET_H

#include <gtk/gtk.h>

#include "gs-app.h"

G_BEGIN_DECLS

#define GS_TYPE_REVIEW_WIDGET (gs_review_widget_get_type ())

G_DECLARE_DERIVABLE_TYPE (GsReviewWidget, gs_review_widget, GS, REVIEW_WIDGET, GtkBin)

struct _GsReviewWidgetClass
{
	GtkBinClass	 parent_class;
};

GtkWidget	*gs_review_widget_new			(void);
void		 gs_review_widget_set_rating		(GsReviewWidget	*review,
							 gint		 rating);
void		 gs_review_widget_set_author		(GsReviewWidget	*review,
							 const gchar	*author);
void		 gs_review_widget_set_date		(GsReviewWidget	*review,
							 GDateTime	*date);
void		 gs_review_widget_set_summary		(GsReviewWidget	*review,
							 const gchar	*summary);
void		 gs_review_widget_set_text		(GsReviewWidget	*review,
							 const gchar	*text);

G_END_DECLS

#endif /* GS_REVIEW_WIDGET_H */

/* vim: set noexpandtab: */

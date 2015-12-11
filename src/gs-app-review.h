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

#ifndef __GS_APP_REVIEW_H
#define __GS_APP_REVIEW_H

#include <glib-object.h>
#include "gs-app-review.h"

G_BEGIN_DECLS

#define GS_TYPE_APP_REVIEW (gs_app_review_get_type ())

G_DECLARE_FINAL_TYPE (GsAppReview, gs_app_review, GS, APP_REVIEW, GObject)

GsAppReview	*gs_app_review_new			(void);

const gchar	*gs_app_review_get_summary			(GsAppReview	*review);
void		 gs_app_review_set_summary			(GsAppReview	*review,
						 const gchar	*summary);

const gchar	*gs_app_review_get_text			(GsAppReview	*review);
void		 gs_app_review_set_text			(GsAppReview	*review,
						 const gchar	*text);

gint	 gs_app_review_get_rating			(GsAppReview	*review);
void		 gs_app_review_set_rating			(GsAppReview	*review,
						 gint	rating);

const gchar	*gs_app_review_get_version			(GsAppReview	*review);
void		 gs_app_review_set_version			(GsAppReview	*review,
						 const gchar	*version);

const gchar	*gs_app_review_get_reviewer			(GsAppReview	*review);
void		 gs_app_review_set_reviewer			(GsAppReview	*review,
						 const gchar	*reviewer);

guint64		 gs_app_review_get_date			(GsAppReview	*review);
void		 gs_app_review_set_date			(GsAppReview	*review,
						 guint64	date);

void		 gs_app_review_set_is_useful			(GsAppReview	*review,
						 gboolean	is_useful);

void		 gs_app_review_mark_innapropriate			(GsAppReview	*review);

G_END_DECLS

#endif /* __GS_APP_REVIEW_H */

/* vim: set noexpandtab: */

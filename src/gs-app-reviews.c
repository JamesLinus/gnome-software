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

#include "gs-app-reviews.h"

struct _GsAppReviews
{
	GObject			 parent_instance;
};

G_DEFINE_TYPE (GsAppReviews, gs_app_reviews, G_TYPE_OBJECT)

/**
 * gs_app_reviews_class_init:
 * @klass: The GsAppReviewClass
 **/
static void
gs_app_reviews_class_init (GsAppReviewsClass *klass)
{
}

/**
 * gs_app_reviews_init:
 **/
static void
gs_app_reviews_init (GsAppReviews *reviews)
{
}

/**
 * gs_app_reviews_new:
 *
 * Return value: a new GsAppReviews object.
 **/
GsAppReviews *
gs_app_reviews_new (void)
{
	GsAppReview *reviews;
	reviews = g_object_new (GS_TYPE_APP_REVIEWS, NULL);
	return GS_APP_REVIEWS (reviews);
}

/* vim: set noexpandtab: */

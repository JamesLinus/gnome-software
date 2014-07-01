/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Matthias Clasen <mclasen@redhat.com>
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

#include "gs-feature-tile.h"

struct _GsFeatureTilePrivate
{
	AsApp		*app;
	GtkWidget	*button;
	GtkWidget	*image;
	GtkWidget	*title;
	GtkWidget	*subtitle;
	GtkWidget	*waiting;
	GtkCssProvider  *provider;
};

G_DEFINE_TYPE_WITH_PRIVATE (GsFeatureTile, gs_feature_tile, GTK_TYPE_BIN)

enum {
	SIGNAL_CLICKED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

AsApp *
gs_feature_tile_get_app (GsFeatureTile *tile)
{
	GsFeatureTilePrivate *priv;

	g_return_val_if_fail (GS_IS_FEATURE_TILE (tile), NULL);

	priv = gs_feature_tile_get_instance_private (tile);
	return priv->app;
}

static void
app_state_changed (AsApp *app, GParamSpec *pspec, GsFeatureTile *tile)
{
        GsFeatureTilePrivate *priv;
        AtkObject *accessible;
        gchar *name;

        priv = gs_feature_tile_get_instance_private (tile);
        accessible = gtk_widget_get_accessible (priv->button);

        switch (as_app_get_state (app)) {
        case AS_APP_STATE_INSTALLED:
        case AS_APP_STATE_INSTALLING:
        case AS_APP_STATE_REMOVING:
                name = g_strdup_printf ("%s (%s)",
                                        as_app_get_name (app, NULL),
                                        _("Installed"));
                break;
        case AS_APP_STATE_UPDATABLE:
                name = g_strdup_printf ("%s (%s)",
                                        as_app_get_name (app, NULL),
                                        _("Updates"));
                break;
        case AS_APP_STATE_AVAILABLE:
        default:
                name = g_strdup (as_app_get_name (app, NULL));
                break;
        }

        if (GTK_IS_ACCESSIBLE (accessible)) {
                atk_object_set_name (accessible, name);
                atk_object_set_description (accessible, as_app_get_comment (app, NULL));
        }
        g_free (name);
}

void
gs_feature_tile_set_app (GsFeatureTile *tile, AsApp *app)
{
	GsFeatureTilePrivate *priv;
	GString *data = NULL;
	const gchar *background;
	const gchar *stroke_color;
	const gchar *text_color;
	const gchar *text_shadow;
	gchar *tmp;

	g_return_if_fail (GS_IS_FEATURE_TILE (tile));
	g_return_if_fail (GS_IS_APP (app) || app == NULL);

	priv = gs_feature_tile_get_instance_private (tile);

	g_clear_object (&priv->app);
	if (!app)
		return;

	priv->app = g_object_ref (app);

	gtk_widget_hide (priv->waiting);

        g_signal_connect (priv->app, "notify::state",
                          G_CALLBACK (app_state_changed), tile);
        app_state_changed (priv->app, NULL, tile);

	gtk_label_set_label (GTK_LABEL (priv->title), as_app_get_name (app, NULL));
	gtk_label_set_label (GTK_LABEL (priv->subtitle), as_app_get_comment (app, NULL));

	/* check the app has the featured data */
	text_color = as_app_get_metadata_item (app, "Featured::text-color");
	if (text_color == NULL) {
		tmp = gs_app_to_string (app);
		g_warning ("%s has no featured data: %s",
			   as_app_get_id (app), tmp);
		g_free (tmp);
		goto out;
	}
	background = as_app_get_metadata_item (app, "Featured::background");
	stroke_color = as_app_get_metadata_item (app, "Featured::stroke-color");
	text_shadow = as_app_get_metadata_item (app, "Featured::text-shadow");

	data = g_string_sized_new (1024);
	g_string_append (data, ".button.featured-tile {\n");
	g_string_append (data, "  all: unset;\n");
	g_string_append (data, "  padding: 0;\n");
	g_string_append (data, "  border-radius: 0;\n");
	g_string_append (data, "  border-width: 1px;\n");
	g_string_append (data, "  border-image: none;\n");
	g_string_append_printf (data, "  border-color: %s;\n", stroke_color);
	if (text_shadow != NULL)
		g_string_append_printf (data, "  text-shadow: %s;\n", text_shadow);
	g_string_append_printf (data, "  color: %s;\n", text_color);
	g_string_append (data, "  -GtkWidget-focus-padding: 0;\n");
	g_string_append_printf (data, "  outline-color: alpha(%s, 0.75);\n", text_color);
	g_string_append (data, "  outline-style: dashed;\n");
	g_string_append (data, "  outline-offset: 2px;\n");
	g_string_append_printf (data, "  background: %s;\n", background);
	g_string_append (data, "}\n");
	g_string_append (data, ".button.featured-tile:hover {\n");
	g_string_append (data, "  background: linear-gradient(to bottom,\n");
	g_string_append (data, "                              alpha(#fff,0.16),\n");
	g_string_append_printf (data,
				"                              alpha(#aaa,0.16)), %s;\n",
				background);
	g_string_append (data, "}\n");
	gtk_css_provider_load_from_data (priv->provider, data->str, -1, NULL);
out:
	if (data != NULL)
		g_string_free (data, TRUE);
}

static void
gs_feature_tile_destroy (GtkWidget *widget)
{
	GsFeatureTile *tile = GS_FEATURE_TILE (widget);
	GsFeatureTilePrivate *priv;

	priv = gs_feature_tile_get_instance_private (tile);

	g_clear_object (&priv->app);
	g_clear_object (&priv->provider);

	GTK_WIDGET_CLASS (gs_feature_tile_parent_class)->destroy (widget);
}

static void
button_clicked (GsFeatureTile *tile)
{
	GsFeatureTilePrivate *priv;

	priv = gs_feature_tile_get_instance_private (tile);
	if (priv->app)
		g_signal_emit (tile, signals[SIGNAL_CLICKED], 0);
}

static void
gs_feature_tile_init (GsFeatureTile *tile)
{
	GsFeatureTilePrivate *priv;

	gtk_widget_set_has_window (GTK_WIDGET (tile), FALSE);
	gtk_widget_init_template (GTK_WIDGET (tile));
	priv = gs_feature_tile_get_instance_private (tile);
	g_signal_connect_swapped (priv->button, "clicked",
				  G_CALLBACK (button_clicked), tile);

	priv->provider = gtk_css_provider_new ();
	gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
						   GTK_STYLE_PROVIDER (priv->provider),
						   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
gs_feature_tile_class_init (GsFeatureTileClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	widget_class->destroy = gs_feature_tile_destroy;

	signals [SIGNAL_CLICKED] =
		g_signal_new ("clicked",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GsFeatureTileClass, clicked),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/software/feature-tile.ui");

	gtk_widget_class_bind_template_child_private (widget_class, GsFeatureTile, button);
	gtk_widget_class_bind_template_child_private (widget_class, GsFeatureTile, image);
	gtk_widget_class_bind_template_child_private (widget_class, GsFeatureTile, title);
	gtk_widget_class_bind_template_child_private (widget_class, GsFeatureTile, subtitle);
	gtk_widget_class_bind_template_child_private (widget_class, GsFeatureTile, waiting);
}

GtkWidget *
gs_feature_tile_new (AsApp *app)
{
	GsFeatureTile *tile;

	tile = g_object_new (GS_TYPE_FEATURE_TILE, NULL);
	gs_feature_tile_set_app (tile, app);

	return GTK_WIDGET (tile);
}

/* vim: set noexpandtab: */

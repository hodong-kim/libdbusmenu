/*
A library to take the object model made consistent by libdbusmenu-glib
and visualize it in GTK.

Copyright 2009 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of either or both of the following licenses:

1) the GNU Lesser General Public License version 3, as published by the 
Free Software Foundation; and/or
2) the GNU Lesser General Public License version 2.1, as published by 
the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY or FITNESS FOR A PARTICULAR 
PURPOSE.  See the applicable version of the GNU Lesser General Public 
License for more details.

You should have received a copy of both the GNU Lesser General Public 
License version 3 and version 2.1 along with this program.  If not, see 
<http://www.gnu.org/licenses/>
*/

#include "menuitem.h"

/**
	dbusmenu_menuitem_property_set_image:
	@menuitem: The #DbusmenuMenuitem to set the property on.
	@property: Name of the property to set.
	@data: The image to place on the property.

	This function takes the pixbuf that is stored in @data and
	turns it into a base64 encoded PNG so that it can be placed
	onto a standard #DbusmenuMenuitem property.

	Return value: Whether the function was able to set the property
		or not.
*/
gboolean
dbusmenu_menuitem_property_set_image (DbusmenuMenuitem * menuitem, const gchar * property, const GdkPixbuf * data)
{
	g_return_val_if_fail(GDK_IS_PIXBUF(data), FALSE);
	g_return_val_if_fail(DBUSMENU_IS_MENUITEM(menuitem), FALSE);
	g_return_val_if_fail(property != NULL && property[0] != '\0', FALSE);

	GError * error = NULL;
	gchar * png_data;
	gsize png_data_len;

	if (!gdk_pixbuf_save_to_buffer((GdkPixbuf *)data, &png_data, &png_data_len, "png", &error, NULL)) {
		if (error == NULL) {
			g_warning("Unable to create pixbuf data stream: %d", (gint)png_data_len);
		} else {
			g_warning("Unable to create pixbuf data stream: %s", error->message);
			g_error_free(error);
			error = NULL;
		}

		return FALSE;
	}

	gchar * prop_str = g_base64_encode((guchar *)png_data, png_data_len);
	gboolean propreturn = FALSE;
	propreturn = dbusmenu_menuitem_property_set(menuitem, property, prop_str);

	g_free(prop_str);
	g_free(png_data);

	return propreturn;
}

/**
	dbusmenu_menuitem_property_get_image:
	@menuitem: The #DbusmenuMenuite to look for the property on
	@property: The name of the property to look for.

	This function looks on the menu item for a property by the
	name of @property.  If one exists it tries to turn it into
	a #GdkPixbuf.  It assumes that the property is a base64 encoded
	PNG file like the one created by #dbusmenu_menuite_property_set_image.

	Return value: A pixbuf or #NULL to signal error.
*/
GdkPixbuf *
dbusmenu_menuitem_property_get_image (DbusmenuMenuitem * menuitem, const gchar * property)
{
	g_return_val_if_fail(DBUSMENU_IS_MENUITEM(menuitem), NULL);
	g_return_val_if_fail(property != NULL && property[0] != '\0', NULL);

	const gchar * value = dbusmenu_menuitem_property_get(menuitem, property);

	/* There is no icon */
	if (value == NULL || value[0] == '\0') {
		return NULL;
	}

	gsize length = 0;
	guchar * icondata = g_base64_decode(value, &length);
	
	GInputStream * input = g_memory_input_stream_new_from_data(icondata, length, NULL);
	if (input == NULL) {
		g_warning("Cound not create input stream from icon property data");
		g_free(icondata);
		return NULL;
	}

	GError * error = NULL;
	GdkPixbuf * icon = gdk_pixbuf_new_from_stream(input, NULL, &error);

	if (error != NULL) {
		g_warning("Unable to build Pixbuf from icon data: %s", error->message);
		g_error_free(error);
	}

	error = NULL;
	g_input_stream_close(input, NULL, &error);
	if (error != NULL) {
		g_warning("Unable to close input stream: %s", error->message);
		g_error_free(error);
	}
	g_free(icondata);

	return icon;
}

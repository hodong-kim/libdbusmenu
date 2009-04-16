#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "server.h"
#include "server-marshal.h"

/* DBus Prototypes */
static gboolean _dbusmenu_server_get_property (void);
static gboolean _dbusmenu_server_get_properties (void);
static gboolean _dbusmenu_server_call (void);
static gboolean _dbusmenu_server_list_properties (void);

#include "dbusmenu-server.h"

/* Privates, I'll show you mine... */
typedef struct _DbusmenuServerPrivate DbusmenuServerPrivate;

struct _DbusmenuServerPrivate
{
	DbusmenuMenuitem * root;
	gchar * dbusobject;
};

#define DBUSMENU_SERVER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), DBUSMENU_TYPE_SERVER, DbusmenuServerPrivate))

/* Signals */
enum {
	ID_PROP_UPDATE,
	ID_UPDATE,
	LAYOUT_UPDATE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* Properties */
enum {
	PROP_0,
	PROP_DBUS_OBJECT,
	PROP_ROOT_NODE,
	PROP_LAYOUT
};

/* Prototype */
static void dbusmenu_server_class_init (DbusmenuServerClass *class);
static void dbusmenu_server_init       (DbusmenuServer *self);
static void dbusmenu_server_dispose    (GObject *object);
static void dbusmenu_server_finalize   (GObject *object);
static void set_property (GObject * obj, guint id, const GValue * value, GParamSpec * pspec);
static void get_property (GObject * obj, guint id, GValue * value, GParamSpec * pspec);

G_DEFINE_TYPE (DbusmenuServer, dbusmenu_server, G_TYPE_OBJECT);

static void
dbusmenu_server_class_init (DbusmenuServerClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	g_type_class_add_private (class, sizeof (DbusmenuServerPrivate));

	object_class->dispose = dbusmenu_server_dispose;
	object_class->finalize = dbusmenu_server_finalize;
	object_class->set_property = set_property;
	object_class->get_property = get_property;

	signals[ID_PROP_UPDATE] =   g_signal_new(DBUSMENU_SERVER_SIGNAL_ID_PROP_UPDATE,
	                                         G_TYPE_FROM_CLASS(class),
	                                         G_SIGNAL_RUN_LAST,
	                                         G_STRUCT_OFFSET(DbusmenuServerClass, id_prop_update),
	                                         NULL, NULL,
	                                         _dbusmenu_server_marshal_VOID__UINT_STRING_STRING,
	                                         G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING);
	signals[ID_UPDATE] =        g_signal_new(DBUSMENU_SERVER_SIGNAL_ID_UPDATE,
	                                         G_TYPE_FROM_CLASS(class),
	                                         G_SIGNAL_RUN_LAST,
	                                         G_STRUCT_OFFSET(DbusmenuServerClass, id_update),
	                                         NULL, NULL,
	                                         g_cclosure_marshal_VOID__UINT,
	                                         G_TYPE_NONE, 1, G_TYPE_UINT);
	signals[LAYOUT_UPDATE] =    g_signal_new(DBUSMENU_SERVER_SIGNAL_LAYOUT_UPDATE,
	                                         G_TYPE_FROM_CLASS(class),
	                                         G_SIGNAL_RUN_LAST,
	                                         G_STRUCT_OFFSET(DbusmenuServerClass, layout_update),
	                                         NULL, NULL,
	                                         g_cclosure_marshal_VOID__VOID,
	                                         G_TYPE_NONE, 0);


	g_object_class_install_property (object_class, PROP_DBUS_OBJECT,
	                                 g_param_spec_string(DBUSMENU_SERVER_PROP_DBUS_OBJECT, "DBus object path",
	                                              "The object that represents this set of menus on DBus",
	                                              "/org/freedesktop/dbusmenu",
	                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class, PROP_ROOT_NODE,
	                                 g_param_spec_object(DBUSMENU_SERVER_PROP_ROOT_NODE, "Root menu node",
	                                              "The base object of the menus that are served",
	                                              DBUSMENU_TYPE_MENUITEM,
	                                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class, PROP_LAYOUT,
	                                 g_param_spec_string(DBUSMENU_SERVER_PROP_LAYOUT, "XML Layout of the menus",
	                                              "A simple XML string that describes the layout of the menus",
	                                              "<menu />",
	                                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	dbus_g_object_type_install_info(DBUSMENU_TYPE_SERVER, &dbus_glib__dbusmenu_server_object_info);

	return;
}

static void
dbusmenu_server_init (DbusmenuServer *self)
{
	DbusmenuServerPrivate * priv = DBUSMENU_SERVER_GET_PRIVATE(self);

	priv->root = NULL;
	priv->dbusobject = NULL;

	return;
}

static void
dbusmenu_server_dispose (GObject *object)
{
	G_OBJECT_CLASS (dbusmenu_server_parent_class)->dispose (object);
	return;
}

static void
dbusmenu_server_finalize (GObject *object)
{
	G_OBJECT_CLASS (dbusmenu_server_parent_class)->finalize (object);
	return;
}

static void
set_property (GObject * obj, guint id, const GValue * value, GParamSpec * pspec)
{
	DbusmenuServerPrivate * priv = DBUSMENU_SERVER_GET_PRIVATE(obj);

	switch (id) {
	case PROP_DBUS_OBJECT:
		g_return_if_fail(priv->dbusobject == NULL);
		priv->dbusobject = g_value_dup_string(value);
		DBusGConnection * connection = dbus_g_bus_get(DBUS_BUS_SESSION, NULL);
		dbus_g_connection_register_g_object(connection,
											priv->dbusobject,
											obj);
		break;
	case PROP_ROOT_NODE:
		if (priv->root != NULL) {
			g_object_unref(G_OBJECT(priv->root));
			priv->root = NULL;
		}
		priv->root = DBUSMENU_MENUITEM(g_value_get_object(value));
		if (priv->root != NULL) {
			g_object_ref(G_OBJECT(priv->root));
		}
		break;
	case PROP_LAYOUT:
		/* Can't set this, fall through to error */
		g_warning("Can not set property: layout");
	default:
		g_return_if_reached();
		break;
	}

	return;
}

static void
xmlarray_foreach_free (gpointer arrayentry, gpointer userdata)
{
	if (arrayentry != NULL) {
		g_free(arrayentry);
	}

	return;
}

static void
get_property (GObject * obj, guint id, GValue * value, GParamSpec * pspec)
{
	DbusmenuServerPrivate * priv = DBUSMENU_SERVER_GET_PRIVATE(obj);

	switch (id) {
	case PROP_DBUS_OBJECT:
		g_value_set_string(value, priv->dbusobject);
		break;
	case PROP_ROOT_NODE:
		g_value_set_object(value, priv->root);
		break;
	case PROP_LAYOUT: {
		GPtrArray * xmlarray = g_ptr_array_new();
		g_ptr_array_add(xmlarray, g_strdup("<menu>"));
		if (priv->root != NULL) {
			dbusmenu_menuitem_buildxml(priv->root, xmlarray);
		}
		g_ptr_array_add(xmlarray, g_strdup("</menu>"));
		g_ptr_array_add(xmlarray, NULL);

		/* build string */
		gchar * finalstring = g_strjoinv("\n", (gchar **)xmlarray->pdata);
		g_value_take_string(value, finalstring);

		g_ptr_array_foreach(xmlarray, xmlarray_foreach_free, NULL);
		g_ptr_array_free(xmlarray, TRUE);
		break;
	}
	default:
		g_return_if_reached();
		break;
	}

	return;
}

/* DBus interface */
static gboolean 
_dbusmenu_server_get_property (void)
{

	return TRUE;
}

static gboolean
_dbusmenu_server_get_properties (void)
{

	return TRUE;
}

static gboolean
_dbusmenu_server_call (void)
{

	return TRUE;
}

static gboolean
_dbusmenu_server_list_properties (void)
{

	return TRUE;
}

/* Public Interface */
DbusmenuServer *
dbusmenu_server_new (const gchar * object)
{
	if (object == NULL) {
		object = "/org/freedesktop/dbusmenu";
	}

	DbusmenuServer * self = g_object_new(DBUSMENU_TYPE_SERVER,
	                                     DBUSMENU_SERVER_PROP_DBUS_OBJECT, object,
	                                     NULL);

	return self;
}

void
dbusmenu_server_set_root (DbusmenuServer * self, DbusmenuMenuitem * root)
{
	GValue rootvalue = {0};
	g_value_init(&rootvalue, G_TYPE_OBJECT);
	g_value_set_object(&rootvalue, root);
	g_object_set_property(G_OBJECT(self), DBUSMENU_SERVER_PROP_ROOT_NODE, &rootvalue);
	return;
}




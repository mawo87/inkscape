#define __SP_GRADIENT_C__

/*
 * SVG <stop> <linearGradient> and <radialGradient> implementation
 *
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#define noSP_GRADIENT_VERBOSE

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include <libnr/nr-gradient.h>

#include <gtk/gtksignal.h>

#include "helper/nr-gradient-gpl.h"
#include "svg/svg.h"
#include "xml/repr-private.h"
#include "attributes.h"
#include "document-private.h"
#include "sp-object-repr.h"
#include "sp-gradient.h"
#include "uri-references.h"

#define SP_MACROS_SILENT
#include "macros.h"

/* Has to be power of 2 */
#define NCOLORS NR_GRADIENT_VECTOR_LENGTH

static void sp_stop_class_init(SPStopClass *klass);
static void sp_stop_init(SPStop *stop);

static void sp_stop_build(SPObject *object, SPDocument *document, SPRepr *repr);
static void sp_stop_set(SPObject *object, unsigned key, gchar const *value);
static SPRepr *sp_stop_write(SPObject *object, SPRepr *repr, guint flags);

static SPObjectClass *stop_parent_class;

GType
sp_stop_get_type (void)
{
	static GType type = 0;
	if (!type) {
		GTypeInfo info = {
			sizeof (SPStopClass),
			NULL, NULL,
			(GClassInitFunc) sp_stop_class_init,
			NULL, NULL,
			sizeof (SPStop),
			16,
			(GInstanceInitFunc) sp_stop_init,
			NULL,	/* value_table */
		};
		type = g_type_register_static (SP_TYPE_OBJECT, "SPStop", &info, (GTypeFlags)0);
	}
	return type;
}

static void sp_stop_class_init(SPStopClass *klass)
{
	SPObjectClass *sp_object_class = (SPObjectClass *) klass;

	stop_parent_class = (SPObjectClass *) g_type_class_ref(SP_TYPE_OBJECT);

	sp_object_class->build = sp_stop_build;
	sp_object_class->set = sp_stop_set;
	sp_object_class->write = sp_stop_write;
}

static void
sp_stop_init (SPStop *stop)
{
	stop->offset = 0.0;
	sp_color_set_rgb_rgba32 (&stop->color, 0x000000ff);
}

static void sp_stop_build(SPObject *object, SPDocument *document, SPRepr *repr)
{
	if (((SPObjectClass *) stop_parent_class)->build)
		(* ((SPObjectClass *) stop_parent_class)->build) (object, document, repr);

	sp_object_read_attr (object, "offset");
	sp_object_read_attr (object, "stop-color");
	sp_object_read_attr (object, "stop-opacity");
	sp_object_read_attr (object, "style");
}

static void
sp_stop_set (SPObject *object, unsigned int key, const gchar *value)
{
	guint32 color;
	gdouble opacity;
	const gchar *p;

	SPStop *stop = SP_STOP (object);

	switch (key) {
	case SP_ATTR_STYLE:
		/* fixme: We are reading simple values 3 times during
		 *        build (Lauris) */
		/* fixme: We need presentation attributes etc. */
		p = sp_object_get_style_property (object, "stop-color", "black");
		color = sp_svg_read_color (p, sp_color_get_rgba32_ualpha (&stop->color, 0x00));
		sp_color_set_rgb_rgba32 (&stop->color, color);
		p = sp_object_get_style_property (object, "stop-opacity", "1");
		opacity = sp_svg_read_percentage (p, stop->opacity);
		stop->opacity = opacity;
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
		break;
	case SP_PROP_STOP_COLOR:
		/* fixme: We need presentation attributes etc. */
		p = sp_object_get_style_property (object, "stop-color", "black");
		color = sp_svg_read_color (p, sp_color_get_rgba32_ualpha (&stop->color, 0x00));
		sp_color_set_rgb_rgba32 (&stop->color, color);
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
		break;
	case SP_PROP_STOP_OPACITY:
		/* fixme: We need presentation attributes etc. */
		p = sp_object_get_style_property (object, "stop-opacity", "1");
		opacity = sp_svg_read_percentage (p, stop->opacity);
		stop->opacity = opacity;
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
		break;
	case SP_ATTR_OFFSET:
		stop->offset = sp_svg_read_percentage (value, 0.0);
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG);
		break;
	default:
		if (((SPObjectClass *) stop_parent_class)->set)
			(* ((SPObjectClass *) stop_parent_class)->set) (object, key, value);
		break;
	}
}

static SPRepr *
sp_stop_write (SPObject *object, SPRepr *repr, guint flags)
{
	gchar c[64], s[1024];

	SPStop *stop = SP_STOP (object);

	if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
		repr = sp_repr_new ("stop");
	}

	sp_svg_write_color (c, 64, sp_color_get_rgba32_ualpha (&stop->color, 255));
	g_snprintf (s, 1024, "stop-color:%s;stop-opacity:%g;", c, stop->opacity);
	sp_repr_set_attr (repr, "style", s);
	sp_repr_set_attr (repr, "stop-color", NULL);
	sp_repr_set_attr (repr, "stop-opacity", NULL);
	sp_repr_set_double (repr, "offset", stop->offset);

	if (((SPObjectClass *) stop_parent_class)->write)
		(* ((SPObjectClass *) stop_parent_class)->write) (object, repr, flags);

	return repr;
}

/*
 * Gradient
 */

static void sp_gradient_class_init (SPGradientClass *klass);
static void sp_gradient_init (SPGradient *gr);

static void sp_gradient_build (SPObject *object, SPDocument *document, SPRepr *repr);
static void sp_gradient_release (SPObject *object);
static void sp_gradient_set (SPObject *object, unsigned int key, const gchar *value);
static void sp_gradient_child_added (SPObject *object, SPRepr *child, SPRepr *ref);
static void sp_gradient_remove_child (SPObject *object, SPRepr *child);
static void sp_gradient_modified (SPObject *object, guint flags);
static SPRepr *sp_gradient_write (SPObject *object, SPRepr *repr, guint flags);

static void gradient_ref_modified (SPObject *href, guint flags, SPGradient *gradient);

static void sp_gradient_invalidate_vector (SPGradient *gr);
static void sp_gradient_rebuild_vector (SPGradient *gr);

static void gradient_ref_changed(SPObject *old_ref, SPObject *ref, SPGradient *gradient);

static SPPaintServerClass *gradient_parent_class;

GType
sp_gradient_get_type (void)
{
	static GType gradient_type = 0;
	if (!gradient_type) {
		GTypeInfo gradient_info = {
			sizeof (SPGradientClass),
			NULL, NULL,
			(GClassInitFunc) sp_gradient_class_init,
			NULL, NULL,
			sizeof (SPGradient),
			16,
			(GInstanceInitFunc) sp_gradient_init,
			NULL,	/* value_table */
		};
		gradient_type = g_type_register_static (SP_TYPE_PAINT_SERVER, "SPGradient", &gradient_info, (GTypeFlags)0);
	}
	return gradient_type;
}

static void
sp_gradient_class_init (SPGradientClass *klass)
{
	SPObjectClass *sp_object_class = (SPObjectClass *) klass;

	gradient_parent_class = (SPPaintServerClass *)g_type_class_ref (SP_TYPE_PAINT_SERVER);

	sp_object_class->build = sp_gradient_build;
	sp_object_class->release = sp_gradient_release;
	sp_object_class->set = sp_gradient_set;
	sp_object_class->child_added = sp_gradient_child_added;
	sp_object_class->remove_child = sp_gradient_remove_child;
	sp_object_class->modified = sp_gradient_modified;
	sp_object_class->write = sp_gradient_write;
}

static void
sp_gradient_init (SPGradient *gr)
{
	gr->ref = new SPGradientReference(SP_OBJECT(gr));
	gr->ref->changedSignal().connect(SigC::bind(SigC::slot(gradient_ref_changed), gr));

	/* Fixme: reprs being rearranged (e.g. via the XML editor)
	 *        may require us to clear the state */
	gr->state = SP_GRADIENT_STATE_UNKNOWN;

	gr->units = SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX;
	nr_matrix_set_identity (NR_MATRIX_D_FROM_DOUBLE (gr->transform));

	gr->spread = SP_GRADIENT_SPREAD_PAD;
	gr->spread_set = FALSE;

	gr->stops = NULL;
	gr->has_stops = FALSE;

	gr->vector = NULL;
	gr->color = NULL;

	gr->len = 0.0;
}

static void
sp_gradient_build (SPObject *object, SPDocument *document, SPRepr *repr)
{
	SPGradient *gradient = SP_GRADIENT (object);

	if (((SPObjectClass *) gradient_parent_class)->build)
		(* ((SPObjectClass *) gradient_parent_class)->build) (object, document, repr);

	/* fixme: Add all children, not only stops? */
	SPObject *last = NULL;
	for (SPRepr *rchild = repr->children; rchild != NULL; rchild = rchild->next) {
		GType type;
		SPObject *child;
		type = sp_repr_type_lookup (rchild);
		if (g_type_is_a (type, SP_TYPE_OBJECT)) {
			child = SP_OBJECT(g_object_new(type, 0));
			if (last) {
				last->next = sp_object_attach_reref (object, child, NULL);
			} else {
				gradient->stops = sp_object_attach_reref (object, child, NULL);
			}
			sp_object_invoke_build (child, document, rchild, SP_OBJECT_IS_CLONED (object));
			/* Set has_stops flag */
			if (SP_IS_STOP (child)) gradient->has_stops = TRUE;
			last = child;
		}
	}

	sp_object_read_attr (object, "gradientUnits");
	sp_object_read_attr (object, "gradientTransform");
	sp_object_read_attr (object, "spreadMethod");
	sp_object_read_attr (object, "xlink:href");

	/* Register ourselves */
	sp_document_add_resource (document, "gradient", object);
}

static void
sp_gradient_release (SPObject *object)
{
	SPGradient *gradient = (SPGradient *) object;

#ifdef SP_GRADIENT_VERBOSE
	g_print ("Releasing gradient %s\n", SP_OBJECT_ID (object));
#endif

	if (SP_OBJECT_DOCUMENT (object)) {
		/* Unregister ourselves */
		sp_document_remove_resource (SP_OBJECT_DOCUMENT (object), "gradient", SP_OBJECT (object));
	}

	if (gradient->ref) {
		gradient->ref->detach();
		delete gradient->ref;
		gradient->ref = NULL;
	}

	if (gradient->color) {
		g_free (gradient->color);
		gradient->color = NULL;
	}

	if (gradient->vector) {
		g_free (gradient->vector);
		gradient->vector = NULL;
	}

	while (gradient->stops) {
		gradient->stops = sp_object_detach_unref (object, gradient->stops);
	}

	if (((SPObjectClass *) gradient_parent_class)->release)
		((SPObjectClass *) gradient_parent_class)->release (object);
}

static void
sp_gradient_set (SPObject *object, unsigned int key, const gchar *value)
{
	SPGradient *gr = SP_GRADIENT (object);

	/* fixme: We should unset properties, if val == NULL */
	switch (key) {
	case SP_ATTR_GRADIENTUNITS:
		if (value) {
			if (!strcmp (value, "userSpaceOnUse")) {
				gr->units = SP_GRADIENT_UNITS_USERSPACEONUSE;
			} else {
				gr->units = SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX;
			}
			gr->units_set = TRUE;
		} else {
			gr->units_set = FALSE;
		}
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG);
		break;
	case SP_ATTR_GRADIENTTRANSFORM: {
		NRMatrix t;
		if (value && sp_svg_transform_read (value, &t)) {
			int i;
			for (i = 0; i < 6; i++) gr->transform[i] = t.c[i];
			gr->transform_set = TRUE;
		} else {
			nr_matrix_set_identity (NR_MATRIX_D_FROM_DOUBLE (gr->transform));
			gr->transform_set = FALSE;
		}
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG);
		break;
	}
	case SP_ATTR_SPREADMETHOD:
		if (value) {
			if (!strcmp (value, "reflect")) {
				gr->spread = SP_GRADIENT_SPREAD_REFLECT;
			} else if (!strcmp (value, "repeat")) {
				gr->spread = SP_GRADIENT_SPREAD_REPEAT;
			} else {
				gr->spread = SP_GRADIENT_SPREAD_PAD;
			}
			gr->spread_set = TRUE;
		} else {
			gr->spread_set = FALSE;
		}
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG);
		break;
	case SP_ATTR_XLINK_HREF: {
		if (value) {
			try {
				gr->ref->attach(Inkscape::URI(value));
			} catch (Inkscape::BadURIException &e) {
				g_warning("%s", e.what());
				gr->ref->detach();
			}
		} else {
			gr->ref->detach();
		}
		break;
	}
	default:
		if (((SPObjectClass *) gradient_parent_class)->set)
			((SPObjectClass *) gradient_parent_class)->set (object, key, value);
		break;
	}
}

static void
gradient_ref_changed(SPObject *old_ref, SPObject *ref, SPGradient *gr)
{
	if (old_ref) {
		sp_signal_disconnect_by_data(old_ref, gr);
	}
	if (SP_IS_GRADIENT (ref)) {
		g_signal_connect(G_OBJECT (ref), "modified", G_CALLBACK (gradient_ref_modified), gr);
	}
	/* Fixme: what should the flags (second) argument be? */
	gradient_ref_modified(ref, 0, gr);
}

static void
sp_gradient_child_added (SPObject *object, SPRepr *child, SPRepr *ref)
{
	SPGradient *gr = SP_GRADIENT (object);

	if (((SPObjectClass *) gradient_parent_class)->child_added)
		(* ((SPObjectClass *) gradient_parent_class)->child_added) (object, child, ref);

	sp_gradient_invalidate_vector (gr);

	GType type = sp_repr_type_lookup (child);
	SPObject *ochild = SP_OBJECT(g_object_new(type, 0));
	ochild->parent = object;

	SPObject *prev = NULL;
	if (ref) {
		prev = gr->stops;
		while (prev->repr != ref) prev = prev->next;
	}

	if (!prev) {
		ochild->next = gr->stops;
		gr->stops = ochild;
	} else {
		ochild->next = prev->next;
		prev->next = ochild;
	}

	sp_object_invoke_build (ochild, object->document, child, SP_OBJECT_IS_CLONED (object));

	/* Fixme: (Lauris) */
	if (SP_IS_STOP (ochild)) gr->has_stops = TRUE;

	/* Fixme: should we schedule "modified" here? */
}

static void
sp_gradient_remove_child (SPObject *object, SPRepr *child)
{
	SPGradient *gr = SP_GRADIENT (object);

	if (((SPObjectClass *) gradient_parent_class)->remove_child)
		(* ((SPObjectClass *) gradient_parent_class)->remove_child) (object, child);

	sp_gradient_invalidate_vector (gr);

	SPObject *prev = NULL;
	SPObject *ochild = gr->stops;
	while (ochild->repr != child) {
		prev = ochild;
		ochild = ochild->next;
	}

	if (prev) {
		prev->next = ochild->next;
	} else {
		gr->stops = ochild->next;
	}

	ochild->parent = NULL;
	ochild->next = NULL;
	g_object_unref (G_OBJECT (ochild));

	/* Fixme: (Lauris) */
	gr->has_stops = FALSE;
	for (ochild = gr->stops; ochild != NULL; ochild = ochild->next) {
		if (SP_IS_STOP (ochild)) {
			gr->has_stops = TRUE;
			break;
		}
	}

	/* Fixme: should we schedule "modified" here? */
}

static void
sp_gradient_modified (SPObject *object, guint flags)
{
	SPGradient *gr = SP_GRADIENT (object);

	if (flags & SP_OBJECT_CHILD_MODIFIED_FLAG) {
		sp_gradient_invalidate_vector (gr);
	}

	if (flags & SP_OBJECT_MODIFIED_FLAG) flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
	flags &= SP_OBJECT_MODIFIED_CASCADE;

	GSList *l = NULL;
	for (SPObject *child = gr->stops; child != NULL; child = child->next) {
		g_object_ref (G_OBJECT (child));
		l = g_slist_prepend (l, child);
	}
	l = g_slist_reverse (l);
	while (l) {
		SPObject *child = SP_OBJECT (l->data);
		l = g_slist_remove (l, child);
		if (flags || (child->mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
			sp_object_invoke_modified (child, flags);
		}
		g_object_unref (G_OBJECT (child));
	}
}

static SPRepr *
sp_gradient_write (SPObject *object, SPRepr *repr, guint flags)
{
	SPGradient *gr = SP_GRADIENT (object);

	if (((SPObjectClass *) gradient_parent_class)->write)
		(* ((SPObjectClass *) gradient_parent_class)->write) (object, repr, flags);

	if (flags & SP_OBJECT_WRITE_BUILD) {
		GSList *l;
		l = NULL;
		for (SPObject *child = gr->stops; child != NULL; child = child->next) {
			SPRepr *crepr;
			crepr = sp_object_invoke_write (child, NULL, flags);
			if (crepr) l = g_slist_prepend (l, crepr);
		}
		while (l) {
			sp_repr_add_child (repr, (SPRepr *) l->data, NULL);
			sp_repr_unref ((SPRepr *) l->data);
			l = g_slist_remove (l, l->data);
		}
	}

	if (gr->ref->getURI()) {
		gchar *uri_string = gr->ref->getURI()->toString();
		sp_repr_set_attr(repr, "xlink:href", uri_string);
		g_free(uri_string);
	}

	if ((flags & SP_OBJECT_WRITE_ALL) || gr->units_set) {
		switch (gr->units) {
		case SP_GRADIENT_UNITS_USERSPACEONUSE:
			sp_repr_set_attr (repr, "gradientUnits", "userSpaceOnUse");
			break;
		default:
			sp_repr_set_attr (repr, "gradientUnits", "objectBoundingBox");
			break;
		}
	}

	if ((flags & SP_OBJECT_WRITE_ALL) || gr->transform_set) {
		gchar c[256];
		NRMatrix t;
		int i;
		for (i = 0; i < 6; i++) t.c[i] = gr->transform[i];
		if (sp_svg_transform_write (c, 256, &t)) {
			sp_repr_set_attr (repr, "gradientTransform", c);
		} else {
			sp_repr_set_attr (repr, "gradientTransform", NULL);
		}
	}

	if ((flags & SP_OBJECT_WRITE_ALL) || gr->spread_set) {
		switch (gr->spread) {
		case SP_GRADIENT_SPREAD_REFLECT:
			sp_repr_set_attr (repr, "spreadMethod", "reflect");
			break;
		case SP_GRADIENT_SPREAD_REPEAT:
			sp_repr_set_attr (repr, "spreadMethod", "repeat");
			break;
		default:
			sp_repr_set_attr (repr, "spreadMethod", "pad");
			break;
		}
	}

	return repr;
}

/* Forces vector to be built, if not present (i.e. changed) */

void
sp_gradient_ensure_vector (SPGradient *gradient)
{
	g_return_if_fail (gradient != NULL);
	g_return_if_fail (SP_IS_GRADIENT (gradient));

	if (!gradient->vector) {
		sp_gradient_rebuild_vector (gradient);
	}
}

void
sp_gradient_set_vector (SPGradient *gradient, SPGradientVector *vector)
{
	g_return_if_fail (gradient != NULL);
	g_return_if_fail (SP_IS_GRADIENT (gradient));
	g_return_if_fail (vector != NULL);

	if (gradient->color) {
		g_free (gradient->color);
		gradient->color = NULL;
	}

	if (gradient->vector && (gradient->vector->nstops != vector->nstops)) {
		g_free (gradient->vector);
		gradient->vector = NULL;
	}
	if (!gradient->vector) {
		gradient->vector = (SPGradientVector *)g_malloc (sizeof (SPGradientVector) + (vector->nstops - 1) * sizeof (SPGradientStop));
	}
	memcpy (gradient->vector, vector, sizeof (SPGradientVector) + (vector->nstops - 1) * sizeof (SPGradientStop));

	sp_object_request_modified (SP_OBJECT (gradient), SP_OBJECT_MODIFIED_FLAG);
}

void
sp_gradient_set_units (SPGradient *gr, SPGradientUnits units)
{
	if (units != gr->units) {
		gr->units = units;
		gr->units_set = TRUE;
		sp_object_request_modified (SP_OBJECT (gr), SP_OBJECT_MODIFIED_FLAG);
	}
}

void
sp_gradient_set_spread (SPGradient *gr, SPGradientSpread spread)
{
	if (spread != gr->spread) {
		gr->spread = spread;
		gr->spread_set = TRUE;
		sp_object_request_modified (SP_OBJECT (gr), SP_OBJECT_MODIFIED_FLAG);
	}
}

void
sp_gradient_repr_set_vector (SPGradient *gr, SPRepr *repr, SPGradientVector *vector)
{
	g_return_if_fail (gr != NULL);
	g_return_if_fail (SP_IS_GRADIENT (gr));
	g_return_if_fail (repr != NULL);

	/* We have to be careful, as vector may be our own, so construct repr list at first */
	GSList *cl = NULL;
	if (vector) {
		for (int i = 0; i < vector->nstops; i++) {
			gchar c[64], s[256];
			SPRepr *child = sp_repr_new ("stop");
			sp_repr_set_double (child, "offset",
						      vector->stops[i].offset * (vector->end - vector->start) + vector->start);
			sp_svg_write_color (c, 64, sp_color_get_rgba32_ualpha (&vector->stops[i].color, 0x00));
			g_snprintf (s, 256, "stop-color:%s;stop-opacity:%g;", c, vector->stops[i].opacity);
			sp_repr_set_attr (child, "style", s);
			/* Order will be reversed here */
			cl = g_slist_prepend (cl, child);
		}
	}

	/* Now collect stops from original repr */
	GSList *sl = NULL;
	for (SPRepr *child = repr->children; child != NULL; child = child->next) {
		if (!strcmp (sp_repr_name (child), "stop")) {
			sl = g_slist_prepend (sl, child);
		}
	}
	/* Remove all stops */
	while (sl) {
		/* fixme: This should work, unless we make gradient
		 *        into generic group */
		sp_repr_unparent ((SPRepr *)sl->data);
		sl = g_slist_remove (sl, sl->data);
	}

	/* And insert new children from list */
	while (cl) {
		SPRepr *child = static_cast<SPRepr *>(cl->data);
		sp_repr_add_child(repr, child, NULL);
		/* TODO: Previously, the below passed as first
		   argument an expression always equal to NULL (which
		   would generate a CRITICAL warning from
		   sp_repr_unref if this code were executed).  I've
		   guessed that the intention is to unref cl->data,
		   but I can't work out how to get this code called,
		   to check that it's doing the right thing.  Remove
		   the below g_warning once you're sure that this new
		   version is correct. */
		g_warning("Congratulations, you've found a way to get this code to run.  Please run under valgrind (or other detector of leaks and premature free's) to check that the below unreferencing (from the above sp_repr_new call) is correct.");
		sp_repr_unref(child);
		cl = g_slist_remove(cl, child);
	}
}

static void
gradient_ref_modified (SPObject *href, guint flags, SPGradient *gradient)
{
	sp_gradient_invalidate_vector (gradient);
	sp_object_request_modified (SP_OBJECT (gradient), SP_OBJECT_MODIFIED_FLAG);
}

/* Creates normalized color vector */

static void
sp_gradient_invalidate_vector (SPGradient *gr)
{
	if (gr->color) {
		g_free (gr->color);
		gr->color = NULL;
	}

	if (gr->vector) {
		g_free (gr->vector);
		gr->vector = NULL;
	}
}

static void
sp_gradient_rebuild_vector (SPGradient *gr)
{
	gint len = 0;
	SPColor color;
	sp_color_set_rgb_rgba32 (&color, 0x00000000);
	gfloat opacity = 0.0;
	gdouble offsets = 0, offsete = 0.0;
	gboolean oset = FALSE;
	for (SPObject *child = gr->stops; child != NULL; child = child->next) {
		if (SP_IS_STOP (child)) {
			SPStop *stop = SP_STOP (child);
			if (!oset) {
				oset = TRUE;
				offsets = offsete = stop->offset;
				len += 1;
			} else if (stop->offset > (offsete + 1e-9)) {
				offsete = stop->offset;
				len += 1;
			}
			sp_color_copy (&color, &stop->color);
			opacity = stop->opacity;
		}
	}

	gr->has_stops = (len != 0);

	SPGradient *ref = gr->ref->getObject();
	if ( !len && ref ) {
		/* Copy vector from referenced gradient */
		sp_gradient_ensure_vector(ref);
		if ( !gr->vector ||
		     ( gr->vector->nstops != ref->vector->nstops ) )
		{
			if (gr->vector) g_free (gr->vector);
			gr->vector = (SPGradientVector *)g_malloc (sizeof (SPGradientVector) + (ref->vector->nstops - 1) * sizeof (SPGradientStop));
			gr->vector->nstops = ref->vector->nstops;
		}
		memcpy (gr->vector, ref->vector, sizeof (SPGradientVector) + (gr->vector->nstops - 1) * sizeof (SPGradientStop));
		return;
	}

	gint vlen = MAX (len, 2);

	if (!gr->vector || gr->vector->nstops != vlen) {
		if (gr->vector) g_free (gr->vector);
		gr->vector = (SPGradientVector *)g_malloc (sizeof (SPGradientVector) + (vlen - 1) * sizeof (SPGradientStop));
		gr->vector->nstops = vlen;
	}

	if (len < 2) {
		gr->vector->start = 0.0;
		gr->vector->end = 1.0;
		gr->vector->stops[0].offset = 0.0;
		sp_color_copy (&gr->vector->stops[0].color, &color);
		gr->vector->stops[0].opacity = opacity;
		gr->vector->stops[1].offset = 1.0;
		sp_color_copy (&gr->vector->stops[1].color, &color);
		gr->vector->stops[1].opacity = opacity;
		return;
	}

	/* o' = (o - oS) / (oE - oS) */
	gr->vector->start = offsets;
	gr->vector->end = offsete;

	gint pos = 0;
	gdouble offset = offsets;
	gr->vector->stops[0].offset = 0.0;
	for (SPObject *child = gr->stops; child != NULL; child = child->next) {
		if (SP_IS_STOP (child)) {
			SPStop *stop;
			stop = SP_STOP (child);
			if (stop->offset > (offset + 1e-9)) {
				pos += 1;
				gr->vector->stops[pos].offset = (stop->offset - offsets) / (offsete - offsets);
				offset = stop->offset;
			}
			sp_color_copy (&gr->vector->stops[pos].color, &stop->color);
			gr->vector->stops[pos].opacity = stop->opacity;
		}
	}
}

void
sp_gradient_ensure_colors (SPGradient *gr)
{
	if (!gr->vector) {
		sp_gradient_rebuild_vector (gr);
	}

	if (!gr->color) {
		gr->color = g_new (guchar, 4 * NCOLORS);
	}

	for (gint i = 0; i < gr->vector->nstops - 1; i++) {
		guint32 color = sp_color_get_rgba32_falpha (&gr->vector->stops[i].color, gr->vector->stops[i].opacity);
		gint r0 = (color >> 24) & 0xff;
		gint g0 = (color >> 16) & 0xff;
		gint b0 = (color >> 8) & 0xff;
		gint a0 = color & 0xff;
		color = sp_color_get_rgba32_falpha (&gr->vector->stops[i + 1].color, gr->vector->stops[i + 1].opacity);
		gint r1 = (color >> 24) & 0xff;
		gint g1 = (color >> 16) & 0xff;
		gint b1 = (color >> 8) & 0xff;
		gint a1 = color & 0xff;
		gint o0 = (gint) floor (gr->vector->stops[i].offset * (NCOLORS - 0.001));
		gint o1 = (gint) floor (gr->vector->stops[i + 1].offset * (NCOLORS - 0.001));
		if (o1 > o0) {
			gint dr = ((r1 - r0) << 16) / (o1 - o0);
			gint dg = ((g1 - g0) << 16) / (o1 - o0);
			gint db = ((b1 - b0) << 16) / (o1 - o0);
			gint da = ((a1 - a0) << 16) / (o1 - o0);
			gint r = r0 << 16;
			gint g = g0 << 16;
			gint b = b0 << 16;
			gint a = a0 << 16;
			for (int j = o0; j < o1 + 1; j++) {
				gr->color[4 * j] = r >> 16;
				gr->color[4 * j + 1] = g >> 16;
				gr->color[4 * j + 2] = b >> 16;
				gr->color[4 * j + 3] = a >> 16;
				r += dr;
				g += dg;
				b += db;
				a += da;
			}
		}
	}

	gr->len = gr->vector->end - gr->vector->start;
}

/*
 * Renders gradient vector to buffer
 *
 * len, width, height, rowstride - buffer parameters (1 or 2 dimensional)
 * span - full integer width of requested gradient
 * pos - buffer starting position in span
 *
 * RGB buffer background should be set up before
 */

void
sp_gradient_render_vector_line_rgba (SPGradient *gradient, guchar *buf, gint len, gint pos, gint span)
{
	g_return_if_fail (gradient != NULL);
	g_return_if_fail (SP_IS_GRADIENT (gradient));
	g_return_if_fail (buf != NULL);
	g_return_if_fail (len > 0);
	g_return_if_fail (pos >= 0);
	g_return_if_fail (pos + len <= span);
	g_return_if_fail (span > 0);

	if (!gradient->color) {
		sp_gradient_ensure_colors (gradient);
	}

	gint idx = (pos * 1024 << 8) / span;
	gint didx = (1024 << 8) / span;

	for (gint x = 0; x < len; x++) {
		// Can this be done with 4 byte copies?
		*buf++ = gradient->color[4 * (idx >> 8)];
		*buf++ = gradient->color[4 * (idx >> 8) + 1];
		*buf++ = gradient->color[4 * (idx >> 8) + 2];
		*buf++ = gradient->color[4 * (idx >> 8) + 3];
		idx += didx;
	}
}

void
sp_gradient_render_vector_line_rgb (SPGradient *gradient, guchar *buf, gint len, gint pos, gint span)
{
	g_return_if_fail (gradient != NULL);
	g_return_if_fail (SP_IS_GRADIENT (gradient));
	g_return_if_fail (buf != NULL);
	g_return_if_fail (len > 0);
	g_return_if_fail (pos >= 0);
	g_return_if_fail (pos + len <= span);
	g_return_if_fail (span > 0);

	if (!gradient->color) {
		sp_gradient_ensure_colors (gradient);
	}

	gint idx = (pos * 1024 << 8) / span;
	gint didx = (1024 << 8) / span;

	for (gint x = 0; x < len; x++) {
		gint r = gradient->color[4 * (idx >> 8)];
		gint g = gradient->color[4 * (idx >> 8) + 1];
		gint b = gradient->color[4 * (idx >> 8) + 2];
		gint a = gradient->color[4 * (idx >> 8) + 3];
		
		gint fc = (r - *buf) * a;
		buf[0] = *buf + ((fc + (fc >> 8) + 0x80) >> 8);
		
		fc = (g - *buf) * a;
		buf[1] = *buf + ((fc + (fc >> 8) + 0x80) >> 8);
		
		fc = (b - *buf) * a;
		buf[2] = *buf + ((fc + (fc >> 8) + 0x80) >> 8);
		
		buf += 3;
		idx += didx;
	}
}

void
sp_gradient_render_vector_block_rgba (SPGradient *gradient, guchar *buf, gint width, gint height, gint rowstride,
				      gint pos, gint span, gboolean horizontal)
{
	g_return_if_fail (gradient != NULL);
	g_return_if_fail (SP_IS_GRADIENT (gradient));
	g_return_if_fail (buf != NULL);
	g_return_if_fail (width > 0);
	g_return_if_fail (height > 0);
	g_return_if_fail (pos >= 0);
	g_return_if_fail ((horizontal && (pos + width <= span)) || (!horizontal && (pos + height <= span)));
	g_return_if_fail (span > 0);

	if (horizontal) {
		sp_gradient_render_vector_line_rgba (gradient, buf, width, pos, span);
		for (gint y = 1; y < height; y++) {
			memcpy (buf + y * rowstride, buf, 4 * width);
		}
	} else {
		guchar *tmp = (guchar *)alloca (4 * height);
		sp_gradient_render_vector_line_rgba (gradient, tmp, height, pos, span);
		for (gint y = 0; y < height; y++) {
			guchar *b = buf + y * rowstride;
			for (gint x = 0; x < width; x++) {
				*b++ = tmp[0];
				*b++ = tmp[1];
				*b++ = tmp[2];
				*b++ = tmp[3];
			}
			tmp += 4;
		}
	}
}

void
sp_gradient_render_vector_block_rgb (SPGradient *gradient, guchar *buf, gint width, gint height, gint rowstride,
				     gint pos, gint span, gboolean horizontal)
{
	g_return_if_fail (gradient != NULL);
	g_return_if_fail (SP_IS_GRADIENT (gradient));
	g_return_if_fail (buf != NULL);
	g_return_if_fail (width > 0);
	g_return_if_fail (height > 0);
	g_return_if_fail (pos >= 0);
	g_return_if_fail ((horizontal && (pos + width <= span)) || (!horizontal && (pos + height <= span)));
	g_return_if_fail (span > 0);

	if (horizontal) {
		guchar *tmp = (guchar*)alloca (4 * width);
		sp_gradient_render_vector_line_rgba (gradient, tmp, width, pos, span);
		for (gint y = 0; y < height; y++) {
			guchar *t = tmp;
			for (gint x = 0; x < width; x++) {
				gint a = t[3];
				gint fc = (t[0] - buf[0]) * a;
				buf[0] = buf[0] + ((fc + (fc >> 8) + 0x80) >> 8);
				fc = (t[1] - buf[1]) * a;
				buf[1] = buf[1] + ((fc + (fc >> 8) + 0x80) >> 8);
				fc = (t[2] - buf[2]) * a;
				buf[2] = buf[2] + ((fc + (fc >> 8) + 0x80) >> 8);
				buf += 3;
				t += 4;
			}
		}
	} else {
		guchar *tmp = (guchar*)alloca (4 * height);
		sp_gradient_render_vector_line_rgba (gradient, tmp, height, pos, span);
		for (gint y = 0; y < height; y++) {
			guchar *t = tmp + 4 * y;
			for (gint x = 0; x < width; x++) {
				gint a = t[3];
				gint fc = (t[0] - buf[0]) * a;
				buf[0] = buf[0] + ((fc + (fc >> 8) + 0x80) >> 8);
				fc = (t[1] - buf[1]) * a;
				buf[1] = buf[1] + ((fc + (fc >> 8) + 0x80) >> 8);
				fc = (t[2] - buf[2]) * a;
				buf[2] = buf[2] + ((fc + (fc >> 8) + 0x80) >> 8);
			}
		}
	}
}

NRMatrix *
sp_gradient_get_g2d_matrix_f (SPGradient *gr, NRMatrix *ctm, NRRect *bbox, NRMatrix *g2d)
{
	if (gr->units == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX) {
		NRMatrix bb2u;
		
		bb2u.c[0] = bbox->x1 - bbox->x0;
		bb2u.c[1] = 0.0;
		bb2u.c[2] = 0.0;
		bb2u.c[3] = bbox->y1 - bbox->y0;
		bb2u.c[4] = bbox->x0;
		bb2u.c[5] = bbox->y0;

		nr_matrix_multiply (g2d, &bb2u, ctm);
	} else {
		*g2d = *ctm;
	}

	return g2d;
}

NRMatrix *
sp_gradient_get_gs2d_matrix_f (SPGradient *gr, NRMatrix *ctm, NRRect *bbox, NRMatrix *gs2d)
{
	if (gr->units == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX) {
		NRMatrix bb2u, gs2u;

		bb2u.c[0] = bbox->x1 - bbox->x0;
		bb2u.c[1] = 0.0;
		bb2u.c[2] = 0.0;
		bb2u.c[3] = bbox->y1 - bbox->y0;
		bb2u.c[4] = bbox->x0;
		bb2u.c[5] = bbox->y0;

		nr_matrix_multiply (&gs2u, (NRMatrix *) gr->transform, &bb2u);
		nr_matrix_multiply (gs2d, &gs2u, ctm);
	} else {
		nr_matrix_multiply (gs2d, (NRMatrix *) gr->transform, ctm);
	}

	return gs2d;
}

void
sp_gradient_set_gs2d_matrix_f (SPGradient *gr, NRMatrix *ctm, NRRect *bbox, NRMatrix *gs2d)
{
	NRMatrix g2d, d2g, gs2g;

	SP_PRINT_MATRIX ("* GS2D:", gs2d);

	if (gr->units == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX) {
		NRMatrix bb2u;

		bb2u.c[0] = bbox->x1 - bbox->x0;
		bb2u.c[1] = 0.0;
		bb2u.c[2] = 0.0;
		bb2u.c[3] = bbox->y1 - bbox->y0;
		bb2u.c[4] = bbox->x0;
		bb2u.c[5] = bbox->y0;

		SP_PRINT_MATRIX ("* BB2U:", &bb2u);

		nr_matrix_multiply (&g2d, &bb2u, ctm);
	} else {
		g2d = *ctm;
	}

	SP_PRINT_MATRIX ("* G2D:", &g2d);

	nr_matrix_invert (&d2g, &g2d);
	SP_PRINT_MATRIX ("* D2G:", &d2g);
	SP_PRINT_MATRIX ("* G2D:", &g2d);
	nr_matrix_invert (&g2d, &d2g);
	SP_PRINT_MATRIX ("* D2G:", &d2g);
	SP_PRINT_MATRIX ("* G2D:", &g2d);


	nr_matrix_multiply (&gs2g, gs2d, &d2g);
	SP_PRINT_MATRIX ("* GS2G:", &gs2g);
	
	for(int i = 0; i < 6; i++)
		gr->transform[i] = gs2g.c[i];

	gr->transform_set = TRUE;

	sp_object_request_modified (SP_OBJECT (gr), SP_OBJECT_MODIFIED_FLAG);
}

/*
 * Linear Gradient
 */

typedef struct _SPLGPainter SPLGPainter;

struct _SPLGPainter {
	SPPainter painter;
	SPLinearGradient *lg;

	NRLGradientRenderer lgr;
};

static void sp_lineargradient_class_init(SPLinearGradientClass *klass);
static void sp_lineargradient_init(SPLinearGradient *lg);

static void sp_lineargradient_build(SPObject *object, SPDocument *document, SPRepr *repr);
static void sp_lineargradient_set(SPObject *object, unsigned key, gchar const *value);
static SPRepr *sp_lineargradient_write(SPObject *object, SPRepr *repr, guint flags);

static SPPainter *sp_lineargradient_painter_new (SPPaintServer *ps, double const *affine, NRRect const *bbox);
static void sp_lineargradient_painter_free (SPPaintServer *ps, SPPainter *painter);

static void sp_lg_fill (SPPainter *painter, NRPixBlock *pb);

static SPGradientClass *lg_parent_class;

GType
sp_lineargradient_get_type (void)
{
	static GType type = 0;
	if (!type) {
		GTypeInfo info = {
			sizeof (SPLinearGradientClass),
			NULL, NULL,
			(GClassInitFunc) sp_lineargradient_class_init,
			NULL, NULL,
			sizeof (SPLinearGradient),
			16,
			(GInstanceInitFunc) sp_lineargradient_init,
			NULL,	/* value_table */
		};
		type = g_type_register_static (SP_TYPE_GRADIENT, "SPLinearGradient", &info, (GTypeFlags)0);
	}
	return type;
}

static void sp_lineargradient_class_init(SPLinearGradientClass *klass)
{
	SPObjectClass *sp_object_class = (SPObjectClass *) klass;
	SPPaintServerClass *ps_class = (SPPaintServerClass *) klass;

	lg_parent_class = (SPGradientClass*)g_type_class_ref (SP_TYPE_GRADIENT);

	sp_object_class->build = sp_lineargradient_build;
	sp_object_class->set = sp_lineargradient_set;
	sp_object_class->write = sp_lineargradient_write;

	ps_class->painter_new = sp_lineargradient_painter_new;
	ps_class->painter_free = sp_lineargradient_painter_free;
}

static void sp_lineargradient_init(SPLinearGradient *lg)
{
	sp_svg_length_unset (&lg->x1, SP_SVG_UNIT_PERCENT, 0.0, 0.0);
	sp_svg_length_unset (&lg->y1, SP_SVG_UNIT_PERCENT, 0.0, 0.0);
	sp_svg_length_unset (&lg->x2, SP_SVG_UNIT_PERCENT, 1.0, 1.0);
	sp_svg_length_unset (&lg->y2, SP_SVG_UNIT_PERCENT, 0.0, 0.0);
}

static void sp_lineargradient_build(SPObject *object, SPDocument *document, SPRepr *repr)
{
	if (((SPObjectClass *) lg_parent_class)->build)
		(* ((SPObjectClass *) lg_parent_class)->build) (object, document, repr);

	sp_object_read_attr (object, "x1");
	sp_object_read_attr (object, "y1");
	sp_object_read_attr (object, "x2");
	sp_object_read_attr (object, "y2");
}

static void
sp_lineargradient_set (SPObject *object, unsigned int key, const gchar *value)
{
	SPLinearGradient *lg = SP_LINEARGRADIENT (object);

	switch (key) {
	case SP_ATTR_X1:
		if (!sp_svg_length_read (value, &lg->x1)) {
			sp_svg_length_unset (&lg->x1, SP_SVG_UNIT_PERCENT, 0.0, 0.0);
		}
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG);
		break;
	case SP_ATTR_Y1:
		if (!sp_svg_length_read (value, &lg->y1)) {
			sp_svg_length_unset (&lg->y1, SP_SVG_UNIT_PERCENT, 0.0, 0.0);
		}
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG);
		break;
	case SP_ATTR_X2:
		if (!sp_svg_length_read (value, &lg->x2)) {
			sp_svg_length_unset (&lg->x2, SP_SVG_UNIT_PERCENT, 1.0, 1.0);
		}
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG);
		break;
	case SP_ATTR_Y2:
		if (!sp_svg_length_read (value, &lg->y2)) {
			sp_svg_length_unset (&lg->y2, SP_SVG_UNIT_PERCENT, 0.0, 0.0);
		}
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG);
		break;
	default:
		if (((SPObjectClass *) lg_parent_class)->set)
			(* ((SPObjectClass *) lg_parent_class)->set) (object, key, value);
		break;
	}
}

static SPRepr *
sp_lineargradient_write (SPObject *object, SPRepr *repr, guint flags)
{
	SPLinearGradient *lg = SP_LINEARGRADIENT (object);

	if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
		repr = sp_repr_new ("linearGradient");
	}

	if ((flags & SP_OBJECT_WRITE_ALL) || lg->x1.set)
		sp_repr_set_double (repr, "x1", lg->x1.computed);
	if ((flags & SP_OBJECT_WRITE_ALL) || lg->y1.set)
		sp_repr_set_double (repr, "y1", lg->y1.computed);
	if ((flags & SP_OBJECT_WRITE_ALL) || lg->x2.set)
		sp_repr_set_double (repr, "x2", lg->x2.computed);
	if ((flags & SP_OBJECT_WRITE_ALL) || lg->y2.set)
		sp_repr_set_double (repr, "y2", lg->y2.computed);

	if (((SPObjectClass *) lg_parent_class)->write)
		(* ((SPObjectClass *) lg_parent_class)->write) (object, repr, flags);

	return repr;
}

/*
 * Basically we have to deal with transformations
 *
 * 1) color2norm - maps point in (0,NCOLORS) vector to (0,1) vector
 *    fixme: I do not know how to deal with start > 0 and end < 1
 * 2) norm2pos - maps (0,1) vector to x1,y1 - x2,y2
 * 2) gradientTransform
 * 3) bbox2user
 * 4) ctm == userspace to pixel grid
 */

static SPPainter *
sp_lineargradient_painter_new (SPPaintServer *ps, const gdouble *ctm, const NRRect *bbox)
{
	SPGradient *gr;
	SPLGPainter *lgp;
	gdouble color2norm[6], color2px[6];
	NRMatrix v2px;

	SPLinearGradient *lg = SP_LINEARGRADIENT (ps);
	gr = SP_GRADIENT (ps);

	if (!gr->color) sp_gradient_ensure_colors (gr);

	lgp = g_new (SPLGPainter, 1);

	lgp->painter.type = SP_PAINTER_IND;
	lgp->painter.fill = sp_lg_fill;

	lgp->lg = lg;

	/* fixme: Technically speaking, we map NCOLORS on line
	 *        [start,end] onto line [0,1] (Lauris) */
	/* fixme: I almost think we should fill color array start and
	 *        end in that case (Lauris) */
	/* fixme: The alternative would be to leave these just empty
	 *        garbage or something similar (Lauris) */
	/* fixme: Originally I had 1023.9999 here - not sure whether
	 *        we have really to cut out ceil int (Lauris) */
	nr_matrix_set_identity (NR_MATRIX_D_FROM_DOUBLE (color2norm));

	if (gr->units == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX) {
		gdouble norm2pos[6], bbox2user[6];
		gdouble color2pos[6], color2tpos[6], color2user[6];

		nr_matrix_set_identity (NR_MATRIX_D_FROM_DOUBLE (norm2pos));

		/* gradientTransform goes here (Lauris) */
		SP_PRINT_TRANSFORM ("gradientTransform", gr->transform);

		/* BBox to user coordinate system */
		bbox2user[0] = bbox->x1 - bbox->x0;
		bbox2user[1] = 0.0;
		bbox2user[2] = 0.0;
		bbox2user[3] = bbox->y1 - bbox->y0;
		bbox2user[4] = bbox->x0;
		bbox2user[5] = bbox->y0;
		SP_PRINT_TRANSFORM ("bbox2user", bbox2user);

		/* CTM goes here */
		SP_PRINT_TRANSFORM ("ctm", ctm);

		nr_matrix_multiply (NR_MATRIX_D_FROM_DOUBLE (color2pos), NR_MATRIX_D_FROM_DOUBLE (color2norm), NR_MATRIX_D_FROM_DOUBLE (norm2pos));
		SP_PRINT_TRANSFORM ("color2pos", color2pos);
		nr_matrix_multiply (NR_MATRIX_D_FROM_DOUBLE (color2tpos), NR_MATRIX_D_FROM_DOUBLE (color2pos), NR_MATRIX_D_FROM_DOUBLE (gr->transform));
		SP_PRINT_TRANSFORM ("color2tpos", color2tpos);
		nr_matrix_multiply (NR_MATRIX_D_FROM_DOUBLE (color2user), NR_MATRIX_D_FROM_DOUBLE (color2tpos), NR_MATRIX_D_FROM_DOUBLE (bbox2user));
		SP_PRINT_TRANSFORM ("color2user", color2user);
		nr_matrix_multiply (NR_MATRIX_D_FROM_DOUBLE (color2px), NR_MATRIX_D_FROM_DOUBLE (color2user), NR_MATRIX_D_FROM_DOUBLE (ctm));
		SP_PRINT_TRANSFORM ("color2px", color2px);
	} else {
		gdouble norm2pos[6];
		gdouble color2pos[6], color2tpos[6];
		/* Problem: What to do, if we have mixed lengths and
		 * percentages? */
		/* Currently we do ignore percentages at all, but that
		 * is not good (lauris) */

		nr_matrix_set_identity (NR_MATRIX_D_FROM_DOUBLE (norm2pos));

		/* gradientTransform goes here (Lauris) */
		SP_PRINT_TRANSFORM ("gradientTransform", gr->transform);

		/* CTM goes here */
		SP_PRINT_TRANSFORM ("ctm", ctm);

		nr_matrix_multiply (NR_MATRIX_D_FROM_DOUBLE (color2pos), NR_MATRIX_D_FROM_DOUBLE (color2norm), NR_MATRIX_D_FROM_DOUBLE (norm2pos));
		SP_PRINT_TRANSFORM ("color2pos", color2pos);
		nr_matrix_multiply (NR_MATRIX_D_FROM_DOUBLE (color2tpos), NR_MATRIX_D_FROM_DOUBLE (color2pos), NR_MATRIX_D_FROM_DOUBLE (gr->transform));
		SP_PRINT_TRANSFORM ("color2tpos", color2tpos);
		nr_matrix_multiply (NR_MATRIX_D_FROM_DOUBLE (color2px), NR_MATRIX_D_FROM_DOUBLE (color2tpos), NR_MATRIX_D_FROM_DOUBLE (ctm));
		SP_PRINT_TRANSFORM ("color2px", color2px);
	}

	v2px.c[0] = color2px[0];
	v2px.c[1] = color2px[1];
	v2px.c[2] = color2px[2];
	v2px.c[3] = color2px[3];
	v2px.c[4] = color2px[4];
	v2px.c[5] = color2px[5];

	nr_lgradient_renderer_setup (&lgp->lgr, gr->color, gr->spread, &v2px,
				     lg->x1.computed, lg->y1.computed, lg->x2.computed, lg->y2.computed);

	return (SPPainter *) lgp;
}

static void
sp_lineargradient_painter_free (SPPaintServer *ps, SPPainter *painter)
{
	g_free (painter);
}

void
sp_lineargradient_set_position (SPLinearGradient *lg, gdouble x1, gdouble y1, gdouble x2, gdouble y2)
{
	g_return_if_fail (lg != NULL);
	g_return_if_fail (SP_IS_LINEARGRADIENT (lg));

	/* fixme: units? (Lauris)  */
	sp_svg_length_set (&lg->x1, SP_SVG_UNIT_NONE, x1, x1);
	sp_svg_length_set (&lg->y1, SP_SVG_UNIT_NONE, y1, y1);
	sp_svg_length_set (&lg->x2, SP_SVG_UNIT_NONE, x2, x2);
	sp_svg_length_set (&lg->y2, SP_SVG_UNIT_NONE, y2, y2);

	sp_object_request_modified (SP_OBJECT (lg), SP_OBJECT_MODIFIED_FLAG);
}

/* Builds flattened repr tree of gradient - i.e. no href */

SPRepr *
sp_lineargradient_build_repr (SPLinearGradient *lg, gboolean vector)
{
	g_return_val_if_fail (lg != NULL, NULL);
	g_return_val_if_fail (SP_IS_LINEARGRADIENT (lg), NULL);

	SPRepr *repr = sp_repr_new ("linearGradient");

	sp_object_invoke_write ((SPObject *) lg, repr, SP_OBJECT_WRITE_EXT | SP_OBJECT_WRITE_ALL);

	if (vector) {
		sp_gradient_ensure_vector ((SPGradient *) lg);
		sp_gradient_repr_set_vector ((SPGradient *) lg, repr, ((SPGradient *) lg)->vector);
	}

	return repr;
}

static void
sp_lg_fill (SPPainter *painter, NRPixBlock *pb)
{
	SPLGPainter *lgp = (SPLGPainter *) painter;

	nr_render ((NRRenderer *) &lgp->lgr, pb, NULL);
}

/*
 * Radial Gradient
 */

typedef struct _SPRGPainter SPRGPainter;

struct _SPRGPainter {
	SPPainter painter;
	SPRadialGradient *rg;
	NRRGradientRenderer rgr;
};

static void sp_radialgradient_class_init (SPRadialGradientClass *klass);
static void sp_radialgradient_init (SPRadialGradient *rg);

static void sp_radialgradient_build (SPObject *object, SPDocument *document, SPRepr *repr);
static void sp_radialgradient_set (SPObject *object, unsigned int key, const gchar *value);
static SPRepr *sp_radialgradient_write (SPObject *object, SPRepr *repr, guint flags);

static SPPainter *sp_radialgradient_painter_new (SPPaintServer *ps, const gdouble *affine, const NRRect *bbox);
static void sp_radialgradient_painter_free (SPPaintServer *ps, SPPainter *painter);

static void sp_rg_fill (SPPainter *painter, NRPixBlock *pb);

static SPGradientClass *rg_parent_class;

GType
sp_radialgradient_get_type (void)
{
	static GType type = 0;
	if (!type) {
		GTypeInfo info = {
			sizeof (SPRadialGradientClass),
			NULL, NULL,
			(GClassInitFunc) sp_radialgradient_class_init,
			NULL, NULL,
			sizeof (SPRadialGradient),
			16,
			(GInstanceInitFunc) sp_radialgradient_init,
			NULL,	/* value_table */
		};
		type = g_type_register_static (SP_TYPE_GRADIENT, "SPRadialGradient", &info, (GTypeFlags)0);
	}
	return type;
}

static void sp_radialgradient_class_init(SPRadialGradientClass *klass)
{
	SPObjectClass *sp_object_class = (SPObjectClass *) klass;
	SPPaintServerClass *ps_class = (SPPaintServerClass *) klass;

	rg_parent_class = (SPGradientClass*)g_type_class_ref (SP_TYPE_GRADIENT);

	sp_object_class->build = sp_radialgradient_build;
	sp_object_class->set = sp_radialgradient_set;
	sp_object_class->write = sp_radialgradient_write;

	ps_class->painter_new = sp_radialgradient_painter_new;
	ps_class->painter_free = sp_radialgradient_painter_free;
}

static void
sp_radialgradient_init (SPRadialGradient *rg)
{
	sp_svg_length_unset (&rg->cx, SP_SVG_UNIT_PERCENT, 0.5, 0.5);
	sp_svg_length_unset (&rg->cy, SP_SVG_UNIT_PERCENT, 0.5, 0.5);
	sp_svg_length_unset (&rg->r, SP_SVG_UNIT_PERCENT, 0.5, 0.5);
	sp_svg_length_unset (&rg->fx, SP_SVG_UNIT_PERCENT, 0.5, 0.5);
	sp_svg_length_unset (&rg->fy, SP_SVG_UNIT_PERCENT, 0.5, 0.5);
}

static void
sp_radialgradient_build (SPObject *object, SPDocument *document, SPRepr *repr)
{
	if (((SPObjectClass *) rg_parent_class)->build)
		(* ((SPObjectClass *) rg_parent_class)->build) (object, document, repr);

	sp_object_read_attr (object, "cx");
	sp_object_read_attr (object, "cy");
	sp_object_read_attr (object, "r");
	sp_object_read_attr (object, "fx");
	sp_object_read_attr (object, "fy");
}

static void
sp_radialgradient_set (SPObject *object, unsigned int key, const gchar *value)
{
	SPRadialGradient *rg = SP_RADIALGRADIENT (object);

	switch (key) {
	case SP_ATTR_CX:
		if (!sp_svg_length_read (value, &rg->cx)) {
			sp_svg_length_unset (&rg->cx, SP_SVG_UNIT_PERCENT, 0.5, 0.5);
		}
		if (!rg->fx.set) {
			rg->fx.value = rg->cx.value;
			rg->fx.computed = rg->cx.computed;
		}
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG);
		break;
	case SP_ATTR_CY:
		if (!sp_svg_length_read (value, &rg->cy)) {
			sp_svg_length_unset (&rg->cy, SP_SVG_UNIT_PERCENT, 0.5, 0.5);
		}
		if (!rg->fy.set) {
			rg->fy.value = rg->cy.value;
			rg->fy.computed = rg->cy.computed;
		}
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG);
		break;
	case SP_ATTR_R:
		if (!sp_svg_length_read (value, &rg->r)) {
			sp_svg_length_unset (&rg->r, SP_SVG_UNIT_PERCENT, 0.5, 0.5);
		}
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG);
		break;
	case SP_ATTR_FX:
		if (!sp_svg_length_read (value, &rg->fx)) {
			sp_svg_length_unset (&rg->fx, rg->cx.unit, rg->cx.value, rg->cx.computed);
		}
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG);
		break;
	case SP_ATTR_FY:
		if (!sp_svg_length_read (value, &rg->fy)) {
			sp_svg_length_unset (&rg->fy, rg->cy.unit, rg->cy.value, rg->cy.computed);
		}
		sp_object_request_modified (object, SP_OBJECT_MODIFIED_FLAG);
		break;
	default:
		if (((SPObjectClass *) rg_parent_class)->set)
			((SPObjectClass *) rg_parent_class)->set (object, key, value);
		break;
	}
}

static SPRepr *
sp_radialgradient_write (SPObject *object, SPRepr *repr, guint flags)
{
	SPRadialGradient *rg = SP_RADIALGRADIENT (object);

	if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
		repr = sp_repr_new ("radialGradient");
	}

	if ((flags & SP_OBJECT_WRITE_ALL) || rg->cx.set) sp_repr_set_double (repr, "cx", rg->cx.computed);
	if ((flags & SP_OBJECT_WRITE_ALL) || rg->cy.set) sp_repr_set_double (repr, "cy", rg->cy.computed);
	if ((flags & SP_OBJECT_WRITE_ALL) || rg->r.set) sp_repr_set_double (repr, "r", rg->r.computed);
	if ((flags & SP_OBJECT_WRITE_ALL) || rg->fx.set) sp_repr_set_double (repr, "fx", rg->fx.computed);
	if ((flags & SP_OBJECT_WRITE_ALL) || rg->fy.set) sp_repr_set_double (repr, "fy", rg->fy.computed);

	if (((SPObjectClass *) rg_parent_class)->write)
		(* ((SPObjectClass *) rg_parent_class)->write) (object, repr, flags);

	return repr;
}

static SPPainter *
sp_radialgradient_painter_new (SPPaintServer *ps, const gdouble *ctm, const NRRect *bbox)
{
	SPRGPainter *rgp;
	NRMatrix gs2px;

	SPRadialGradient *rg = SP_RADIALGRADIENT (ps);
	SPGradient *gr = SP_GRADIENT (ps);

	if (!gr->color) sp_gradient_ensure_colors (gr);

	rgp = g_new (SPRGPainter, 1);

	rgp->painter.type = SP_PAINTER_IND;
	rgp->painter.fill = sp_rg_fill;

	rgp->rg = rg;

	/* fixme: We may try to normalize here too, look at
	 *        linearGradient (Lauris) */

	if (gr->units == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX) {
		NRMatrix bbox2user;
		NRMatrix gs2user;

		/* fixme: We may try to normalize here too, look at
		 *        linearGradient (Lauris) */

		/* gradientTransform goes here (Lauris) */

		/* BBox to user coordinate system */
		// translate(bbox.topleft()) * scale(bbox.dimension())
		bbox2user.c[0] = bbox->x1 - bbox->x0;
		bbox2user.c[1] = 0.0;
		bbox2user.c[2] = 0.0;
		bbox2user.c[3] = bbox->y1 - bbox->y0;
		bbox2user.c[4] = bbox->x0;
		bbox2user.c[5] = bbox->y0;

		/* fixme: (Lauris) */
		nr_matrix_multiply (&gs2user, (NRMatrix *) gr->transform, &bbox2user);
		nr_matrix_multiply (&gs2px, &gs2user, (NRMatrix *) ctm);
	} else {
		/* Problem: What to do, if we have mixed lengths and
		 * percentages? */
		/* Currently we do ignore percentages at all, but that
		 * is not good (lauris) */

		/* fixme: We may try to normalize here too, look at
		 *        linearGradient (Lauris) */

		/* fixme: (Lauris) */
		nr_matrix_multiply (&gs2px, (NRMatrix *) gr->transform, (NRMatrix *) ctm);
	}

	nr_rgradient_renderer_setup (&rgp->rgr, gr->color, gr->spread,
				     &gs2px,
				     rg->cx.computed, rg->cy.computed,
				     rg->fx.computed, rg->fy.computed,
				     rg->r.computed);

	return (SPPainter *) rgp;
}

static void
sp_radialgradient_painter_free (SPPaintServer *ps, SPPainter *painter)
{
	g_free (painter);
}

void
sp_radialgradient_set_position (SPRadialGradient *rg, gdouble cx, gdouble cy, gdouble fx, gdouble fy, gdouble r)
{
	g_return_if_fail (rg != NULL);
	g_return_if_fail (SP_IS_RADIALGRADIENT (rg));

	/* fixme: units? (Lauris)  */
	sp_svg_length_set (&rg->cx, SP_SVG_UNIT_NONE, cx, cx);
	sp_svg_length_set (&rg->cy, SP_SVG_UNIT_NONE, cy, cy);
	sp_svg_length_set (&rg->fx, SP_SVG_UNIT_NONE, fx, fx);
	sp_svg_length_set (&rg->fy, SP_SVG_UNIT_NONE, fy, fy);
	sp_svg_length_set (&rg->r, SP_SVG_UNIT_NONE, r, r);

	sp_object_request_modified (SP_OBJECT (rg), SP_OBJECT_MODIFIED_FLAG);
}


/* Builds flattened repr tree of gradient - i.e. no href */

SPRepr *
sp_radialgradient_build_repr (SPRadialGradient *rg, gboolean vector)
{
	g_return_val_if_fail (rg != NULL, NULL);
	g_return_val_if_fail (SP_IS_RADIALGRADIENT (rg), NULL);

	SPRepr *repr = sp_repr_new ("radialGradient");

	sp_object_invoke_write ((SPObject *) rg, repr, SP_OBJECT_WRITE_EXT | SP_OBJECT_WRITE_ALL);

	if (vector) {
		sp_gradient_ensure_vector ((SPGradient *) rg);
		sp_gradient_repr_set_vector ((SPGradient *) rg, repr, ((SPGradient *) rg)->vector);
	}

	return repr;
}

static void
sp_rg_fill (SPPainter *painter, NRPixBlock *pb)
{
	SPRGPainter *rgp = (SPRGPainter *) painter;

	nr_render ((NRRenderer *) &rgp->rgr, pb, NULL);
}


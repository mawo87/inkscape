#ifndef __SP_DYNA_DRAW_CONTEXT_H__
#define __SP_DYNA_DRAW_CONTEXT_H__

/*
 * Handwriting-like drawing mode
 *
 * Authors:
 *   Mitsuru Oka <oka326@parkcity.ne.jp>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * The original dynadraw code:
 *   Paul Haeberli <paul@sgi.com>
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 1999-2002 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "display/curve.h"
#include "event-context.h"
#include <display/display-forward.h>
#include <libnr/nr-point.h>

#define SP_TYPE_DYNA_DRAW_CONTEXT (sp_dyna_draw_context_get_type())
#define SP_DYNA_DRAW_CONTEXT(o) (GTK_CHECK_CAST((o), SP_TYPE_DYNA_DRAW_CONTEXT, SPDynaDrawContext))
#define SP_DYNA_DRAW_CONTEXT_CLASS(k) (GTK_CHECK_CLASS_CAST((k), SP_TYPE_DYNA_DRAW_CONTEXT, SPDynaDrawContextClass))
#define SP_IS_DYNA_DRAW_CONTEXT(o) (GTK_CHECK_TYPE((o), SP_TYPE_DYNA_DRAW_CONTEXT))
#define SP_IS_DYNA_DRAW_CONTEXT_CLASS(k) (GTK_CHECK_CLASS_TYPE((k), SP_TYPE_DYNA_DRAW_CONTEXT))

class SPDynaDrawContext;
class SPDynaDrawContextClass;

#define SAMPLING_SIZE 16        /* fixme: ?? */

struct SPDynaDrawContext
{
    SPEventContext event_context;

    SPCurve *accumulated;
    GSList *segments;
    /* current shape and curves */
    SPCanvasItem *currentshape;
    SPCurve *currentcurve;
    SPCurve *cal1;
    SPCurve *cal2;
    /* temporary work area */
    NR::Point point1[SAMPLING_SIZE];
    NR::Point point2[SAMPLING_SIZE];
    gint npoints;

    /* repr */
    SPRepr *repr;

    /* time_id if use timeout */
    gint timer_id;

    /* DynaDraw */
    NR::Point cur;
    NR::Point vel;
    NR::Point acc;
    NR::Point ang;
    NR::Point last;
    NR::Point del;
    /* attributes */
    /* fixme: shuld be merge dragging and dynahand ?? */
    guint dragging : 1;           /* mouse state: mouse is dragging */
    guint dynahand : 1;           /* mouse state: mouse is in draw */
    guint use_timeout : 1;
    guint use_calligraphic : 1;
    guint fixed_angle : 1;
    double mass, drag;
    double angle;
    double width;

    double vel_thin;
    double flatness;
};

struct SPDynaDrawContextClass
{
    SPEventContextClass parent_class;
};

GtkType sp_dyna_draw_context_get_type(void);

#endif

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=c++:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :

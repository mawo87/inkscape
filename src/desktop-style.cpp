#define __SP_DESKTOP_STYLE_C__

/*
 * Desktop style management
 *
 * Authors:
 *   bulia byak
 *
 * Copyright (C) 2004 authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "macros.h"
#include "desktop.h"
#include "sp-object.h"
#include "color.h"
#include "color-rgba.h"
#include "svg/stringstream.h"
#include "svg/svg.h"
#include "selection.h"
#include "sp-tspan.h"
#include "fontsize-expansion.h"
#include "inkscape.h"
#include "style.h"
#include "prefs-utils.h"
#include "sp-use.h"
#include "sp-flowtext.h"
#include "sp-flowregion.h"
#include "sp-gradient.h"
#include "sp-linear-gradient.h"
#include "sp-radial-gradient.h"
#include "sp-pattern.h"
#include "xml/repr.h"

#include "desktop-style.h"

void
sp_desktop_set_color(SPDesktop *desktop, ColorRGBA const &color, bool is_relative, bool fill)
{
    if (is_relative) {
        g_warning("FIXME: relative color setting not yet implemented");
        return;
    }

    guint32 rgba = SP_RGBA32_F_COMPOSE(color[0], color[1], color[2], color[3]);
    gchar b[64];
    sp_svg_write_color(b, 64, rgba);
    SPCSSAttr *css = sp_repr_css_attr_new();
    if (fill) {
        sp_repr_css_set_property(css, "fill", b);
        Inkscape::SVGOStringStream osalpha;
        osalpha << color[3];
        sp_repr_css_set_property(css, "fill-opacity", osalpha.str().c_str());
    } else {
        sp_repr_css_set_property(css, "stroke", b);
        Inkscape::SVGOStringStream osalpha;
        osalpha << color[3];
        sp_repr_css_set_property(css, "stroke-opacity", osalpha.str().c_str());
    }

    sp_desktop_set_style(desktop, css);

    sp_repr_css_attr_unref(css);
}

void
sp_desktop_apply_css_recursive(SPObject *o, SPCSSAttr *css, bool skip_lines)
{
    // non-items should not have style
    if (!SP_IS_ITEM(o))
        return;

    // 1. tspans with role=line are not regular objects in that they are not supposed to have style of their own,
    // but must always inherit from the parent text. Same for textPath.
    // However, if the line tspan or textPath contains some style (old file?), we reluctantly set our style to it too.

    // 2. Generally we allow setting style on clones (though fill&stroke currently forbids this,
    // will be fixed) but when it's inside flowRegion, do not touch it; it's just styleless shape
    // (because that's how Inkscape does flowtext). We also should not set style to its parents
    // because it will be inherited. So we skip them.

    if (!(skip_lines
          && ((SP_IS_TSPAN(o) && SP_TSPAN(o)->role == SP_TSPAN_ROLE_LINE) || SP_IS_TEXTPATH(o))
          && !SP_OBJECT_REPR(o)->attribute("style"))
        &&
        !(SP_IS_FLOWTEXT(o) ||
          SP_IS_FLOWREGION(o) ||
          SP_IS_FLOWREGIONEXCLUDE(o) ||
          (SP_IS_USE(o) &&
           SP_OBJECT_PARENT(o) &&
           (SP_IS_FLOWREGION(SP_OBJECT_PARENT(o)) ||
            SP_IS_FLOWREGIONEXCLUDE(SP_OBJECT_PARENT(o))
           )
          )
         )
        ) {

        SPCSSAttr *css_set = sp_repr_css_attr_new();
        sp_repr_css_merge(css_set, css);

        // Scale the style by the inverse of the accumulated parent transform in the paste context.
        {
            NR::Matrix const local(sp_item_i2doc_affine(SP_ITEM(o)));
            double const ex(NR::expansion(local));
            if ( ( ex != 0. )
                 && ( ex != 1. ) ) {
                sp_css_attr_scale(css_set, 1/ex);
            }
        }

        sp_repr_css_change(SP_OBJECT_REPR(o), css_set, "style");

        sp_repr_css_attr_unref(css_set);
    }

    // setting style on child of clone spills into the clone original (via shared repr), don't do it!
    if (SP_IS_USE(o))
        return;

    for (SPObject *child = sp_object_first_child(SP_OBJECT(o)) ; child != NULL ; child = SP_OBJECT_NEXT(child) ) {
        if (sp_repr_css_property(css, "opacity", NULL) != NULL) {
            // Unset properties which are accumulating and thus should not be set recursively.
            // For example, setting opacity 0.5 on a group recursively would result in the visible opacity of 0.25 for an item in the group.
            SPCSSAttr *css_recurse = sp_repr_css_attr_new();
            sp_repr_css_merge(css_recurse, css);
            sp_repr_css_set_property(css_recurse, "opacity", NULL);
            sp_desktop_apply_css_recursive(child, css_recurse, skip_lines);
            sp_repr_css_attr_unref(css_recurse);
        } else {
            sp_desktop_apply_css_recursive(child, css, skip_lines);
        }
    }
}

void
sp_desktop_set_style(SPDesktop *desktop, SPCSSAttr *css, bool change)
{
// 1. Set internal value
    sp_repr_css_merge(desktop->current, css);

// 1a. Write to prefs; make a copy and unset any URIs first
    SPCSSAttr *css_write = sp_repr_css_attr_new();
    sp_repr_css_merge(css_write, css);
    sp_css_attr_unset_uris(css_write);
    sp_repr_css_change(inkscape_get_repr(INKSCAPE, "desktop"), css_write, "style");
    sp_repr_css_attr_unref(css_write);

    if (!change)
        return;

// 2. Emit signal
    bool intercepted = desktop->_set_style_signal.emit(css);

// FIXME: in set_style, compensate pattern and gradient fills, stroke width, rect corners, font
// size for the object's own transform so that pasting fills does not depend on preserve/optimize

// 3. If nobody has intercepted the signal, apply the style to the selection
    if (!intercepted) {
        for (GSList const *i = desktop->selection->itemList(); i != NULL; i = i->next) {
            // todo: if the style is text-only, apply only to texts?
            sp_desktop_apply_css_recursive(SP_OBJECT(i->data), css, true);
        }
    }
}

SPCSSAttr *
sp_desktop_get_style(SPDesktop *desktop, bool with_text)
{
    SPCSSAttr *css = sp_repr_css_attr_new();
    sp_repr_css_merge(css, desktop->current);
    if (!css->attributeList()) {
        sp_repr_css_attr_unref(css);
        return NULL;
    } else {
        if (!with_text) {
            css = sp_css_attr_unset_text(css);
        }
        return css;
    }
}

guint32
sp_desktop_get_color(SPDesktop *desktop, bool is_fill)
{
    guint32 r = 0; // if there's no color, return black
    gchar const *property = sp_repr_css_property(desktop->current,
                                                 is_fill ? "fill" : "stroke",
                                                 "#000");

    if (desktop->current && property) { // if there is style and the property in it,
        if (strncmp(property, "url", 3)) { // and if it's not url,
            // read it
            r = sp_svg_read_color(property, r);
        }
    }

    return r;
}

void
sp_desktop_apply_style_tool(SPDesktop *desktop, Inkscape::XML::Node *repr, char const *tool, bool with_text)
{
    SPCSSAttr *css_current = sp_desktop_get_style(desktop, with_text);
    if ((prefs_get_int_attribute(tool, "usecurrent", 0) != 0) && css_current) {
        sp_repr_css_set(repr, css_current, "style");
    } else {
        Inkscape::XML::Node *tool_repr = inkscape_get_repr(INKSCAPE, tool);
        if (tool_repr) {
            SPCSSAttr *css = sp_repr_css_attr_inherited(tool_repr, "style");
            sp_repr_css_set(repr, css, "style");
            sp_repr_css_attr_unref(css);
        }
    }
    if (css_current) {
        sp_repr_css_attr_unref(css_current);
    }
}

/**
Returns the font size (in SVG pixels) of the text tool style (if text tool uses its own style) or desktop style (otherwise)
*/
double
sp_desktop_get_font_size_tool(SPDesktop *desktop)
{
    gchar const *desktop_style = inkscape_get_repr(INKSCAPE, "desktop")->attribute("style");
    gchar const *style_str = NULL;
    if ((prefs_get_int_attribute("tools.text", "usecurrent", 0) != 0) && desktop_style) {
        style_str = desktop_style;
    } else {
        Inkscape::XML::Node *tool_repr = inkscape_get_repr(INKSCAPE, "tools.text");
        if (tool_repr) {
            style_str = tool_repr->attribute("style");
        }
    }

    double ret = 12;
    if (style_str) {
        SPStyle *style = sp_style_new();
        sp_style_merge_from_style_string(style, style_str);
        ret = style->font_size.computed;
        sp_style_unref(style);
    }
    return ret;
}

int
objects_query_fillstroke (GSList *objects, SPStyle *style_res, bool const isfill)
{

    if (g_slist_length(objects) == 0) {
        /* No objects, set empty */
        return QUERY_STYLE_NOTHING;
    }

    SPIPaint *paint_res = isfill? &style_res->fill : &style_res->stroke;
    paint_res->type = SP_PAINT_TYPE_IMPOSSIBLE;
    paint_res->set = TRUE;

    gfloat c[4];
    c[0] = 0.0;
    c[1] = 0.0;
    c[2] = 0.0;
    c[3] = 0.0;
    gint num = 0;

    for (GSList const *i = objects; i != NULL; i = i->next) {
        SPObject *obj = SP_OBJECT (i->data);
        SPStyle *style = SP_OBJECT_STYLE (obj);
        if (!style) continue;

        SPIPaint *paint = isfill? &style->fill : &style->stroke;

        // 1. Bail out with QUERY_STYLE_MULTIPLE_DIFFERENT if necessary

        if ((paint_res->type != SP_PAINT_TYPE_IMPOSSIBLE) && (paint->type != paint_res->type || (paint_res->set != paint->set))) {
            return QUERY_STYLE_MULTIPLE_DIFFERENT;  // different types of paint
        }

        if (paint_res->set && paint->set && paint_res->type == SP_PAINT_TYPE_PAINTSERVER) {
            // both previous paint and this paint were a server, see if the servers are compatible

            SPPaintServer *server_res = isfill? SP_STYLE_FILL_SERVER (style_res) : SP_STYLE_STROKE_SERVER (style_res);
            SPPaintServer *server = isfill? SP_STYLE_FILL_SERVER (style) : SP_STYLE_STROKE_SERVER (style);

            if (SP_IS_LINEARGRADIENT (server_res)) {

                if (!SP_IS_LINEARGRADIENT(server))
                   return QUERY_STYLE_MULTIPLE_DIFFERENT;  // different kind of server

                SPGradient *vector = sp_gradient_get_vector ( SP_GRADIENT (server), FALSE );
                SPGradient *vector_res = sp_gradient_get_vector ( SP_GRADIENT (server_res), FALSE );
                if (vector_res != vector)
                   return QUERY_STYLE_MULTIPLE_DIFFERENT;  // different gradient vectors

            } else if (SP_IS_RADIALGRADIENT (server_res)) {

                if (!SP_IS_RADIALGRADIENT(server))
                   return QUERY_STYLE_MULTIPLE_DIFFERENT;  // different kind of server

                SPGradient *vector = sp_gradient_get_vector ( SP_GRADIENT (server), FALSE );
                SPGradient *vector_res = sp_gradient_get_vector ( SP_GRADIENT (server_res), FALSE );
                if (vector_res != vector)
                   return QUERY_STYLE_MULTIPLE_DIFFERENT;  // different gradient vectors

            } else if (SP_IS_PATTERN (server_res)) {

                if (!SP_IS_PATTERN(server))
                   return QUERY_STYLE_MULTIPLE_DIFFERENT;  // different kind of server
 
                SPPattern *pat = pattern_getroot (SP_PATTERN (server));
                SPPattern *pat_res = pattern_getroot (SP_PATTERN (server_res));
                if (pat_res != pat)
                   return QUERY_STYLE_MULTIPLE_DIFFERENT;  // different pattern roots
            }
        }

        // 2. Sum color, copy server from paint to paint_res

        if (paint_res->set && paint->set && paint->type == SP_PAINT_TYPE_COLOR) {
            // average color
            gfloat d[3];
            sp_color_get_rgb_floatv (&paint->value.color, d);
            c[0] += d[0];
            c[1] += d[1];
            c[2] += d[2];
            c[3] += SP_SCALE24_TO_FLOAT (isfill? style->fill_opacity.value : style->stroke_opacity.value);
            num ++;
        }

       if (paint_res->set && paint->set && paint->type == SP_PAINT_TYPE_PAINTSERVER) { // copy the server
           if (isfill) {
               SP_STYLE_FILL_SERVER (style_res) = SP_STYLE_FILL_SERVER (style);
           } else {
               SP_STYLE_STROKE_SERVER (style_res) = SP_STYLE_STROKE_SERVER (style);
           }
       }
       paint_res->type = paint->type;
       paint_res->set = paint->set;
       style_res->fill_rule.computed = style->fill_rule.computed; // no averaging on this, just use the last one
    }

    // After all objects processed, divide the color if necessary and return
    if (paint_res->set && paint_res->type == SP_PAINT_TYPE_COLOR) { // set the color
        g_assert (num >= 1);

        c[0] /= num;
        c[1] /= num;
        c[2] /= num;
        c[3] /= num;
        sp_color_set_rgb_float(&paint_res->value.color, c[0], c[1], c[2]);
        if (isfill) {
            style_res->fill_opacity.value = SP_SCALE24_FROM_FLOAT (c[3]);
        } else {
            style_res->stroke_opacity.value = SP_SCALE24_FROM_FLOAT (c[3]);
        }
        if (num > 1) 
            return QUERY_STYLE_MULTIPLE_AVERAGED;
        else 
            return QUERY_STYLE_SINGLE;
    }

    // Not color
    if (g_slist_length(objects) > 1) {
        return QUERY_STYLE_MULTIPLE_SAME;
    } else {
        return QUERY_STYLE_SINGLE;
    }
}


int
sp_desktop_query_style(SPDesktop *desktop, SPStyle *style, int property)
{
    int ret = desktop->_query_style_signal.emit(style, property);

    if (ret != QUERY_STYLE_NOTHING)  
        return ret; // subselection returned a style, pass it on

    // otherwise, do querying and averaging over selection
    if (property == QUERY_STYLE_PROPERTY_FILL) {
        return objects_query_fillstroke ((GSList *) desktop->selection->itemList(), style, true);
    } else if (property == QUERY_STYLE_PROPERTY_STROKE) {
        return objects_query_fillstroke ((GSList *) desktop->selection->itemList(), style, false);
    }

    return QUERY_STYLE_NOTHING;
}


/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :

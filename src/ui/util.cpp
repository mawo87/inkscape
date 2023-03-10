// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Utility functions for UI
 *
 * Authors:
 *   Tavmjong Bah
 *   John Smith
 *
 * Copyright (C) 2004, 2013, 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "util.h"

#include <cairomm/pattern.h>
#include <cstdint>
#include <gdkmm/rgba.h>
#include <gtkmm.h>
#include <stdexcept>

/**
 * Recursively look through pre-constructed widget parents for a specific named widget.
 */
Gtk::Widget *get_widget_by_name(Gtk::Container *parent, const std::string &name)
{
    for (auto child : parent->get_children()) {
        if (name == child->get_name())
            return child;
        if (auto recurse = dynamic_cast<Gtk::Container *>(child)) {
            if (auto decendant = get_widget_by_name(recurse, name))
                return decendant;
        }
    }
    return nullptr;
}

/*
 * Ellipse text if longer than maxlen, "50% start text + ... + ~50% end text"
 * Text should be > length 8 or just return the original text
 */
Glib::ustring ink_ellipsize_text(Glib::ustring const &src, size_t maxlen)
{
    if (src.length() > maxlen && maxlen > 8) {
        size_t p1 = (size_t) maxlen / 2;
        size_t p2 = (size_t) src.length() - (maxlen - p1 - 1);
        return src.substr(0, p1) + "…" + src.substr(p2);
    }
    return src;
}

/**
 * Show widget, if the widget has a Gtk::Reveal parent, reveal instead.
 *
 * @param widget - The child widget to show.
 */
void reveal_widget(Gtk::Widget *widget, bool show)
{
    auto revealer = dynamic_cast<Gtk::Revealer *>(widget->get_parent());
    if (revealer) {
        revealer->set_reveal_child(show);
    }
    if (show) {
        widget->show();
    } else if (!revealer) {
        widget->hide();
    }
}


bool is_widget_effectively_visible(Gtk::Widget const *widget) {
    if (!widget) return false;

    // TODO: what's the right way to determine if widget is visible on the screen?
    return widget->get_child_visible();
}

namespace Inkscape {
namespace UI {
void resize_widget_children(Gtk::Widget *widget) {
    if(widget) {
        Gtk::Allocation allocation;
        int             baseline;
        widget->get_allocated_size(allocation, baseline);
        widget->size_allocate(allocation, baseline);
    }
}
}
}

Gdk::RGBA mix_colors(const Gdk::RGBA& a, const Gdk::RGBA& b, float ratio) {
    auto lerp = [](double v0, double v1, double t){ return (1.0 - t) * v0 + t * v1; };
    Gdk::RGBA result;
    result.set_rgba(
        lerp(a.get_red(),   b.get_red(),   ratio),
        lerp(a.get_green(), b.get_green(), ratio),
        lerp(a.get_blue(),  b.get_blue(),  ratio),
        lerp(a.get_alpha(), b.get_alpha(), ratio)
    );
    return result;
}

Gdk::RGBA get_background_color(const Glib::RefPtr<Gtk::StyleContext> &context,
                               Gtk::StateFlags state) {
    return get_context_color(context, GTK_STYLE_PROPERTY_BACKGROUND_COLOR, state);
}

Gdk::RGBA get_context_color(const Glib::RefPtr<Gtk::StyleContext> &context,
                            const gchar *property,
                            Gtk::StateFlags state) {
    GdkRGBA *c;
    gtk_style_context_get(context->gobj(),
                          static_cast<GtkStateFlags>(state),
                          property, &c, nullptr);
    return Glib::wrap(c);
}

// 2Geom <-> Cairo

Cairo::RectangleInt geom_to_cairo(const Geom::IntRect &rect)
{
    return Cairo::RectangleInt{rect.left(), rect.top(), rect.width(), rect.height()};
}

Geom::IntRect cairo_to_geom(const Cairo::RectangleInt &rect)
{
    return Geom::IntRect::from_xywh(rect.x, rect.y, rect.width, rect.height);
}

Cairo::Matrix geom_to_cairo(const Geom::Affine &affine)
{
    return Cairo::Matrix(affine[0], affine[1], affine[2], affine[3], affine[4], affine[5]);
}

Geom::IntPoint dimensions(const Cairo::RefPtr<Cairo::ImageSurface> &surface)
{
    return Geom::IntPoint(surface->get_width(), surface->get_height());
}

Geom::IntPoint dimensions(const Gdk::Rectangle &allocation)
{
    return Geom::IntPoint(allocation.get_width(), allocation.get_height());
}

Cairo::RefPtr<Cairo::LinearGradient> create_cubic_gradient(
    Geom::Rect rect,
    const Gdk::RGBA& from,
    const Gdk::RGBA& to,
    Geom::Point ctrl1,
    Geom::Point ctrl2,
    Geom::Point p0,
    Geom::Point p1,
    int steps
) {
    // validate input points
    for (auto&& pt : {p0, ctrl1, ctrl2, p1}) {
        if (pt.x() < 0 || pt.x() > 1 ||
            pt.y() < 0 || pt.y() > 1) {
            throw std::invalid_argument("Invalid points for cubic gradient; 0..1 coordinates expected.");
        }
    }
    if (steps < 2 || steps > 999) {
        throw std::invalid_argument("Invalid number of steps for cubic gradient; 2 to 999 steps expected.");
    }

    auto g = Cairo::LinearGradient::create(rect.min().x(), rect.min().y(), rect.max().x(), rect.max().y());

    --steps;
    for (int step = 0; step <= steps; ++step) {
        auto t = 1.0 * step / steps;
        auto s = 1.0 - t;
        auto p = (t * t * t) * p0 + (3 * t * t * s) * ctrl1 + (3 * t * s * s) * ctrl2 + (s * s * s) * p1;

        auto offset = p.x();
        auto ratio = p.y();

        auto color = mix_colors(from, to, ratio);
        g->add_color_stop_rgba(offset, color.get_red(), color.get_green(), color.get_blue(), color.get_alpha());
    }

    return g;
}

Gdk::RGBA change_alpha(const Gdk::RGBA& color, double new_alpha) {
    auto copy(color);
    copy.set_alpha(new_alpha);
    return copy;
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :

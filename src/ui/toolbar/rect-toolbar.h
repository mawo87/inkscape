// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_RECT_TOOLBAR_H
#define SEEN_RECT_TOOLBAR_H

/**
 * @file
 * Rect aux toolbar
 */
/* Authors:
 *   MenTaLguY <mental@rydia.net>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Frank Felfe <innerspace@iname.com>
 *   John Cliff <simarilius@yahoo.com>
 *   David Turner <novalis@gnu.org>
 *   Josh Andler <scislac@scislac.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Maximilian Albert <maximilian.albert@gmail.com>
 *   Tavmjong Bah <tavmjong@free.fr>
 *   Abhishek Sharma
 *   Kris De Gussem <Kris.DeGussem@gmail.com>
 *
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2003 MenTaLguY
 * Copyright (C) 1999-2011 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gtkmm/adjustment.h>

#include "toolbar.h"

#include "xml/node-observer.h"

class SPDesktop;
class SPItem;
class SPRect;

namespace Gtk {
class Toolbutton;
}

namespace Inkscape {
class Selection;

namespace XML {
class Node;
}

namespace UI {
namespace Tools {
class ToolBase;
}

namespace Widget {
class LabelToolItem;
class SpinButtonToolItem;
class UnitTracker;
}

namespace Toolbar {
class RectToolbar
	: public Toolbar
	, private Inkscape::XML::NodeObserver
{
private:
    UI::Widget::UnitTracker *_tracker;

    XML::Node *_repr{nullptr};
    SPItem *_item;

    UI::Widget::LabelToolItem      *_mode_item;
    UI::Widget::SpinButtonToolItem *_width_item;
    UI::Widget::SpinButtonToolItem *_height_item;
    UI::Widget::SpinButtonToolItem *_rx_item;
    UI::Widget::SpinButtonToolItem *_ry_item;
    Gtk::ToolButton *_not_rounded;

    Glib::RefPtr<Gtk::Adjustment> _width_adj;
    Glib::RefPtr<Gtk::Adjustment> _height_adj;
    Glib::RefPtr<Gtk::Adjustment> _rx_adj;
    Glib::RefPtr<Gtk::Adjustment> _ry_adj;

    bool _freeze{false};
    bool _single{true};

    void value_changed(Glib::RefPtr<Gtk::Adjustment>&  adj,
                       gchar const                    *value_name,
                       void (SPRect::*setter)(gdouble));

    void sensitivize();
    void defaults();
    void watch_ec(SPDesktop* desktop, Inkscape::UI::Tools::ToolBase* ec);
    void selection_changed(Inkscape::Selection *selection);

    sigc::connection _changed;

	void notifyAttributeChanged(Inkscape::XML::Node &node, GQuark name,
								Inkscape::Util::ptr_shared old_value,
								Inkscape::Util::ptr_shared new_value) final;

protected:
    RectToolbar(SPDesktop *desktop);
    ~RectToolbar() override;

public:
    static GtkWidget * create(SPDesktop *desktop);
};

}
}
}

#endif /* !SEEN_RECT_TOOLBAR_H */

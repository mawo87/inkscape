// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "swatch-selector.h"

#include <glibmm/i18n.h>

#include "document-undo.h"
#include "document.h"
#include "gradient-chemistry.h"

#include "object/sp-stop.h"

#include "svg/css-ostringstream.h"
#include "svg/svg-color.h"

#include "ui/icon-names.h"
#include "ui/widget/color-notebook.h"
#include "ui/widget/gradient-selector.h"

#include "xml/node.h"

namespace Inkscape {
namespace UI {
namespace Widget {

SwatchSelector::SwatchSelector() :
    Gtk::Box(Gtk::ORIENTATION_VERTICAL),
    _gsel(nullptr),
    _updating_color(false)
{
    using Inkscape::UI::Widget::ColorNotebook;

    _gsel = Gtk::manage(new GradientSelector());
    _gsel->setMode(GradientSelector::MODE_SWATCH);

    _gsel->show();

    pack_start(*_gsel);

    auto color_selector = Gtk::manage(new ColorNotebook(_selected_color));
    color_selector->set_label(_("Swatch color"));
    color_selector->show();
    pack_start(*color_selector);

    //_selected_color.signal_grabbed.connect(sigc::mem_fun(*this, &SwatchSelector::_grabbedCb));
    _selected_color.signal_dragged.connect(sigc::mem_fun(*this, &SwatchSelector::_changedCb));
    _selected_color.signal_released.connect(sigc::mem_fun(*this, &SwatchSelector::_changedCb));
    // signal_changed doesn't get called if updating shape with colour.
    _selected_color.signal_changed.connect(sigc::mem_fun(*this, &SwatchSelector::_changedCb));
}

SwatchSelector::~SwatchSelector()
{
    _gsel = nullptr;
}

GradientSelector *SwatchSelector::getGradientSelector()
{
    return _gsel;
}

void SwatchSelector::_changedCb()
{
    if (_updating_color) {
        return;
    }
    // TODO might have to block cycles

    if (_gsel && _gsel->getVector()) {
        SPGradient *gradient = _gsel->getVector();
        SPGradient *ngr = sp_gradient_ensure_vector_normalized(gradient);
        if (ngr != gradient) {
            /* Our master gradient has changed */
            // TODO replace with proper - sp_gradient_vector_widget_load_gradient(GTK_WIDGET(swsel->_gsel), ngr);
        }

        ngr->ensureVector();


        SPStop* stop = ngr->getFirstStop();
        if (stop) {
            SPColor color = _selected_color.color();
            gfloat alpha = _selected_color.alpha();
            guint32 rgb = color.toRGBA32( 0x00 );

            // TODO replace with generic shared code that also handles icc-color
            Inkscape::CSSOStringStream os;
            gchar c[64];
            sp_svg_write_color(c, sizeof(c), rgb);
            os << "stop-color:" << c << ";stop-opacity:" << static_cast<gdouble>(alpha) <<";";
            stop->setAttribute("style", os.str());

            DocumentUndo::done(ngr->document, _("Change swatch color"), INKSCAPE_ICON("color-gradient"));
        }
    }
}

void SwatchSelector::connectchangedHandler( GCallback handler, void *data )
{
    GObject* obj = G_OBJECT(_gsel);
    g_signal_connect( obj, "changed", handler, data );
}

void SwatchSelector::setVector(SPDocument */*doc*/, SPGradient *vector)
{
    //GtkVBox * box = gobj();
    _gsel->setVector((vector) ? vector->document : nullptr, vector);

    if ( vector && vector->isSolid() ) {
        SPStop* stop = vector->getFirstStop();

        guint32 const colorVal = stop->get_rgba32();
        _updating_color = true;
        _selected_color.setValue(colorVal);
        _updating_color = false;
        // gtk_widget_show_all( GTK_WIDGET(_csel) );
    } else {
        //gtk_widget_hide( GTK_WIDGET(_csel) );
    }

/*
*/
}

} // namespace Widget
} // namespace UI
} // namespace Inkscape



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

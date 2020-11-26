// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief A dialog for the about screen
 *
 * Copyright (C) Martin Owens 2019 <doctormo@gmail.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <string>
#include <fstream>
#include <streambuf>

#include "about.h"
#include "document.h"
#include "inkscape-version.h"

#include "io/resource.h"

#include "ui/util.h"
#include "ui/view/svg-view-widget.h"

#include "util/units.h"

using namespace Inkscape::IO;
using namespace Inkscape::UI::View;

namespace Inkscape {
namespace UI {
namespace Dialog {

static Gtk::Window *window = nullptr;
static Gtk::Notebook *tabs = nullptr;

void close_about_screen() {
    window->hide();
}
bool show_copy_button(Gtk::Button *button, Gtk::Label *label) {
    reveal_widget(button, true);
    reveal_widget(label, false);
    return false;
}
void copy_version(Gtk::Button *button, Gtk::Label *label) {
    auto clipboard = Gtk::Clipboard::get();
    clipboard->set_text(Inkscape::version_string);
    if (label) {
        reveal_widget(button, false);
        reveal_widget(label, true);
        Glib::signal_timeout().connect_seconds(
            sigc::bind(sigc::ptr_fun(&show_copy_button), button, label), 2);
    }
}

void AboutDialog::show_about() {

    if(!window) {
        // Load glade file here
        Glib::ustring gladefile = Resource::get_filename(Resource::UIS, "inkscape-about.glade");
        Glib::RefPtr<Gtk::Builder> builder;
        try {
            builder = Gtk::Builder::create_from_file(gladefile);
        } catch (const Glib::Error &ex) {
            g_error("Glade file loading failed for about screen dialog");
            return;
        }
        builder->get_widget("about-screen-window", window);
        builder->get_widget("tabs", tabs);
        if(!tabs || !window) {
            g_error("Window or tabs in glade file are missing or do not have the right ids.");
            return;
        }
        // Automatic signal handling (requires -rdynamic compile flag)
        //gtk_builder_connect_signals(builder->gobj(), NULL);

        // When automatic handling fails
        Gtk::Button *version;
        Gtk::Label *label;
        builder->get_widget("version", version);
        builder->get_widget("version-copied", label);
        if(version) {
            version->set_label(Inkscape::version_string);
            version->signal_clicked().connect(
                    sigc::bind(sigc::ptr_fun(&copy_version), version, label));
        }

        // Render the about screen image via inkscape SPDocument
        auto filename = Resource::get_filename(Resource::SCREENS, "about.svg", true, false);
        SPDocument *doc = SPDocument::createNewDoc(filename.c_str(), TRUE);

        // Bind glade's container to our SVGViewWidget class
        if(doc) {
            //SVGViewWidget *viewer;
            //builder->get_widget_derived("image-container", viewer, doc);
            //Gtk::manage(viewer);
            auto viewer = Gtk::manage(new Inkscape::UI::View::SVGViewWidget(doc));
            double width = doc->getWidth().value("px");
            double height = doc->getHeight().value("px");
            viewer->setResize(width, height);

            Gtk::AspectFrame *splash_widget;
            builder->get_widget("aspect-frame", splash_widget);
            splash_widget->unset_label();
            splash_widget->set_shadow_type(Gtk::SHADOW_NONE);
            splash_widget->property_ratio() = width / height;
            splash_widget->add(*viewer);
            splash_widget->show_all();
        } else {
            g_error("Error loading about screen SVG.");
        }

        Gtk::TextView *authors;
        builder->get_widget("credits-authors", authors);
        if(authors) {
            std::ifstream fn(Resource::get_filename(Resource::DOCS, "AUTHORS"));
            std::string str((std::istreambuf_iterator<char>(fn)),
                             std::istreambuf_iterator<char>());
            authors->get_buffer()->set_text(str.c_str());
        }

        Gtk::TextView *translators;
        builder->get_widget("credits-translators", translators);
        if(translators) {
            std::ifstream fn(Resource::get_filename(Resource::DOCS, "TRANSLATORS"));
            std::string str((std::istreambuf_iterator<char>(fn)),
                             std::istreambuf_iterator<char>());
            translators->get_buffer()->set_text(str.c_str());
        }

        Gtk::Label *license;
        builder->get_widget("license-text", license);
        if(license) {
            std::ifstream fn(Resource::get_filename(Resource::DOCS, "LICENSE"));
            std::string str((std::istreambuf_iterator<char>(fn)),
                             std::istreambuf_iterator<char>());
            license->set_markup(str.c_str());
        }
    }
    if(window) {
        window->show();
        tabs->set_current_page(0);
    } else {
        g_error("About screen window couldn't be loaded. Missing window id in glade file.");
    }
}
  

} // namespace Dialog
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
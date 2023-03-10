// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Symbols dialog
 */
/* Authors:
 *   Tavmjong Bah, Martin Owens
 *
 * Copyright (C) 2012 Tavmjong Bah
 *               2013 Martin Owens
 *               2017 Jabiertxo Arraiza
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DIALOG_SYMBOLS_H
#define INKSCAPE_UI_DIALOG_SYMBOLS_H

#include <gtkmm.h>
#include <vector>

#include "display/drawing.h"
#include "helper/auto-connection.h"
#include "include/gtkmm_version.h"
#include "ui/dialog/dialog-base.h"

class SPObject;
class SPSymbol;
class SPUse;

namespace Inkscape {
namespace UI {
namespace Dialog {

struct SymbolColumns : public Gtk::TreeModel::ColumnRecord
{
    Gtk::TreeModelColumn<Glib::ustring>             symbol_id;
    Gtk::TreeModelColumn<Glib::ustring>             symbol_title;
    Gtk::TreeModelColumn<Glib::ustring>             symbol_doc_title;
    Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> symbol_image;
    Gtk::TreeModelColumn<Geom::Point>               doc_dimensions;

    SymbolColumns()
    {
        add(symbol_id);
        add(symbol_title);
        add(symbol_doc_title);
        add(symbol_image);
        add(doc_dimensions);
    }
};

/**
 * A dialog that displays selectable symbols and allows users to drag or paste
 * those symbols from the dialog into the document.
 *
 * Symbol documents are loaded from the preferences paths and displayed in a
 * drop-down list to the user. The user then selects which of the symbols
 * documents they want to get symbols from. The first document in the list is
 * always the current document.
 *
 * This then updates an icon-view with all the symbols available. Selecting one
 * puts it onto the clipboard. Dragging it or pasting it onto the canvas copies
 * the symbol from the symbol document, into the current document and places a
 * new <use> element at the correct location on the canvas.
 *
 * Selected groups on the canvas can be added to the current document's symbols
 * table, and symbols can be removed from the current document. This allows
 * new symbols documents to be constructed and if saved in the prefs folder will
 * make those symbols available for all future documents.
 */

const int SYMBOL_ICON_SIZES[] = {16, 24, 32, 48, 64};

class SymbolsDialog : public DialogBase
{
public:
    SymbolsDialog(char const *prefsPath = "/dialogs/symbols");
    ~SymbolsDialog() override;

private:
    void documentReplaced() override;
    void selectionChanged(Inkscape::Selection *selection) override;

    Glib::ustring CURRENTDOC;
    Glib::ustring ALLDOCS;
    SymbolColumns const _columns;

    void packless();
    void packmore();
    void zoomin();
    void zoomout();
    void rebuild();
    void insertSymbol();
    void revertSymbol();
    void defsModified(SPObject *object, guint flags);
    SPDocument* selectedSymbols();
    void iconChanged();
    void sendToClipboard(Gtk::TreeModel::Path const &symbol_path, Geom::Rect const &bbox);
    std::optional<Gtk::TreeModel::Path> getSelected() const;
    Glib::ustring getSymbolId(std::optional<Gtk::TreeModel::Path> const &path) const;
    Glib::ustring getSymbolDocTitle(std::optional<Gtk::TreeModel::Path> const &path) const;
    Geom::Point getSymbolDimensions(std::optional<Gtk::TreeModel::Path> const &path) const;
    void iconDragDataGet(const Glib::RefPtr<Gdk::DragContext>& context, Gtk::SelectionData& selection_data, guint info, guint time);
    void onDragStart();
    void getSymbolsTitle();
    Glib::ustring documentTitle(SPDocument* doc);
    std::pair<std::string, SPDocument*> getSymbolsSet(std::string title);
    void addSymbol(SPSymbol *symbol, Glib::ustring doc_title);
    SPDocument* symbolsPreviewDoc();
    void symbolsInDocRecursive (SPObject *r, std::map<Glib::ustring, std::pair<Glib::ustring, SPSymbol*> > &l, Glib::ustring doc_title);
    std::map<Glib::ustring, std::pair<Glib::ustring, SPSymbol*> > symbolsInDoc( SPDocument* document, Glib::ustring doc_title);
    void useInDoc(SPObject *r, std::vector<SPUse*> &l);
    std::vector<SPUse*> useInDoc( SPDocument* document);
    void beforeSearch(GdkEventKey* evt);
    void unsensitive(GdkEventKey* evt);
    void searchsymbols();
    void addSymbols();
    void addSymbolsInDoc(SPDocument* document);
    void showOverlay();
    void hideOverlay();
    void clearSearch();
    bool callbackSymbols();
    bool callbackAllSymbols();
    Glib::ustring get_active_base_text(Glib::ustring title = "selectedcombo");
    void enableWidgets(bool enable);
    Glib::ustring ellipsize(Glib::ustring data, size_t limit);
    gchar const* styleFromUse( gchar const* id, SPDocument* document);
    Glib::RefPtr<Gdk::Pixbuf> drawSymbol(SPObject *symbol);
    Glib::RefPtr<Gdk::Pixbuf> getOverlay(gint width, gint height);
    /* Keep track of all symbol template documents */
    std::map<Glib::ustring, std::pair<Glib::ustring, SPSymbol*> > l;
    // Index into sizes which is selected
    int pack_size;
    // Scale factor
    int scale_factor;
    bool sensitive;
    double previous_height;
    double previous_width;
    Geom::Point _last_mousedown; ///< Last button press position in the icon view coordinates.
    bool all_docs_processed;
    bool icons_found;
    size_t number_docs;
    size_t number_symbols;
    size_t counter_symbols;
    Glib::RefPtr<Gtk::ListStore> store;
    Glib::ustring search_str;
    Gtk::ComboBoxText* symbol_set;
    Gtk::SearchEntry* search;
    Gtk::IconView* icon_view;
    Gtk::Button* add_symbol;
    Gtk::Button* remove_symbol;
    Gtk::Button* zoom_in;
    Gtk::Button* zoom_out;
    Gtk::Button* more;
    Gtk::Button* fewer;
    Gtk::Box* tools;
    Gtk::Overlay* overlay;
    Gtk::Image* overlay_icon;
    Gtk::Image* overlay_opacity;
    Gtk::Label* overlay_title;
    Gtk::Label* overlay_desc;
    Gtk::ScrolledWindow *scroller;
    Gtk::ToggleButton* fit_symbol;
    Gtk::IconSize iconsize;

    SPDocument* preview_document; /* Document to render single symbol */

    sigc::connection idleconn;

    /* For rendering the template drawing */
    unsigned key;
    Inkscape::Drawing renderDrawing;

    std::vector<sigc::connection> gtk_connections;
    Inkscape::auto_connection defs_modified;
};

} //namespace Dialogs
} //namespace UI
} //namespace Inkscape


#endif // INKSCAPE_UI_DIALOG_SYMBOLS_H

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

// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief A dialog for CSS selectors
 */
/* Authors:
 *   Kamalpreet Kaur Grewal
 *   Tavmjong Bah
 *
 * Copyright (C) Kamalpreet Kaur Grewal 2016 <grewalkamal005@gmail.com>
 * Copyright (C) Tavmjong Bah 2017 <tavmjong@free.fr>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "styledialog.h"
#include "verbs.h"
#include "selection.h"
#include "attribute-rel-svg.h"
#include "inkscape.h"
#include "document-undo.h"

#include "ui/icon-loader.h"
#include "ui/widget/iconrenderer.h"

#include "xml/attribute-record.h"
#include "xml/node-observer.h"

#include <glibmm/i18n.h>
#include <glibmm/regex.h>

#include <map>
#include <utility>

//#define DEBUG_STYLEDIALOG

using Inkscape::DocumentUndo;
using Inkscape::Util::List;
using Inkscape::XML::AttributeRecord;

/**
 * This macro is used to remove spaces around selectors or any strings when
 * parsing is done to update XML style element or row labels in this dialog.
 */
#define REMOVE_SPACES(x) x.erase(0, x.find_first_not_of(' ')); \
    x.erase(x.find_last_not_of(' ') + 1);

namespace Inkscape {
namespace UI {
namespace Dialog {

// Keeps a watch on style element
class StyleDialog::NodeObserver : public Inkscape::XML::NodeObserver {
public:
    NodeObserver(StyleDialog* styleDialog) :
        _styleDialog(styleDialog)
    {
        g_debug("StyleDialog::NodeObserver: Constructor");
    };

    void notifyContentChanged(Inkscape::XML::Node &node,
                                      Inkscape::Util::ptr_shared old_content,
                                      Inkscape::Util::ptr_shared new_content) override;

    StyleDialog * _styleDialog;
};


void
StyleDialog::NodeObserver::notifyContentChanged(
    Inkscape::XML::Node &/*node*/,
    Inkscape::Util::ptr_shared /*old_content*/,
    Inkscape::Util::ptr_shared /*new_content*/ ) {

    g_debug("StyleDialog::NodeObserver::notifyContentChanged");

    _styleDialog->_readStyleElement();
    _styleDialog->_selectRow();
}


// Keeps a watch for new/removed/changed nodes
// (Must update objects that selectors match.)
class StyleDialog::NodeWatcher : public Inkscape::XML::NodeObserver {
public:
    NodeWatcher(StyleDialog* styleDialog, Inkscape::XML::Node *repr) :
        _styleDialog(styleDialog),
        _repr(repr)
    {
        g_debug("StyleDialog::NodeWatcher: Constructor");
    };

    void notifyChildAdded( Inkscape::XML::Node &/*node*/,
                                   Inkscape::XML::Node &child,
                                   Inkscape::XML::Node */*prev*/ ) override
    {
        if ( _styleDialog && _repr ) {
            _styleDialog->_nodeAdded( child );
        }
    }

    void notifyChildRemoved( Inkscape::XML::Node &/*node*/,
                                     Inkscape::XML::Node &child,
                                     Inkscape::XML::Node */*prev*/ ) override
    {
        if ( _styleDialog && _repr ) {
            _styleDialog->_nodeRemoved( child );
        }
    }

    void notifyAttributeChanged( Inkscape::XML::Node &node,
                                         GQuark qname,
                                         Util::ptr_shared /*old_value*/,
                                         Util::ptr_shared /*new_value*/ ) override {
        if ( _styleDialog && _repr ) {

            // For the moment only care about attributes that are directly used in selectors.
            const gchar * cname = g_quark_to_string (qname );
            Glib::ustring name;
            if (cname) {
                name = cname;
            }

            if ( name == "id" || name == "class" ) {
                _styleDialog->_nodeChanged( node );
            }
        }
    }

    StyleDialog * _styleDialog;
    Inkscape::XML::Node * _repr;  // Need to track if document changes.
};

void
StyleDialog::_nodeAdded( Inkscape::XML::Node &node ) {

    StyleDialog::NodeWatcher *w = new StyleDialog::NodeWatcher (this, &node);
    node.addObserver (*w);
    _nodeWatchers.push_back(w);

    _readStyleElement();
    _selectRow();
}

void
StyleDialog::_nodeRemoved( Inkscape::XML::Node &repr ) {

    for (auto it = _nodeWatchers.begin(); it != _nodeWatchers.end(); ++it) {
        if ( (*it)->_repr == &repr ) {
            (*it)->_repr->removeObserver (**it);
            _nodeWatchers.erase( it );
            break;
        }
    }

    _readStyleElement();
    _selectRow();
}

void
StyleDialog::_nodeChanged( Inkscape::XML::Node &object ) {

    _readStyleElement();
    _selectRow();
}

StyleDialog::TreeStore::TreeStore()
= default;


/**
 * Allow dragging only selectors.
 */
bool
StyleDialog::TreeStore::row_draggable_vfunc(const Gtk::TreeModel::Path& path) const
{
    g_debug("StyleDialog::TreeStore::row_draggable_vfunc");

    auto unconstThis = const_cast<StyleDialog::TreeStore*>(this);
    const_iterator iter = unconstThis->get_iter(path);
    if (iter) {
        Gtk::TreeModel::Row row = *iter;
        bool is_draggable      = row[_styledialog->_mColumns._colIsSelector];
        return is_draggable;
    }
    return Gtk::TreeStore::row_draggable_vfunc(path);
}


/**
 * Allow dropping only in between other selectors.
 */
bool
StyleDialog::TreeStore::row_drop_possible_vfunc(const Gtk::TreeModel::Path& dest,
                                                const Gtk::SelectionData& selection_data) const
{
    g_debug("StyleDialog::TreeStore::row_drop_possible_vfunc");

    Gtk::TreeModel::Path dest_parent = dest;
    dest_parent.up();
    return dest_parent.empty();
}


// This is only here to handle updating style element after a drag and drop.
void
StyleDialog::TreeStore::on_row_deleted(const TreeModel::Path& path)
{
    g_debug("on_row_deleted");

    if (_styledialog->_updating) return;  // Don't write if we deleted row (other than from DND)

    _styledialog->_writeStyleElement();
}


Glib::RefPtr<StyleDialog::TreeStore> StyleDialog::TreeStore::create(StyleDialog *styledialog)
{
    StyleDialog::TreeStore * store = new StyleDialog::TreeStore();
    store->_styledialog = styledialog;
    store->set_column_types( store->_styledialog->_mColumns );
    return Glib::RefPtr<StyleDialog::TreeStore>( store );
}

/**
 * Constructor
 * A treeview and a set of two buttons are added to the dialog. _addSelector
 * adds selectors to treeview. _delSelector deletes the selector from the dialog.
 * Any addition/deletion of the selectors updates XML style element accordingly.
 */
StyleDialog::StyleDialog() :
    UI::Widget::Panel("/dialogs/style", SP_VERB_DIALOG_STYLE),
    _updating(false),
    _textNode(nullptr),
    _desktopTracker()
{
    g_debug("StyleDialog::StyleDialog");

    // Tree
    Inkscape::UI::Widget::IconRenderer * addRenderer = manage(
                new Inkscape::UI::Widget::IconRenderer() );
    addRenderer->add_icon("edit-delete");
    addRenderer->add_icon("list-add");

    _store = TreeStore::create(this);
    _treeView.set_model(_store);
    _treeView.set_headers_visible(true);
    _treeView.enable_model_drag_source();
    _treeView.enable_model_drag_dest( Gdk::ACTION_MOVE );

    int addCol = _treeView.append_column("", *addRenderer) - 1;
    Gtk::TreeViewColumn *col = _treeView.get_column(addCol);
    if ( col ) {
        col->add_attribute( addRenderer->property_icon(), _mColumns._colIsSelector );
    }
    _treeView.append_column("CSS Selector", _mColumns._colSelector);
    _treeView.set_expander_column(*(_treeView.get_column(1)));

    // Pack widgets
    _paned.set_orientation(Gtk::ORIENTATION_VERTICAL);
    _paned.pack1(_mainBox, Gtk::SHRINK);
    _mainBox.set_orientation(Gtk::ORIENTATION_VERTICAL);
    _mainBox.pack_start(_scrolledWindow, Gtk::PACK_EXPAND_WIDGET);
    _scrolledWindow.add(_treeView);
    _scrolledWindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

    create = manage( new Gtk::Button() );
    _styleButton(*create, "list-add", "Add a new CSS Selector");
    create->signal_clicked().connect(sigc::mem_fun(*this, &StyleDialog::_addSelector));

    del = manage( new Gtk::Button() );
    _styleButton(*del, "list-remove", "Remove a CSS Selector");
    del->signal_clicked().connect(sigc::mem_fun(*this, &StyleDialog::_delSelector));
    del->set_sensitive(false);

    _mainBox.pack_end(_buttonBox, Gtk::PACK_SHRINK);

    _buttonBox.pack_start(*create, Gtk::PACK_SHRINK);
    _buttonBox.pack_start(*del, Gtk::PACK_SHRINK);

    _getContents()->pack_start(_paned, Gtk::PACK_EXPAND_WIDGET);


    // Signal handlers
    _treeView.signal_button_release_event().connect(   // Needs to be release, not press.
        sigc::mem_fun(*this,  &StyleDialog::_handleButtonEvent),
        false);

    _treeView.signal_button_release_event().connect_notify(
        sigc::mem_fun(*this, &StyleDialog::_buttonEventsSelectObjs),
        false);

    // Document & Desktop
    _desktop_changed_connection = _desktopTracker.connectDesktopChanged(
        sigc::mem_fun(*this, &StyleDialog::_handleDesktopChanged) );
    _desktopTracker.connect(GTK_WIDGET(gobj()));

    _document_replaced_connection = getDesktop()->connectDocumentReplaced(
        sigc::mem_fun(this, &StyleDialog::_handleDocumentReplaced));

    _selection_changed_connection = getDesktop()->getSelection()->connectChanged(
        sigc::hide(sigc::mem_fun(this, &StyleDialog::_handleSelectionChanged)));

    // Add watchers
    _updateWatchers();

    // Load tree
    _readStyleElement();
    _selectRow();

    if (!_store->children().empty()) {
        del->set_sensitive(true);
    }

}


/**
 * @brief StyleDialog::~StyleDialog
 * Class destructor
 */
StyleDialog::~StyleDialog()
{
    g_debug("StyleDialog::~StyleDialog");
    _desktop_changed_connection.disconnect();
    _document_replaced_connection.disconnect();
    _selection_changed_connection.disconnect();
}


/**
 * @brief StyleDialog::_styleTextNode
 * @return Inkscape::XML::Node* pointing to a style element's text node.
 * Returns the style element's text node. If there is no style element, one is created.
 * Ditto for text node.
 */
Inkscape::XML::Node* StyleDialog::_getStyleTextNode()
{

    Inkscape::XML::Node *styleNode = nullptr;
    Inkscape::XML::Node *textNode = nullptr;

    Inkscape::XML::Node *root = SP_ACTIVE_DOCUMENT->getReprRoot();
    for (unsigned i = 0; i < root->childCount(); ++i) {
        if (Glib::ustring(root->nthChild(i)->name()) == "svg:style") {

            styleNode = root->nthChild(i);

            for (unsigned j = 0; j < styleNode->childCount(); ++j) {
                if (styleNode->nthChild(j)->type() == Inkscape::XML::TEXT_NODE) {
                    textNode = styleNode->nthChild(j);
                }
            }

            if (textNode == nullptr) {
                // Style element found but does not contain text node!
                std::cerr << "StyleDialog::_getStyleTextNode(): No text node!" << std::endl;
                textNode = SP_ACTIVE_DOCUMENT->getReprDoc()->createTextNode("");
                styleNode->appendChild(textNode);
                Inkscape::GC::release(textNode);
            }
        }
    }

    if (styleNode == nullptr) {
        // Style element not found, create one
        styleNode = SP_ACTIVE_DOCUMENT->getReprDoc()->createElement("svg:style");
        textNode  = SP_ACTIVE_DOCUMENT->getReprDoc()->createTextNode("");

        styleNode->appendChild(textNode);
        Inkscape::GC::release(textNode);

        root->addChild(styleNode, nullptr);
        Inkscape::GC::release(styleNode);
    }

    if (_textNode != textNode) {
        _textNode = textNode;
        NodeObserver *no = new NodeObserver(this);
        textNode->addObserver(*no);
    }

    return textNode;
}


/**
 * @brief StyleDialog::_readStyleElement
 * Fill the Gtk::TreeStore from the svg:style element.
 */
void StyleDialog::_readStyleElement()
{
    g_debug("StyleDialog::_readStyleElement: updating %s", (_updating ? "true" : "false"));

    if (_updating) return; // Don't read if we wrote style element.
    _updating = true;
    _store->clear();

    Inkscape::XML::Node * textNode = _getStyleTextNode();
    if (textNode == nullptr) {
        std::cerr << "StyleDialog::_readStyleElement: No text node!" << std::endl;
    }

    // Get content from style text node.
    std::string content = (textNode->content() ? textNode->content() : "");

    // Remove end-of-lines (check it works on Windoze).
    content.erase(std::remove(content.begin(), content.end(), '\n'), content.end());

    // Remove comments (/* xxx */)
    while(content.find("/*") != std::string::npos) {
        size_t start = content.find("/*");
        content.erase(start, (content.find("*/", start) - start) +2);
    }

    // First split into selector/value chunks.
    // An attempt to use Glib::Regex failed. A C++11 version worked but
    // reportedly has problems on Windows. Using split_simple() is simpler
    // and probably faster.
    //
    // Glib::RefPtr<Glib::Regex> regex1 =
    //   Glib::Regex::create("([^\\{]+)\\{([^\\{]+)\\}");
    //
    // Glib::MatchInfo minfo;
    // regex1->match(content, minfo);

    // Split on curly brackets. Even tokens are selectors, odd are values.
    std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("[}{]", content);

    // If text node is empty, return (avoids problem with negative below).
    if (tokens.size() == 0) {
        _updating = false;
        return;
    }

    for (unsigned i = 0; i < tokens.size()-1; i += 2) {

        Glib::ustring selector = tokens[i];
        REMOVE_SPACES(selector); // Remove leading/trailing spaces

        // Get list of objects selector matches
        std::vector<SPObject *> objVec = _getObjVec( selector );

        Glib::ustring properties;
        // Check to make sure we do have a value to match selector.
        if ((i+1) < tokens.size()) {
            properties = tokens[i+1];
        } else {
            std::cerr << "StyleDialog::_readStyleElement: Missing values "
                "for last selector!" << std::endl;
        }
        REMOVE_SPACES(properties);

        Gtk::TreeModel::Row row = *(_store->append());
        row[_mColumns._colSelector]   = selector;
        row[_mColumns._colIsSelector] = true;
        row[_mColumns._colObj]        = objVec;
        row[_mColumns._colProperties] = properties;
        
        // Add as children, objects that match selector.
        for (auto& obj: objVec) {
            if (obj->cloned) continue; // Skip cloned objects (they also don't have 'id').
            Gtk::TreeModel::Row childrow = *(_store->append(row->children()));
            childrow[_mColumns._colSelector]   = "#" + Glib::ustring(obj->getId());
            childrow[_mColumns._colIsSelector] = false;
            childrow[_mColumns._colObj]        = std::vector<SPObject *>(1, obj);
            childrow[_mColumns._colProperties] = "";  // Unused
        }
    }
    _updating = false;
}


/**
 * @brief StyleDialog::_writeStyleElement
 * Update the content of the style element as selectors (or objects) are added/removed.
 */
void StyleDialog::_writeStyleElement()
{
    _updating = true;

    Glib::ustring styleContent;
    for (auto& row: _store->children()) {
        styleContent = styleContent + row[_mColumns._colSelector] +
            " { " + row[_mColumns._colProperties] + " }\n";
    }
    // We could test if styleContent is empty and then delete the style node here but there is no
    // harm in keeping it around ...

    Inkscape::XML::Node *textNode = _getStyleTextNode();
    textNode->setContent(styleContent.c_str());

    DocumentUndo::done(SP_ACTIVE_DOCUMENT, SP_VERB_DIALOG_STYLE, _("Edited style element."));

    _updating = false;
    g_debug("StyleDialog::_writeStyleElement(): | %s |", styleContent.c_str());
}


void StyleDialog::_addWatcherRecursive(Inkscape::XML::Node *node) {

    g_debug("StyleDialog::_addWatcherRecursive()");

    StyleDialog::NodeWatcher *w = new StyleDialog::NodeWatcher(this, node);
    node->addObserver(*w);
    _nodeWatchers.push_back(w);

    for (unsigned i = 0; i < node->childCount(); ++i) {
        _addWatcherRecursive(node->nthChild(i));
    }
}

/**
 * @brief StyleDialog::_updateWatchers
 * Update the watchers on objects.
 */
void StyleDialog::_updateWatchers()
{
    _updating = true;

    // Remove old document watchers
    while (!_nodeWatchers.empty()) {
        StyleDialog::NodeWatcher *w = _nodeWatchers.back();
        w->_repr->removeObserver(*w);
        _nodeWatchers.pop_back();
        delete w;
    }

    // Recursively add new watchers
    Inkscape::XML::Node *root = SP_ACTIVE_DOCUMENT->getReprRoot();
    _addWatcherRecursive(root);

    g_debug("StyleDialog::_updateWatchers(): %d", (int)_nodeWatchers.size());

    _updating = false;
}


/**
 * @brief StyleDialog::_addToSelector
 * @param row
 * Add selected objects on the desktop to the selector corresponding to 'row'.
 */
void StyleDialog::_addToSelector(Gtk::TreeModel::Row row)
{
    g_debug("StyleDialog::_addToSelector: Entrance");
    if (*row) {

        Glib::ustring selector = row[_mColumns._colSelector];

        if (selector[0] == '#') {
            // 'id' selector... add selected object's id's to list.
            Inkscape::Selection* selection = getDesktop()->getSelection();
            for (auto& obj: selection->objects()) {

                Glib::ustring id = (obj->getId()?obj->getId():"");

                std::vector<SPObject *> objVec = row[_mColumns._colObj];
                bool found = false;
                for (auto& obj: objVec) {
                    if (id == obj->getId()) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    // Update row
                    objVec.push_back(obj); // Adding to copy so need to update tree
                    row[_mColumns._colObj]      = objVec;
                    row[_mColumns._colSelector] = _getIdList( objVec );

                    // Add child row
                    Gtk::TreeModel::Row childrow = *(_store->append(row->children()));
                    childrow[_mColumns._colSelector]   = "#" + Glib::ustring(obj->getId());
                    childrow[_mColumns._colIsSelector] = false;
                    childrow[_mColumns._colObj]        = std::vector<SPObject *>(1, obj);
                    childrow[_mColumns._colProperties] = "";  // Unused
                }
            }
        }

        else if (selector[0] == '.') {
            // 'class' selector... add value to class attribute of selected objects.

            // Get first class (split on white space or comma)
            std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("[,\\s]+", selector);
            Glib::ustring className = tokens[0];
            className.erase(0,1);

            // Get list of objects to modify
            Inkscape::Selection* selection = getDesktop()->getSelection();
            std::vector<SPObject *> objVec( selection->objects().begin(),
                                            selection->objects().end() );

            _insertClass( objVec, className );

            row[_mColumns._colObj] = _getObjVec( selector );

            for (auto& obj: objVec) {
                // Add child row
                Gtk::TreeModel::Row childrow = *(_store->append(row->children()));
                childrow[_mColumns._colSelector]   = "#" + Glib::ustring(obj->getId());
                childrow[_mColumns._colIsSelector] = false;
                childrow[_mColumns._colObj]        = std::vector<SPObject *>(1, obj);
                childrow[_mColumns._colProperties] = "";  // Unused
            }
        }

        else {
            // Do nothing for element selectors.
            // std::cout << "  Element selector... doing nothing!" << std::endl;
        }
    }

    _writeStyleElement();
}


/**
 * @brief StyleDialog::_removeFromSelector
 * @param row
 * Remove the object corresponding to 'row' from the parent selector.
 */
void StyleDialog::_removeFromSelector(Gtk::TreeModel::Row row)
{
    g_debug("StyleDialog::_removeFromSelector: Entrance");
    if (*row) {

        Glib::ustring objectLabel = row[_mColumns._colSelector];
        Gtk::TreeModel::iterator iter = row->parent();
        if (iter) {
            Gtk::TreeModel::Row parent = *iter;
            Glib::ustring selector = parent[_mColumns._colSelector];

            if (selector[0] == '#') {
                // 'id' selector... remove selected object's id's to list.

                // Erase from selector label.
                auto i = selector.find(objectLabel);
                if (i != Glib::ustring::npos) {
                    selector.erase(i, objectLabel.length());
                }
                // Erase any comma/space
                if (i != Glib::ustring::npos && selector[i] == ',') {
                    selector.erase(i, 1);
                }
                if (i != Glib::ustring::npos && selector[i] == ' ') {
                    selector.erase(i, 1);
                }

                // Update store
                if (selector.empty()) {
                    _store->erase(parent);
                } else {
                    // Save new selector and update object vector.
                    parent[_mColumns._colSelector] = selector;
                    parent[_mColumns._colObj]      = _getObjVec( selector );
                    _store->erase(row);
                }
            }

            else if (selector[0] == '.') {
                // 'class' selector... remove value to class attribute of selected objects.

                std::vector<SPObject *> objVec = row[_mColumns._colObj]; // Just one
                
                // Get first class (split on white space or comma)
                std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("[,\\s]+", selector);
                Glib::ustring className = tokens[0];
                className.erase(0,1); // Erase '.'
                
                // Erase class name from 'class' attribute.
                Glib::ustring classAttr = objVec[0]->getRepr()->attribute("class");
                auto i = classAttr.find( className );
                if (i != Glib::ustring::npos) {
                    classAttr.erase(i, className.length());
                }
                if (i != Glib::ustring::npos && classAttr[i] == ' ') {
                    classAttr.erase(i, 1);
                }
                objVec[0]->getRepr()->setAttribute("class", classAttr);

                parent[_mColumns._colObj] = _getObjVec( selector );
                _store->erase(row);
            }

            else {
                // Do nothing for element selectors.
                // std::cout << "  Element selector... doing nothing!" << std::endl;
            }
        }
    }

    _writeStyleElement();

}


/**
 * @brief StyleDialog::_getIdList
 * @param sel
 * @return This function returns a comma separated list of ids for objects in input vector.
 * It is used in creating an 'id' selector. It relies on objects having 'id's.
 */
Glib::ustring StyleDialog::_getIdList(std::vector<SPObject*> sel)
{
    Glib::ustring str;
    for (auto& obj: sel) {
        str += "#" + Glib::ustring(obj->getId()) + ", ";
    }
    if (!str.empty()) {
        str.erase(str.size()-1); // Remove space at end. c++11 has pop_back() but not ustring.
        str.erase(str.size()-1); // Remove comma at end.
    }
    return str;
}

/**
 * @brief StyleDialog::_getObjVec
 * @param selector: a valid CSS selector string.
 * @return objVec: a vector of pointers to SPObject's the selector matches.
 * Return a vector of all objects that selector matches.
 */
std::vector<SPObject *> StyleDialog::_getObjVec(Glib::ustring selector) {

    std::vector<SPObject *> objVec = SP_ACTIVE_DOCUMENT->getObjectsBySelector( selector );

    g_debug("StyleDialog::_getObjVec: | %s |", selector.c_str());
    for (auto& obj: objVec) {
        g_debug("  %s", obj->getId() ? obj->getId() : "null");
    }

    return objVec;
}


/**
 * @brief StyleDialog::_insertClass
 * @param objs: list of objects to insert class
 * @param class: class to insert
 * Insert a class name into objects' 'class' attribute.
 */
void StyleDialog::_insertClass(const std::vector<SPObject *>& objVec, const Glib::ustring& className) {

    for (auto& obj: objVec) {

        if (!obj->getRepr()->attribute("class")) {
            // 'class' attribute does not exist, create it.
            obj->getRepr()->setAttribute("class", className);
        } else {
            // 'class' attribute exists, append.
            Glib::ustring classAttr = obj->getRepr()->attribute("class");

            // Split on white space.
            std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("\\s+", classAttr);
            bool add = true;
            for (auto& token: tokens) {
                if (token == className) {
                    add = false; // Might be useful to still add...
                    break;
                }
            }
            if (add) {
                obj->getRepr()->setAttribute("class", classAttr + " " + className );
            }
        }
    }
 }


/**
 * @brief StyleDialog::_selectObjects
 * @param eventX
 * @param eventY
 * This function selects objects in the drawing corresponding to the selector
 * selected in the treeview.
 */
void StyleDialog::_selectObjects(int eventX, int eventY)
{
    g_debug("StyleDialog::_selectObjects: %d, %d", eventX, eventY);

    getDesktop()->selection->clear();
    Gtk::TreeViewColumn *col = _treeView.get_column(1);
    Gtk::TreeModel::Path path;
    int x2 = 0;
    int y2 = 0;
    // To do: We should be able to do this via passing in row.
    if (_treeView.get_path_at_pos(eventX, eventY, path, col, x2, y2)) {
        if (col == _treeView.get_column(1)) {
            Gtk::TreeModel::iterator iter = _store->get_iter(path);
            if (iter) {
                Gtk::TreeModel::Row row = *iter;
                Gtk::TreeModel::Children children = row.children();
                std::vector<SPObject *> objVec = row[_mColumns._colObj];

                for (auto obj : objVec) {
                    getDesktop()->selection->add(obj);
                }
            }
        }
    }
}


/**
 * @brief StyleDialog::_addSelector
 *
 * This function opens a dialog to add a selector. The dialog is prefilled
 * with an 'id' selector containing a list of the id's of selected objects
 * or with a 'class' selector if no objects are selected.
 */
void StyleDialog::_addSelector()
{
    g_debug("StyleDialog::_addSelector: Entrance");

    // Store list of selected elements on desktop (not to be confused with selector).
    Inkscape::Selection* selection = getDesktop()->getSelection();
    std::vector<SPObject *> objVec( selection->objects().begin(),
                                    selection->objects().end() );

    // ==== Create popup dialog ====
    Gtk::Dialog *textDialogPtr =  new Gtk::Dialog();
    textDialogPtr->add_button(_("Cancel"), Gtk::RESPONSE_CANCEL);
    textDialogPtr->add_button(_("Add"),    Gtk::RESPONSE_OK);

    Gtk::Entry *textEditPtr = manage ( new Gtk::Entry() );
    textDialogPtr->get_content_area()->pack_start(*textEditPtr, Gtk::PACK_SHRINK);

    Gtk::Label *textLabelPtr = manage ( new Gtk::Label(
      _("Invalid entry: Not an id (#), class (.), or element CSS selector.")
    ) );
    textDialogPtr->get_content_area()->pack_start(*textLabelPtr, Gtk::PACK_SHRINK);

    /**
     * By default, the entrybox contains 'Class1' as text. However, if object(s)
     * is(are) selected and user clicks '+' at the bottom of dialog, the
     * entrybox will have the id(s) of the selected objects as text.
     */
    if (getDesktop()->getSelection()->isEmpty()) {
        textEditPtr->set_text(".Class1");
    } else {
        textEditPtr->set_text(_getIdList(objVec));
    }

    Gtk::Requisition sreq1, sreq2;
    textDialogPtr->get_preferred_size(sreq1, sreq2);
    int minWidth = 200;
    int minHeight = 100;
    minWidth  = (sreq2.width  > minWidth  ? sreq2.width  : minWidth );
    minHeight = (sreq2.height > minHeight ? sreq2.height : minHeight);
    textDialogPtr->set_size_request(minWidth, minHeight);
    textEditPtr->show();
    textLabelPtr->hide();
    textDialogPtr->show();


    // ==== Get response ====
    int result = -1;
    bool invalid = true;
    Glib::ustring selectorValue;

    while (invalid) {
        result = textDialogPtr->run();
        if (result != Gtk::RESPONSE_OK) { // Cancel, close dialog, etc.
            textDialogPtr->hide();
            delete textDialogPtr;
            return;
        }
        /**
         * @brief selectorName
         * This string stores selector name. The text from entrybox is saved as name
         * for selector. If the entrybox is empty, the text (thus selectorName) is
         * set to ".Class1"
         */
        selectorValue = textEditPtr->get_text();
        Glib::ustring firstWord = selectorValue.substr(0, selectorValue.find_first_of(" >+~"));

        del->set_sensitive(true);

        if (selectorValue[0] == '.' ||
            selectorValue[0] == '#' ||
            selectorValue[0] == '*' ||
            SPAttributeRelSVG::isSVGElement( firstWord ) ) {
            invalid = false;
        } else {
            textLabelPtr->show();
        }
    }
    delete textDialogPtr;

    // ==== Handle response ====

    // If class selector, add selector name to class attribute for each object
    if (selectorValue[0] == '.') {

        Glib::ustring className = selectorValue;
        className.erase(0,1);
        _insertClass(objVec, className);
    }

    // Generate a new object vector (we could have an element selector,
    // the user could have edited the id selector list, etc.).
    objVec = _getObjVec( selectorValue );

    // Add entry to GUI tree
    Gtk::TreeModel::Row row = *(_store->append());
    row[_mColumns._colSelector]   = selectorValue;
    row[_mColumns._colIsSelector] = true;
    row[_mColumns._colObj]        = objVec;

    // Add as children objects that match selector.
    for (auto& obj: objVec) {
        Gtk::TreeModel::Row childrow = *(_store->append(row->children()));
        childrow[_mColumns._colSelector]   = "#" + Glib::ustring(obj->getId());
        childrow[_mColumns._colIsSelector] = false;
        childrow[_mColumns._colObj]        = std::vector<SPObject *>(1, obj);
    }

    // Add entry to style element
    _writeStyleElement();
}

/**
 * @brief StyleDialog::_delSelector
 * This function deletes selector when '-' at the bottom is clicked.
 * Note: If deleting a class selector, class attributes are NOT changed.
 */
void StyleDialog::_delSelector()
{
    g_debug("StyleDialog::_delSelector");

    Glib::RefPtr<Gtk::TreeSelection> refTreeSelection = _treeView.get_selection();
    Gtk::TreeModel::iterator iter = refTreeSelection->get_selected();
    if (iter) {
        Gtk::TreeModel::Row row = *iter;
        _updating = true;
        _store->erase(iter);
        _updating = false;
        _writeStyleElement();
    }
}

/**
 * @brief StyleDialog::_handleButtonEvent
 * @param event
 * @return
 * Handles the event when '+' button in front of a selector name is clicked or when a '-' button in
 * front of a child object is clicked. In the first case, the selected objects on the desktop (if
 * any) are added as children of the selector in the treeview. In the latter case, the object
 * corresponding to the row is removed from the selector.
 */
bool StyleDialog::_handleButtonEvent(GdkEventButton *event)
{
    g_debug("StyleDialog::_handleButtonEvent: Entrance");

    if (event->type == GDK_BUTTON_RELEASE && event->button == 1) {
        Gtk::TreeViewColumn *col = nullptr;
        Gtk::TreeModel::Path path;
        int x = static_cast<int>(event->x);
        int y = static_cast<int>(event->y);
        int x2 = 0;
        int y2 = 0;
        if (_treeView.get_path_at_pos(x, y, path, col, x2, y2)) {
            if (col == _treeView.get_column(0)) {
                Gtk::TreeModel::iterator iter = _store->get_iter(path);
                Gtk::TreeModel::Row row = *iter;

                // Add or remove objects from a
                if (!row.parent()) {
                    // Add selected objects to selector.
                    _addToSelector(row);
                } else {
                    // Remove object from selector
                    _removeFromSelector(row);
                }
            }
        }
    }
    return false;
}

// -------------------------------------------------------------------

class PropertyData
{
public:
    PropertyData() = default;;
    PropertyData(Glib::ustring name) : _name(std::move(name)) {};

    void _setSheetValue(Glib::ustring value) { _sheetValue = value; };
    void _setAttrValue(Glib::ustring value)  { _attrValue  = value; };
    Glib::ustring _getName()       { return _name;       };
    Glib::ustring _getSheetValue() { return _sheetValue; };
    Glib::ustring _getAttrValue()  { return _attrValue;  };

private:
    Glib::ustring _name;
    Glib::ustring _sheetValue;
    Glib::ustring _attrValue;
};

// -------------------------------------------------------------------


/**
 * Handle document replaced. (Happens when a default document is immediately replaced by another
 * document in a new window.)
 */
void
StyleDialog::_handleDocumentReplaced(SPDesktop *desktop, SPDocument * /* document */)
{
    g_debug("StyleDialog::handleDocumentReplaced()");

    _selection_changed_connection.disconnect();

    _selection_changed_connection = desktop->getSelection()->connectChanged(
        sigc::hide(sigc::mem_fun(this, &StyleDialog::_handleSelectionChanged)));

    _updateWatchers();
    _readStyleElement();
    _selectRow();
}


/*
 * When a dialog is floating, it is connected to the active desktop.
 */
void
StyleDialog::_handleDesktopChanged(SPDesktop* desktop) {
    g_debug("StyleDialog::handleDesktopReplaced()");

    if (getDesktop() == desktop) {
        // This will happen after construction of dialog. We've already
        // set up signals so just return.
        return;
    }

    _selection_changed_connection.disconnect();
    _document_replaced_connection.disconnect();

    setDesktop( desktop );

    _selection_changed_connection = desktop->getSelection()->connectChanged(
        sigc::hide(sigc::mem_fun(this, &StyleDialog::_handleSelectionChanged)));
    _document_replaced_connection = desktop->connectDocumentReplaced(
        sigc::mem_fun(this, &StyleDialog::_handleDocumentReplaced));

    _updateWatchers();
    _readStyleElement();
    _selectRow();
}


/*
 * Handle a change in which objects are selected in a document.
 */
void
StyleDialog::_handleSelectionChanged() {
    g_debug("StyleDialog::_handleSelectionChanged()");
    _selectRow();
}


/**
 * @brief StyleDialog::_buttonEventsSelectObjs
 * @param event
 * This function detects single or double click on a selector in any row. Clicking
 * on a selector selects the matching objects on the desktop. A double click will
 * in addition open the CSS dialog.
 */
void StyleDialog::_buttonEventsSelectObjs(GdkEventButton* event )
{
    g_debug("StyleDialog::_buttonEventsSelectObjs");

    _updating = true;

    if (event->type == GDK_BUTTON_RELEASE && event->button == 1) {
        int x = static_cast<int>(event->x);
        int y = static_cast<int>(event->y);
        _selectObjects(x, y);
    }
    _updating = false;
}


/**
 * @brief StyleDialog::_selectRow
 * This function selects the row in treeview corresponding to an object selected
 * in the drawing. If more than one row matches, the first is chosen.
 */
void StyleDialog::_selectRow()
{
    g_debug("StyleDialog::_selectRow: updating: %s", (_updating ? "true" : "false"));

    if (_updating || !getDesktop()) return; // Avoid updating if we have set row via dialog.

    if (SP_ACTIVE_DESKTOP != getDesktop()) {
        std::cerr << "StyleDialog::_selectRow: SP_ACTIVE_DESKTOP != getDesktop()" << std::endl;
        return;
    }

    Inkscape::Selection* selection = getDesktop()->getSelection();
    if (!selection->isEmpty()) {
        SPObject *obj = selection->objects().back();

        Gtk::TreeModel::Children children = _store->children();
        for(auto row : children) {

            std::vector<SPObject *> objVec = row[_mColumns._colObj];
            for (auto & i : objVec) {
                if (obj->getId() == i->getId()) {
                    _treeView.get_selection()->select(row);
                    return;
                }
            }
        }
    }

    // Selection empty or no row matches.
    _treeView.get_selection()->unselect_all();
}


/**
 * @brief StyleDialog::_styleButton
 * @param btn
 * @param iconName
 * @param tooltip
 * Set the style of '+' and '-' buttons at the bottom of dialog.
 */
void StyleDialog::_styleButton(Gtk::Button& btn, char const* iconName,
                               char const* tooltip)
{
    GtkWidget *child = sp_get_icon_image(iconName, GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_show(child);
    btn.add(*manage(Glib::wrap(child)));
    btn.set_relief(Gtk::RELIEF_NONE);
    btn.set_tooltip_text (tooltip);
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
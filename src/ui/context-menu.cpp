/*
 * Unser-interface related object extension
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * This code is in public domain
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "context-menu.h"
#include "../xml/repr.h"
#include "desktop.h"
#include "document.h"
#include "document-undo.h"
#include "message-stack.h"
#include "preferences.h"
#include "ui/dialog/dialog-manager.h"
#include "verbs.h"

using Inkscape::DocumentUndo;

static void sp_object_type_menu(GType type, SPObject *object, SPDesktop *desktop, GtkMenu *menu);

/* Append object-specific part to context menu */

void sp_object_menu(SPObject *object, SPDesktop *desktop, GtkMenu *menu)
{
    GObjectClass *klass;
    klass = G_OBJECT_GET_CLASS(object);
    while (G_TYPE_CHECK_CLASS_TYPE((klass), SP_TYPE_OBJECT)) {
        GType type;
        type = G_TYPE_FROM_CLASS(klass);
        sp_object_type_menu(type, object, desktop, menu);
        klass = (GObjectClass*)g_type_class_peek_parent(klass);
    }
}

/* Implementation */

#include <gtk/gtk.h>
#include <glibmm/i18n.h>

#include "selection.h"
#include "selection-chemistry.h"
#include "sp-anchor.h"
#include "sp-clippath.h"
#include "sp-image.h"
#include "sp-mask.h"
#include "sp-path.h"
#include "sp-text.h"
#include "desktop-handles.h"
#include "dialogs/spellcheck.h"
#include "ui/dialog/object-attributes.h"
#include "ui/dialog/object-properties.h"



static void sp_item_menu(SPObject *object, SPDesktop *desktop, GtkMenu *menu);
static void sp_group_menu(SPObject *object, SPDesktop *desktop, GtkMenu *menu);
static void sp_anchor_menu(SPObject *object, SPDesktop *desktop, GtkMenu *menu);
static void sp_image_menu(SPObject *object, SPDesktop *desktop, GtkMenu *menu);
static void sp_shape_menu(SPObject *object, SPDesktop *desktop, GtkMenu *menu);
static void sp_text_menu(SPObject *object, SPDesktop *desktop, GtkMenu *menu);

static void sp_object_type_menu(GType type, SPObject *object, SPDesktop *desktop, GtkMenu *menu)
{
    static GHashTable *t2m = NULL;
    void (* handler)(SPObject *object, SPDesktop *desktop, GtkMenu *menu);
    if (!t2m) {
        t2m = g_hash_table_new(NULL, NULL);
        g_hash_table_insert(t2m, GUINT_TO_POINTER(SP_TYPE_ITEM), (void*)sp_item_menu);
        g_hash_table_insert(t2m, GUINT_TO_POINTER(SP_TYPE_GROUP), (void*)sp_group_menu);
        g_hash_table_insert(t2m, GUINT_TO_POINTER(SP_TYPE_ANCHOR), (void*)sp_anchor_menu);
        g_hash_table_insert(t2m, GUINT_TO_POINTER(SP_TYPE_IMAGE), (void*)sp_image_menu);
        g_hash_table_insert(t2m, GUINT_TO_POINTER(SP_TYPE_SHAPE), (void*)sp_shape_menu);
        g_hash_table_insert(t2m, GUINT_TO_POINTER(SP_TYPE_TEXT), (void*)sp_text_menu);
    }
    handler = (void (*)(SPObject*, SPDesktop*, GtkMenu*))g_hash_table_lookup(t2m, GUINT_TO_POINTER(type));
    if (handler) handler(object, desktop, menu);
}

/* SPItem */

static void sp_item_properties(GtkMenuItem *menuitem, SPItem *item);
static void sp_item_select_this(GtkMenuItem *menuitem, SPItem *item);
static void sp_item_create_link(GtkMenuItem *menuitem, SPItem *item);
static void sp_set_mask(GtkMenuItem *menuitem, SPItem *item);
static void sp_release_mask(GtkMenuItem *menuitem, SPItem *item);
static void sp_set_clip(GtkMenuItem *menuitem, SPItem *item);
static void sp_release_clip(GtkMenuItem *menuitem, SPItem *item);

/* Generate context menu item section */
static void sp_item_menu(SPObject *object, SPDesktop *desktop, GtkMenu *m)
{
    SPItem *item;
    GtkWidget *w;

    item = (SPItem *) object;

    /* Item dialog */
    w = gtk_menu_item_new_with_mnemonic(_("_Object Properties..."));
    g_object_set_data(G_OBJECT(w), "desktop", desktop);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_item_properties), item);
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);
    /* Separator */
    w = gtk_menu_item_new();
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);
    /* Select item */
    w = gtk_menu_item_new_with_mnemonic(_("_Select This"));
    if (sp_desktop_selection(desktop)->includes(item)) {
        gtk_widget_set_sensitive(w, FALSE);
    } else {
        g_object_set_data(G_OBJECT(w), "desktop", desktop);
        g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_item_select_this), item);
    }
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);
    /* Create link */
    w = gtk_menu_item_new_with_mnemonic(_("_Create Link"));
    g_object_set_data(G_OBJECT(w), "desktop", desktop);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_item_create_link), item);
    gtk_widget_set_sensitive(w, !SP_IS_ANCHOR(item));
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);
    /* Set mask */
    w = gtk_menu_item_new_with_mnemonic(_("Set Mask"));
    g_object_set_data(G_OBJECT(w), "desktop", desktop);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_set_mask), item);
    if ((item && item->mask_ref && item->mask_ref->getObject()) || (item->clip_ref && item->clip_ref->getObject())) {
        gtk_widget_set_sensitive(w, FALSE);
    } else {
        gtk_widget_set_sensitive(w, TRUE);
    }
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);
    /* Release mask */
    w = gtk_menu_item_new_with_mnemonic(_("Release Mask"));
    g_object_set_data(G_OBJECT(w), "desktop", desktop);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_release_mask), item);
    if (item && item->mask_ref && item->mask_ref->getObject()) {
        gtk_widget_set_sensitive(w, TRUE);
    } else {
        gtk_widget_set_sensitive(w, FALSE);
    }
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);
    /* Set Clip */
    w = gtk_menu_item_new_with_mnemonic(_("Set _Clip"));
    g_object_set_data(G_OBJECT(w), "desktop", desktop);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_set_clip), item);
    if ((item && item->mask_ref && item->mask_ref->getObject()) || (item->clip_ref && item->clip_ref->getObject())) {
        gtk_widget_set_sensitive(w, FALSE);
    } else {
        gtk_widget_set_sensitive(w, TRUE);
    }
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);
    /* Release Clip */
    w = gtk_menu_item_new_with_mnemonic(_("Release C_lip"));
    g_object_set_data(G_OBJECT(w), "desktop", desktop);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_release_clip), item);
    if (item && item->clip_ref && item->clip_ref->getObject()) {
        gtk_widget_set_sensitive(w, TRUE);
    } else {
        gtk_widget_set_sensitive(w, FALSE);
    }
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);

}

static void sp_item_properties(GtkMenuItem *menuitem, SPItem *item)
{
    SPDesktop *desktop;

    g_assert(SP_IS_ITEM(item));

    desktop = (SPDesktop*)g_object_get_data(G_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

    sp_desktop_selection(desktop)->set(item);

    // sp_item_dialog();
    desktop->_dlg_mgr->showDialog("ObjectProperties");
}


static void sp_set_mask(GtkMenuItem *menuitem, SPItem *item)
{
    SPDesktop *desktop;

    g_assert(SP_IS_ITEM(item));

    desktop = (SPDesktop*)g_object_get_data(G_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

	sp_selection_set_mask(desktop, false, false);
}


static void sp_release_mask(GtkMenuItem *menuitem, SPItem *item)
{
    SPDesktop *desktop;

    g_assert(SP_IS_ITEM(item));

    desktop = (SPDesktop*)g_object_get_data(G_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

    sp_selection_unset_mask(desktop, false);
}


static void sp_set_clip(GtkMenuItem *menuitem, SPItem *item)
{
    SPDesktop *desktop;

    g_assert(SP_IS_ITEM(item));

    desktop = (SPDesktop*)g_object_get_data(G_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

	sp_selection_set_mask(desktop, true, false);
}


static void sp_release_clip(GtkMenuItem *menuitem, SPItem *item)
{
    SPDesktop *desktop;

    g_assert(SP_IS_ITEM(item));

    desktop = (SPDesktop*)g_object_get_data(G_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

    sp_selection_unset_mask(desktop, true);
}


static void sp_item_select_this(GtkMenuItem *menuitem, SPItem *item)
{
    SPDesktop *desktop;

    g_assert(SP_IS_ITEM(item));

    desktop = (SPDesktop*)g_object_get_data(G_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

    sp_desktop_selection(desktop)->set(item);
}

static void sp_item_create_link(GtkMenuItem *menuitem, SPItem *item)
{
    g_assert(SP_IS_ITEM(item));
    g_assert(!SP_IS_ANCHOR(item));

    SPDesktop *desktop = (SPDesktop*)g_object_get_data(G_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

    Inkscape::XML::Document *xml_doc = desktop->doc()->getReprDoc();
    Inkscape::XML::Node *repr = xml_doc->createElement("svg:a");
    item->parent->getRepr()->addChild(repr, item->getRepr());
    SPObject *object = item->document->getObjectByRepr(repr);
    g_return_if_fail(SP_IS_ANCHOR(object));

    const char *id = item->getRepr()->attribute("id");
    Inkscape::XML::Node *child = item->getRepr()->duplicate(xml_doc);
    item->deleteObject(false);
    repr->addChild(child, NULL);
    child->setAttribute("id", id);

    Inkscape::GC::release(repr);
    Inkscape::GC::release(child);

    DocumentUndo::done(object->document, SP_VERB_NONE,
                       _("Create link"));

    sp_desktop_selection(desktop)->set(SP_ITEM(object));
    desktop->_dlg_mgr->showDialog("ObjectAttributes");
}

/* SPGroup */

static void sp_item_group_ungroup_activate(GtkMenuItem *menuitem, SPGroup *group);

static void sp_group_menu(SPObject *object, SPDesktop *desktop, GtkMenu *menu)
{
    SPItem *item=SP_ITEM(object);
    GtkWidget *w;

    /* "Ungroup" */
    w = gtk_menu_item_new_with_mnemonic(_("_Ungroup"));
    g_object_set_data(G_OBJECT(w), "desktop", desktop);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_item_group_ungroup_activate), item);
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), w);
}

static void sp_item_group_ungroup_activate(GtkMenuItem *menuitem, SPGroup *group)
{
    SPDesktop *desktop;
    GSList *children;

    g_assert(SP_IS_GROUP(group));

    desktop = (SPDesktop*)g_object_get_data(G_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

    children = NULL;
    sp_item_group_ungroup(group, &children);

    sp_desktop_selection(desktop)->setList(children);
    g_slist_free(children);
}

/* SPAnchor */

static void sp_anchor_link_properties(GtkMenuItem *menuitem, SPAnchor *anchor);
static void sp_anchor_link_follow(GtkMenuItem *menuitem, SPAnchor *anchor);
static void sp_anchor_link_remove(GtkMenuItem *menuitem, SPAnchor *anchor);

static void sp_anchor_menu(SPObject *object, SPDesktop *desktop, GtkMenu *m)
{
    SPItem *item;
    GtkWidget *w;

    item = (SPItem *) object;

    /* Link dialog */
    w = gtk_menu_item_new_with_mnemonic(_("Link _Properties..."));
    g_object_set_data(G_OBJECT(w), "desktop", desktop);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_anchor_link_properties), item);
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);
    /* Select item */
    w = gtk_menu_item_new_with_mnemonic(_("_Follow Link"));
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_anchor_link_follow), item);
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);
    /* Reset transformations */
    w = gtk_menu_item_new_with_mnemonic(_("_Remove Link"));
    g_object_set_data(G_OBJECT(w), "desktop", desktop);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_anchor_link_remove), item);
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);
}

static void sp_anchor_link_properties(GtkMenuItem *menuitem, SPAnchor */*anchor*/)
{
    SPDesktop *desktop = (SPDesktop*)g_object_get_data(G_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);
    desktop->_dlg_mgr->showDialog("ObjectAttributes");
}

static void sp_anchor_link_follow(GtkMenuItem */*menuitem*/, SPAnchor *anchor)
{
    g_return_if_fail(anchor != NULL);
    g_return_if_fail(SP_IS_ANCHOR(anchor));

    /* shell out to an external browser here */
}

static void sp_anchor_link_remove(GtkMenuItem */*menuitem*/, SPAnchor *anchor)
{
    GSList *children;

    g_return_if_fail(anchor != NULL);
    g_return_if_fail(SP_IS_ANCHOR(anchor));

    children = NULL;
    sp_item_group_ungroup(SP_GROUP(anchor), &children);

    g_slist_free(children);
}

/* Image */

static void sp_image_image_properties(GtkMenuItem *menuitem, SPAnchor *anchor);
static void sp_image_image_edit(GtkMenuItem *menuitem, SPAnchor *anchor);

static void sp_image_menu(SPObject *object, SPDesktop *desktop, GtkMenu *m)
{
    SPItem *item = SP_ITEM(object);
    GtkWidget *w;

    /* Link dialog */
    w = gtk_menu_item_new_with_mnemonic(_("Image _Properties..."));
    g_object_set_data(G_OBJECT(w), "desktop", desktop);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_image_image_properties), item);
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);

    w = gtk_menu_item_new_with_mnemonic(_("Edit Externally..."));
    g_object_set_data(G_OBJECT(w), "desktop", desktop);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_image_image_edit), item);
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);
    Inkscape::XML::Node *ir = object->getRepr();
    const gchar *href = ir->attribute("xlink:href");
    if ( (!href) || ((strncmp(href, "data:", 5) == 0)) ) {
        gtk_widget_set_sensitive( w, FALSE );
    }
}

static void sp_image_image_properties(GtkMenuItem *menuitem, SPAnchor */*anchor*/)
{
    SPDesktop *desktop = (SPDesktop*)g_object_get_data(G_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);
    desktop->_dlg_mgr->showDialog("ObjectAttributes");
}

static gchar* getImageEditorName() {
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    gchar* value = 0;
    Glib::ustring choices = prefs->getString("/options/bitmapeditor/value");
    if (!choices.empty()) {
        value = g_strdup(choices.c_str());
    }
    if (!value) {
        value = g_strdup("gimp");
    }
    return value;
}

static void sp_image_image_edit(GtkMenuItem *menuitem, SPAnchor *anchor)
{
    SPObject* obj = anchor;
    Inkscape::XML::Node *ir = obj->getRepr();
    const gchar *href = ir->attribute("xlink:href");

    GError* errThing = 0;
    Glib::ustring cmdline = getImageEditorName();
    Glib::ustring fullname;
    
#ifdef WIN32
    // g_spawn_command_line_sync parsing is done according to Unix shell rules,
    // not Windows command interpreter rules. Thus we need to enclose the
    // executable path with sigle quotes.
    int index = cmdline.find(".exe");
    if ( index < 0 ) index = cmdline.find(".bat");
    if ( index < 0 ) index = cmdline.find(".com");
    if ( index >= 0 ) {
        Glib::ustring editorBin = cmdline.substr(0, index + 4).c_str();
				Glib::ustring args = cmdline.substr(index + 4, cmdline.length()).c_str();
        editorBin.insert(0, "'");
        editorBin.append("'");
        cmdline = editorBin;
        cmdline.append(args);
    } else {
        // Enclose the whole command line if no executable path can be extracted.
        cmdline.insert(0, "'");
        cmdline.append("'");
    }
#endif

    if (strncmp (href,"file:",5) == 0) {
    // URI to filename conversion
      fullname = g_filename_from_uri(href, NULL, NULL);
    } else {
      fullname.append(href);
    }
    
    cmdline.append(" '");
    cmdline.append(fullname.c_str());
    cmdline.append("'");

    //printf("##Command line: %s\n", cmdline.c_str());

    g_spawn_command_line_async(cmdline.c_str(),
                  &errThing); 
 
    if ( errThing ) {
        g_warning("Problem launching editor (%d). %s", errThing->code, errThing->message);
        SPDesktop *desktop = (SPDesktop*)g_object_get_data(G_OBJECT(menuitem), "desktop");
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, errThing->message);
        g_error_free(errThing);
        errThing = 0;
    }
}

/* Fill and Stroke entry */
static void sp_fill_settings(GtkMenuItem *menuitem, SPItem *item)
{
    SPDesktop *desktop;

    g_assert(SP_IS_ITEM(item));

    desktop = (SPDesktop*)g_object_get_data(G_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

    if (sp_desktop_selection(desktop)->isEmpty()) {
        sp_desktop_selection(desktop)->set(item);
    }

    desktop->_dlg_mgr->showDialog("FillAndStroke");
}

/* SPShape */
static void sp_shape_menu(SPObject *object, SPDesktop *desktop, GtkMenu *m)
{
    SPItem *item;
    GtkWidget *w;

    item = (SPItem *) object;

    /* Item dialog */
    w = gtk_menu_item_new_with_mnemonic(_("_Fill and Stroke..."));
    g_object_set_data(G_OBJECT(w), "desktop", desktop);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_fill_settings), item);
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);
}

/* Edit Text entry */
static void sp_text_settings(GtkMenuItem *menuitem, SPItem *item)
{
    SPDesktop *desktop;

    g_assert(SP_IS_ITEM(item));

    desktop = (SPDesktop*)g_object_get_data(G_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

    if (sp_desktop_selection(desktop)->isEmpty()) {
        sp_desktop_selection(desktop)->set(item);
    }

    desktop->_dlg_mgr->showDialog("TextFont");
}

/* Spellcheck entry */
static void sp_spellcheck_settings(GtkMenuItem *menuitem, SPItem *item)
{
    SPDesktop *desktop;

    g_assert(SP_IS_ITEM(item));

    desktop = (SPDesktop*)g_object_get_data(G_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

    if (sp_desktop_selection(desktop)->isEmpty()) {
        sp_desktop_selection(desktop)->set(item);
    }

    sp_spellcheck_dialog();
}

/* SPText */
static void sp_text_menu(SPObject *object, SPDesktop *desktop, GtkMenu *m)
{
    SPItem *item;
    GtkWidget *w;

    item = (SPItem *) object;

    /* Fill and Stroke dialog */
    w = gtk_menu_item_new_with_mnemonic(_("_Fill and Stroke..."));
    g_object_set_data(G_OBJECT(w), "desktop", desktop);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_fill_settings), item);
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);
    
    /* Edit Text dialog */
    w = gtk_menu_item_new_with_mnemonic(_("_Text and Font..."));
    g_object_set_data(G_OBJECT(w), "desktop", desktop);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_text_settings), item);
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);

    /* Spellcheck dialog */
    w = gtk_menu_item_new_with_mnemonic(_("Check Spellin_g..."));
    g_object_set_data(G_OBJECT(w), "desktop", desktop);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(sp_spellcheck_settings), item);
    gtk_widget_show(w);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), w);
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

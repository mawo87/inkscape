#define __SP_FONT_SELECTOR_C__

/*
 * Font selection widgets
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Johan Engelen <j.b.c.engelen@ewi.utwente.nl>
 *
 * Copyright (C) 1999-2001 Ximian, Inc.
 * Copyright (C) 2002 Lauris Kaplinski
 * Copyright (C) -2007 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libnrtype/font-instance.h>
#include <libnrtype/font-lister.h>

#include <2geom/transforms.h>

#include <gtk/gtk.h>

#include <glibmm/i18n.h>

#include "desktop.h"
#include "widgets/font-selector.h"

/* SPFontSelector */

struct SPFontSelector
{
    GtkHBox hbox;

    unsigned int block_emit : 1;

    GtkWidget *family;
    GtkWidget *style;
    GtkWidget *size;

    GtkWidget *family_treeview;
    GtkWidget *style_treeview;

    NRNameList families;
    NRStyleList styles;
    int familyidx;
    int styleidx;
    gfloat fontsize;
    bool fontsize_dirty;
    font_instance *font;
};


struct SPFontSelectorClass
{
    GtkHBoxClass parent_class;

    void (* font_set) (SPFontSelector *fsel, font_instance *font);
};

enum {
    FONT_SET,
    LAST_SIGNAL
};

static void sp_font_selector_class_init         (SPFontSelectorClass    *c);
static void sp_font_selector_init               (SPFontSelector         *fsel);
static void sp_font_selector_destroy            (GtkObject              *object);

static void sp_font_selector_family_select_row  (GtkTreeSelection       *selection,
                                                 SPFontSelector         *fsel);

static void sp_font_selector_style_select_row   (GtkTreeSelection       *selection,
                                                 SPFontSelector         *fsel);

static void sp_font_selector_size_changed       (GtkComboBox            *combobox,
                                                 SPFontSelector         *fsel);

static void sp_font_selector_emit_set           (SPFontSelector         *fsel);

namespace {
    const char *sizes[] = {
        "4", "6", "8", "9", "10", "11", "12", "13", "14",
        "16", "18", "20", "22", "24", "28",
        "32", "36", "40", "48", "56", "64", "72", "144",
        NULL
    };
}

static GtkHBoxClass *fs_parent_class = NULL;
static guint fs_signals[LAST_SIGNAL] = { 0 };

GType sp_font_selector_get_type()
{
    static GType type = 0;
    if (!type) {
        GTypeInfo info = {
            sizeof(SPFontSelectorClass),
            0, // base_init
            0, // base_finalize
            (GClassInitFunc)sp_font_selector_class_init,
            0, // class_finalize
            0, // class_data
            sizeof(SPFontSelector),
            0, // n_preallocs
            (GInstanceInitFunc)sp_font_selector_init,
            0 // value_table
        };
        type = g_type_register_static(GTK_TYPE_HBOX, "SPFontSelector", &info, static_cast<GTypeFlags>(0));
    }
    return type;
}

static void sp_font_selector_class_init(SPFontSelectorClass *c)
{
    GtkObjectClass *object_class = (GtkObjectClass *) c;

    fs_parent_class = (GtkHBoxClass* )g_type_class_peek_parent (c);

    fs_signals[FONT_SET] = gtk_signal_new ("font_set",
                                           GTK_RUN_FIRST,
                                           GTK_CLASS_TYPE(object_class),
                                           GTK_SIGNAL_OFFSET(SPFontSelectorClass, font_set),
                                           g_cclosure_marshal_VOID__POINTER,
                                           GTK_TYPE_NONE,
                                           1, GTK_TYPE_POINTER);

    object_class->destroy = sp_font_selector_destroy;
}

static void sp_font_selector_init(SPFontSelector *fsel)
{
        gtk_box_set_homogeneous(GTK_BOX(fsel), TRUE);
        gtk_box_set_spacing(GTK_BOX(fsel), 4);

        /* Family frame */
        GtkWidget *f = gtk_frame_new(_("Font family"));
        gtk_widget_show (f);
        gtk_box_pack_start (GTK_BOX(fsel), f, TRUE, TRUE, 0);

        GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
        gtk_widget_show(sw);
        gtk_container_set_border_width(GTK_CONTAINER (sw), 4);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
        gtk_container_add(GTK_CONTAINER(f), sw);

        Inkscape::FontLister* fontlister = Inkscape::FontLister::get_instance();

        fsel->family_treeview = gtk_tree_view_new ();
        GtkTreeViewColumn *column = gtk_tree_view_column_new ();
        GtkCellRenderer *cell = gtk_cell_renderer_text_new ();
        gtk_tree_view_column_pack_start (column, cell, FALSE);
        gtk_tree_view_column_set_attributes (column, cell, "text", 0, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW(fsel->family_treeview), column);
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(fsel->family_treeview), FALSE);
        Glib::RefPtr<Gtk::ListStore> store = fontlister->get_font_list();
        gtk_tree_view_set_model (GTK_TREE_VIEW(fsel->family_treeview), GTK_TREE_MODEL (Glib::unwrap (store)));
        gtk_container_add(GTK_CONTAINER(sw), fsel->family_treeview);
        gtk_widget_show_all (sw);

        GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(fsel->family_treeview));
        g_signal_connect (G_OBJECT(selection), "changed", G_CALLBACK (sp_font_selector_family_select_row), fsel);
        g_object_set_data (G_OBJECT(fsel), "family-treeview", fsel->family_treeview);


        /* Style frame */
        f = gtk_frame_new(C_("Font selector", "Style"));
        gtk_widget_show(f);
        gtk_box_pack_start(GTK_BOX (fsel), f, TRUE, TRUE, 0);

        GtkWidget *vb = gtk_vbox_new(FALSE, 4);
        gtk_widget_show(vb);
        gtk_container_set_border_width(GTK_CONTAINER (vb), 4);
        gtk_container_add(GTK_CONTAINER(f), vb);

        sw = gtk_scrolled_window_new(NULL, NULL);
        gtk_widget_show(sw);
        gtk_container_set_border_width(GTK_CONTAINER (sw), 4);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
        gtk_box_pack_start(GTK_BOX (vb), sw, TRUE, TRUE, 0);

        fsel->style_treeview = gtk_tree_view_new ();
        column = gtk_tree_view_column_new ();
        cell = gtk_cell_renderer_text_new ();
        gtk_tree_view_column_pack_start (column, cell, FALSE);
        gtk_tree_view_column_set_attributes (column, cell, "text", 0, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW(fsel->style_treeview), column);
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(fsel->style_treeview), FALSE);
        gtk_container_add(GTK_CONTAINER(sw), fsel->style_treeview);
        gtk_widget_show_all (sw);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(fsel->style_treeview));
        g_signal_connect (G_OBJECT(selection), "changed", G_CALLBACK (sp_font_selector_style_select_row), fsel);

        GtkWidget *hb = gtk_hbox_new(FALSE, 4);
        gtk_widget_show(hb);
        gtk_box_pack_start(GTK_BOX(vb), hb, FALSE, FALSE, 0);

        fsel->size = gtk_combo_box_entry_new_text ();
        gtk_widget_set_tooltip_text (fsel->size, _("Font size (px)"));
        gtk_widget_set_size_request(fsel->size, 90, -1);
        g_signal_connect (G_OBJECT(fsel->size), "changed", G_CALLBACK (sp_font_selector_size_changed), fsel);
        gtk_box_pack_end (GTK_BOX(hb), fsel->size, FALSE, FALSE, 0);

        GtkWidget *l = gtk_label_new(_("Font size:"));
        gtk_widget_show_all (l);
        gtk_box_pack_end(GTK_BOX (hb), l, FALSE, FALSE, 0);

        for (unsigned int n = 0; sizes[n]; ++n)
            {
                gtk_combo_box_append_text (GTK_COMBO_BOX(fsel->size), sizes[n]);
            }

        gtk_widget_show_all (fsel->size);

        fsel->familyidx = 0;
        fsel->styleidx = 0;
        fsel->fontsize = 10.0;
        fsel->fontsize_dirty = false;
        fsel->font = NULL;
}

static void sp_font_selector_destroy(GtkObject *object)
{
    SPFontSelector *fsel = SP_FONT_SELECTOR (object);

    if (fsel->font) {
        fsel->font->Unref();
        fsel->font = NULL;
    }

    if (fsel->families.length > 0) {
        nr_name_list_release(&fsel->families);
        fsel->families.length = 0;
    }

    if (fsel->styles.length > 0) {
        nr_style_list_release(&fsel->styles);
        fsel->styles.length = 0;
    }

    if (GTK_OBJECT_CLASS(fs_parent_class)->destroy) {
        GTK_OBJECT_CLASS(fs_parent_class)->destroy(object);
    }
}

static void sp_font_selector_family_select_row(GtkTreeSelection *selection,
                                               SPFontSelector *fsel)
{
    GtkTreeIter   iter;
    GtkTreeModel *model;
    GtkListStore *store;
    GtkTreePath  *path;
    GList        *list=0;

    if (!gtk_tree_selection_get_selected (selection, &model, &iter)) return;

    path = gtk_tree_model_get_path (model, &iter);
    gtk_tree_model_get (model, &iter, 1, &list, -1);
    fsel->familyidx = gtk_tree_path_get_indices (path)[0];
    fsel->styleidx = 0;

    store = gtk_list_store_new (1, G_TYPE_STRING);

    for ( ; list ; list = list->next )
    {
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, 0, (char*)list->data, -1);
    }

    gtk_tree_view_set_model (GTK_TREE_VIEW (fsel->style_treeview), GTK_TREE_MODEL (store));
    path = gtk_tree_path_new ();
    gtk_tree_path_append_index (path, 0);
    gtk_tree_selection_select_path (gtk_tree_view_get_selection (GTK_TREE_VIEW (fsel->style_treeview)), path);
    gtk_tree_path_free (path);
}

static void sp_font_selector_style_select_row (GtkTreeSelection *selection,
                                               SPFontSelector   *fsel)
{
    GtkTreeModel *model;
    GtkTreePath  *path;
    GtkTreeIter   iter;

    if (!gtk_tree_selection_get_selected (selection, &model, &iter)) return;

    path = gtk_tree_model_get_path (model, &iter);
    fsel->styleidx = gtk_tree_path_get_indices (path)[0];

    if (!fsel->block_emit)
    {
        sp_font_selector_emit_set (fsel);
    }
}

static void sp_font_selector_size_changed( GtkComboBox */*cbox*/, SPFontSelector *fsel )
{
    char *text = gtk_combo_box_get_active_text (GTK_COMBO_BOX (fsel->size));
    gfloat old_size = fsel->fontsize;

    gchar *endptr;
    gdouble value = -1;
    if (text) {
        value = g_strtod (text, &endptr);
        if (endptr == text) // conversion failed, non-numeric input
            value = -1;
        free (text);
    }
    if (value <= 0) {
        return; // could not parse value 
    }
    if (value > 10000)
        value = 10000; // somewhat arbitrary, but text&font preview freezes with too huge fontsizes

    fsel->fontsize = value;
    if ( fabs(fsel->fontsize-old_size) > 0.001)
    {
        fsel->fontsize_dirty = true;
    }

    sp_font_selector_emit_set (fsel);
}

static void sp_font_selector_emit_set (SPFontSelector *fsel)
{
    font_instance *font;

    GtkTreeSelection *selection_family;
    GtkTreeSelection *selection_style;
    GtkTreeModel     *model_family;
    GtkTreeModel     *model_style;
    GtkTreeIter       iter_family;
    GtkTreeIter       iter_style;
    char             *family=0, *style=0;

    //We need to check this here since most GtkTreeModel operations are not atomic
    //See GtkListStore documenation, Chapter "Atomic Operations" --mderezynski

    model_family = gtk_tree_view_get_model (GTK_TREE_VIEW (fsel->family_treeview));
    if (!model_family) return;
    model_style = gtk_tree_view_get_model (GTK_TREE_VIEW (fsel->style_treeview));
    if (!model_style) return;

    selection_family = gtk_tree_view_get_selection (GTK_TREE_VIEW (fsel->family_treeview));
    selection_style = gtk_tree_view_get_selection (GTK_TREE_VIEW (fsel->style_treeview));

    if (!gtk_tree_selection_get_selected (selection_family, NULL, &iter_family)) return;
    if (!gtk_tree_selection_get_selected (selection_style, NULL, &iter_style)) return;

    gtk_tree_model_get (model_family, &iter_family, 0, &family, -1);
    gtk_tree_model_get (model_style, &iter_style, 0, &style, -1);

    if ((!family) || (!style)) return;

    font = (font_factory::Default())->FaceFromUIStrings (family, style);

    // FIXME: when a text object uses non-available font, font==NULL and we can't set size
    // (and the size shown in the widget is invalid). To fix, here we must always get some
    // default font, exactly the same as sptext uses for on-canvas display, so that
    // font!=NULL ever.
    if (font != fsel->font || ( font && fsel->fontsize_dirty ) ) {
        if ( font ) {
            font->Ref();
        }
        if ( fsel->font ) {
            fsel->font->Unref();
        }
        fsel->font = font;
        gtk_signal_emit(GTK_OBJECT(fsel), fs_signals[FONT_SET], fsel->font);
    }
    fsel->fontsize_dirty = false;
    if (font) {
        font->Unref();
    }
    font = NULL;
}

GtkWidget *sp_font_selector_new()
{
    SPFontSelector *fsel = (SPFontSelector*) g_object_new(SP_TYPE_FONT_SELECTOR, NULL);

    return (GtkWidget *) fsel;
}

void sp_font_selector_set_font (SPFontSelector *fsel, font_instance *font, double size)
{
    if (font)
    {  
        Gtk::TreePath path;
        font_instance *tempFont = NULL;
        
        Glib::ustring family = font_factory::Default()->GetUIFamilyString(font->descr);

        try {
            path = Inkscape::FontLister::get_instance()->get_row_for_font (family);
        } catch (...) {
            return;
        }

        fsel->block_emit = TRUE;
        gtk_tree_selection_select_path (gtk_tree_view_get_selection (GTK_TREE_VIEW (fsel->family_treeview)), path.gobj());
        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (fsel->family_treeview), path.gobj(), NULL, TRUE, 0.5, 0.5);
        fsel->block_emit = FALSE;

        GList *list = 0;
        GtkTreeIter iter;
        GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW(fsel->family_treeview));
        gtk_tree_model_get_iter (model, &iter, path.gobj());
        gtk_tree_model_get (model, &iter, 1, &list, -1);

        unsigned int currentStyleNumber = 0;
        unsigned int bestStyleNumber = 0;
        
        PangoFontDescription *incomingFont = pango_font_description_copy(font->descr);
        pango_font_description_unset_fields(incomingFont, PANGO_FONT_MASK_SIZE);
        
        char *incomingFontString = pango_font_description_to_string(incomingFont);
        
        tempFont = (font_factory::Default())->FaceFromUIStrings(family.c_str(), (char*)list->data);
        
        PangoFontDescription *bestMatchForFont = NULL;
        if (tempFont) {
            bestMatchForFont = pango_font_description_copy(tempFont->descr);
            tempFont->Unref();
            tempFont = NULL;
        }
        
        pango_font_description_unset_fields(bestMatchForFont, PANGO_FONT_MASK_SIZE);
        
        list = list->next;
        
        while (list) {
            currentStyleNumber++;
            
            tempFont = font_factory::Default()->FaceFromUIStrings(family.c_str(), (char*)list->data);
            
            PangoFontDescription *currentMatchForFont = NULL;
            if (tempFont) {
                currentMatchForFont = pango_font_description_copy(tempFont->descr);
                tempFont->Unref();
                tempFont = NULL;
            }
            
            if (currentMatchForFont) {
                pango_font_description_unset_fields(currentMatchForFont, PANGO_FONT_MASK_SIZE);
                
                char *currentMatchString = pango_font_description_to_string(currentMatchForFont);
                
                if (!strcmp(incomingFontString, currentMatchString)
                        || pango_font_description_better_match(incomingFont, bestMatchForFont, currentMatchForFont)) {
                    // Found a better match for the font we are looking for
                    pango_font_description_free(bestMatchForFont);
                    bestMatchForFont = pango_font_description_copy(currentMatchForFont);
                    bestStyleNumber = currentStyleNumber;
                }
                
                g_free(currentMatchString);
                
                pango_font_description_free(currentMatchForFont);
            }
            
            list = list->next;
        }
        
        if (bestMatchForFont)
            pango_font_description_free(bestMatchForFont);
        if (incomingFont)
            pango_font_description_free(incomingFont);
        g_free(incomingFontString);

        GtkTreePath *path_c = gtk_tree_path_new ();
        gtk_tree_path_append_index (path_c, bestStyleNumber);
        gtk_tree_selection_select_path (gtk_tree_view_get_selection (GTK_TREE_VIEW (fsel->style_treeview)), path_c);
        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (fsel->style_treeview), path_c, NULL, TRUE, 0.5, 0.5);
    
        if (size != fsel->fontsize)
        {
            gchar s[8];
            g_snprintf (s, 8, "%.5g", size); // UI, so printf is ok
            gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(fsel->size))), s);
            fsel->fontsize = size;
        }
    }
    
}

font_instance* sp_font_selector_get_font(SPFontSelector *fsel)
{
    if (fsel->font) {
        fsel->font->Ref();
    }

    return fsel->font;
}

double sp_font_selector_get_size(SPFontSelector *fsel)
{
    return fsel->fontsize;
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

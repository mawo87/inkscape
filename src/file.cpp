#define __SP_FILE_C__

/*
 * File/Print operations
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Chema Celorio <chema@celorio.com>
 *
 * Copyright (C) 1999-2002 Authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

/**
 * Note: This file needs to be cleaned up extensively.
 * What it probably needs is to have one .h file for
 * the API, and two or more .cpp files for the implementations.
 */

#include <config.h>

#include <string.h>
#include <time.h>
#include <libnr/nr-pixops.h>
#include <glib.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkfilesel.h>

#include "macros.h"
#include "xml/repr-private.h"
#include "document.h"
#include "view.h"
#include "dir-util.h"
#include "helper/png-write.h"
#include "dialogs/export.h"
#include "helper/sp-intl.h"
#include "inkscape.h"
#include "desktop.h"
#include "sp-image.h"
#include "interface.h"
#include "print.h"
#include "file.h"
#include "dialogs/dialog-events.h"

#include "sp-namedview.h"

#include "module.h"
#include "modules/menu.h"
#include "modules/system.h"

#ifdef WIN32
#include "modules/win32.h"
#endif

#ifndef WIN32
static void sp_file_save_ok (GtkWidget *save_dialog, SPDocument *doc, const gchar *key);
#endif

/**
 * 'Current' paths.  Used to remember which directory
 * had the last file accessed.
 * Static globals are evil.  This will be gone soon
 * as C++ification continues
 */
gchar *open_path   = NULL;
gchar *save_path   = NULL;
gchar *import_path = NULL;
gchar *export_path = NULL;



/*######################
## N E W
######################*/

void
sp_file_new (void)
{
	SPDocument * doc;
	SPViewWidget *dtw;

	doc = sp_document_new (NULL, TRUE, TRUE);
	g_return_if_fail (doc != NULL);

	dtw = sp_desktop_widget_new (sp_document_namedview (doc, NULL));
	sp_document_unref (doc);
	g_return_if_fail (dtw != NULL);

	sp_create_window (dtw, TRUE);
      sp_namedview_window_from_document (SP_DESKTOP(dtw->view));
}


/*######################
## D E L E T E
######################*/

void
sp_file_exit (void)
{
	sp_ui_close_all ();
	// no need to call inkscape_exit here; last document being closed will take care of that
}


/*######################
## O P E N
######################*/

void
sp_file_open (const gchar *uri, const gchar *key)
{
	SPDocument *doc;

	doc = NULL;
	if (!key) key = SP_MODULE_KEY_INPUT_DEFAULT;
	doc = sp_module_system_open (key, uri);
	if (doc) {
		SPViewWidget *dtw;
		dtw = sp_desktop_widget_new (sp_document_namedview (doc, NULL));
		sp_document_unref (doc);
		sp_create_window (dtw, TRUE);
		sp_namedview_window_from_document (SP_DESKTOP(dtw->view));
	} else {
		gchar *text;
		text = g_strdup_printf(_("Failed to load the requested file %s"), uri);
		sp_ui_error_dialog (text);
		g_free (text);
	}
}

/**
 * OK callback for GTK dialog
 */
static void
file_open_dialog_ok (GtkWidget *widget, GtkFileSelection *fs)
{
	gchar *filename;

	filename = g_strdup (gtk_file_selection_get_filename (fs));

	if (filename && g_file_test (filename, G_FILE_TEST_IS_DIR)) {
		g_free (open_path);
		if (filename[strlen(filename) - 1] != G_DIR_SEPARATOR) {
			open_path = g_strconcat (filename, G_DIR_SEPARATOR_S, NULL);
			g_free (filename);
		} else {
			open_path = filename;
		}
		gtk_file_selection_set_filename (fs, open_path);
		return;
	}

	if (filename != NULL) {
		gchar* key;
		g_free (open_path);
		open_path = g_dirname (filename);
		if (open_path) open_path = g_strconcat (open_path, G_DIR_SEPARATOR_S, NULL);
		key = (gchar*)g_object_get_data (G_OBJECT (fs), "type-key");
		sp_file_open (filename, key);
		g_free (filename);
	}

	gtk_widget_destroy (GTK_WIDGET (fs));
}

/**
 * Cancel callback for GTK dialog
 */
static void
file_open_dialog_cancel (GtkButton *b, GtkFileSelection *fs)
{
	gtk_widget_destroy (GTK_WIDGET (fs));
}

/**
 * Type pulldown menu selection callback for GTK dialog
 */
static void
file_open_dialog_type_selected (SPMenu *menu, gpointer itemdata, GObject *fsel)
{
	g_object_set_data (fsel, "type-key", itemdata);
}

void
sp_file_open_dialog (gpointer object, gpointer data)
{
#ifdef WIN32
	char *filename;
	filename = sp_win32_get_open_filename ((unsigned char *)open_path,
                 (unsigned char *)"SVG files\0*.svg;*.svgz\0All files\0*\0",
                 (unsigned char *)_("Select file to open"));
	if (filename) {
		g_free (open_path);
		open_path = g_dirname (filename);
		if (open_path) open_path = g_strconcat (open_path, G_DIR_SEPARATOR_S, NULL);
		sp_file_open (filename, NULL);
		g_free (filename);
	}
#else
	GtkFileSelection *fsel;
	GtkWidget *hb, *l, *om, *m;

	fsel = (GtkFileSelection *) gtk_file_selection_new (_("Select file to open"));
	gtk_file_selection_hide_fileop_buttons (fsel);

	g_signal_connect (G_OBJECT (fsel->ok_button), "clicked", G_CALLBACK (file_open_dialog_ok), fsel);
	g_signal_connect (G_OBJECT (fsel->cancel_button), "clicked", G_CALLBACK (file_open_dialog_cancel), fsel);

	if (open_path) gtk_file_selection_set_filename (fsel, open_path);

	/* Create file type box */
	hb = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (fsel->main_vbox), hb, FALSE, FALSE, 0);
	om = gtk_option_menu_new ();
	gtk_box_pack_end (GTK_BOX (hb), om, FALSE, FALSE, 0);

 	m = GTK_WIDGET(sp_module_menu_open ());
	g_object_set_data (G_OBJECT (fsel), "type-key", ((SPMenu *) m)->activedata);
	g_signal_connect (G_OBJECT (m), "selected", G_CALLBACK (file_open_dialog_type_selected), fsel);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (om), m);
	l = gtk_label_new (_("File type:"));
	gtk_box_pack_end (GTK_BOX (hb), l, FALSE, FALSE, 0);
	gtk_widget_show_all (hb);

	gtk_window_set_modal (GTK_WINDOW (fsel), TRUE);
	sp_transientize ((GtkWidget *) fsel);

	gtk_widget_show ((GtkWidget *) fsel);
#endif
}




/*######################
## S A V E
######################*/

/**
 * This 'save' function called by the others below
 */
static void
file_save (SPDocument *doc, const gchar *uri, const gchar *key)
{
	if (!doc) return;
	if (!uri) return;

	if (!key) key = SP_MODULE_KEY_OUTPUT_DEFAULT;
 	return sp_module_system_save (key, doc, uri);
}

static gboolean
sp_file_save_dialog (SPDocument *doc)
{
#ifdef WIN32
	char *filename;
	unsigned int spns;
	filename = sp_win32_get_save_filename ((unsigned char *)save_path, &spns);
	if (filename && *filename) {
		file_save (doc, filename, (spns) ? SP_MODULE_KEY_OUTPUT_SVG_INKSCAPE : SP_MODULE_KEY_OUTPUT_SVG);
		g_free (save_path);
		save_path = g_dirname (filename);
		save_path = g_strdup (save_path);
		g_free (filename);
		
		return TRUE;
	} else {
		return FALSE;
	}
#else
	GtkFileSelection *fs;
	GtkWidget *dlg, *hb, *l, *om, *menu;
	int b;

	dlg = gtk_file_selection_new (_("Save file"));
	g_object_set_data (G_OBJECT (dlg), "document", doc);
	gtk_window_set_modal (GTK_WINDOW (dlg), TRUE);

	fs = GTK_FILE_SELECTION (dlg);

	hb = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (fs->main_vbox), hb, FALSE, FALSE, 0);
	om = gtk_option_menu_new ();
	gtk_box_pack_end (GTK_BOX (hb), om, FALSE, FALSE, 0);

	menu = GTK_WIDGET(sp_module_menu_save ());
	gtk_option_menu_set_menu (GTK_OPTION_MENU (om), menu);
	l = gtk_label_new (_("File type:"));
	gtk_box_pack_end (GTK_BOX (hb), l, FALSE, FALSE, 0);
	gtk_widget_show_all (hb);

	gtk_window_set_modal (GTK_WINDOW (dlg), TRUE);
	sp_transientize (dlg);
	b = gtk_dialog_run (GTK_DIALOG (dlg));

	if (b == GTK_RESPONSE_OK) {
		sp_file_save_ok (dlg, doc, (gchar const *)((SPMenu *) menu)->activedata);
		
		gtk_widget_destroy (dlg);
		return TRUE;
	} else {
		gtk_widget_destroy (dlg);
		return FALSE;
	}
#endif
}

#ifndef WIN32
static void
sp_file_save_ok (GtkWidget *save_dialog, SPDocument *doc, gchar const *key)
{
	GtkFileSelection *fs;
	const gchar *filename;
	const gchar      *raw_filename;

	fs = GTK_FILE_SELECTION (save_dialog);

	filename = gtk_file_selection_get_filename (fs);
	raw_filename = gtk_entry_get_text (GTK_ENTRY (fs->selection_entry));

	g_assert (filename && raw_filename);

	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		if (g_file_test (filename, G_FILE_TEST_IS_DIR)) {
			if (filename[strlen (filename) - 1] != G_DIR_SEPARATOR) {
				gchar *s = g_strconcat (filename, G_DIR_SEPARATOR_S, NULL);
				gtk_file_selection_set_filename (fs, s);
				g_free (s);
			} else {
				gtk_file_selection_set_filename (fs, filename);
			}
		} else {
			/* TODO: Handle overwriting files differently - TJG */
			gtk_widget_set_sensitive (GTK_WIDGET (fs), FALSE);
			file_save (doc, filename, key);
			gtk_widget_set_sensitive (GTK_WIDGET (fs), TRUE);
		}
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (fs), FALSE);
		file_save (doc, filename, key);
		gtk_widget_set_sensitive (GTK_WIDGET (fs), TRUE);
	}
}
#endif

gboolean
sp_file_save_document (SPDocument *doc)
{
	SPRepr * repr;
	const gchar * fn;
	gboolean success;

	repr = sp_document_repr_root (doc);

	fn = sp_repr_attr (repr, "sodipodi:modified");
	if (fn != NULL) {
		if (doc->uri == NULL) {
			success = sp_file_save_dialog (doc);
		} else {
			/* TODO: This currently requires a recognizable extension to
				 be on the file name - odd stuff won't work */
			fn = g_strdup (doc->uri);
			file_save(doc, fn, SP_MODULE_KEY_AUTODETECT);
			success = TRUE;
			g_free ((void *) fn);
		}

		if (success) {
			sp_view_set_statusf_flash (SP_VIEW(SP_ACTIVE_DESKTOP), "Document saved.");
		} else {
			sp_view_set_statusf_flash (SP_VIEW(SP_ACTIVE_DESKTOP), "Document not saved.");
		}

	} else {
		sp_view_set_statusf_flash (SP_VIEW(SP_ACTIVE_DESKTOP), "No changes need to be saved.");
		success = TRUE;
	}
	
	return success;
}

void
sp_file_save (gpointer object, gpointer data)
{
	if (!SP_ACTIVE_DOCUMENT) return;

	sp_namedview_document_from_window (SP_ACTIVE_DESKTOP);

	sp_file_save_document (SP_ACTIVE_DOCUMENT);
}

void
sp_file_save_as (gpointer object, gpointer data)
{
	if (!SP_ACTIVE_DOCUMENT) return;

	sp_namedview_document_from_window (SP_ACTIVE_DESKTOP);

	sp_file_save_dialog (SP_ACTIVE_DOCUMENT);

	//TODO: make this dependent on the success of the save operation
	sp_view_set_statusf_flash (SP_VIEW(SP_ACTIVE_DESKTOP), "Document saved.");
}




/*######################
## I M P O R T
######################*/

static void
file_import (SPDocument *doc, const gchar *filename)
{
	SPRepr *rdoc;
	const gchar *e, *docbase, *relname;
	SPRepr * repr;
	SPReprDoc * rnewdoc;

	if (filename && g_file_test (filename, G_FILE_TEST_IS_DIR)) {
		g_free (import_path);
		if (filename[strlen(filename) - 1] != G_DIR_SEPARATOR) {
			import_path = g_strconcat (filename, G_DIR_SEPARATOR_S, NULL);
		} else {
			import_path = g_strdup (filename);
		}
		return;
	}

	if (filename == NULL) return;

	import_path = g_dirname (filename);
	if (import_path) import_path = g_strconcat (import_path, G_DIR_SEPARATOR_S, NULL);

	rdoc = sp_document_repr_root (doc);

	docbase = sp_repr_attr (rdoc, "sodipodi:docbase");
	relname = sp_relative_path_from_path (filename, docbase);
	/* fixme: this should be implemented with mime types */
	e = sp_extension_from_path (filename);

	if ((e == NULL) || (strcmp (e, "svg") == 0) || (strcmp (e, "xml") == 0)) {
		SPRepr * newgroup;
		const gchar * style;
		SPRepr * child;

		rnewdoc = sp_repr_read_file (filename, SP_SVG_NS_URI);
		if (rnewdoc == NULL) return;
		repr = sp_repr_document_root (rnewdoc);
		style = sp_repr_attr (repr, "style");

		newgroup = sp_repr_new ("g");
		sp_repr_set_attr (newgroup, "style", style);

		for (child = repr->children; child != NULL; child = child->next) {
			SPRepr * newchild;
			newchild = sp_repr_duplicate (child);
			sp_repr_append_child (newgroup, newchild);
		}

		sp_repr_document_unref (rnewdoc);

		sp_document_add_repr (doc, newgroup);
		sp_repr_unref (newgroup);
		sp_document_done (doc);
		return;
	}

	if ((strcmp (e, "png") == 0) ||
	    (strcmp (e, "jpg") == 0) ||
	    (strcmp (e, "jpeg") == 0) ||
	    (strcmp (e, "bmp") == 0) ||
	    (strcmp (e, "gif") == 0) ||
	    (strcmp (e, "tiff") == 0) ||
	    (strcmp (e, "xpm") == 0)) {
		/* Try pixbuf */
		GdkPixbuf *pb;
        GError *err = NULL;
		pb = gdk_pixbuf_new_from_file (filename, &err);
		if (pb) {
			/* We are readable */
			repr = sp_repr_new ("image");
			sp_repr_set_attr (repr, "xlink:href", relname);
			sp_repr_set_attr (repr, "sodipodi:absref", filename);
			sp_repr_set_double (repr, "width", gdk_pixbuf_get_width (pb));
			sp_repr_set_double (repr, "height", gdk_pixbuf_get_height (pb));
			sp_document_add_repr (doc, repr);
			sp_repr_unref (repr);
			sp_document_done (doc);
			gdk_pixbuf_unref (pb);
		} else {
            //error stuff here
            if (err) {
                gchar *text;
		        text = g_strdup_printf(
                      _("Unable to import image '%s': %s"),
                      filename, err->message);
                sp_ui_error_dialog (text);
                g_free (text);
                g_error_free (err);
            }
        }
	}
}

void
sp_file_import (GtkWidget * widget)
{
        SPDocument *doc;
#ifdef WIN32
	char *filename;
#else
	GtkWidget *w;
	int b;
#endif

        doc = SP_ACTIVE_DOCUMENT;
	if (!SP_IS_DOCUMENT(doc)) return;

#ifdef WIN32
	filename = sp_win32_get_open_filename ((unsigned char *)import_path,
                                     (unsigned char *)"Image files\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tiff;*.xpm\0"
					     "SVG files\0*.svg\0"
					     "All files\0*\0", (unsigned char *)_("Select file to import"));
	if (filename) {
		file_import (doc, filename);
		g_free (filename);
	}
#else
	w = gtk_file_selection_new (_("Select file to import"));
	gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION (w));
	if (import_path) gtk_file_selection_set_filename (GTK_FILE_SELECTION (w), import_path);
	gtk_window_set_modal (GTK_WINDOW (w), TRUE);
	sp_transientize (w);
	b = gtk_dialog_run (GTK_DIALOG (w));

	if (b == GTK_RESPONSE_OK) {
		const gchar *filename;
		filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (w));
		file_import (doc, filename);
	}

	gtk_widget_destroy (w);
#endif
}



/*######################
## E X P O R T
######################*/

void
sp_file_export_dialog (void *widget)
{
	sp_export_dialog ();
}

#include <display/nr-arena-item.h>
#include <display/nr-arena.h>

struct SPEBP {
	int width, height, sheight;
	guchar r, g, b, a;
	NRArenaItem *root;
	guchar *px;
	unsigned int (*status) (float, void *);
	void *data;
};

static int
sp_export_get_rows (const guchar **rows, int row, int num_rows, void *data)
{
	struct SPEBP *ebp;
	NRPixBlock pb;
	NRRectL bbox;
	NRGC gc;
	int r, c;

	ebp = (struct SPEBP *) data;

	if (ebp->status) {
		if (!ebp->status ((float) row / ebp->height, ebp->data)) return 0;
	}

	num_rows = MIN (num_rows, ebp->sheight);
	num_rows = MIN (num_rows, ebp->height - row);

	/* Set area of interest */
	bbox.x0 = 0;
	bbox.y0 = row;
	bbox.x1 = ebp->width;
	bbox.y1 = row + num_rows;
	/* Update to renderable state */
	nr_matrix_set_identity (&gc.transform);
	nr_arena_item_invoke_update (ebp->root, &bbox, &gc, NR_ARENA_ITEM_STATE_ALL, NR_ARENA_ITEM_STATE_NONE);

	nr_pixblock_setup_extern (&pb, NR_PIXBLOCK_MODE_R8G8B8A8N, bbox.x0, bbox.y0, bbox.x1, bbox.y1, ebp->px, 4 * ebp->width, FALSE, FALSE);

	for (r = 0; r < num_rows; r++) {
		guchar *p;
		p = NR_PIXBLOCK_PX (&pb) + r * pb.rs;
		for (c = 0; c < ebp->width; c++) {
			*p++ = ebp->r;
			*p++ = ebp->g;
			*p++ = ebp->b;
			*p++ = ebp->a;
		}
	}

	/* Render */
	nr_arena_item_invoke_render (ebp->root, &bbox, &pb, 0);

	for (r = 0; r < num_rows; r++) {
		rows[r] = NR_PIXBLOCK_PX (&pb) + r * pb.rs;
	}

	nr_pixblock_release (&pb);

	return num_rows;
}



void
sp_export_png_file (SPDocument *doc, const gchar *filename,
		    double x0, double y0, double x1, double y1,
		    unsigned int width, unsigned int height,
		    unsigned long bgcolor,
			 unsigned int (*status) (float, void*), void *data)
{
	NRMatrix affine;
	gdouble t;
	NRArena *arena;
	struct SPEBP ebp;
	unsigned int dkey;

	g_return_if_fail (doc != NULL);
	g_return_if_fail (SP_IS_DOCUMENT (doc));
	g_return_if_fail (filename != NULL);
	g_return_if_fail (width >= 1);
	g_return_if_fail (height >= 1);

	sp_document_ensure_up_to_date (doc);

	/* Go to document coordinates */
	t = y0;
	y0 = sp_document_height (doc) - y1;
	y1 = sp_document_height (doc) - t;

	/*
	 * 1) a[0] * x0 + a[2] * y1 + a[4] = 0.0
	 * 2) a[1] * x0 + a[3] * y1 + a[5] = 0.0
	 * 3) a[0] * x1 + a[2] * y1 + a[4] = width
	 * 4) a[1] * x0 + a[3] * y0 + a[5] = height
	 * 5) a[1] = 0.0;
	 * 6) a[2] = 0.0;
	 *
	 * (1,3) a[0] * x1 - a[0] * x0 = width
	 * a[0] = width / (x1 - x0)
	 * (2,4) a[3] * y0 - a[3] * y1 = height
	 * a[3] = height / (y0 - y1)
	 * (1) a[4] = -a[0] * x0
	 * (2) a[5] = -a[3] * y1
	 */

	affine.c[0] = width / ((x1 - x0) * 1.25);
	affine.c[1] = 0.0;
	affine.c[2] = 0.0;
	affine.c[3] = height / ((y1 - y0) * 1.25);
	affine.c[4] = -affine.c[0] * x0 * 1.25;
	affine.c[5] = -affine.c[3] * y0 * 1.25;

	//SP_PRINT_MATRIX ("SVG2PNG", &affine);

	ebp.width = width;
	ebp.height = height;
	ebp.r = NR_RGBA32_R (bgcolor);
	ebp.g = NR_RGBA32_G (bgcolor);
	ebp.b = NR_RGBA32_B (bgcolor);
	ebp.a = NR_RGBA32_A (bgcolor);

	/* Create new arena */
	arena = (NRArena *) nr_object_new (NR_TYPE_ARENA);
	dkey = sp_item_display_key_new (1);
	/* Create ArenaItem and set transform */
	ebp.root = sp_item_invoke_show (SP_ITEM (sp_document_root (doc)), arena, dkey, SP_ITEM_SHOW_PRINT);
	nr_arena_item_set_transform (ebp.root, &affine);

	ebp.status = status;
	ebp.data = data;

	if ((width < 256) || ((width * height) < 32768)) {
		ebp.px = nr_pixelstore_64K_new (FALSE, 0);
		ebp.sheight = 65536 / (4 * width);
		sp_png_write_rgba_striped (filename, width, height, sp_export_get_rows, &ebp);
		nr_pixelstore_64K_free (ebp.px);
	} else {
		ebp.px = nr_new (guchar, 4 * 64 * width);
		ebp.sheight = 64;
		sp_png_write_rgba_striped (filename, width, height, sp_export_get_rows, &ebp);
		nr_free (ebp.px);
	}

	/* Free Arena and ArenaItem */
	sp_item_invoke_hide (SP_ITEM (sp_document_root (doc)), dkey);
	nr_arena_item_unref (ebp.root);
	nr_object_unref ((NRObject *) arena);
}


/*######################
## P R I N T
######################*/


void
sp_file_print (void)
{
	SPDocument *doc;
	doc = SP_ACTIVE_DOCUMENT;
	if (doc) sp_print_document (doc, FALSE);
}

void
sp_file_print_direct (void)
{
	SPDocument *doc;
	doc = SP_ACTIVE_DOCUMENT;
	if (doc) sp_print_document (doc, TRUE);
}

void
sp_file_print_preview (gpointer object, gpointer data)
{
	SPDocument *doc;

	doc = SP_ACTIVE_DOCUMENT;

	if (doc) {
		sp_print_preview_document (doc);
	}
}


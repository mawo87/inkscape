#define __SP_DEFS_C__

/*
 * SVG <defs> implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 2000-2002 authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

/*
 * fixme: We should really check childrens validity - currently everything
 * flips in
 */

#include "xml/repr-private.h"
#include "sp-object-repr.h"
#include "sp-defs.h"

static void sp_defs_class_init(SPDefsClass *dc);
static void sp_defs_init(SPDefs *defs);

static void sp_defs_release(SPObject *object);
static void sp_defs_update(SPObject *object, SPCtx *ctx, guint flags);
static void sp_defs_modified(SPObject *object, guint flags);
static SPRepr *sp_defs_write(SPObject *object, SPRepr *repr, guint flags);

static SPObjectClass *parent_class;

GType sp_defs_get_type(void)
{
    static GType defs_type = 0;
    
    if (!defs_type) {
        GTypeInfo defs_info = {
            sizeof(SPDefsClass),
            NULL,	/* base_init */
            NULL,	/* base_finalize */
            (GClassInitFunc) sp_defs_class_init,
            NULL,	/* class_finalize */
            NULL,	/* class_data */
            sizeof(SPDefs),
            16,	/* n_preallocs */
            (GInstanceInitFunc) sp_defs_init,
            NULL,	/* value_table */
        };
        defs_type = g_type_register_static(SP_TYPE_OBJECT, "SPDefs", &defs_info, (GTypeFlags) 0);
    }
    
    return defs_type;
}

static void sp_defs_class_init(SPDefsClass *dc)
{
    SPObjectClass *sp_object_class = (SPObjectClass *) dc;
    
    sp_object_class->release = sp_defs_release;
    sp_object_class->update = sp_defs_update;
    sp_object_class->modified = sp_defs_modified;
    sp_object_class->write = sp_defs_write;
}

static void sp_defs_init(SPDefs *defs)
{
    
}

static void sp_defs_release(SPObject *object)
{
    if (((SPObjectClass *) (parent_class))->release) {
        ((SPObjectClass *) (parent_class))->release(object);
    }
}

static void sp_defs_update(SPObject *object, SPCtx *ctx, guint flags)
{
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }

    flags &= SP_OBJECT_MODIFIED_CASCADE;

    GSList *l = NULL;
    for ( SPObject *child = sp_object_first_child(object) ; child != NULL; child = SP_OBJECT_NEXT(child) ) {
        g_object_ref(G_OBJECT(child));
        l = g_slist_prepend(l, child);
    }
    
    l = g_slist_reverse(l);
    
    while (l) {
        SPObject *child = SP_OBJECT(l->data);
        l = g_slist_remove(l, child);
        if (flags || (child->uflags & SP_OBJECT_MODIFIED_FLAG)) {
            child->updateDisplay(ctx, flags);
        }
        g_object_unref(G_OBJECT(child));
    }
}

static void sp_defs_modified(SPObject *object, guint flags)
{
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }
    
    flags &= SP_OBJECT_MODIFIED_CASCADE;
    
    GSList *l = NULL;
    for ( SPObject *child = sp_object_first_child(object) ; child != NULL; child = SP_OBJECT_NEXT(child) ) {
        g_object_ref(G_OBJECT(child));
        l = g_slist_prepend(l, child);
    }
    
    l = g_slist_reverse(l);
    
    while (l) {
        SPObject *child = SP_OBJECT(l->data);
        l = g_slist_remove(l, child);
        if (flags || (child->mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            child->emitModified(flags);
        }
        g_object_unref(G_OBJECT (child));
    }
}

static SPRepr *sp_defs_write(SPObject *object, SPRepr *repr, guint flags)
{
    if (flags & SP_OBJECT_WRITE_BUILD) {
        
        if (!repr) {
            repr = sp_repr_new("defs");
        }
        
        GSList *l = NULL;
        for ( SPObject *child = sp_object_first_child(object) ; child != NULL; child = SP_OBJECT_NEXT(child) ) {
            SPRepr *crepr = child->updateRepr(NULL, flags);
            l = g_slist_prepend(l, crepr);
        }
        
        while (l) {
            sp_repr_add_child(repr, (SPRepr *) l->data, NULL);
            sp_repr_unref((SPRepr *) l->data);
            l = g_slist_remove(l, l->data);
        }
        
    } else {
        for ( SPObject *child = sp_object_first_child(object) ; child != NULL; child = SP_OBJECT_NEXT(child) ) {
            child->updateRepr(flags);
        }
    }

    if (((SPObjectClass *) (parent_class))->write) {
        (* ((SPObjectClass *) (parent_class))->write)(object, repr, flags);
    }

    return repr;
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

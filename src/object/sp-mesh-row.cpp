// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @gradient meshrow class.
 */
/* Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak
 *   Johan Engelen <j.b.c.engelen@ewi.utwente.nl>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Tavmjong Bah <tavjong@free.fr>
 *
 * Copyright (C) 1999,2005 authors
 * Copyright (C) 2010 Jon A. Cruz
 * Copyright (C) 2012 Tavmjong Bah
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include "sp-mesh-row.h"
#include "style.h"

SPMeshrow* SPMeshrow::getNextMeshrow()
{
    SPMeshrow *result = nullptr;

    for (SPObject* obj = getNext(); obj && !result; obj = obj->getNext()) {
        if (is<SPMeshrow>(obj)) {
            result = cast<SPMeshrow>(obj);
        }
    }

    return result;
}

SPMeshrow* SPMeshrow::getPrevMeshrow()
{
    SPMeshrow *result = nullptr;

    for (SPObject* obj = getPrev(); obj; obj = obj->getPrev()) {
        // The closest previous SPObject that is an SPMeshrow *should* be ourself.
        if (is<SPMeshrow>(obj)) {
            auto meshrow = cast<SPMeshrow>(obj);
            // Sanity check to ensure we have a proper sibling structure.
            if (meshrow->getNextMeshrow() == this) {
                result = meshrow;
            } else {
                g_warning("SPMeshrow previous/next relationship broken");
            }
            break;
        }
    }

    return result;
}


/*
 * Mesh Row
 */
SPMeshrow::SPMeshrow() : SPObject() {
}

SPMeshrow::~SPMeshrow() = default;

void SPMeshrow::build(SPDocument* doc, Inkscape::XML::Node* repr) {
	SPObject::build(doc, repr);
}


/**
 * Virtual build: set meshrow attributes from its associated XML node.
 */
void SPMeshrow::set(SPAttr /*key*/, const gchar* /*value*/) {
}

/**
 * modified
 */
void SPMeshrow::modified(unsigned int flags) {

    flags &= SP_OBJECT_MODIFIED_CASCADE;
    std::vector<SPObject *> l;
    for (auto& child: children) {
        sp_object_ref(&child);
        l.push_back(&child);
    }

    for (auto child:l) {
        if (flags || (child->mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            child->emitModified(flags);
        }
        sp_object_unref(child);
    }
}


/**
 * Virtual set: set attribute to value.
 */
Inkscape::XML::Node* SPMeshrow::write(Inkscape::XML::Document* xml_doc, Inkscape::XML::Node* repr, guint flags) {
    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:meshrow");
    }

    SPObject::write(xml_doc, repr, flags);

    return repr;
}

/**
 * Virtual write: write object attributes to repr.
 */

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

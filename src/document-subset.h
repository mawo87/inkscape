// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::DocumentSubset - view of a document including only a subset
 *                            of nodes
 *
 * Copyright 2006  MenTaLguY  <mental@rydia.net>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_DOCUMENT_SUBSET_H
#define SEEN_INKSCAPE_DOCUMENT_SUBSET_H

#include <cstddef>
#include <memory>
#include <sigc++/connection.h>
#include <sigc++/functors/slot.h>

class SPObject;
class SPDocument;

namespace Inkscape {

class DocumentSubset
{
public:
    bool includes(SPObject *obj) const;

    SPObject *parentOf(SPObject *obj) const;
    unsigned childCount(SPObject *obj) const;
    unsigned indexOf(SPObject *obj) const;
    SPObject *nthChildOf(SPObject *obj, unsigned n) const;

    sigc::connection connectChanged(sigc::slot<void ()> slot) const;
    sigc::connection connectAdded(sigc::slot<void (SPObject *)> slot) const;
    sigc::connection connectRemoved(sigc::slot<void (SPObject *)> slot) const;

protected:
    DocumentSubset();
    ~DocumentSubset();

    void _addOne(SPObject *obj);
    void _removeOne(SPObject *obj) { _remove(obj, false); }
    void _removeSubtree(SPObject *obj) { _remove(obj, true); }
    void _clear();

private:
    DocumentSubset(DocumentSubset const &) = delete; // no copy
    void operator=(DocumentSubset const &) = delete; // no assign

    void _remove(SPObject *obj, bool subtree);

    struct Relations;

    std::unique_ptr<Relations> _relations;
};

}

#endif
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

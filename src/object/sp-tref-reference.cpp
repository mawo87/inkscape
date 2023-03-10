// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * The reference corresponding to href of <tref> element.
 *
 * Copyright (C) 2007 Gail Banaszkiewicz
 *
 * This file was created based on sp-use-reference.cpp
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

//#include "enums.h"
#include "sp-tref-reference.h"

#include "sp-text.h"
#include "sp-tref.h"


bool SPTRefReference::_acceptObject(SPObject * const obj) const
{
    SPObject *owner = getOwner();
    if (is<SPTRef>(owner))
        return URIReference::_acceptObject(obj);
    else
        return false;
}


void SPTRefReference::updateObserver()
{
    SPObject *referred = getObject();

    if (referred) {
        if (subtreeObserved) {
            subtreeObserved->removeObserver(*this);
        }

        subtreeObserved = std::make_unique<Inkscape::XML::Subtree>(*referred->getRepr());
        subtreeObserved->addObserver(*this);
    }
}


void SPTRefReference::notifyChildAdded(Inkscape::XML::Node &/*node*/, Inkscape::XML::Node &/*child*/,
                                       Inkscape::XML::Node */*prev*/)
{
    SPObject *owner = getOwner();

    if (owner && is<SPTRef>(owner)) {
        sp_tref_update_text(cast<SPTRef>(owner));
    }
}


void SPTRefReference::notifyChildRemoved(Inkscape::XML::Node &/*node*/, Inkscape::XML::Node &/*child*/,
                                         Inkscape::XML::Node */*prev*/)
{
    SPObject *owner = getOwner();

    if (owner && is<SPTRef>(owner)) {
        sp_tref_update_text(cast<SPTRef>(owner));
    }
}


void SPTRefReference::notifyChildOrderChanged(Inkscape::XML::Node &/*node*/, Inkscape::XML::Node &/*child*/,
                                              Inkscape::XML::Node */*old_prev*/, Inkscape::XML::Node */*new_prev*/)
{
    SPObject *owner = getOwner();

    if (owner && is<SPTRef>(owner)) {
        sp_tref_update_text(cast<SPTRef>(owner));
    }
}


void SPTRefReference::notifyContentChanged(Inkscape::XML::Node &/*node*/,
                                           Inkscape::Util::ptr_shared /*old_content*/,
                                           Inkscape::Util::ptr_shared /*new_content*/)
{
    SPObject *owner = getOwner();

    if (owner && is<SPTRef>(owner)) {
        sp_tref_update_text(cast<SPTRef>(owner));
    }
}


void SPTRefReference::notifyAttributeChanged(Inkscape::XML::Node &/*node*/, GQuark /*name*/,
                                             Inkscape::Util::ptr_shared /*old_value*/,
                                             Inkscape::Util::ptr_shared /*new_value*/)
{
    // Do nothing - tref only cares about textual content
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

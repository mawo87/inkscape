// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * The reference corresponding to href of LPE Path parameter.
 *
 * Copyright (C) 2008 Johan Engelen
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "live_effects/parameter/path-reference.h"

#include "object/sp-shape.h"
#include "object/sp-text.h"

namespace Inkscape {
namespace LivePathEffect {

SPItem *PathReference::getObject() const
{
    return static_cast<SPItem*>(URIReference::getObject());
}

bool PathReference::_acceptObject(SPObject *obj) const
{
    if (is<SPShape>(obj) || is<SPText>(obj)) {
        /* Refuse references to lpeobject */
        if (obj == getOwner()) {
            return false;
        }
        // TODO: check whether the referred path has this LPE applied, if so: deny deny deny!
        return URIReference::_acceptObject(obj);
    } else {
        return false;
    }
}

} // namespace LivePathEffect
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :

// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_PERSP3D_REFERENCE_H
#define SEEN_PERSP3D_REFERENCE_H

/*
 * The reference corresponding to the inkscape:perspectiveID attribute
 *
 * Copyright (C) 2007 Johan Engelen
 * Copyright (C) 2007 Maximilian Albert
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cstddef>
#include <sigc++/sigc++.h>

#include "uri-references.h"
#include "persp3d.h"

class SPObject;

namespace Inkscape {
namespace XML {
class Node;
}
}

class Persp3DReference : public Inkscape::URIReference {
public:
    Persp3DReference(SPObject *obj);
    ~Persp3DReference() override;

    Persp3D *getObject() const {
        return cast<Persp3D>(URIReference::getObject());
    }

    SPObject *owner;

    // concerning the Persp3D (we only use SPBox3D) that is referred to:
    char *persp_href;
    Inkscape::XML::Node *persp_repr;
    Persp3D *persp;

    sigc::connection _changed_connection;
    sigc::connection _modified_connection;
    sigc::connection _delete_connection;

    void link(char* to);
    void unlink();
    void start_listening(Persp3D* to);
    void quit_listening();

protected:
    bool _acceptObject(SPObject *obj) const override;
};

#endif /* !SEEN_PERSP3D_REFERENCE_H */

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

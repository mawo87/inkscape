// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_NR_FILTER_OFFSET_H
#define SEEN_NR_FILTER_OFFSET_H

/*
 * feOffset filter primitive renderer
 *
 * Authors:
 *   Niko Kiirala <niko@kiirala.com>
 *
 * Copyright (C) 2007 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "display/nr-filter-primitive.h"
#include "display/nr-filter-slot.h"
#include "display/nr-filter-units.h"

namespace Inkscape {
namespace Filters {

class FilterOffset : public FilterPrimitive
{
public:
    FilterOffset();
    ~FilterOffset() override;

    void render_cairo(FilterSlot &slot) const override;
    void area_enlarge(Geom::IntRect &area, Geom::Affine const &trans) const override;
    bool can_handle_affine(Geom::Affine const &) const override;
    double complexity(Geom::Affine const &ctm) const override;

    void set_dx(double amount);
    void set_dy(double amount);

    Glib::ustring name() const override { return Glib::ustring("Offset"); }

private:
    double dx, dy;
};

} // namespace Filters
} // namespace Inkscape

#endif // SEEN_NR_FILTER_OFFSET_H
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

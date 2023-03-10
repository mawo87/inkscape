// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * feMerge filter effect renderer
 *
 * Authors:
 *   Niko Kiirala <niko@kiirala.com>
 *
 * Copyright (C) 2007 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <vector>
#include "display/cairo-utils.h"
#include "display/nr-filter-merge.h"
#include "display/nr-filter-slot.h"
#include "display/nr-filter-utils.h"

namespace Inkscape {
namespace Filters {

FilterMerge::FilterMerge()
    : _input_image({NR_FILTER_SLOT_NOT_SET}) {}

void FilterMerge::render_cairo(FilterSlot &slot) const
{
    if (_input_image.empty()) return;

    Geom::Rect vp = filter_primitive_area(slot.get_units());
    slot.set_primitive_area(_output, vp); // Needed for tiling

    // output is RGBA if at least one input is RGBA
    bool rgba32 = false;
    cairo_surface_t *out = nullptr;
    for (auto &i : _input_image) {
        cairo_surface_t *in = slot.getcairo(i);
        if (cairo_surface_get_content(in) == CAIRO_CONTENT_COLOR_ALPHA) {
            out = ink_cairo_surface_create_identical(in);
            set_cairo_surface_ci(out, color_interpolation);
            rgba32 = true;
            break;
        }
    }

    if (!rgba32) {
        out = ink_cairo_surface_create_identical(slot.getcairo(_input_image[0]));
    }
    cairo_t *out_ct = cairo_create(out);

    for (auto &i : _input_image) {
        cairo_surface_t *in = slot.getcairo(i);

        set_cairo_surface_ci(in, color_interpolation);
        cairo_set_source_surface(out_ct, in, 0, 0);
        cairo_paint(out_ct);
    }

    cairo_destroy(out_ct);
    slot.set(_output, out);
    cairo_surface_destroy(out);
}

bool FilterMerge::can_handle_affine(Geom::Affine const &) const
{
    // Merge is a per-pixel primitive and is immutable under transformations
    return true;
}

double FilterMerge::complexity(Geom::Affine const &) const
{
    return 1.02;
}

bool FilterMerge::uses_background() const
{
    for (int input : _input_image) {
        if (input == NR_FILTER_BACKGROUNDIMAGE || input == NR_FILTER_BACKGROUNDALPHA) {
            return true;
        }
    }
    return false;
}

void FilterMerge::set_input(int slot)
{
    _input_image[0] = slot;
}

void FilterMerge::set_input(int input, int slot)
{
    if (input < 0) return;

    if (_input_image.size() > input) {
        _input_image[input] = slot;
    } else {
        for (int i = _input_image.size(); i < input ; i++) {
            _input_image.emplace_back(NR_FILTER_SLOT_NOT_SET);
        }
        _input_image.push_back(slot);
    }
}

} // namespace Filters
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :

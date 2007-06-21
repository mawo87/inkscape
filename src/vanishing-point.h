/*
 * Vanishing point for 3D perspectives
 *
 * Authors:
 *   Maximilian Albert <Anhalter42@gmx.de>
 *
 * Copyright (C) 2007 authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifndef SEEN_VANISHING_POINT_H
#define SEEN_VANISHING_POINT_H

#include "libnr/nr-point.h"
#include "line-geometry.h"

namespace Box3D {

enum VPState {
    VP_FINITE = 0, // perspective lines meet in the VP
    VP_INFINITE    // perspective lines are parallel
};

enum PerspDir {
    X,
    Y,
    Z,
    NONE
};

// FIXME: Store the PerspDir of the VP inside the class
class VanishingPoint : public NR::Point {
public:
    inline VanishingPoint() : NR::Point() {};
    /***
    inline VanishingPoint(NR::Point const &pt, NR::Point const &ref = NR::Point(0,0))
                         : NR::Point (pt),
                           ref_pt (ref),
                           v_dir (pt[NR::X] - ref[NR::X], pt[NR::Y] - ref[NR::Y]) {}
    inline VanishingPoint(NR::Coord x, NR::Coord y, NR::Point const &ref = NR::Point(0,0))
                         : NR::Point (x, y),
                           ref_pt (ref),
                           v_dir (x - ref[NR::X], y - ref[NR::Y]) {}
    ***/
    VanishingPoint(NR::Point const &pt, NR::Point const &inf_dir, VPState st);
    VanishingPoint(NR::Point const &pt);
    VanishingPoint(NR::Point const &dir, VPState const state);
    VanishingPoint(NR::Point const &pt, NR::Point const &direction);
    VanishingPoint(NR::Coord x, NR::Coord y);
    VanishingPoint(NR::Coord x, NR::Coord y, VPState const state);
    VanishingPoint(NR::Coord x, NR::Coord y, NR::Coord dir_x, NR::Coord dir_y);
    VanishingPoint(VanishingPoint const &rhs);

    bool is_finite();
    VPState toggle_parallel();
    void draw(PerspDir const axis); // Draws a point on the canvas if state == VP_FINITE
    //inline VPState state() { return state; }
	
    VPState state;
    //NR::Point ref_pt; // point of reference to compute the direction of parallel lines
    NR::Point v_dir; // direction of perslective lines if the VP has state == VP_INFINITE

private:
};


} // namespace Box3D


/** A function to print out the VanishingPoint (prints the coordinates) **/
/***
inline std::ostream &operator<< (std::ostream &out_file, const VanishingPoint &vp) {
    out_file << vp;
    return out_file;
}
***/


#endif /* !SEEN_VANISHING_POINT_H */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :

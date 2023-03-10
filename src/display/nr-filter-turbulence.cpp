// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * feTurbulence filter primitive renderer
 *
 * Authors:
 *   World Wide Web Consortium <http://www.w3.org/>
 *   Felipe Corrêa da Silva Sanches <juca@members.fsf.org>
 *
 * This file has a considerable amount of code adapted from
 *  the W3C SVG filter specs, available at:
 *  http://www.w3.org/TR/SVG11/filters.html#feTurbulence
 *
 * W3C original code is licensed under the terms of
 *  the (GPL compatible) W3C® SOFTWARE NOTICE AND LICENSE:
 *  http://www.w3.org/Consortium/Legal/2002/copyright-software-20021231
 *
 * Copyright (C) 2007 authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "display/cairo-templates.h"
#include "display/cairo-utils.h"
#include "display/nr-filter.h"
#include "display/nr-filter-turbulence.h"
#include "display/nr-filter-units.h"
#include "display/nr-filter-utils.h"
#include <cmath>

namespace Inkscape {
namespace Filters{

class TurbulenceGenerator
{
public:
    TurbulenceGenerator()
        : _tile()
        , _baseFreq()
        , _latticeSelector()
        , _gradient()
        , _seed(0)
        , _octaves(0)
        , _stitchTiles(false)
        , _wrapx(0)
        , _wrapy(0)
        , _wrapw(0)
        , _wraph(0)
        , _inited(false)
        , _fractalnoise(false) {}

    void init(long seed, Geom::Rect const &tile, Geom::Point const &freq, bool stitch, bool fractalnoise, int octaves)
    {
        // setup random number generator
        _setupSeed(seed);

        // set values
        _tile = tile;
        _baseFreq = freq;
        _stitchTiles = stitch;
        _fractalnoise = fractalnoise;
        _octaves = octaves;

        int i;
        for (int k = 0; k < 4; ++k) {
            for (i = 0; i < BSize; ++i) {
                _latticeSelector[i] = i;

                do {
                    _gradient[i][k][0] = static_cast<double>(_random() % (BSize * 2) - BSize) / BSize;
                    _gradient[i][k][1] = static_cast<double>(_random() % (BSize * 2) - BSize) / BSize;
                } while (_gradient[i][k][0] == 0 && _gradient[i][k][1] == 0);

                // normalize gradient
                double s = hypot(_gradient[i][k][0], _gradient[i][k][1]);
                _gradient[i][k][0] /= s;
                _gradient[i][k][1] /= s;
            }
        }
        while (--i) {
            // shuffle lattice selectors
            int j = _random() % BSize;
            std::swap(_latticeSelector[i], _latticeSelector[j]);
        }

        // fill out the remaining part of the gradient
        for (i = 0; i < BSize + 2; ++i)
        {
            _latticeSelector[BSize + i] = _latticeSelector[i];

            for (int k = 0; k < 4; ++k) {
                _gradient[BSize + i][k][0] = _gradient[i][k][0];
                _gradient[BSize + i][k][1] = _gradient[i][k][1];
            }
        }

        // When stitching tiled turbulence, the frequencies must be adjusted
        // so that the tile borders will be continuous.
        if (_stitchTiles) {
            if (_baseFreq[Geom::X] != 0.0) {
                double freq = _baseFreq[Geom::X];
                double lo = std::floor(_tile.width() * freq) / _tile.width();
                double hi = std::ceil(_tile.width() * freq) / _tile.width();
                _baseFreq[Geom::X] = freq / lo < hi / freq ? lo : hi;
            }
            if (_baseFreq[Geom::Y] != 0.0) {
                double freq = _baseFreq[Geom::Y];
                double lo = std::floor(_tile.height() * freq) / _tile.height();
                double hi = std::ceil(_tile.height() * freq) / _tile.height();
                _baseFreq[Geom::Y] = freq / lo < hi / freq ? lo : hi;
            }

            _wrapw = _tile.width() * _baseFreq[Geom::X] + 0.5;
            _wraph = _tile.height() * _baseFreq[Geom::Y] + 0.5;
            _wrapx = _tile.left() * _baseFreq[Geom::X] + PerlinOffset + _wrapw;
            _wrapy = _tile.top() * _baseFreq[Geom::Y] + PerlinOffset + _wraph;
        }
        _inited = true;
    }

    G_GNUC_PURE
    guint32 turbulencePixel(Geom::Point const &p) const
    {
        int wrapx = _wrapx, wrapy = _wrapy, wrapw = _wrapw, wraph = _wraph;

        double pixel[4];
        double x = p[Geom::X] * _baseFreq[Geom::X];
        double y = p[Geom::Y] * _baseFreq[Geom::Y];
        double ratio = 1.0;

        for (double & k : pixel)
            k = 0.0;

        for (int octave = 0; octave < _octaves; ++octave)
        {
            double tx = x + PerlinOffset;
            double bx = floor(tx);
            double rx0 = tx - bx, rx1 = rx0 - 1.0;
            int bx0 = bx, bx1 = bx0 + 1;

            double ty = y + PerlinOffset;
            double by = floor(ty);
            double ry0 = ty - by, ry1 = ry0 - 1.0;
            int by0 = by, by1 = by0 + 1;

            if (_stitchTiles) {
                if (bx0 >= wrapx) bx0 -= wrapw;
                if (bx1 >= wrapx) bx1 -= wrapw;
                if (by0 >= wrapy) by0 -= wraph;
                if (by1 >= wrapy) by1 -= wraph;
            }
            bx0 &= BMask;
            bx1 &= BMask;
            by0 &= BMask;
            by1 &= BMask;

            int i = _latticeSelector[bx0];
            int j = _latticeSelector[bx1];
            int b00 = _latticeSelector[i + by0];
            int b01 = _latticeSelector[i + by1];
            int b10 = _latticeSelector[j + by0];
            int b11 = _latticeSelector[j + by1];

            double sx = _scurve(rx0);
            double sy = _scurve(ry0);

            double result[4];
            // channel numbering: R=0, G=1, B=2, A=3
            for (int k = 0; k < 4; ++k) {
                double const *qxa = _gradient[b00][k];
                double const *qxb = _gradient[b10][k];
                double a = _lerp(sx, rx0 * qxa[0] + ry0 * qxa[1],
                                     rx1 * qxb[0] + ry0 * qxb[1]);
                double const *qya = _gradient[b01][k];
                double const *qyb = _gradient[b11][k];
                double b = _lerp(sx, rx0 * qya[0] + ry1 * qya[1],
                                     rx1 * qyb[0] + ry1 * qyb[1]);
                result[k] = _lerp(sy, a, b);
            }

            if (_fractalnoise) {
                for (int k = 0; k < 4; ++k)
                    pixel[k] += result[k] / ratio;
            } else {
                for (int k = 0; k < 4; ++k)
                    pixel[k] += fabs(result[k]) / ratio;
            }

            x *= 2;
            y *= 2;
            ratio *= 2;

            if(_stitchTiles)
            {
                // Update stitch values. Subtracting PerlinOffset before the multiplication and
                // adding it afterward simplifies to subtracting it once.
                wrapw *= 2;
                wraph *= 2;
                wrapx = wrapx*2 - PerlinOffset;
                wrapy = wrapy*2 - PerlinOffset;
            }
        }

        if (_fractalnoise) {
            guint32 r = CLAMP_D_TO_U8((pixel[0]*255.0 + 255.0) / 2);
            guint32 g = CLAMP_D_TO_U8((pixel[1]*255.0 + 255.0) / 2);
            guint32 b = CLAMP_D_TO_U8((pixel[2]*255.0 + 255.0) / 2);
            guint32 a = CLAMP_D_TO_U8((pixel[3]*255.0 + 255.0) / 2);
            r = premul_alpha(r, a);
            g = premul_alpha(g, a);
            b = premul_alpha(b, a);
            ASSEMBLE_ARGB32(pxout, a,r,g,b);
            return pxout;
        } else {
            guint32 r = CLAMP_D_TO_U8(pixel[0]*255.0);
            guint32 g = CLAMP_D_TO_U8(pixel[1]*255.0);
            guint32 b = CLAMP_D_TO_U8(pixel[2]*255.0);
            guint32 a = CLAMP_D_TO_U8(pixel[3]*255.0);
            r = premul_alpha(r, a);
            g = premul_alpha(g, a);
            b = premul_alpha(b, a);
            ASSEMBLE_ARGB32(pxout, a,r,g,b);
            return pxout;
        }
    }

    //G_GNUC_PURE
    /*guint32 turbulencePixel(Geom::Point const &p) const {
        if (!_fractalnoise) {
            guint32 r = CLAMP_D_TO_U8(turbulence(0, p)*255.0);
            guint32 g = CLAMP_D_TO_U8(turbulence(1, p)*255.0);
            guint32 b = CLAMP_D_TO_U8(turbulence(2, p)*255.0);
            guint32 a = CLAMP_D_TO_U8(turbulence(3, p)*255.0);
            r = premul_alpha(r, a);
            g = premul_alpha(g, a);
            b = premul_alpha(b, a);
            ASSEMBLE_ARGB32(pxout, a,r,g,b);
            return pxout;
        } else {
            guint32 r = CLAMP_D_TO_U8((turbulence(0, p)*255.0 + 255.0) / 2);
            guint32 g = CLAMP_D_TO_U8((turbulence(1, p)*255.0 + 255.0) / 2);
            guint32 b = CLAMP_D_TO_U8((turbulence(2, p)*255.0 + 255.0) / 2);
            guint32 a = CLAMP_D_TO_U8((turbulence(3, p)*255.0 + 255.0) / 2);
            r = premul_alpha(r, a);
            g = premul_alpha(g, a);
            b = premul_alpha(b, a);
            ASSEMBLE_ARGB32(pxout, a,r,g,b);
            return pxout;
        }
    }*/

    bool ready() const { return _inited; }
    void dirty() { _inited = false; }

private:
    void _setupSeed(long seed)
    {
        _seed = seed;
        if (_seed <= 0) _seed = -(_seed % (RAND_m - 1)) + 1;
        if (_seed > RAND_m - 1) _seed = RAND_m - 1;
    }

    long _random()
    {
        /* Produces results in the range [1, 2**31 - 2].
         * Algorithm is: r = (a * r) mod m
         * where a = 16807 and m = 2**31 - 1 = 2147483647
         * See [Park & Miller], CACM vol. 31 no. 10 p. 1195, Oct. 1988
         * To test: the algorithm should produce the result 1043618065
         * as the 10,000th generated number if the original seed is 1. */
        _seed = RAND_a * (_seed % RAND_q) - RAND_r * (_seed / RAND_q);
        if (_seed <= 0) _seed += RAND_m;
        return _seed;
    }

    static inline double _scurve(double t)
    {
        return t * t * (3.0 - 2.0 * t);
    }

    static inline double _lerp(double t, double a, double b)
    {
        return a + t * (b - a);
    }

    // random number generator constants
    static long constexpr
        RAND_m = 2147483647, // 2**31 - 1
        RAND_a = 16807, // 7**5; primitive root of m
        RAND_q = 127773, // m / a
        RAND_r = 2836; // m % a

    // other constants
    static int constexpr BSize = 0x100;
    static int constexpr BMask = 0xff;

    static double constexpr PerlinOffset = 4096.0;

    Geom::Rect _tile;
    Geom::Point _baseFreq;
    int _latticeSelector[2 * BSize + 2];
    double _gradient[2 * BSize + 2][4][2];
    long _seed;
    int _octaves;
    bool _stitchTiles;
    int _wrapx;
    int _wrapy;
    int _wrapw;
    int _wraph;
    bool _inited;
    bool _fractalnoise;
};

FilterTurbulence::FilterTurbulence()
    : gen(std::make_unique<TurbulenceGenerator>())
    , XbaseFrequency(0)
    , YbaseFrequency(0)
    , numOctaves(1)
    , seed(0)
    , updated(false)
    , fTileWidth(10) //guessed
    , fTileHeight(10) //guessed
    , fTileX(1) //guessed
    , fTileY(1) //guessed
{
}

FilterTurbulence::~FilterTurbulence() = default;

void FilterTurbulence::set_baseFrequency(int axis, double freq)
{
    if (axis == 0) XbaseFrequency = freq;
    if (axis == 1) YbaseFrequency = freq;
    gen->dirty();
}

void FilterTurbulence::set_numOctaves(int num)
{
    numOctaves = num;
    gen->dirty();
}

void FilterTurbulence::set_seed(double s)
{
    seed = s;
    gen->dirty();
}

void FilterTurbulence::set_stitchTiles(bool st)\
{
    stitchTiles = st;
    gen->dirty();
}

void FilterTurbulence::set_type(FilterTurbulenceType t)
{
    type = t;
    gen->dirty();
}

void FilterTurbulence::set_updated(bool /*u*/)
{
}

struct Turbulence
{
    Turbulence(TurbulenceGenerator const &gen, Geom::Affine const &trans, int x0, int y0)
        : _gen(gen)
        , _trans(trans)
        , _x0(x0), _y0(y0) {}

    guint32 operator()(int x, int y)
    {
        Geom::Point point(x + _x0, y + _y0);
        point *= _trans;
        return _gen.turbulencePixel(point);
    }

private:
    TurbulenceGenerator const &_gen;
    Geom::Affine _trans;
    int _x0, _y0;
};

void FilterTurbulence::render_cairo(FilterSlot &slot) const
{
    cairo_surface_t *input = slot.getcairo(_input);
    cairo_surface_t *out = ink_cairo_surface_create_same_size(input, CAIRO_CONTENT_COLOR_ALPHA);

    // It is probably possible to render at a device scale greater than one
    // but for the moment rendering at a device scale of one is the easiest.
    // cairo_image_surface_get_width() returns width in pixels but
    // cairo_surface_create_similar() requires width in device units so divide by device scale.
    // We are rendering at a device scale of 1... so divide by device scale again!
    double x_scale = 0;
    double y_scale = 0;
    cairo_surface_get_device_scale(input, &x_scale, &y_scale);
    int width  = ceil(cairo_image_surface_get_width( input)/x_scale/x_scale);
    int height = ceil(cairo_image_surface_get_height(input)/y_scale/y_scale);
    cairo_surface_t *temp = cairo_surface_create_similar (input, CAIRO_CONTENT_COLOR_ALPHA, width, height);
    cairo_surface_set_device_scale( temp, 1, 1 );

    // color_interpolation_filter is determined by CSS value (see spec. Turbulence).
    set_cairo_surface_ci(out, color_interpolation);

    if (!gen->ready()) {
        Geom::Point ta(fTileX, fTileY);
        Geom::Point tb(fTileX + fTileWidth, fTileY + fTileHeight);
        gen->init(seed, Geom::Rect(ta, tb),
                  Geom::Point(XbaseFrequency, YbaseFrequency), stitchTiles,
                  type == TURBULENCE_FRACTALNOISE, numOctaves);
    }

    Geom::Affine unit_trans = slot.get_units().get_matrix_primitiveunits2pb().inverse();
    Geom::Rect slot_area = slot.get_slot_area();
    double x0 = slot_area.min()[Geom::X];
    double y0 = slot_area.min()[Geom::Y];
    ink_cairo_surface_synthesize(temp, Turbulence(*gen, unit_trans, x0, y0));

    // cairo_surface_write_to_png( temp, "turbulence0.png" );

    cairo_t *ct = cairo_create(out);
    cairo_set_source_surface(ct, temp, 0, 0);
    cairo_paint(ct);
    cairo_destroy(ct);

    cairo_surface_destroy(temp);

    cairo_surface_mark_dirty(out);

    slot.set(_output, out);
    cairo_surface_destroy(out);
}

double FilterTurbulence::complexity(Geom::Affine const &) const
{
    return 5.0;
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

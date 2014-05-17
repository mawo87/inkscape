#define INKSCAPE_LPE_SIMPLIFY_C
/*
 * Released under GNU GPL, read the file 'COPYING' for more information
 */
#include <gtkmm/box.h>
#include <gtkmm/entry.h>
#include "live_effects/lpe-simplify.h"
#include "display/curve.h"
#include "live_effects/parameter/parameter.h"
#include <glibmm/i18n.h>
#include "helper/geom.h"
#include "livarot/Path.h"
#include "splivarot.h"
#include <2geom/svg-path-parser.h>
#include "desktop.h"
#include "inkscape.h"
#include "svg/svg.h"
#include "ui/tools/node-tool.h"
#include <2geom/d2.h>
#include <2geom/generic-rect.h>
#include <2geom/interval.h>


namespace Inkscape {
namespace LivePathEffect {

LPESimplify::LPESimplify(LivePathEffectObject *lpeobject)
    : Effect(lpeobject),
            steps(_("Steps:"),_("Change number of simplify steps "), "steps", &wr, this,1),
            threshold(_("Roughly threshold:"), _("Roughly threshold:"), "threshold", &wr, this, 0.003),
            helper_size(_("Helper size:"), _("Helper size"), "helper_size", &wr, this, 2.),
            nodes(_("Helper nodes"), _("Show helper nodes"), "nodes", &wr, this, false),
            handles(_("Helper handles"), _("Show helper handles"), "handles", &wr, this, false),
            simplifyindividualpaths(_("Paths separately"), _("Simplifying paths (separately)"), "simplifyindividualpaths", &wr, this, false),
            simplifyJustCoalesce(_("Just coalesce"), _("Simplify just coalesce"), "simplifyJustCoalesce", &wr, this, false)
            {
                registerParameter(dynamic_cast<Parameter *>(&steps));
                registerParameter(dynamic_cast<Parameter *>(&threshold));
                registerParameter(dynamic_cast<Parameter *>(&helper_size));
                registerParameter(dynamic_cast<Parameter *>(&nodes));
                registerParameter(dynamic_cast<Parameter *>(&handles));
                registerParameter(dynamic_cast<Parameter *>(&simplifyindividualpaths));
                registerParameter(dynamic_cast<Parameter *>(&simplifyJustCoalesce));
                threshold.param_set_range(0., Geom::infinity());
                threshold.param_set_increments(0.0001, 0.0001);
                threshold.param_set_digits(6);
                steps.param_set_range(0, 100);
                steps.param_set_increments(1, 1);
                steps.param_set_digits(0);
                helper_size.param_set_range(0.1, 100);
                helper_size.param_set_increments(1, 1);
                helper_size.param_set_digits(1);
}

LPESimplify::~LPESimplify() {}

void
LPESimplify::doBeforeEffect (SPLPEItem const* lpeitem)
{
    if(!hp.empty()){
        hp.clear();
    }
    bbox = SP_ITEM(lpeitem)->visualBounds();

}

Gtk::Widget *
LPESimplify::newWidget()
{
    // use manage here, because after deletion of Effect object, others might still be pointing to this widget.
    Gtk::VBox * vbox = Gtk::manage( new Gtk::VBox(Effect::newWidget()) );
    vbox->set_border_width(5);
    vbox->set_homogeneous(false);
    vbox->set_spacing(2);
    std::vector<Parameter *>::iterator it = param_vector.begin();
    Gtk::HBox * buttons = Gtk::manage(new Gtk::HBox(true,0));
    Gtk::HBox * buttonsTwo = Gtk::manage(new Gtk::HBox(true,0));
    while (it != param_vector.end()) {
        if ((*it)->widget_is_visible) {
            Parameter * param = *it;
            Gtk::Widget * widg = dynamic_cast<Gtk::Widget *>(param->param_newWidget());
            if (param->param_key == "simplifyindividualpaths" || 
                param->param_key == "simplifyJustCoalesce")
            {
                Glib::ustring * tip = param->param_getTooltip();
                if (widg) {
                    buttonsTwo->pack_start(*widg, true, true, 2);
                    if (tip) {
                        widg->set_tooltip_text(*tip);
                    } else {
                        widg->set_tooltip_text("");
                        widg->set_has_tooltip(false);
                    }
                }
            } else if (param->param_key == "nodes" || 
                param->param_key == "handles")
            {
                Glib::ustring * tip = param->param_getTooltip();
                if (widg) {
                    buttons->pack_start(*widg, true, true, 2);
                    if (tip) {
                        widg->set_tooltip_text(*tip);
                    } else {
                        widg->set_tooltip_text("");
                        widg->set_has_tooltip(false);
                    }
                }
            }else{
                Glib::ustring * tip = param->param_getTooltip();
                if (widg) {
                    Gtk::HBox * scalarParameter = dynamic_cast<Gtk::HBox *>(widg);
                    std::vector< Gtk::Widget* > childList = scalarParameter->get_children();
                    Gtk::Entry* entryWidg = dynamic_cast<Gtk::Entry *>(childList[1]);
                    entryWidg->set_width_chars(8);
                    vbox->pack_start(*widg, true, true, 2);
                    if (tip) {
                        widg->set_tooltip_text(*tip);
                    } else {
                        widg->set_tooltip_text("");
                        widg->set_has_tooltip(false);
                    }
                }
            }
        }

        ++it;
    }
    vbox->pack_start(*buttons,true, true, 2);
    vbox->pack_start(*buttonsTwo,true, true, 2);
    return dynamic_cast<Gtk::Widget *>(vbox);
}

void 
LPESimplify::doEffect(SPCurve *curve) {
    Geom::PathVector const original_pathv = pathv_to_linear_and_cubic_beziers(curve->get_pathvector());
    gdouble size  = Geom::L2(bbox->dimensions());
    //size /= Geom::Affine(0,0,0,0,0,0).descrim();
    Path* pathliv = Path_for_pathvector(original_pathv);
    if(simplifyindividualpaths){
        size = Geom::L2(Geom::bounds_fast(original_pathv)->dimensions());
    }
    for (int unsigned i = 0; i < steps; i++){
        if ( simplifyJustCoalesce ) {
           pathliv->Coalesce(threshold * size);
        }else{
           pathliv->ConvertEvenLines(threshold * size);
           pathliv->Simplify(threshold * size);
        }
    }
    Geom::PathVector outres = Geom::parse_svg_path(pathliv->svg_dump_path());
    generateHelperPath(outres);
    curve->set_pathvector(outres);
    if(SP_ACTIVE_DESKTOP && INK_IS_NODE_TOOL(SP_ACTIVE_DESKTOP->event_context)){
        SPDesktop* desktop = SP_ACTIVE_DESKTOP;
        Inkscape::UI::Tools::NodeTool *nt = static_cast<Inkscape::UI::Tools::NodeTool*>(desktop->event_context);
        nt->update_helperpath();
    }
}

void
LPESimplify::generateHelperPath(Geom::PathVector result)
{
    if(!handles && !nodes){
        return;
    }

    if(steps < 1){
        return;
    }

    Geom::CubicBezier const *cubic = NULL;
    for (Geom::PathVector::iterator path_it = result.begin(); path_it != result.end(); ++path_it) {
        //Si está vacío...
        if (path_it->empty()){
          continue;
        }
        //Itreadores
        Geom::Path::const_iterator curve_it1 = path_it->begin(); // incoming curve
        Geom::Path::const_iterator curve_it2 =
            ++(path_it->begin());                                // outgoing curve
        Geom::Path::const_iterator curve_endit =
            path_it->end_default(); // this determines when the loop has to stop
        
        if (path_it->closed()) {
          // if the path is closed, maybe we have to stop a bit earlier because the
          // closing line segment has zerolength.
          const Geom::Curve &closingline =
              path_it->back_closed(); // the closing line segment is always of type
                                      // Geom::LineSegment.
          if (are_near(closingline.initialPoint(), closingline.finalPoint())) {
            // closingline.isDegenerate() did not work, because it only checks for
            // *exact* zero length, which goes wrong for relative coordinates and
            // rounding errors...
            // the closing line segment has zero-length. So stop before that one!
            curve_endit = path_it->end_open();
          }
        }
        if(nodes){
            drawNode(curve_it1->initialPoint());
        }
        while (curve_it2 != curve_endit) {
          cubic = dynamic_cast<Geom::CubicBezier const *>(&*curve_it1);
          if (cubic) {
            if(handles){
                drawHandle((*cubic)[1]);
                drawHandle((*cubic)[2]);
                drawHandleLine((*cubic)[0],(*cubic)[1]);
                drawHandleLine((*cubic)[2],(*cubic)[3]);
            }
          } 
          if(nodes){
              drawNode(curve_it1->finalPoint());
          }
          ++curve_it1;
          ++curve_it2;
        }
        cubic = dynamic_cast<Geom::CubicBezier const *>(&*curve_it1);
        if (cubic) {
          if(handles){
            drawHandle((*cubic)[1]);
            drawHandle((*cubic)[2]);
            drawHandleLine((*cubic)[0],(*cubic)[1]);
            drawHandleLine((*cubic)[2],(*cubic)[3]);
          }
        } 
        if(nodes){
          drawNode(curve_it1->finalPoint());
        }
    }
}

void 
LPESimplify::drawNode(Geom::Point p)
{
    double r = helper_size/0.67;
    char const * svgd;
    svgd = "M 0.999993,0.5 C 1.000065,0.7757576 0.7761859,1 0.4999926,1 0.2237994,1 -7.933901e-5,0.7757576 -7.339015e-6,0.5 -7.933901e-5,0.2242424 0.2237994,0 0.4999926,0 0.7761859,0 1.000065,0.2242424 0.999993,0.5 Z m -0.058561,0 C 0.9414949,0.74327 0.7438375,0.9416286 0.4999928,0.9416286 0.2561481,0.9416286 0.0584908,0.74327 0.0585543,0.5 0.0584908,0.25673 0.2561481,0.0583714 0.4999928,0.0583714 0.7438375,0.0583714 0.9414949,0.25673 0.9414313,0.5 Z m -0.3828447,0 c 8.5e-6,0.030303 -0.026228,0.060606 -0.058593,0.060606 -0.032366,0 -0.058603,-0.030303 -0.058593,-0.060606 -8.5e-6,-0.030303 0.026227,-0.060606 0.058593,-0.060606 0.032366,0 0.058603,0.030303 0.058593,0.060606 z";
    Geom::PathVector pathv = sp_svg_read_pathv(svgd);
    pathv *= Geom::Affine(r,0,0,r,0,0);
    pathv += p - Geom::Point(0.5*r,0.5*r);
    hp.push_back(pathv[0]);
    hp.push_back(pathv[1]);
    hp.push_back(pathv[2]);
}

void
LPESimplify::drawHandle(Geom::Point p)
{
    double r = helper_size/0.67;
    char const * svgd;
    svgd = "M 0.6999623,0.35 C 0.7000128,0.5430303 0.5433044,0.7 0.3499775,0.7 0.1566506,0.7 -5.778776e-5,0.5430303 -7.344202e-6,0.35 -5.778776e-5,0.1569697 0.1566506,0 0.3499775,0 0.5433044,0 0.7000128,0.1569697 0.6999623,0.35 Z";
    Geom::PathVector pathv = sp_svg_read_pathv(svgd);
    pathv *= Geom::Affine(r,0,0,r,0,0);
    pathv += p - Geom::Point(0.35*r,0.35*r);
    hp.push_back(pathv[0]);
}


void
LPESimplify::drawHandleLine(Geom::Point p,Geom::Point p2)
{
    Geom::Path path;
    path.start( p );
    path.appendNew<Geom::LineSegment>( p2 );
    hp.push_back(path);
}

void
LPESimplify::addCanvasIndicators(SPLPEItem const */*lpeitem*/, std::vector<Geom::PathVector> &hp_vec)
{
    hp_vec.push_back(hp);
}


}; //namespace LivePathEffect
}; /* namespace Inkscape */

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

#ifndef SEEN_PREFERENCES_SKELETON_H
#define SEEN_PREFERENCES_SKELETON_H

#include <inkscape_version.h>

static char const preferences_skeleton[] =
"<inkscape version=\"" INKSCAPE_VERSION "\"\n"
"  xmlns:sodipodi=\"http://inkscape.sourceforge.net/DTD/sodipodi-0.dtd\"\n"
"  xmlns:inkscape=\"http://www.inkscape.org/namespaces/inkscape\">\n"
"  <group id=\"window\">\n"
"    <group id=\"menu\" state=\"1\"/>\n"
"    <group id=\"commands\" state=\"1\"/>\n"
"    <group id=\"toppanel\" state=\"1\"/>\n"
"    <group id=\"toolbox\" state=\"1\"/>\n"
"    <group id=\"statusbar\" state=\"1\"/>\n"
"    <group id=\"rulers\" state=\"1\"/>\n"
"    <group id=\"scrollbars\" state=\"1\"/>\n"
"  </group>\n"
"  <group id=\"fullscreen\">\n"
"    <group id=\"menu\" state=\"1\"/>\n"
"    <group id=\"commands\" state=\"1\"/>\n"
"    <group id=\"toppanel\" state=\"1\"/>\n"
"    <group id=\"toolbox\" state=\"1\"/>\n"
"    <group id=\"statusbar\" state=\"1\"/>\n"
"    <group id=\"rulers\" state=\"1\"/>\n"
"    <group id=\"scrollbars\" state=\"1\"/>\n"
"  </group>\n"
"\n"
"  <group id=\"documents\">\n"
"    <group id=\"recent\"/>\n"
"  </group>\n"
"\n"
"  <group id=\"template\">\n"
"    <sodipodi:namedview\n"
"       id=\"base\"\n"
"       pagecolor=\"#ffffff\"\n"
"       bordercolor=\"#666666\"\n"
"       borderopacity=\"1.0\"\n"
"       inkscape:pageopacity=\"0.0\"\n"
"       inkscape:pageshadow=\"2\"\n"
"       inkscape:zoom=\"0.43415836\"\n"
"       inkscape:cx=\"305.259528\"\n"
"       inkscape:cy=\"417.849475\"\n"
"       inkscape:window-width=\"640\"\n"
"       inkscape:window-height=\"480\" />\n"
"  </group>\n"
"\n"
"  <group id=\"tools\"\n"
"         style=\"fill:none;fill-opacity:0.75;\n"
"                 stroke:black;stroke-opacity:1;stroke-width:1pt;stroke-linejoin:miter;stroke-linecap:butt;\">\n"
"    <group id=\"shapes\" style=\"fill-rule:evenodd;\" selcue=\"1\">\n"
"      <eventcontext id=\"rect\" style=\"fill:blue;\" usecurrent=\"1\"/>\n"
"      <eventcontext id=\"arc\" style=\"fill:red;\" end=\"0\" start=\"0\" usecurrent=\"1\"/>\n"
"      <eventcontext id=\"star\" magnitude=\"5\" style=\"fill:yellow;\" usecurrent=\"1\"/>\n"
"      <eventcontext id=\"spiral\" style=\"fill:none;stroke:black;\" usecurrent=\"0\"/>\n"
"    </group>\n"
"    <group id=\"freehand\"\n"
"         style=\"fill:none;fill-rule:evenodd;stroke:black;stroke-opacity:1;stroke-linejoin:miter;stroke-linecap:butt;\">\n"
"      <eventcontext id=\"pencil\" tolerance=\"10.0\" selcue=\"1\" style=\"stroke-width:1pt;\" usecurrent=\"0\"/>\n"
"      <eventcontext id=\"pen\" mode=\"drag\" selcue=\"1\" style=\"stroke-width:0.25pt;\" usecurrent=\"0\"/>\n"
"    </group>\n"
"    <eventcontext id=\"calligraphic\" style=\"fill:black;fill-opacity:1;fill-rule:nonzero;stroke:none;\"\n"
"                       mass=\"0.02\" drag=\"1\" angle=\"30\" width=\"0.20\" thinning=\"0.25\" flatness=\"0.9\" selcue=\"1\"/>\n"
"    <eventcontext id=\"text\"  usecurrent=\"0\"\n"
"                  style=\"fill:black;fill-opacity:1;stroke:none;font-family:Bitstream Vera Sans;font-style:normal;font-weight:normal;font-size:12px;\" selcue=\"1\"/>\n"
"    <eventcontext id=\"nodes\" selcue=\"1\"/>\n"
"    <eventcontext id=\"zoom\" selcue=\"1\"/>\n"
"    <eventcontext id=\"dropper\" selcue=\"1\"/>\n"
"    <eventcontext id=\"select\" selcue=\"1\"/>\n"
"  </group>\n"
"  <group id=\"palette\">\n"
"    <group id=\"dashes\">\n"
"      <dash id=\"solid\" style=\"stroke-dasharray:none;\"/>\n"
"      <dash id=\"dash-1-1\" style=\"stroke-dasharray:1 1;\"/>\n"
"      <dash id=\"dash-1-2\" style=\"stroke-dasharray:1 2;\"/>\n"
"      <dash id=\"dash-1-3\" style=\"stroke-dasharray:1 3;\"/>\n"
"      <dash id=\"dash-1-4\" style=\"stroke-dasharray:1 4;\"/>\n"
"      <dash id=\"dash-1-6\" style=\"stroke-dasharray:1 6;\"/>\n"
"      <dash id=\"dash-1-8\" style=\"stroke-dasharray:1 8;\"/>\n"
"      <dash id=\"dash-1-12\" style=\"stroke-dasharray:1 12;\"/>\n"
"      <dash id=\"dash-1-24\" style=\"stroke-dasharray:1 24;\"/>\n"
"      <dash id=\"dash-1-48\" style=\"stroke-dasharray:1 48;\"/>\n"
"      <dash id=\"dash-2-1\" style=\"stroke-dasharray:2 1;\"/>\n"
"      <dash id=\"dash-3-1\" style=\"stroke-dasharray:3 1;\"/>\n"
"      <dash id=\"dash-4-1\" style=\"stroke-dasharray:4 1;\"/>\n"
"      <dash id=\"dash-6-1\" style=\"stroke-dasharray:6 1;\"/>\n"
"      <dash id=\"dash-8-1\" style=\"stroke-dasharray:8 1;\"/>\n"
"      <dash id=\"dash-12-1\" style=\"stroke-dasharray:12 1;\"/>\n"
"      <dash id=\"dash-24-1\" style=\"stroke-dasharray:24 1;\"/>\n"
"      <dash id=\"dash-2-2\" style=\"stroke-dasharray:2 2;\"/>\n"
"      <dash id=\"dash-3-3\" style=\"stroke-dasharray:3 3;\"/>\n"
"      <dash id=\"dash-4-4\" style=\"stroke-dasharray:4 4;\"/>\n"
"      <dash id=\"dash-6-6\" style=\"stroke-dasharray:6 6;\"/>\n"
"      <dash id=\"dash-8-8\" style=\"stroke-dasharray:8 8;\"/>\n"
"      <dash id=\"dash-12-12\" style=\"stroke-dasharray:12 12;\"/>\n"
"      <dash id=\"dash-24-24\" style=\"stroke-dasharray:24 24;\"/>\n"
"      <dash id=\"dash-2-4\" style=\"stroke-dasharray:2 4;\"/>\n"
"      <dash id=\"dash-4-2\" style=\"stroke-dasharray:4 2;\"/>\n"
"      <dash id=\"dash-2-6\" style=\"stroke-dasharray:2 6;\"/>\n"
"      <dash id=\"dash-6-2\" style=\"stroke-dasharray:6 2;\"/>\n"
"      <dash id=\"dash-4-8\" style=\"stroke-dasharray:4 8;\"/>\n"
"      <dash id=\"dash-8-4\" style=\"stroke-dasharray:8 4;\"/>\n"
"      <dash id=\"dash-2-1-012-1\" style=\"stroke-dasharray:2 1 0.5 1;\"/>\n"
"      <dash id=\"dash-4-2-1-2\" style=\"stroke-dasharray:4 2 1 2;\"/>\n"
"      <dash id=\"dash-8-2-1-2\" style=\"stroke-dasharray:8 2 1 2;\"/>\n"
"      <dash id=\"dash-012-012\" style=\"stroke-dasharray:0.5 0.5;\"/>\n"
"      <dash id=\"dash-014-014\" style=\"stroke-dasharray:0.25 0.25;\"/>\n"
"      <dash id=\"dash-0110-0110\" style=\"stroke-dasharray:0.1 0.1;\"/>\n"
"      <dash id=\"dash-0150-0150\" style=\"stroke-dasharray:0.02 0.02;\"/>\n"
"    </group>\n"
"  </group>\n"
"  <group id=\"colorselector\" page=\"0\"/>\n"
"  <group id=\"dialogs\">\n"
"    <group id=\"toolbox\"/>\n"
"    <group id=\"fillstroke\"/>\n"
"    <group id=\"textandfont\"/>\n"
"    <group id=\"transformation\"/>\n"
"    <group id=\"align\"/>\n"
"    <group id=\"xml\"/>\n"
"    <group id=\"find\"/>\n"
"    <group id=\"documentoptions\"/>\n"
"    <group id=\"preferences\"/>\n"
"    <group id=\"gradienteditor\"/>\n"
"    <group id=\"object\"/>\n"
"    <group id=\"export\">\n"
"      <group id=\"exportarea\"/>\n"
"      <group id=\"defaultxdpi\"/>\n"
"    </group>\n"
"    <group id=\"save_as\" default=\"\" append_extension=\"1\" path=\"\"/>\n"
"    <group id=\"open\" path=\"\"/>\n"
"    <group id=\"debug\" redirect=\"0\"/>\n"
"  </group>\n"
"  <group id=\"printing\">\n"
"    <settings id=\"ps\"/>\n"
"  </group>\n"
"\n"
"  <group id=\"options\">\n"
"    <group id=\"nudgedistance\" value=\"2\"/>\n"
"    <group id=\"rotationsnapsperpi\" value=\"12\"/>\n"
"    <group id=\"cursortolerance\" value=\"8.0\"/>\n"
"    <group id=\"dragtolerance\" value=\"4.0\"/>\n"
"    <group id=\"savewindowgeometry\" value=\"1\"/>\n"
"    <group id=\"defaultoffsetwidth\" value=\"2\"/>\n"
"    <group id=\"defaultscale\" value=\"2\"/>\n"
"    <group id=\"maxrecentdocuments\" value=\"20\"/>\n"
"    <group id=\"zoomincrement\" value=\"1.414213562\"/>\n"
"    <group id=\"keyscroll\" value=\"10\"/>\n"
"    <group id=\"wheelscroll\" value=\"40\"/>\n"
"    <group id=\"transientpolicy\" value=\"1\"/>\n"
"    <group id=\"scrollingacceleration\" value=\"0.35\"/>\n"
"    <group id=\"autoscrollspeed\" value=\"0.7\"/>\n"
"    <group id=\"autoscrolldistance\" value=\"-10\"/>\n"
"    <group id=\"simplifythreshold\" value=\"0.002\"/>\n"
"    <group id=\"dialogsskiptaskbar\" value=\"1\"/>\n"
"    <group id=\"arenatilescachesize\" value=\"8192\"/>\n"
"    <group id=\"preservetransform\" value=\"0\"/>\n"
"    <group id=\"clonecompensation\" value=\"0\"/>\n"
"    <group id=\"cloneorphans\" value=\"0\"/>\n"
"    <group id=\"stickyzoom\" value=\"0\"/>\n"
"    <group id=\"selcue\" value=\"2\"/>\n"
"    <group id=\"importbitmapsasimages\" value=\"1\"/>\n"
"    <group id=\"transform\" stroke=\"1\" rectcorners=\"0\" pattern=\"1\" gradient=\"1\" />\n"
"    <group id=\"createbitmap\" minsize=\"250\"/>\n"
"  </group>\n"
"\n"
"  <group id=\"extensions\">"
"  </group>\n"
"\n"
"  <group id=\"desktop\""
"         style=\"\">\n"
"  </group>\n"
"\n"
"</inkscape>\n";

#define PREFERENCES_SKELETON_SIZE (sizeof(preferences_skeleton) - 1)


#endif /* !SEEN_PREFERENCES_SKELETON_H */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=c++:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :

#ifndef __SP_DESKTOP_STYLE_H__
#define __SP_DESKTOP_STYLE_H__

/*
 * Desktop style management
 *
 * Authors:
 *   bulia byak
 *
 * Copyright (C) 2004 authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "xml/xml-forward.h"

void sp_desktop_apply_css_recursive (SPObject *o, SPCSSAttr *css, bool skip_lines);
void sp_desktop_set_color (SPDesktop *desktop, const ColorRGBA &color, bool is_relative, bool fill);
void sp_desktop_set_style (SPDesktop *desktop, SPCSSAttr *css, bool change = true);
SPCSSAttr *sp_desktop_get_style (SPDesktop *desktop, bool with_text);
void sp_desktop_apply_style_tool (SPDesktop *desktop, SPRepr *repr, const char *tool, bool with_text);

#endif

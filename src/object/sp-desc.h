// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SP_DESC_H
#define SEEN_SP_DESC_H

/*
 * SVG <desc> implementation
 *
 * Authors:
 *   Jeff Schiller <codedread@gmail.com>
 *
 * Copyright (C) 2008 Jeff Schiller
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "sp-object.h"

class SPDesc final : public SPObject {
public:
	SPDesc();
	~SPDesc() override;
    int tag() const override { return tag_of<decltype(*this)>; }

protected:
	Inkscape::XML::Node* write(Inkscape::XML::Document* doc, Inkscape::XML::Node* repr, unsigned int flags) override;
};

#endif

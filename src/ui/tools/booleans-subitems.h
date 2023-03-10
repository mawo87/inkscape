// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 *
 *//*
 * Authors:
 *   Martin Owens
 *
 * Copyright (C) 2022 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_TOOLS_BOOLEANS_SUBITEMS_H
#define INKSCAPE_UI_TOOLS_BOOLEANS_SUBITEMS_H

#include <2geom/pathvector.h>
#include <vector>
#include <functional>

class SPItem;

namespace Inkscape {

class SubItem;
using WorkItem = std::shared_ptr<SubItem>;
using WorkItems = std::vector<WorkItem>;

/**
 * When an item is broken, each broken part is represented by
 * the SubItem class. This class hold information such as the
 * original items it originated from and the paths that it
 * consists of.
 **/
class SubItem
{
public:

    SubItem(Geom::PathVector paths, SPItem *item)
        : _paths(std::move(paths))
        , _item(item)
    {}

    SubItem(const SubItem &copy)
        : SubItem(copy._paths, copy._item)
    {}

    SubItem &operator+=(const SubItem &other);

    bool contains(const Geom::Point &pt) const;

    const Geom::PathVector &get_pathv() const { return _paths; }
    SPItem *get_item() const { return _item; }

    static WorkItems build_mosaic(std::vector<SPItem*> &&items);
    static WorkItems generate_holes(const WorkItems &items);

    bool getSelected() const { return _selected; }
    void setSelected(bool selected) { _selected = selected; }
private:
    Geom::PathVector _paths;
    SPItem *_item;
    bool _selected = false;
};

} // namespace Inkscape

#endif // INKSCAPE_UI_TOOLS_BOOLEANS_SUBITEMS_H

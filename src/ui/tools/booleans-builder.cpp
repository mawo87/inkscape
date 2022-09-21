// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Boolean tool shape builder.
 *
 *//*
 * Authors:
 *   Martin Owens
 *
 * Copyright (C) 2022 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "booleans-builder.h"

#include "actions/actions-undo-document.h"
#include "display/control/canvas-item-group.h"
#include "display/control/canvas-item-bpath.h"
#include "object/object-set.h"
#include "object/sp-item.h"
#include "ui/widget/canvas.h"
#include "svg/svg.h"

namespace Inkscape {

using TaskType = int;
enum TaskTypes : TaskType {
    TASKED_NONE,
    TASKED_ADD,
    TASKED_DELETE
};
static constexpr std::array<uint32_t, 6> fills = {0x00000055, 0x0291ffff, 0x8eceffff, 0x0291ffff, 0xf299d6ff, 0xff0db3ff};

BooleanBuilder::BooleanBuilder(ObjectSet *set)
    : _set(set)
{
    // Current state of all the items
    _work_items = SubItem::build_mosaic(set->items_vector());

    auto root = _set->desktop()->getCanvas()->get_canvas_item_root();
    _group = std::make_unique<Inkscape::CanvasItemGroup>(root);

    redraw_items();
}

BooleanBuilder::~BooleanBuilder() = default;

/**
 * Control the visual appearence of this particular bpath
 */
static void redraw_item(CanvasItemBpath &bpath, bool selected, TaskType task)
{
    bpath.set_fill(fills[(int)task * 2 + (int)selected], SP_WIND_RULE_POSITIVE);
    bpath.set_stroke(task == TASKED_NONE ? 0x000000dd : 0xffffffff);
    bpath.set_stroke_width(task == TASKED_NONE ? 1.0 : 3.0);
}

/**
 * Update to visuals with the latest subitem list.
 */
void BooleanBuilder::redraw_items()
{
    _screen_items.clear();

    for (auto &subitem : _work_items) {
        // Construct BPath from each subitem!
        auto bpath = std::make_shared<Inkscape::CanvasItemBpath>(_group.get(), subitem->get_pathv(), false);
        redraw_item(*bpath, subitem->getSelected(), TASKED_NONE);
        _screen_items.emplace_back(subitem, std::move(bpath));
    }

    // Selectively handle the undo actions being enabled / disabled
    enable_undo_actions(_set->document(), _undo.size(), _redo.size());
}

std::optional<ItemPair> BooleanBuilder::get_item(const Geom::Point &point)
{
    for (auto &pair : _screen_items) {
        if (pair.second->contains(point, 2.0))
            return pair;
    }
    return {};
}

/**
 * Highlight any shape under the mouse at this point.
 */
bool BooleanBuilder::highlight(const Geom::Point &point, bool add)
{
    if (has_task())
        return true;

    bool done = false;
    for (auto &[work, vis] : _screen_items) {
        bool hover = !done && vis->contains(point, 2.0);
        redraw_item(*vis, work->getSelected(), hover ? (add ? TASKED_ADD : TASKED_DELETE) : TASKED_NONE);
        if (hover)
            vis->raise_to_top();
        done = done || hover;
    }
    return done;
}

/**
 * Select the shape under the cursor
 */
bool BooleanBuilder::task_select(const Geom::Point &point, bool add_task)
{
    if (has_task())
        task_cancel();
    if (auto maybe = get_item(point)) {
        _add_task = add_task;
        auto &[work, vis] = maybe.value();
        _work_task = std::make_shared<SubItem>(*work);
        _work_task->setSelected(true);
        _screen_task = std::make_shared<Inkscape::CanvasItemBpath>(_group.get(), _work_task->get_pathv(), false);
        redraw_item(*_screen_task, true, add_task ? TASKED_ADD : TASKED_DELETE);
        vis->set_visible(false);
        redraw_item(*vis, false, TASKED_NONE);
        return true;
    }
    return false;
}

bool BooleanBuilder::task_add(const Geom::Point &point)
{
    if (!has_task())
        return false;
    if (auto maybe = get_item(point)) {
        auto &[work, vis] = maybe.value();
        // Invisible items are already processed.
        if (vis->is_visible()) {
            vis->set_visible(false);
            *_work_task += *work;
            _screen_task->set_bpath(_work_task->get_pathv(), false);
            return true;
        }
    }
    return false;
}

void BooleanBuilder::task_cancel()
{
    _work_task.reset();
    _screen_task.reset();
    for (auto &[work, vis] : _screen_items) {
        vis->set_visible(true);
    }
}

void BooleanBuilder::task_commit()
{
    if (!has_task())
        return;

    // Manage undo/redo
    _undo.emplace_back(std::move(_work_items));
    _redo.clear();

    // A. Delete all items from _work_items that aren't visible
    _work_items.clear();
    for (auto &[work, vis] : _screen_items) {
        if (vis->is_visible()) {
            _work_items.emplace_back(work);
        }
    }
    if (_add_task) {
        // B. Add _work_task to _work_items for union tasks
        _work_items.emplace_back(std::move(_work_task));
    }

    // C. Reset everything
    redraw_items();
    _work_task.reset();
    _screen_task.reset();
}

/**
 * Commit the changes to the document (finish)
 */
std::vector<SPObject *> BooleanBuilder::shape_commit(bool all)
{
    std::vector<SPObject *> ret;
    auto doc = _set->document();
    auto items = _set->items_vector();
    bool has_changes = _undo.size();

    // Only commit anything if we have changes, return selection.
    if (!has_changes && !all) {
        ret.insert(ret.begin(), items.begin(), items.end());
        return ret;
    }

    // Count number of selected items.
    int selected = 0;
    for (auto const &subitem : _work_items) {
        selected += (int)subitem->getSelected();
    }

    for (auto const &subitem : _work_items) {
        // Either this object is selected, or no objects are selected at all.
        if (!subitem->getSelected() && selected)
            continue;
        auto item = subitem->get_item();
        // For the rare occasion the user generates from a hole (no item)
        if (!item)
            item = *items.begin();
        if (!item) {
            g_warning("Can't generate itemless object in boolean-builder.");
            continue;
        }
        auto parent = dynamic_cast<SPItem *>(item->parent);

        Inkscape::XML::Node *repr = doc->getReprDoc()->createElement("svg:path");
        repr->setAttribute("d", sp_svg_write_path(subitem->get_pathv() * parent->dt2i_affine()));
        repr->setAttribute("style", item->getRepr()->attribute("style"));
        parent->getRepr()->addChild(repr, item->getRepr());
        ret.emplace_back(doc->getObjectByRepr(repr));
    }
    _work_items.clear();

    for (auto item : items) {
        sp_object_ref(item, nullptr);
        item->deleteObject(true, true);
        sp_object_unref(item, nullptr);
    }
    return ret;
}

void BooleanBuilder::undo()
{
    if (_undo.empty())
        return;

    // Cancel any task;
    task_cancel();

    // Shuffle the undo stack
    _redo.emplace_back(std::move(_work_items));
    _work_items = std::move(_undo.back());
    _undo.pop_back();

    // Redraw the screen items
    redraw_items();
}

void BooleanBuilder::redo()
{
    if (_redo.empty())
        return;

    // Cancel any task;
    task_cancel();

    // Shuffle the undo stack
    _undo.emplace_back(std::move(_work_items));
    _work_items = std::move(_redo.back());
    _redo.pop_back();

    // Redraw the screen items
    redraw_items();
}

} // namespace Inkscape
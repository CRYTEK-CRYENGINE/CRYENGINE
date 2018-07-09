/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include <CrySerialization/yasli/Pointers.h>
#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/BinArchive.h>
#include <CrySerialization/yasli/Pointers.h>
#if YASLI_INCLUDE_PROPERTY_TREE_CONFIG_LOCAL
#include <CrySerialization/yasli/ConfigLocal.h>
#endif
#include "PropertyTree.h"
#include "IDrawContext.h"
#include "Serialization.h"
#include "PropertyTreeModel.h"
#include "PropertyTreeStyle.h"
#include "ValidatorBlock.h"

#include <CrySerialization/yasli/ClassFactory.h>

#include "PropertyOArchive.h"
#include "PropertyIArchive.h"
#include "Unicode.h"

#include <limits.h>
#include "PropertyTreeMenuHandler.h"
#include "IUIFacade.h"
#include "IMenu.h"

#include "MathUtils.h"

#include "PropertyRowObject.h"

using yasli::Serializers;

// ---------------------------------------------------------------------------

TreeConfig::TreeConfig()
: immediateUpdate(true)
, valueColumnWidth(.5f)
, filter(YASLI_DEFAULT_FILTER)
, fullRowContainers(true)
, showContainerIndices(true)
, filterWhenType(true)
, sliderUpdateDelay(25)
, undoEnabled(false)
, fullUndo(false)
, multiSelection(false)
, enableActions(true)
{
	defaultRowHeight = 22;
	tabSize = defaultRowHeight;
}

TreeConfig TreeConfig::defaultConfig;

// ---------------------------------------------------------------------------

void PropertyTreeMenuHandler::onMenuFilter()
{
	tree->startFilter("");
}

void PropertyTreeMenuHandler::onMenuFilterByName()
{
	tree->startFilter(filterName.c_str());
}

void PropertyTreeMenuHandler::onMenuFilterByValue()
{
	tree->startFilter(filterValue.c_str());
}

void PropertyTreeMenuHandler::onMenuFilterByType()
{
	tree->startFilter(filterType.c_str());
}

void PropertyTreeMenuHandler::onMenuUndo()
{
	tree->model()->undo();
}

void PropertyTreeMenuHandler::onMenuCopy()
{
	tree->copyRow(row);
}

void PropertyTreeMenuHandler::onMenuPaste()
{
	tree->pasteRow(row);
}
// ---------------------------------------------------------------------------

PropertyTreeStyle PropertyTree::defaultTreeStyle_;

PropertyTree::PropertyTree(IUIFacade* uiFacade)
: attachedPropertyTree_(0)
, ui_(uiFacade)
, autoRevert_(true)
, leftBorder_(0)
, rightBorder_(0)
, cursorX_(0)
, filterMode_(false)
, propertySplitterPos_(150)
, splitterDragging_(false)
, pressPoint_(-1, -1)
, pressDelta_(0, 0)
, pointerMovedSincePress_(false)
, lastStillPosition_(-1, -1)
, mouseOverRow_(0)
, pressedRow_(0)
, capturedRow_(0)
, dragCheckMode_(false)
, dragCheckValue_(false)
, archiveContext_()
, outlineMode_(false)
, hideSelection_(false)
, zoomLevel_(10)
, validatorBlock_(new ValidatorBlock)
, style_(new PropertyTreeStyle(defaultTreeStyle_))
, defaultRowHeight_(22)
{
	model_.reset(new PropertyTreeModel(this));
	model_->setExpandLevels(config_.expandLevels);
	model_->setUndoEnabled(config_.undoEnabled);
	model_->setFullUndo(config_.fullUndo);
}

PropertyTree::~PropertyTree()
{
	clearMenuHandlers();
	delete ui_;
}

bool PropertyTree::onRowKeyDown(PropertyRow* row, const KeyEvent* ev)
{
	using namespace property_tree;
	PropertyTreeMenuHandler handler;
	handler.row = row;
	handler.tree = this;

	if(row->onKeyDown(this, ev))
		return true;

	switch(ev->key()){
	case KEY_C:
	if (ev->modifiers() == MODIFIER_CONTROL)
		handler.onMenuCopy();
	return true;
	case KEY_V:
	if (ev->modifiers() == MODIFIER_CONTROL)
		handler.onMenuPaste();
	return true;
	case KEY_Z:
	if (ev->modifiers() == MODIFIER_CONTROL)
		if(model()->canUndo()){
			model()->undo();
			return true;
		}
	break;
	case KEY_F2:
	if (ev->modifiers() == 0) {
		if(selectedRow()) {
			PropertyActivationEvent ev;
			ev.tree = this;
			ev.reason = ev.REASON_KEYBOARD;
			ev.force = true;
			selectedRow()->onActivate(ev);
		}
	}
	break;
	case KEY_MENU:
	{
		if (ev->modifiers() == 0) {
			std::auto_ptr<property_tree::IMenu> menu(ui()->createMenu());

			if(onContextMenu(row, *menu)){
				Rect rect(row->rect());
				menu->exec(Point(rect.left() + rect.height(), rect.bottom()));
			}
			return true;
		}
		break;
	}
	}

	PropertyRow* focusedRow = model()->focusedRow();
	if(!focusedRow)
		return false;
	PropertyRow* parentRow = focusedRow->nonPulledParent();
	int x = parentRow->horizontalIndex(this, focusedRow);
	int y = model()->root()->verticalIndex(this, parentRow);
	PropertyRow* selectedRow = 0;
	switch(ev->key()){
	case KEY_UP:
		//if (filterMode_ && y == 0) {
		//	startFilter("");
		//}
		//else 
		{
			selectedRow = model()->root()->rowByVerticalIndex(this, --y);
			if (selectedRow)
				selectedRow = selectedRow->rowByHorizontalIndex(this, cursorX_);
		}
		break;
	case KEY_DOWN:
		//if (filterMode_) {
		//	setFocus();
		//}
		//else 
		{
			selectedRow = model()->root()->rowByVerticalIndex(this, ++y);
			if (selectedRow)
				selectedRow = selectedRow->rowByHorizontalIndex(this, cursorX_);
		}
		break;
	case KEY_LEFT:
		selectedRow = parentRow->rowByHorizontalIndex(this, cursorX_ = --x);
		if(selectedRow == focusedRow && parentRow->canBeToggled(this) && parentRow->expanded()){
			expandRow(parentRow, false);
			selectedRow = model()->focusedRow();
		}
		break;
	case KEY_RIGHT:
		selectedRow = parentRow->rowByHorizontalIndex(this, cursorX_ = ++x);
		if(selectedRow == focusedRow && parentRow->canBeToggled(this) && !parentRow->expanded()){
			expandRow(parentRow, true);
			selectedRow = model()->focusedRow();
		}
		break;
	case KEY_HOME:
		if (ev->modifiers() == MODIFIER_CONTROL) {
			selectedRow = parentRow->rowByHorizontalIndex(this, cursorX_ = INT_MIN);
		}
		else {
			selectedRow = model()->root()->rowByVerticalIndex(this, 0);
			if (selectedRow)
				selectedRow = selectedRow->rowByHorizontalIndex(this, cursorX_);
		}
		break;
	case KEY_END:
		if (ev->modifiers() == MODIFIER_CONTROL) {
			selectedRow = parentRow->rowByHorizontalIndex(this, cursorX_ = INT_MAX);
		}
		else {
			selectedRow = model()->root()->rowByVerticalIndex(this, INT_MAX);
			if (selectedRow)
				selectedRow = selectedRow->rowByHorizontalIndex(this, cursorX_);
		}
		break;
	case KEY_SPACE:
		if (config_.filterWhenType)
			break;
	case KEY_ENTER:
	case KEY_RETURN:
		if(focusedRow->canBeToggled(this))
			expandRow(focusedRow, !focusedRow->expanded());
		else {
			PropertyActivationEvent e;
			e.tree = this;
			e.reason = e.REASON_KEYBOARD;
			e.force = false;
			focusedRow->onActivate(e);
		}
		break;
	}
	if(selectedRow){
		onRowSelected(std::vector<PropertyRow*>(1, selectedRow), false, false);	
		return true;
	}
	return false;
}

struct FirstIssueVisitor
{
	ValidatorEntryType entryType_;
	PropertyRow* startRow_;
	PropertyRow* result;

	FirstIssueVisitor(ValidatorEntryType type, PropertyRow* startRow)
	: entryType_(type)
	, startRow_(startRow)
	, result()
	{
	}

	ScanResult operator()(PropertyRow* row, PropertyTree* tree, int)
	{
		if ((row->pulledUp() || row->pulledBefore()) && row->nonPulledParent() == startRow_)
			return SCAN_SIBLINGS;
		if (row->validatorCount()) {
			if (const ValidatorEntry* validatorEntries = tree->_validatorBlock()->getEntry(row->validatorIndex(), row->validatorCount())) {
				for (int i = 0; i < row->validatorCount(); ++i) {
					const ValidatorEntry* validatorEntry = validatorEntries + i;
					if (validatorEntry->type == entryType_) {
						result = row;
						return SCAN_FINISHED;
					}
				}
			}
		}
		return SCAN_CHILDREN_SIBLINGS;
	}
};

void PropertyTree::jumpToNextHiddenValidatorIssue(bool isError, PropertyRow* start)
{
	FirstIssueVisitor op(isError ? VALIDATOR_ENTRY_ERROR : VALIDATOR_ENTRY_WARNING, start);
	start->scanChildren(op, this);

	PropertyRow* row = op.result;

	vector<PropertyRow*> parents;
	while (row && row->parent())  {
		parents.push_back(row);
		row = row->parent();
	}
	for (int i = (int)parents.size()-1; i >= 0; --i) {
		if (!parents[i]->visible(this))
			break;
		row = parents[i];
	}
	if (row)
		setSelectedRow(row);

	updateValidatorIcons();
	updateHeights();
}

static void rowsInBetween(vector<PropertyRow*>* rows, PropertyRow* a, PropertyRow* b)
{
	if (!a)
		return;
	if (!b)
		return;
	vector<PropertyRow*> pathA;
	PropertyRow* rootA = a;
	while (rootA->parent()) {
		pathA.push_back(rootA);
		rootA = rootA->parent();
	}

	vector<PropertyRow*> pathB;
	PropertyRow* rootB = b;
	while (rootB->parent()) {
		pathB.push_back(rootB);
		rootB = rootB->parent();
	}

	if (rootA != rootB)
		return;

	const PropertyRow* commonParent = rootA;
	int maxDepth = min((int)pathA.size(), (int)pathB.size());
	for (int i = 0; i < maxDepth; ++i) {
		PropertyRow* parentA = pathA[(int)pathA.size() - 1 - i];
		PropertyRow* parentB = pathB[(int)pathB.size() - 1 - i];
		if (parentA != parentB) {
			int indexA = commonParent->childIndex(parentA);
			int indexB = commonParent->childIndex(parentB);
			int minIndex = min(indexA, indexB);
			int maxIndex = max(indexA, indexB);
			for (int j = minIndex; j <= maxIndex; ++j)
				rows->push_back((PropertyRow*)commonParent->childByIndex(j));
			return;
		}
		commonParent = parentA;
	}
}

bool PropertyTree::onRowLMBDown(PropertyRow* row, const Rect& rowRect, Point point, bool controlPressed, bool shiftPressed)
{
	pressPoint_ = point;
	pressDelta_ = Point(0, 0);
	pointerMovedSincePress_ = false;
	row = model()->root()->hit(this, point);
	if(row){
		if (!row->isRoot()) {
			if(row->plusRect(this).contains(point) && toggleRow(row))
				return true;
			if (row->validatorWarningIconRect(this).contains(point)) {
				jumpToNextHiddenValidatorIssue(false, row);
				return true;
			}
			if (row->validatorErrorIconRect(this).contains(point)) {
				jumpToNextHiddenValidatorIssue(true, row);
				return true;
			}
		}

		PropertyRow* rowToSelect = row;
		while (rowToSelect && !rowToSelect->isSelectable())
			rowToSelect = rowToSelect->parent();

		if (rowToSelect) {
			if (!shiftPressed || !multiSelectable()) {
				onRowSelected(std::vector<PropertyRow*>(1, rowToSelect), multiSelectable() && controlPressed, true);	
				lastSelectedRow_ = rowToSelect;
			}
			else {
				vector<PropertyRow*> rowsToSelect;

				rowsInBetween(&rowsToSelect, lastSelectedRow_, rowToSelect);
				onRowSelected(rowsToSelect, false, true);
			}
		}
	}

	PropertyTreeModel::UpdateLock lock = model()->lockUpdate();
	row = model()->root()->hit(this, point);
	if(row && !row->isRoot()){
		bool changed = false;
		if (row->widgetRect(this).contains(point)) {
			DragCheckBegin dragCheck = row->onMouseDragCheckBegin();
			if (dragCheck != DRAG_CHECK_IGNORE) {
				dragCheckValue_ = dragCheck == DRAG_CHECK_SET;
				dragCheckMode_ = true;
				changed = row->onMouseDragCheck(this, dragCheckValue_);
			}
		}
		
		if (!dragCheckMode_) {
			bool capture = row->onMouseDown(this, point, changed);
			if(!changed){
				if(capture)
					return true;
				else if(row->widgetRect(this).contains(point)){
					if(row->widgetPlacement() != PropertyRow::WIDGET_ICON)
						interruptDrag();
					PropertyActivationEvent e;
					e.force = false;
					e.tree = this;
					e.clickPoint = point;
					row->onActivate(e);
					return false;
				}
			}
		}
	}
	return false;
}

void PropertyTree::onMouseStill()
{
	if (capturedRow_) {
		PropertyDragEvent e;
		e.tree = this;
		e.pos = ui()->cursorPosition();
		e.start = pressPoint_;

		capturedRow_->onMouseStill(e);
		lastStillPosition_ = e.pos;
	}
}

void PropertyTree::onRowLMBUp(PropertyRow* row, const Rect& rowRect, Point point)
{
	// Row might be invalidated during onMouseUp() call, so acquire it locally and check reference counter afterwards.
	// If there is only one reference left (which is the shared pointer here), then the tree was changed
	// during the onMouseUp() and the row is not needed anymore.
	const yasli::SharedPtr<PropertyRow> sharedRow(row);
	row->onMouseUp(this, point);

	if (row->refCount() > 1) {
		if (!pointerMovedSincePress_ && (pressPoint_ - point).manhattanLength() < 1 && row->widgetRect(this).contains(point)) {
			PropertyActivationEvent e;
			e.tree = this;
			e.clickPoint = point;
			e.reason = e.REASON_RELEASE;
			row->onActivate(e);
		}
	}
}

void PropertyTree::onRowRMBDown(PropertyRow* row, const Rect& rowRect, Point point)
{
	SharedPtr<PropertyRow> handle = row;
	PropertyRow* menuRow = 0;

	if (row->isSelectable()){
		menuRow = row;
	}
	else{
		if (row->parent() && row->parent()->isSelectable())
			menuRow = row->parent();
	}

	if (menuRow) {
		onRowSelected(std::vector<PropertyRow*>(1, menuRow), false, true);	
		std::auto_ptr<property_tree::IMenu> menu(ui()->createMenu());
		clearMenuHandlers();
		if(onContextMenu(menuRow, *menu))
			menu->exec(point);
	}
}

void PropertyTree::expandParents(PropertyRow* row)
{
	bool hasChanges = false;
	typedef std::vector<PropertyRow*> Parents;
	Parents parents;
	PropertyRow* p = row->nonPulledParent()->parent();
	while(p){
		parents.push_back(p);
		p = p->parent();
	}
	Parents::iterator it;
	for(it = parents.begin(); it != parents.end(); ++it) {
		PropertyRow* row = *it;
		if (!row->expanded() || hasChanges) {
			row->_setExpanded(true);
			hasChanges = true;
		}
	}
	if (hasChanges) {
		updateValidatorIcons();
		updateHeights();
	}
}

void PropertyTree::expandAll()
{
	expandChildren(0);
}


void PropertyTree::expandChildren(PropertyRow* root)
{
	if(!root){
		root = model()->root();
		PropertyRow::iterator it;
		for (PropertyRows::iterator it = root->begin(); it != root->end(); ++it){
			PropertyRow* row = *it;
			row->setExpandedRecursive(this, true);
		}
		root->setLayoutChanged();
	}
	else
		root->setExpandedRecursive(this, true);

	for (PropertyRow* r = root; r != 0; r = r->parent())
		r->setLayoutChanged();

	updateHeights();
}

void PropertyTree::collapseAll()
{
	collapseChildren(0);
}

void PropertyTree::collapseChildren(PropertyRow* root)
{
	if(!root){
		root = model()->root();

		PropertyRow::iterator it;
		for (PropertyRows::iterator it = root->begin(); it != root->end(); ++it){
			PropertyRow* row = *it;
			row->setExpandedRecursive(this, false);
		}
	}
	else{
		root->setExpandedRecursive(this, false);
		PropertyRow* row = model()->focusedRow();
		while(row){
			if(root == row){
				model()->selectRow(row, true);
				break;
			}
			row = row->parent();
		}
	}

	for (PropertyRow* r = root; r != 0; r = r->parent())
		r->setLayoutChanged();

	updateHeights();
}


void PropertyTree::expandRow(PropertyRow* row, bool expanded, bool updateHeights)
{
	bool hasChanges = false;
	if (row->expanded() != expanded) {
		row->_setExpanded(expanded);
		hasChanges = true;
	}

	for (PropertyRow* r = row; r != 0; r = r->parent())
		r->setLayoutChanged();

    if(!row->expanded()){
		PropertyRow* f = model()->focusedRow();
		while(f){
			if(row == f){
				model()->selectRow(row, true);
				break;
			}
			f = f->parent();
		}
	}

	if (hasChanges)
		updateValidatorIcons();
	if (hasChanges && updateHeights)
		this->updateHeights();
}

Point PropertyTree::treeSize() const
{
	return size_ + (compact() ? Point(0,0) : Point(8, 8));
}

void PropertyTree::YASLI_SERIALIZE_METHOD(Archive& ar)
{
	model()->YASLI_SERIALIZE_METHOD(ar, this);

	if(ar.isInput()){
		updateHeights();
		ensureVisible(model()->focusedRow());
		updateAttachedPropertyTree(false);
		updateHeights();
		onSelected();
	}
}

void PropertyTree::ensureVisible(PropertyRow* row, bool update, bool considerChildren)
{
	if (row == 0)
		return;
	if(row->isRoot())
		return;

	expandParents(row);

	Rect rect = row->rect();
	if(rect.bottom() > area_.bottom() + offset_.y()){
		offset_.setY(max(0, rect.bottom() - area_.bottom()));
	}
	if(rect.top() < area_.top() + offset_.y()){
		offset_.setY(max(0, rect.top() - area_.top()));
	}
	updateScrollBar();
	if(update)
		repaint();
}

void PropertyTree::onRowSelected(const std::vector<PropertyRow*>& rows, bool addSelection, bool adjustCursorPos)
{
	for (size_t i = 0; i < rows.size(); ++i) {
		PropertyRow* row = rows[i];
		if(!row->isRoot()) {
			bool addRowToSelection = !(addSelection && row->selected() && model()->selection().size() > 1) || i > 0;
			bool exclusiveSelection = !addSelection && i == 0;
			model()->selectRow(row, addRowToSelection, exclusiveSelection);
		}
	}
	if (!rows.empty()) {
		ensureVisible(rows.back(), true, false);
		if(adjustCursorPos)
			cursorX_ = rows.back()->nonPulledParent()->horizontalIndex(this, rows.back());
	}
	updateAttachedPropertyTree(false);
	onSelected();
}

bool PropertyTree::attach(const yasli::Serializers& serializers)
{
	bool changed = false;
	if (attached_.size() != serializers.size())
		changed = true;
	else {
		for (size_t i = 0; i < serializers.size(); ++i) {
			if (attached_[i].serializer() != serializers[i]) {
				changed = true;
				break;
			}
		}
	}

	// We can't perform plain copying here, as it was before:
	//   attached_ = serializers;
	// ...as move forwarder calls copying constructor with non-const argument
	// which invokes second templated constructor of Serializer, which is not what we need.
	if (changed) {
		attached_.assign(serializers.begin(), serializers.end());
		model_->clearUndo();
	}

	revertNoninterrupting();

	return changed;
}

void PropertyTree::attach(const yasli::Serializer& serializer)
{
	if (attached_.size() != 1 || attached_[0].serializer() != serializer) {
		attached_.clear();
		attached_.push_back(yasli::Object(serializer));
		model_->clearUndo();
	}
	revert();
}

void PropertyTree::attach(const yasli::Object& object)
{
	attached_.clear();
	attached_.push_back(object);

	revert();
}

void PropertyTree::detach()
{
	capturedRow_ = nullptr;
	mouseOverRow_ = nullptr;
	lastSelectedRow_.release();
	_cancelWidget();
	attached_.clear();
	if (model_->root())
		model()->root()->clear();
	updateHeights();
	repaint();
}

void PropertyTree::_cancelWidget()
{
	// make sure that widget_ is null the moment widget is destroyed, so we
	// don't get focus callbacks to act on destroyed widget.
	std::auto_ptr<property_tree::InplaceWidget> widget(widget_.release());
}

int PropertyTree::revertObjects(vector<void*> objectAddresses)
{
	int result = 0;
	for (size_t i = 0; i < objectAddresses.size(); ++i) {
		if (revertObject(objectAddresses[i]))
			++result;
	}
	return result;
}

bool PropertyTree::revertObject(void* objectAddress)
{
	PropertyRow* row = model()->root()->findByAddress(objectAddress);
	if (row && row->isObject()) {
		// TODO:
		// revertObjectRow(row);
		return true;
	}
	return false;
}


void PropertyTree::revert()
{
	interruptDrag();
	_cancelWidget();
	capturedRow_ = 0;
	mouseOverRow_ = 0;
	lastSelectedRow_.release();

	if (!attached_.empty()) {
		validatorBlock_->clear();
		PropertyOArchive oa(model_.get(), model_->root(), validatorBlock_.get());
		oa.setOutlineMode(outlineMode_);
		if (archiveContext_)
			oa.setLastContext(archiveContext_);
		oa.setFilter(config_.filter);

		Objects::iterator it = attached_.begin();
		onAboutToSerialize(oa);
		(*it)(oa);
		onSerialized(oa);
		
		PropertyTreeModel model2(this);
		while(++it != attached_.end()){
			PropertyOArchive oa2(&model2, model2.root(), validatorBlock_.get());
			oa2.setOutlineMode(outlineMode_);
			oa2.setLastContext(archiveContext_);
			yasli::Context treeContext(oa2, this);
			oa2.setFilter(config_.filter);
			onAboutToSerialize(oa2);
			(*it)(oa2);
			onSerialized(oa2);
			model_->root()->intersect(model2.root());
		}
		//revertTime_ = int(timer.elapsed());

		if (attached_.size() != 1)
			validatorBlock_->clear();
		applyValidation();
	}
	else
		model_->clear();

	if (filterMode_) {
		if (model_->root())
			model_->root()->updateLabel(this, 0, false);
		resetFilter();
	}
	else {
		updateHeights();
	}

	repaint();
	updateAttachedPropertyTree(true);
}

struct ValidatorVisitor
{
	ValidatorVisitor(ValidatorBlock* validator)
	: validator_(validator)
	{
	}

	ScanResult operator()(PropertyRow* row, PropertyTree* tree, int)
	{
		const void* rowHandle = row->searchHandle();
		int index = 0;
		int count = 0;
		if (validator_->findHandleEntries(&index, &count, rowHandle, row->typeId()))
		{
			validator_->markAsUsed(index, count);
			if (row->setValidatorEntry(index, count))
				row->setLabelChanged();
		}
		else
		{
			if (row->setValidatorEntry(0, 0))
				row->setLabelChanged();
		}

		return SCAN_CHILDREN_SIBLINGS;
	}

protected:
	ValidatorBlock* validator_;
};

void PropertyTree::applyValidation()
{
	if (!validatorBlock_->isEnabled())
		return;

	ValidatorVisitor visitor(validatorBlock_.get());
	model()->root()->scanChildren(visitor, this);

	int rootFirst = 0;
	int rootCount = 0;
	// Gather all the items with unknown handle/type pair at root level.
	validatorBlock_->mergeUnusedItemsWithRootItems(&rootFirst, &rootCount, model()->root()->searchHandle(), model()->root()->typeId());
	model()->root()->setValidatorEntry(rootFirst, rootCount);
	model()->root()->setLabelChanged();

	updateValidatorIcons();
}

void PropertyTree::revertNoninterrupting()
{
	if (!capturedRow_)
		revert();
}

void PropertyTree::apply(bool continuous)
{
	//QElapsedTimer timer;
	//timer.start();

	if (!attached_.empty())
	{
		Objects::iterator it;
		PropertyIArchive ia(model_.get(), model_->root());
		for(it = attached_.begin(); it != attached_.end(); ++it)
		{
			PropertyRow* row = selectedRow();
			ia.setModifiedRowName(row ? row->name() : nullptr);
			ia.setLastContext(archiveContext_);
 			yasli::Context treeContext(ia, this);
 			ia.setFilter(config_.filter);
			onAboutToSerialize(ia);
			(*it)(ia);
			onSerialized(ia);
		}
	}

	if (continuous)
		onContinuousChange();
	else
		onChanged();
	//applyTime_ = timer.elapsed();
}

void PropertyTree::applyInplaceEditor()
{
	if (widget_.get())
		widget_->commit();
}

bool PropertyTree::spawnWidget(PropertyRow* row, bool ignoreReadOnly)
{
	if(!widget_.get() || widgetRow_ != row){
		interruptDrag();
		setWidget(0, 0);
		property_tree::InplaceWidget* newWidget = 0;
		if ((ignoreReadOnly && row->userReadOnlyRecurse()) || !row->userReadOnly())
			newWidget = row->createWidget(this);
		setWidget(newWidget, row);
		return newWidget != 0;
	}
	return false;
}

void PropertyTree::addMenuHandler(PropertyRowMenuHandler* handler)
{
	menuHandlers_.push_back(handler);
}

void PropertyTree::clearMenuHandlers()
{
	for (size_t i = 0; i < menuHandlers_.size(); ++i)
	{
		PropertyRowMenuHandler* handler = menuHandlers_[i];
		delete handler;
	}
	menuHandlers_.clear();
}

static yasli::string quoteIfNeeded(const char* str)
{
	if (!str)
		return yasli::string();
	if (strchr(str, ' ') != 0) {
		yasli::string result;
		result = "\"";
		result += str;
		result += "\"";
		return result;
	}
	else {
		return yasli::string(str);
	}
}

bool PropertyTree::onContextMenu(PropertyRow* r, IMenu& menu)
{
	SharedPtr<PropertyRow> row(r);
	PropertyTreeMenuHandler* handler = new PropertyTreeMenuHandler();
	addMenuHandler(handler);
	handler->tree = this;
	handler->row = row;

	PropertyRow::iterator it;
	for(it = row->begin(); it != row->end(); ++it){
		PropertyRow* child = *it;
		if(child->isContainer() && child->pulledUp())
			child->onContextMenu(menu, this);
	}
	row->onContextMenu(menu, this);
	if(config_.undoEnabled){
		if(!menu.isEmpty())
			menu.addSeparator();
		menu.addAction("Undo", "Ctrl+Z", model()->canUndo() ? 0 : MENU_DISABLED, handler, &PropertyTreeMenuHandler::onMenuUndo);
	}
	if(!menu.isEmpty())
		menu.addSeparator();

	menu.addAction("Copy", "Ctrl+C", 0, handler, &PropertyTreeMenuHandler::onMenuCopy);

	if(!row->userReadOnly()){
		menu.addAction("Paste", "Ctrl+V", canBePasted(row) ? 0 : MENU_DISABLED, handler, &PropertyTreeMenuHandler::onMenuPaste);
	}

	if (model()->root() && !model()->root()->userReadOnly()) {
		PropertyTreeMenuHandler* rootHandler = new PropertyTreeMenuHandler();
		rootHandler->tree = this;
		rootHandler->row = model()->root();
		menu.addSeparator();
		menu.addAction("Copy All", "", 0, rootHandler, &PropertyTreeMenuHandler::onMenuCopy);
		menu.addAction("Paste All", "", canBePasted(model()->root()) ? 0 : MENU_DISABLED, rootHandler, &PropertyTreeMenuHandler::onMenuPaste);
		addMenuHandler(rootHandler);
	}
	menu.addSeparator();

	menu.addAction("Filter...", "Ctrl+F", 0, handler, &PropertyTreeMenuHandler::onMenuFilter);
	IMenu* filter = menu.addMenu("Filter by");
	{
		yasli::string nameFilter = "#";
		nameFilter += quoteIfNeeded(row->labelUndecorated());
		handler->filterName = nameFilter;
		filter->addAction((yasli::string("Name:\t") + nameFilter).c_str(), 0, handler, &PropertyTreeMenuHandler::onMenuFilterByName);

		yasli::string valueFilter = "=";
		valueFilter += quoteIfNeeded(row->valueAsString().c_str());
		handler->filterValue = valueFilter;
		filter->addAction((yasli::string("Value:\t") + valueFilter).c_str(), 0, handler, &PropertyTreeMenuHandler::onMenuFilterByValue);

		yasli::string typeFilter = ":";
		typeFilter += quoteIfNeeded(row->typeNameForFilter(this));
		handler->filterType = typeFilter;
		filter->addAction((yasli::string("Type:\t") + typeFilter).c_str(), 0, handler, &PropertyTreeMenuHandler::onMenuFilterByType);
	}

	// menu.addSeparator();
	// menu.addAction(TRANSLATE("Decompose"), row).connect(this, &PropertyTree::onRowMenuDecompose);
	return true;
}

void PropertyTree::onRowMouseMove(PropertyRow* row, const Rect& rowRect, Point point, Modifier modifiers)
{
	PropertyDragEvent e;
	e.tree = this;
	e.modifier = modifiers;
	e.pos = point;
	e.start = pressPoint_;
	e.totalDelta = pressDelta_;
	row->onMouseDrag(e);
	repaint();
}

struct DecomposeProxy
{
	DecomposeProxy(SharedPtr<PropertyRow>& row) : row(row) {}
	
	void YASLI_SERIALIZE_METHOD(yasli::Archive& ar)
	{
		ar(row, "row", "Row");
	}

	SharedPtr<PropertyRow>& row;
};

void PropertyTree::onRowMenuDecompose(PropertyRow* row)
{
  // SharedPtr<PropertyRow> clonedRow = row->clone();
  // DecomposeProxy proxy(clonedRow);
  // edit(Serializer(proxy), 0, IMMEDIATE_UPDATE, this);
}

void PropertyTree::onModelUpdated(const PropertyRows& rows, bool needApply)
{
	_cancelWidget();

	if(config_.immediateUpdate){
		if (needApply)
			apply();

		if(autoRevert_)
			revert();
		else {
			updateHeights();
			updateAttachedPropertyTree(true);
		}
	}
	else {
		repaint();
	}
}

void PropertyTree::onModelPushUndo(PropertyTreeOperator* op, bool* handled)
{
	onPushUndo();
}

void PropertyTree::setWidget(property_tree::InplaceWidget* widget, PropertyRow* widgetRow)
{
	_cancelWidget();
	widget_.reset(widget);
	widgetRow_ = widgetRow;
	model()->dismissUpdate();
	if(widget_.get()){
		YASLI_ASSERT(widget_->actualWidget());
		_arrangeChildren();
		YASLI_ASSERT(widget_.get());
		if(widget_.get()) // Fix for intermittent crash when editing number fields. It is possible that widget will be destroyed by _arrangeChildren() but the root cause of this issue is not clear.
		{
			widget_->showPopup();
		}
		repaint();
	}
}

void PropertyTree::setExpandLevels(int levels)
{
	config_.expandLevels = levels;
    model()->setExpandLevels(levels);
}

PropertyRow* PropertyTree::selectedRow()
{
    const PropertyTreeModel::Selection &sel = model()->selection();
    if(sel.empty())
        return 0;
    return model()->rowFromPath(sel.front());
}

bool PropertyTree::getSelectedObject(yasli::Object* object)
{
	const PropertyTreeModel::Selection &sel = model()->selection();
	if(sel.empty())
		return 0;
	PropertyRow* row = model()->rowFromPath(sel.front());
	while (row && !row->isObject())
		row = row->parent();
	if (!row)
		return false;

	if (row->isObject()) {
		if (PropertyRowObject* obj = static_cast<PropertyRowObject*>(row)) {
			*object = obj->object();
			return true;
		}
	}
	return false;
}

bool PropertyTree::setSelectedRow(PropertyRow* row)
{
	TreeSelection sel;
	if(row)
		sel.push_back(model()->pathFromRow(row));
	if (model()->selection() != sel) {
		model()->setSelection(sel);
		if (row)
			ensureVisible(row);
		updateAttachedPropertyTree(false);
		repaint();
		return true;
	}
	return false;
}

int PropertyTree::selectedRowCount() const
{
	return (int)model()->selection().size();
}

PropertyRow* PropertyTree::selectedRowByIndex(int index)
{
	const PropertyTreeModel::Selection &sel = model()->selection();
	if (size_t(index) >= sel.size())
		return 0;

	return model()->rowFromPath(sel[index]);
}


bool PropertyTree::selectByAddress(const void* addr, bool keepSelectionIfChildSelected)
{
	return selectByAddresses(vector<const void*>(1, addr), keepSelectionIfChildSelected);
}

bool PropertyTree::selectByAddresses(const vector<const void*>& addresses, bool keepSelectionIfChildSelected)
{
	return selectByAddresses(&addresses.front(), addresses.size(), keepSelectionIfChildSelected);
}



bool PropertyTree::selectByAddresses(const void* const* addresses, size_t addressCount, bool keepSelectionIfChildSelected)
{
	bool result = false;
	if (model()->root()) {
		bool keepSelection = false;
		vector<PropertyRow*> rows;
		for (size_t i = 0; i < addressCount; ++i) {
			const void* addr = addresses[i];
			PropertyRow* row = model()->root()->findByAddress(addr);

			if (keepSelectionIfChildSelected && row && !model()->selection().empty()) {
				keepSelection = true;
				TreeSelection::const_iterator it;
				for(it = model()->selection().begin(); it != model()->selection().end(); ++it){
					PropertyRow* selectedRow = model()->rowFromPath(*it);
					if (!selectedRow)
						continue;
					if (!selectedRow->isChildOf(row)){
						keepSelection = false;
						break;
					}
				}
			}

			if (row)
				rows.push_back(row);
		}

		if (!keepSelection) {
			TreeSelection sel;
			for (size_t j = 0; j < rows.size(); ++j) {
				PropertyRow* row = rows[j];
				if(row)
					sel.push_back(model()->pathFromRow(row));
			}
			if (model()->selection() != sel) {
				model()->setSelection(sel);
				if (!rows.empty())
					ensureVisible(rows.back());
				repaint();
				result = true;
				if (attachedPropertyTree_)
					updateAttachedPropertyTree(false);
			}
		}
	}
	return result;
}

void PropertyTree::setUndoEnabled(bool enabled, bool full)
{
	config_.undoEnabled = enabled;
	config_.fullUndo = full;
    model()->setUndoEnabled(enabled);
    model()->setFullUndo(full);
}

void PropertyTree::attachPropertyTree(PropertyTree* propertyTree) 
{ 
	// TODO:
	// if(attachedPropertyTree_)
	// 	disconnect(attachedPropertyTree_, SIGNAL(signalChanged()), this, SLOT(onAttachedTreeChanged()));
	attachedPropertyTree_ = propertyTree; 
	//if (attachedPropertyTree_)
	//	connect(attachedPropertyTree_, SIGNAL(signalChanged()), this, SLOT(onAttachedTreeChanged()));
	updateAttachedPropertyTree(true); 
}

void PropertyTree::detachPropertyTree()
{
	attachPropertyTree(0);
}

void PropertyTree::setAutoHideAttachedPropertyTree(bool autoHide)
{
	autoHideAttachedPropertyTree_ = autoHide;
}

void PropertyTree::getSelectionSerializers(yasli::Serializers* serializers)
{
	TreeSelection::const_iterator i;
	for(i = model()->selection().begin(); i != model()->selection().end(); ++i){
		PropertyRow* row = model()->rowFromPath(*i);
		if (!row)
			continue;


		while(row && ((row->pulledUp() || row->pulledBefore()) || row->isLeaf())) {
			row = row->parent();
		}
		if (outlineMode_) {
			PropertyRow* topmostContainerElement = 0;
			PropertyRow* r = row;
			while (r && r->parent()) {
				if (r->parent()->isContainer())
					topmostContainerElement = r;
				r = r->parent();
			}
			if (topmostContainerElement != 0)
				row = topmostContainerElement;
		}
		Serializer ser = row->serializer();

		if (ser)
			serializers->push_back(ser);
	}
}

void PropertyTree::updateAttachedPropertyTree(bool revert)
{
	if(attachedPropertyTree_) {
 		Serializers serializers;
 		getSelectionSerializers(&serializers);
 		if (!attachedPropertyTree_->attach(serializers) && revert)
			attachedPropertyTree_->revert();
 	}
}


void PropertyTree::RowFilter::parse(const char* filter)
{
	for (int i = 0; i < NUM_TYPES; ++i) {
		start[i].clear();
		substrings[i].clear();
		tillEnd[i] = false;
	}

	YASLI_ESCAPE(filter != 0, return);

	vector<char> filterBuf(filter, filter + strlen(filter) + 1);
	for (size_t i = 0; i < filterBuf.size(); ++i)
		filterBuf[i] = tolower(filterBuf[i]);

	const char* str = &filterBuf[0];

	Type type = NAME_VALUE;
	while (true)
	{
		bool fromStart = false;
		while (*str == '^') {
			fromStart = true;
			++str;
		}

		const char* tokenStart = str;
		
		if (*str == '\"')
		{
			++str;
			while(*str != '\0' && *str != '\"')
				++str;
		}
		else
		{
			while (*str != '\0' && *str != ' ' && *str != '*' && *str != '=' && *str != ':' && *str != '#')
					++str;
		}
		if (str != tokenStart) {
			if (*tokenStart == '\"' && *str == '\"') {
				start[type].assign(tokenStart + 1, str);
				tillEnd[type] = true;
				++str;
			}
			else
			{
				if (fromStart)
					start[type].assign(tokenStart, str);
				else
					substrings[type].push_back(yasli::string(tokenStart, str));
			}
		}
		while (*str == ' ')
			++str;
		if (*str == '#') {
			type = NAME;
			++str;
		}
		else if (*str == '=') {
			type = VALUE;
			++str;
		}
		else if(*str == ':') {
			type = TYPE;
			++str;
		}
		else if (*str == '\0')
			break;
	}
}

bool PropertyTree::RowFilter::match(const char* textOriginal, Type type, size_t* matchStart, size_t* matchEnd) const
{
	YASLI_ESCAPE(textOriginal, return false);

	char* text;
	{
		size_t textLen = strlen(textOriginal);
        text = (char*)alloca((textLen + 1));
		memcpy(text, textOriginal, (textLen + 1));
		for (char* p = text; *p; ++p)
			*p = tolower(*p);
	}
	
	const yasli::string &start = this->start[type];
	if (tillEnd[type]){
		if (start == text) {
			if (matchStart)
				*matchStart = 0;
			if (matchEnd)
				*matchEnd = start.size();
			return true;
		}
		else
			return false;
	}

	const vector<yasli::string> &substrings = this->substrings[type];

	const char* startPos = text;

	if (matchStart)
		*matchStart = 0;
	if (matchEnd)
		*matchEnd = 0;
	if (!start.empty()) {
		if (strncmp(text, start.c_str(), start.size()) != 0){
            //_freea(text);
			return false;
		}
		if (matchEnd)
			*matchEnd = start.size();
		startPos += start.size();
	}

	size_t numSubstrings = substrings.size();
	for (size_t i = 0; i < numSubstrings; ++i) {
		const char* substr = strstr(startPos, substrings[i].c_str());
		if (!substr){
			return false;
		}
		startPos += substrings[i].size();
		if (matchStart && i == 0 && start.empty()) {
			*matchStart = substr - text;
		}
		if (matchEnd)
			*matchEnd = substr - text + substrings[i].size();
	}
	return true;
}

PropertyRow* PropertyTree::rowByPoint(const Point& pt)
{
	if (!model_->root())
		return 0;
	if (!area_.contains(pt))
		return 0;
  return model_->root()->hit(this, pointToRootSpace(pt));
}

Point PropertyTree::pointToRootSpace(const Point& point) const
{
	return Point(point.x() + offset_.x(), point.y() + offset_.y());
}

Point PropertyTree::pointFromRootSpace(const Point& point) const
{
	return Point(point.x() - offset_.x() + area_.left(), point.y() - offset_.y() + area_.top());
}

bool PropertyTree::toggleRow(PropertyRow* row)
{
	if(!row->canBeToggled(this))
		return false;
	expandRow(row, !row->expanded());
	updateHeights();
	return true;
}

bool PropertyTree::_isCapturedRow(const PropertyRow* row) const
{
	return capturedRow_ == row;
}

void PropertyTree::setValueColumnWidth(float valueColumnWidth) 
{
	if (style_->valueColumnWidth != valueColumnWidth)
	{
		style_->valueColumnWidth = valueColumnWidth; 
		updateHeights(false);
		repaint();
	}
}

void PropertyTree::setArchiveContext(yasli::Context* lastContext)
{
	archiveContext_ = lastContext;
}

PropertyTree::PropertyTree(const PropertyTree&)
{
}


PropertyTree& PropertyTree::operator=(const PropertyTree&)
{
	return *this;
}

void PropertyTree::onAttachedTreeChanged()
{
	revert();
}

Point PropertyTree::_toWidget(Point point) const
{
	return Point(point.x() - offset_.x(), point.y() - offset_.y());
}

struct ValidatorIconVisitor
{
	ScanResult operator()(PropertyRow* row, PropertyTree* tree, int)
	{
		row->resetValidatorIcons();
		if (row->validatorCount()) {
			bool hasErrors = false;
			bool hasWarnings = false;
			if (const ValidatorEntry* validatorEntries = tree->_validatorBlock()->getEntry(row->validatorIndex(), row->validatorCount())) {
				for (int i = 0; i < row->validatorCount(); ++i) {
					const ValidatorEntry* validatorEntry = validatorEntries + i;
					if (validatorEntry->type == VALIDATOR_ENTRY_ERROR)
						hasErrors = true;
					else if (validatorEntry->type == VALIDATOR_ENTRY_WARNING)
						hasWarnings = true;
				}
			}

			if (hasErrors || hasWarnings)
			{
				PropertyRow* lastClosedParent = 0;
				PropertyRow* current = row->parent();
				bool lastWasPulled = row->pulledUp() || row->pulledBefore();
				while (current && current->parent()) {
					if (!current->expanded() && !lastWasPulled && current->visible(tree))
						lastClosedParent = current;
					lastWasPulled = current->pulledUp() || current->pulledBefore();
					current = current->parent();
				}
				if (lastClosedParent)
					lastClosedParent->addValidatorIcons(hasWarnings, hasErrors);
			}
		}
		return SCAN_CHILDREN_SIBLINGS;
	}
};

void PropertyTree::updateValidatorIcons()
{
	if (!validatorBlock_->isEnabled())
		return;
	ValidatorIconVisitor op;
	model()->root()->scanChildren(op, this);
	model()->root()->setLabelChangedToChildren();
}

void PropertyTree::setDefaultTreeStyle(const PropertyTreeStyle& treeStyle)
{
	defaultTreeStyle_ = treeStyle;
}

void PropertyTree::setTreeStyle(const PropertyTreeStyle& style)
{
	*style_ = style;
	updateHeights(true);
}

void PropertyTree::setPackCheckboxes(bool pack)
{
	style_->packCheckboxes = pack;
	updateHeights(true);
}

bool PropertyTree::packCheckboxes() const
{
	return style_->packCheckboxes;
}

void PropertyTree::setCompact(bool compact)
{
	style_->compact = compact;
	repaint();
}

bool PropertyTree::compact() const
{
	return style_->compact;
}

void PropertyTree::setRowSpacing(float rowSpacing)
{
	style_->rowSpacing = rowSpacing;
}

float PropertyTree::rowSpacing() const
{
	return style_->rowSpacing;
}

float PropertyTree::valueColumnWidth() const
{
	return style_->valueColumnWidth;
}

void PropertyTree::setFullRowMode(bool fullRowMode)
{
	style_->fullRowMode = fullRowMode;
	repaint();
}

bool PropertyTree::fullRowMode() const
{
	return style_->fullRowMode;
}

bool PropertyTree::containsWarnings() const
{
	return validatorBlock_->containsWarnings();
}

bool PropertyTree::containsErrors() const
{
	return validatorBlock_->containsErrors();
}

void PropertyTree::focusFirstError()
{
	jumpToNextHiddenValidatorIssue(true, model()->root());
}

Rect PropertyTree::getPropertySplitterRect() const
{
	int splitterStart = propertySplitterPos_ - treeStyle().propertySplitterHalfWidth;
	int splitterWidth = 1 + treeStyle().propertySplitterHalfWidth * 2;
	return Rect(splitterStart, area_.top(), splitterWidth, area_.height());
}

void PropertyTree::setPropertySplitterPos(int pos)
{
	if (pos == propertySplitterPos_)
		return;

	const int splitterBorder = 2 * defaultRowHeight_;
	propertySplitterPos_ = pos;
	if (area_.width() > 0)
		propertySplitterPos_ = min(area_.right() - splitterBorder, max(area_.left() + splitterBorder, pos));
	updateHeights();
}

// vim:ts=4 sw=4:


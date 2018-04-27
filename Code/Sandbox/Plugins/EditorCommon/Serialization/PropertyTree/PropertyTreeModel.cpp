/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include "PropertyTreeModel.h"
#include "PropertyTree.h"
#include "Serialization.h"
#include <CrySerialization/yasli/ClassFactory.h>
#include <CrySerialization/yasli/Callback.h>

PropertyTreeModel::PropertyTreeModel(PropertyTree* tree)
: tree_(tree)
, expandLevels_(0)
, undoEnabled_(true)
, fullUndo_(false)
{
	clear();
}

PropertyTreeModel::~PropertyTreeModel()
{
	root_ = 0;
	defaultTypes_.clear();
	defaultTypesPoly_.clear();
}

TreePath PropertyTreeModel::pathFromRow(PropertyRow* row)
{
	TreePath result;
	if(row)
		while(row->parent()){
            int childIndex = row->parent()->childIndex(row);
            YASLI_ESCAPE(childIndex >= 0, return TreePath());
			result.insert(result.begin(), childIndex);
			row = row->parent();
		}
		return result;
}

void PropertyTreeModel::selectRow(PropertyRow* row, bool select, bool exclusive)
{
	if(exclusive)
		deselectAll();

	row->setSelected(select);

	Selection::iterator it = std::find(selection_.begin(), selection_.end(), pathFromRow(row));
	if(select){
		if(it == selection_.end())
			selection_.push_back(pathFromRow(row));
		setFocusedRow(row);
	}
	else if(it != selection_.end()){
		PropertyRow* it_row = rowFromPath(*it);
		YASLI_ASSERT(it_row->refCount() > 0 && it_row->refCount() < 0xFFFF);
		selection_.erase(it);
	}
}

void PropertyTreeModel::deselectAll()
{
	Selection::iterator it;
	for(it = selection_.begin(); it != selection_.end(); ++it){
		PropertyRow* row = rowFromPath(*it);
		row->setSelected(false);
	}
	selection_.clear();
}

PropertyRow* PropertyTreeModel::rowFromPath(const TreePath& path)
{
	PropertyRow* row = root();
	if (!root())
		return 0;
	TreePath::const_iterator it;
	for(it = path.begin(); it != path.end(); ++it){
		int index = it->index;
		if(index < int(row->count()) && index >= 0){
			PropertyRow* nextRow = row->childByIndex(index);
			if(!nextRow)
				return row;
			else
				row = nextRow;
		}
		else
			return row;
	}
	return row;
}

void PropertyTreeModel::setSelection(const Selection& selection)
{
	deselectAll();
	Selection::const_iterator it;
	for(it = selection.begin(); it != selection.end(); ++it){
		const TreePath& path = *it;
		PropertyRow* row = rowFromPath(path);
		if(row)
			selectRow(row, true, false);
	}
}

void PropertyTreeModel::clear()
{
	if(root_)
		root_->clear();
	root_ = 0;
	setRoot(new PropertyRow());
	root_->setNames("", "root", "rootType");
	selection_.clear();
}

void PropertyTreeModel::onUpdated(const PropertyRows& rows, bool needApply)
{
    signalUpdated(rows, needApply);
}

void PropertyTreeModel::applyOperator(PropertyTreeOperator* op, bool createRedo)
{
    YASLI_ESCAPE(op, return);
    PropertyRow *dest = rowFromPath(op->path_);
    YASLI_ESCAPE(dest && "Unable to apply operator!", return);
    if(op->type_ == PropertyTreeOperator::NONE)
        return;
    YASLI_ESCAPE(op->row_, return);
    if(dest->parent())
        dest->parent()->replaceAndPreserveState(dest, op->row_, this);
    else{
        op->row_->assignRowProperties(root_);
        root_ = op->row_;
    }
    PropertyRow* newRow = op->row_;
    op->row_ = 0;
    rowChanged(newRow);
    // TODO: redo
}

void PropertyTreeModel::undo()
{
    YASLI_ESCAPE(!undoOperators_.empty(), return);
    applyOperator(&undoOperators_.back(), true);
    undoOperators_.pop_back();
}

void PropertyTreeModel::clearUndo()
{
	undoOperators_.clear();
}

PropertyTreeModel::UpdateLock PropertyTreeModel::lockUpdate()
{
	if(updateLock_)
		return updateLock_;
	else {
		UpdateLock lock = new PropertyTreeModel::LockedUpdate(this);;
		updateLock_ = lock;
		lock->release();
		return lock;
	}
}

void PropertyTreeModel::dismissUpdate()
{
	if(updateLock_)
		updateLock_->dismissUpdate();
}

void PropertyTreeModel::requestUpdate(const PropertyRows& rows, bool apply)
{
	if(updateLock_)
		updateLock_->requestUpdate(rows, apply);
	else
		onUpdated(rows, apply);
}

struct RowObtainer {
	RowObtainer(std::vector<char>& states) : states_(states) {}
	ScanResult operator()(PropertyRow* row)
	{
		states_.push_back(row->expanded() ? 1 : 0);
		return row->expanded() ? SCAN_CHILDREN_SIBLINGS : SCAN_SIBLINGS;
	}
protected:
	std::vector<char>& states_;
};

struct RowExpander {
	RowExpander(const std::vector<char>& states) : states_(states), index_(0) {}
	ScanResult operator()(PropertyRow* row, PropertyTree* tree, int index)
	{
		if(size_t(index_) >= states_.size())
			return SCAN_FINISHED;

		if(states_[index_++]){
			if(row->canBeToggled(tree))
				row->_setExpanded(true);
			return SCAN_CHILDREN_SIBLINGS;
		}
		else{
			row->_setExpanded(false);
			return SCAN_SIBLINGS;
		}
	}
protected:
	int index_;
	const std::vector<char>& states_;
};

void PropertyTreeModel::YASLI_SERIALIZE_METHOD(Archive& ar, PropertyTree* tree)
{
	ar(focusedRow_, "focusedRow", 0);		
	ar(selection_, "selection", 0);

	if (root()) {
		std::vector<char> expanded;
		if(ar.isOutput()) {
			RowObtainer op(expanded);
			root()->scanChildren(op);
		}
		ar(expanded, "expanded", 0);
		if(ar.isInput()){
			Selection sel = selection_;
			setSelection(sel);
			RowExpander op(expanded);
			root()->scanChildren(op, tree);
			root()->setLayoutChanged();
			root()->setLayoutChangedToChildren();
		}
	}
}

void PropertyTreeModel::pushUndo(const PropertyTreeOperator& op)
{
    PropertyTreeOperator oper = op;
    bool handled = false;
    signalPushUndo(&oper, &handled);
    if(!handled && oper.row_ != 0)
        undoOperators_.push_back(oper);
}

void PropertyTreeModel::rowAboutToBeChanged(PropertyRow* row)
{
    YASLI_ESCAPE(row, return);
    if(fullUndo_){
        if(undoEnabled_){
            SharedPtr<PropertyRow> clonedRow = root()->clone(constStrings());
            clonedRow->assignRowState(*root(), true);
            pushUndo(PropertyTreeOperator(TreePath(), clonedRow));
        }
        else{
            pushUndo(PropertyTreeOperator(TreePath(), 0));
        }
    }
    else{
        if(undoEnabled_){
			SharedPtr<PropertyRow> clonedRow = row->clone(constStrings());
            clonedRow->assignRowState(*row, true);
            pushUndo(PropertyTreeOperator(pathFromRow(row), clonedRow));
        }
        else{
            pushUndo(PropertyTreeOperator(pathFromRow(row), 0));
        }
    }
}

void PropertyTreeModel::callRowCallback(PropertyRow* row)
{
	PropertyRow* current = row;
	while (true) {
		yasli::CallbackInterface* callback = current->callback();
		if (callback) {
			auto applyFunc = [=](void* arg, const TypeID& type) {
				current->assignToByPointer(arg, callback->type());
			};
			callback->call(applyFunc);
			return;
		}
		current = current->parent();
		if (current)
			current->handleChildrenChange();
		else
			break;
	}
}

void PropertyTreeModel::rowChanged(PropertyRow* row, bool apply)
{
	callRowCallback(row);

	YASLI_ESCAPE(row, return);
	row->setLabelChanged();
	row->setLayoutChanged();

	PropertyRow* parentObj = row;
	while (parentObj->parent() && !parentObj->isObject())
		parentObj = parentObj->parent();

	row->setMultiValue(false);

	PropertyRows rows;
	rows.push_back(parentObj);
	requestUpdate(rows, apply);
}

bool PropertyTreeModel::defaultTypeRegistered(const char* typeName) const
{
	return defaultTypes_.find(typeName) != defaultTypes_.end();
}

void PropertyTreeModel::addDefaultType(PropertyRow* row, const char* typeName)
{
	YASLI_ESCAPE(typeName != 0, return);
	defaultTypes_[typeName] = row;
}

PropertyRow* PropertyTreeModel::defaultType(const char* typeName) const
{
	DefaultTypes::const_iterator it = defaultTypes_.find(typeName);
	YASLI_ESCAPE(it != defaultTypes_.end(), return 0);
	return it->second;
}

void PropertyTreeModel::addDefaultType(const TypeID& type, const PropertyDefaultDerivedTypeValue& value)
{
	YASLI_ASSERT(type != TypeID());

	BaseClass& base = defaultTypesPoly_[type];
	for (DerivedTypes::iterator it = base.types.begin(); it != base.types.end(); ++it){
		if (it->registeredName == value.registeredName) {
			YASLI_ASSERT(it->root == 0);
			*it = value;
			return;
		}
	}

	base.types.push_back(value);
	base.strings.push_back(value.label.c_str());
}

const PropertyDefaultDerivedTypeValue* PropertyTreeModel::defaultType(const TypeID& baseType, int derivedIndex) const
{
	DefaultTypesPoly::const_iterator it = defaultTypesPoly_.find(baseType);
	YASLI_ESCAPE(it != defaultTypesPoly_.end(), return 0);
	const BaseClass& base = it->second;
	YASLI_ESCAPE(size_t(derivedIndex) < base.types.size(), return 0);
	return &base.types[derivedIndex];
}

bool PropertyTreeModel::defaultTypeRegistered(const TypeID& baseType, const char* derivedRegisteredName) const
{
	if (!derivedRegisteredName)
		derivedRegisteredName = "";
	DefaultTypesPoly::const_iterator it = defaultTypesPoly_.find(baseType);

	if (it == defaultTypesPoly_.end())
		return false;

	const BaseClass& base = it->second;
	DerivedTypes::const_iterator dit;
	for (dit = base.types.begin(); dit != base.types.end(); ++dit){
		if (dit->registeredName == derivedRegisteredName)
			return true;
	}
	return false;
}

const yasli::StringList& PropertyTreeModel::typeStringList(const yasli::TypeID& baseType) const
{
	DefaultTypesPoly::const_iterator it = defaultTypesPoly_.find(baseType);

	static yasli::StringList empty;
	YASLI_ESCAPE(it != defaultTypesPoly_.end(), return empty);
	const BaseClass& base = it->second;
	return base.strings;
}

void PropertyTreeModel::signalUpdated(const PropertyRows& rows, bool needApply)
{
	tree_->onModelUpdated(rows, needApply);
}

void PropertyTreeModel::signalPushUndo(PropertyTreeOperator* op, bool* result)
{
	tree_->onModelPushUndo(op, result);
}

PropertyRow* PropertyTreeModel::getNextFocusableSibling(PropertyRow* row, bool pulled)
{
	PropertyRow* parent = row->parent();
	if (parent)
	{
		PropertyRow* selected = nullptr;
		for (PropertyRow* child : *parent)
		{
			if (!child->visible(tree_) || pulled != child->pulledUp())
				continue;

			if (selected)
				return child;

			if (child == row)
				selected = child;
		}
	}
	return nullptr;
}

PropertyRow* PropertyTreeModel::getPrevFocusableSibling(PropertyRow* row, bool pulled)
{
	PropertyRow* parent = row->parent();
	if (parent)
	{
		PropertyRow* selected = nullptr;
		for (PropertyRow* child : *parent)
		{
			if (!child->visible(tree_) || pulled != child->pulledUp())
				continue;

			if (child == row)
				return selected;

			selected = child;
		}
	}
	return nullptr;
}

PropertyRow* PropertyTreeModel::getFirstFocusableChild(PropertyRow* row, bool pulled)
{
	if (!row)
		row = root();

	for (PropertyRow* child : *row)
	{
		if (child->visible(tree_) && pulled == child->pulledUp())
			return child;
	}
	return nullptr;
}

PropertyRow* PropertyTreeModel::getLastFocusableChild(PropertyRow* row, bool pulled)
{
	if (!row)
		row = root();

	for (int i = row->count() -1; i >= 0; --i)
	{
		PropertyRow* child = row->childByIndex(i);
		if (child->visible(tree_))
			return getLastFocusableChild(child, pulled);
	}

	return row;
}

PropertyRow* PropertyTreeModel::getNextFocusableRow()
{
	PropertyRow* focused = focusedRow();
	if (focused)
	{
		PropertyRow* row = getFirstFocusableChild(focused, true);
		if (row)
			return row;
		row = getFirstFocusableChild(focused, false);
		if (row)
			return row;
		
		row = getNextFocusableSibling(focused, true);
		if (row)
			return row;

		row = getNextFocusableSibling(focused, false);
		if (row)
			return row;

		if (focused->parent())
		{
			row = getNextFocusableSibling(focused->parent(), true);
			if (row)
				return row;

			row = getNextFocusableSibling(focused->parent(), false);
			if (row)
				return row;
		}
	}

	return nullptr;
}


PropertyRow* PropertyTreeModel::getPrevFocusableRow()
{
	PropertyRow* focused = focusedRow();
	if (focused)
	{
		PropertyRow* row = getPrevFocusableSibling(focused, false);
		if (!row)
			row = getPrevFocusableSibling(focused, true);

		if (!row)
			return focused->parent();
		
		if (row)
			row = getLastFocusableChild(row, true);

		return row;
	}

	return nullptr;
}

// ----------------------------------------------------------------------------------

bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, TreePathLeaf& value, const char* name, const char* label)
{
	return ar(value.index, name, label);
}

bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, TreeSelection& value, const char* name, const char* label)
{
	return ar(static_cast<std::vector<TreePath>&>(value), name, label);
}


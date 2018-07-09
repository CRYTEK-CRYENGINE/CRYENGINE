/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include "PropertyTree.h"
#include "PropertyTreeModel.h"
#include "PropertyTreeStyle.h"
#include "PropertyRowContainer.h"
#include "ValidatorBlock.h"
#include "IDrawContext.h"
#include <CrySerialization/yasli/ClassFactory.h>
#include <CrySerialization/yasli/Callback.h>
#include "Unicode.h"
#include "Serialization.h"
#include "IUIFacade.h"

#include <CrySerialization/yasli/BinArchive.h>

#include "IMenu.h"
#include "MathUtils.h"

#ifndef _MSC_VER
#include <limits.h>
#endif

#if 0
# define DEBUG_TRACE(fmt, ...) printf(fmt "\n", __VA_ARGS__)
# define DEBUG_TRACE_ROW(fmt, ...) for(PropertyRow* zzzz = this; zzzz; zzzz = zzzz->parent()) printf(" "); printf(fmt "\n", __VA_ARGS__)
#else
# define DEBUG_TRACE(...)
# define DEBUG_TRACE_ROW(...)
#endif
	
enum { TEXT_VALUE_SPACING = 3 };

inline unsigned calcHash(const char* str, unsigned hash = 5381)
{
	while(*str)
		hash = hash*33 + (unsigned char)*str++;
	return hash;
}


template<class T>
inline unsigned calcHash(const T& t, unsigned hash = 5381)
{
	for (int i = 0; i < sizeof(T); i++)
		hash = hash * 33 + ((unsigned char*)&t)[i];
	return hash;
}

// ---------------------------------------------------------------------------

ConstStringList* PropertyRow::constStrings_ = 0;

PropertyRow::PropertyRow()
{
	parent_ = 0;
	callback_ = 0;

	expanded_ = false;
	selected_ = false;
	visible_ = true;
	labelUndecorated_ = 0;
	belongsToFilteredRow_ = false;
	matchFilter_ = true;

	pos_ =  Point(0, 0);
	size_ = Point(-1, -1);
	plusSize_ = 0;
	textPos_ = 0;
	textSizeInitial_ = 0;
	textHash_ = 0;
	textSize_ = 0;
	widgetPos_ = 0;
	widgetSize_ = 0;
	userWidgetSize_ = -1;
	heightIncludingChildren_ = 0;

	name_ = "";
	typeName_ = "";

	pulledUp_ = false;
	pulledBefore_ = false;
	packedAfterPreviousRow_ = false;
	hasPulled_ = false;
	userReadOnly_ = false;
	userReadOnlyRecurse_ = false;
	userFullRow_ = false;
	userPackCheckboxes_ = false;
	userWidgetToContent_ = false;
	multiValue_ = false;
	userHideChildren_ = false;

	label_ = "";
	labelChanged_ = true;
	layoutChanged_ = true;
	hideChildren_ = false;
	validatorHasErrors_ = false;
	validatorHasWarnings_ = false;
	hasTitleIcon_ = false;

	tooltip_ = "";
}

PropertyRow::~PropertyRow()
{
	size_t count = children_.size();
	for (size_t i = 0; i < count; ++i)
		if (children_[i]->parent() == this)
			children_[i]->setParent(0);
	if (callback_)
		callback_->release();
	callback_ = 0;
}

void PropertyRow::setNames(const char* name, const char* label, const char* typeName)
{
	name_ = name;
	YASLI_ASSERT(name);
	label_ = label ? label : "";
	YASLI_ASSERT(strlen(typeName));
	typeName_ = typeName;
}

PropertyRow* PropertyRow::childByIndex(int index)
{
	if(index >= 0 && index < int(children_.size()))
		return children_[index];
	else
		return 0;
}

const PropertyRow* PropertyRow::childByIndex(int index) const
{
	if(index >= 0 && index < int(children_.size()))
		return children_[index];
	else
		return 0;
}

void PropertyRow::_setExpanded(bool expanded)
{
    expanded_ = expanded;
	int numChildren = (int)children_.size();

	for (int i = 0; i < numChildren; ++i) {
		PropertyRow* child = children_[i];
		if(child->pulledUp())
			child->_setExpanded(expanded);
	}

	layoutChanged_ = true;
	setLayoutChangedToChildren();
	
}

struct SetExpandedOp {
    bool expanded_;
    SetExpandedOp(bool expanded) : expanded_(expanded) {}
    ScanResult operator()(PropertyRow* row, PropertyTree* tree, int index)
    {
        if(row->canBeToggled(tree))
            row->_setExpanded(expanded_);
        return SCAN_CHILDREN_SIBLINGS;
    }
};

void PropertyRow::setExpandedRecursive(PropertyTree* tree, bool expanded)
{
	if(canBeToggled(tree))
		_setExpanded(expanded);

	SetExpandedOp op(expanded);
	scanChildren(op, tree);
}

int PropertyRow::childIndex(const PropertyRow* row) const
{
	YASLI_ASSERT(row);
	Rows::const_iterator it = std::find(children_.begin(), children_.end(), row);
	YASLI_ESCAPE(it != children_.end(), return -1);
	return (int)std::distance(children_.begin(), it);
}

bool PropertyRow::isChildOf(const PropertyRow* row) const
{
	const PropertyRow* p = parent();
	while(p){
		if(p == row)
			return true;
		p = p->parent();
	}
	return false;
}

void PropertyRow::add(PropertyRow* row)
{
	children_.push_back(row);
	row->setParent(this);
}

void PropertyRow::addAfter(PropertyRow* row, PropertyRow* after)
{
	iterator it = std::find(children_.begin(), children_.end(), after);
	if(it != children_.end()){
		++it;
		children_.insert(it, row);
	}
	else{
		children_.push_back(row);
	}

	row->setParent(this);
}

void PropertyRow::assignRowState(const PropertyRow& row, bool recurse)
{
	expanded_ = row.expanded_;
	selected_ = row.selected_;
    if(recurse){
		int numChildren = (int)children_.size();
		for (int i = 0; i < numChildren; ++i) {
            PropertyRow* child = children_[i].get();
            YASLI_ESCAPE(child, continue);
			int unusedIndex;
            const PropertyRow* rhsChild = row.findFromIndex(&unusedIndex, child->name(), child->typeName(), i);
            if(rhsChild)
                child->assignRowState(*rhsChild, true);
        }
    }
}

void PropertyRow::assignRowProperties(PropertyRow* row)
{
    YASLI_ESCAPE(row, return);
	parent_ = row->parent_;
	
	userReadOnly_ = row->userReadOnly_;
	userReadOnlyRecurse_ = row->userReadOnlyRecurse_;
	userFixedWidget_ = row->userFixedWidget_;
	pulledUp_ = row->pulledUp_;
	pulledBefore_ = row->pulledBefore_;
	size_ = row->size_;
	pos_ = row->pos_;
	plusSize_ = row->plusSize_;
	textPos_ = row->textPos_;
	textSizeInitial_ = row->textSizeInitial_;
	textHash_ = row->textHash_;
	textSize_ = row->textSize_;
	widgetPos_ = row->widgetPos_;
	widgetSize_ = row->widgetSize_;
	titleIcon_ = row->titleIcon_;
	hasTitleIcon_ = row->hasTitleIcon_;
	userWidgetSize_ = row->userWidgetSize_;
	userWidgetToContent_ = row->userWidgetToContent_;
	callback_ = row->callback_;
	row->callback_ = 0;

    assignRowState(*row, false);
}

void PropertyRow::replaceAndPreserveState(PropertyRow* oldRow, PropertyRow* newRow, PropertyTreeModel* model)
{
	Rows::iterator it = std::find(children_.begin(), children_.end(), oldRow);
	YASLI_ASSERT(it != children_.end());
	if(it != children_.end()){
		newRow->assignRowProperties(*it);
		newRow->labelChanged_ = true;
		*it = newRow;
		if (model)
			model->callRowCallback(newRow);
	}
}

void PropertyRow::erase(PropertyRow* row)
{
	row->setParent(0);
	Rows::iterator it = std::find(children_.begin(), children_.end(), row);
	YASLI_ASSERT(it != children_.end());
	if(it != children_.end())
		children_.erase(it);
}

void PropertyRow::swapChildren(PropertyRow* row, PropertyTreeModel* model)
{
	children_.swap(row->children_);
	iterator it;
	for( it = children_.begin(); it != children_.end(); ++it)
		(**it).setParent(this);
	for( it = row->children_.begin(); it != row->children_.end(); ++it)
		(**it).setParent(row);
	if (model){
		for(it = children_.begin(); it != children_.end(); ++it){
			PropertyRow* child = *it;
			if (PropertyRow* srcChild = row->find(child->name(), child->label(), child->typeName())) {
				child->setCallback(srcChild->callback_);
				srcChild->setCallback(0);
				model->callRowCallback(child);
			}
		}
	}
}

void PropertyRow::addBefore(PropertyRow* row, PropertyRow* before)
{
	if(before == 0)
		children_.insert(children_.begin(), row);
	else{
		iterator it = std::find(children_.begin(), children_.end(), before);
		if(it != children_.end())
			children_.insert(it, row);
		else
			children_.push_back(row);
	}
	row->setParent(this);
}

yasli::wstring PropertyRow::valueAsWString() const
{
    return toWideChar(valueAsString().c_str());
}

yasli::string PropertyRow::valueAsString() const
{
	return yasli::string();
}

SharedPtr<PropertyRow> PropertyRow::clone(ConstStringList* constStrings) const
{
	PropertyRow::setConstStrings(constStrings);
	yasli::BinOArchive oa;
	SharedPtr<PropertyRow> self(const_cast<PropertyRow*>(this));
	oa(self, "row", "Row");

	yasli::BinIArchive ia;
	ia.open(oa);
	SharedPtr<PropertyRow> clonedRow;
	ia(clonedRow, "row", "Row");
	PropertyRow::setConstStrings(0);
	if (clonedRow)
		clonedRow->setHideChildren(hideChildren_);
	return clonedRow;
}

void PropertyRow::YASLI_SERIALIZE_METHOD(Archive& ar)
{
	serializeValue(ar);

	ar(ConstStringWrapper(constStrings_, name_), "name", "name");
	ar(ConstStringWrapper(constStrings_, label_), "label", "label");
	ar(ConstStringWrapper(constStrings_, typeName_), "type", "type");
	ar(reinterpret_cast<std::vector<SharedPtr<PropertyRow> >&>(children_), "children", "!^children");	
	if(ar.isInput()){
		labelChanged_ = true;
		layoutChanged_ = true;
		PropertyRow::iterator it;
		for(it = begin(); it != end(); ){
			PropertyRow* row = *it;
			if(row){
				row->setParent(this);
				++it;
			}
			else{
				YASLI_ASSERT("Missing property row");
				it = erase(it);
			}
		}
	}
}

bool PropertyRow::onActivate(const PropertyActivationEvent& e)
{
	if (e.reason != e.REASON_RELEASE)
	    return e.tree->spawnWidget(this, e.force);
	else
		return false;
}

void PropertyRow::setLabelChanged() 
{ 
	for(PropertyRow* row = this; row != 0; row = row->parent())
		row->labelChanged_ = true;
}

void PropertyRow::setLayoutChanged()
{
	layoutChanged_ = true;
}

void PropertyRow::setLabelChangedToChildren()
{
	size_t numChildren = children_.size();
	for (size_t i = 0; i < numChildren; ++i) {
		children_[i]->labelChanged_ = true;
		children_[i]->setLabelChangedToChildren();
	}
}

void PropertyRow::setLayoutChangedToChildren()
{
	size_t numChildren = children_.size();
	for (size_t i = 0; i < numChildren; ++i) {
		children_[i]->layoutChanged_ = true;
		children_[i]->setLayoutChangedToChildren();
	}
}

void PropertyRow::setLabel(const char* label) 
{
	if (!label)
		label = "";
	if (label_ != label) {
		label_ = label;
		setLabelChanged(); 
	}
}

void PropertyRow::propagateFlagsTopToBottom()
{
	// these flags are reset in parseControlCodes
	if (!userReadOnly_ && !userWidgetToContent_)
		return;
	size_t numChildren = children_.size();
	for (size_t i = 0; i < numChildren; ++i) {
		PropertyRow* r = children_[i];
		if (userReadOnly_)
			r->userReadOnly_ = true;
		if (userWidgetToContent_) {
			r->userWidgetToContent_ = true;
			r->userFixedWidget_ = true;
		}
		r->propagateFlagsTopToBottom();
	}
}

void PropertyRow::setTooltip(const char* tooltip)
{
	tooltip_ = tooltip;
}

bool PropertyRow::setValidatorEntry(int index, int count)
{
	if (index != validatorIndex_ || count != validatorCount_) {
		validatorIndex_ = min(index, 0xffff);
		validatorCount_ = min(count, 0xff);
		validatorsHeight_ = 0;
		return true;
	}
	return false;
}

void PropertyRow::resetValidatorIcons()
{
	validatorHasWarnings_ = false;
	validatorHasErrors_ = false;
}

void PropertyRow::addValidatorIcons(bool hasWarnings, bool hasErrors)
{
	if (hasWarnings )
		validatorHasWarnings_ = true;
	if (hasErrors)
		validatorHasErrors_ = true;
}

void PropertyRow::updateLabel(const PropertyTree* tree, int index, bool parentHidesNonInlineChildren)
{
	if (!labelChanged_) {
		if (pulledUp_)
		parent()->hasPulled_ = true;
		return;
	}

	hasPulled_ = false;

	int numChildren = (int)children_.size();
	for (int i = 0; i < numChildren; ++i) {
		PropertyRow* row = children_[i];
		row->updateLabel(tree, i, hideChildren_);
	}

	parseControlCodes(tree, label_, true);
	bool hiddenByParentFlag = parentHidesNonInlineChildren && !pulledUp_;
	visible_ = (*labelUndecorated_ != '\0' || userFullRow_ || pulledUp_ || isRoot()) && !hiddenByParentFlag;

	propagateFlagsTopToBottom();

	if(pulledContainer())
		pulledContainer()->_setExpanded(expanded());

	layoutChanged_ = true;
	labelChanged_ = false;
}

struct ResetSerializerOp{
    ScanResult operator()(PropertyRow* row)
    {
        row->setSerializer(Serializer());
        return SCAN_CHILDREN_SIBLINGS;
    }
};

void PropertyRow::parseControlCodes(const PropertyTree* tree, const char* ptr, bool changeLabel)
{
	if (changeLabel) {
		userFullRow_ = false;
		pulledUp_ = false;
		pulledBefore_ = false;
		userReadOnly_ = false;
		userReadOnlyRecurse_ = false;
		userFixedWidget_ = false;
		userPackCheckboxes_ = false;
		userWidgetSize_ = -1;
		userHideChildren_ = false;
		userWidgetToContent_ = false;
	}

	while(true){
		if(*ptr == '^'){
			if(parent() && !parent()->isRoot()){
				if(pulledUp_)
					pulledBefore_ = true;
				pulledUp_ = true;
				parent()->hasPulled_ = true;

				if(pulledUp() && isContainer())
					parent()->setPulledContainer(this);
			}
		}
		else if(*ptr == '='){
			userWidgetToContent_ = true;
		}
		else if (*ptr == '+') {
			bool isFirstUpdate = labelUndecorated_ == 0;
			if (isFirstUpdate)
				_setExpanded(true);
		}
		else if (*ptr == '-') {
			bool isFirstUpdate = labelUndecorated_ == 0;
			if (isFirstUpdate)
				_setExpanded(false);
		}
		else if(*ptr == '<')
			userFullRow_ = true;
		else if(*ptr == '>'){
			userFixedWidget_ = true;
			const char* p = ++ptr;
			while(*p >= '0' && *p <= '9')
				++p;
			if(*p == '>'){
				userWidgetSize_ = atoi(ptr);
				ptr = ++p;
			}
			continue;
		}
		else if(*ptr == '-')
			userHideChildren_ = true;
		else if(*ptr == '~'){
            ResetSerializerOp op;
            scanChildren(op);
		}
		else if(*ptr == '!'){
			if(userReadOnly_)
				userReadOnlyRecurse_ = true;
			userReadOnly_ = true;
		}
		else if(*ptr == '|'){
			userPackCheckboxes_ = true;
		}
		else if(*ptr == '['){
			++ptr;
			PropertyRow::iterator it;
			for(it = children_.begin(); it != children_.end(); ++it)
				(*it)->parseControlCodes(tree, ptr, false);

			int counter = 1;
			while(*ptr){
				if(*ptr == ']' && !--counter)
					break;
				else if(*ptr == '[')
					++counter;
				++ptr;
			}
		}
		else
			break;
		++ptr;
	}

	if (isContainer()) {
		// automatically inline children for short arrays
		PropertyRowContainer* container = static_cast<PropertyRowContainer*>(this);
		int numChildren = (int)container->count();
		if (container->isFixedSize() && numChildren > 0 && numChildren <= 4) {
			if (container->childByIndex(0)->inlineInShortArrays()) {
				for(int i = 0; i < numChildren; ++i) {
					PropertyRow* child = container->childByIndex(i);
					child->pulledUp_ = true;
					if (child->label_)
						child->labelUndecorated_ = child->label_ + strlen(child->label_);
				}
				hasPulled_ = true;
				container->setInlined(true);
			}
		}
	}

	if (changeLabel)
		labelUndecorated_ = ptr;

	labelChanged();
}

const char* PropertyRow::typeNameForFilter(PropertyTree* tree) const
{
	return typeName();
}

void PropertyRow::updateTextSizeInitial(const PropertyTree* tree, int index, bool fontChanged)
{
	char containerLabel[16] = "";
	const char* text = rowText(containerLabel, tree, index);
	if(text[0] == '\0' || widgetPlacement() == WIDGET_INSTEAD_OF_TEXT) {
		textSizeInitial_ = 0;
		textHash_ = 0;
	}
	else{
		unsigned hash = calcHash(text);
		property_tree::Font font = rowFont(tree);
		hash = calcHash(font, hash);
		if(hash != textHash_ || fontChanged){
			textSizeInitial_ = tree->ui()->textWidth(text, font) + 3;
			textHash_ = hash;
		}
	}
}

void PropertyRow::calculateMinimalSize(const PropertyTree* tree, int posX, int availableWidth, bool force, int* _extraSizeRemainder, int* _extraSize, int index)
{
	PropertyRow* nonPulled = nonPulledParent();
	if (!layoutChanged_ && !force && !nonPulled->layoutChanged_) {
		DEBUG_TRACE_ROW("... skipping size for %s", label());
		return;
	}

	plusSize_ = 0;
	if(isRoot())
		expanded_ = true;
	else{
		if(nonPulled->isRoot() || (tree->treeStyle().compact && nonPulled->parent()->isRoot()))
			_setExpanded(true);
		else if(!pulledUp())
			plusSize_ = int(tree->treeStyle().firstLevelIndent * tree->_defaultRowHeight());

		if(parent()->pulledUp())
			pulledBefore_ = false;

		if(!visible(tree) && !(isContainer() && pulledUp())){
			size_ = Point(0, 0);
			DEBUG_TRACE_ROW("row '%s' got zero size", label());
			layoutChanged_ = false;
			return;
		}
	}
	
	if (tree->treeStyle().propertySplitter)
	{
		WidgetPlacement widgetPlace = widgetPlacement();
		int splitterStart = tree->getPropertySplitterPos() - tree->treeStyle().propertySplitterHalfWidth;
		int splitterWidth = 1 + tree->treeStyle().propertySplitterHalfWidth * 2;
		int splitterEnd = splitterStart + splitterWidth;
		
		pos_.setX(posX);

		if (!pulledUp())
		{
			posX += plusSize_;
			if (hasTitleIcon_)
			{
				int iconSize = tree->_defaultRowHeight();
				posX += iconSize;
			}
			textPos_ = posX;

			if (isGroupParent(tree))
			{
				textSize_ = tree->rightBorder() - textPos_;
			}
			else
			{
				textSize_ = splitterStart - textPos_;
				textSizeInitial_ = textSize_;
			}

			int minWidgetSize = widgetSizeMin(tree);
			widgetPos_ = splitterEnd;
			widgetSize_ = tree->rightBorder() - splitterEnd;

			if (widgetPlace != WIDGET_VALUE && !hasPulled_)
				widgetSize_ = min(widgetSize_, minWidgetSize);
			
			size_.setX(plusSize_ + splitterWidth + widgetSize_ + (textSize_ ? textSize_ : TEXT_VALUE_SPACING));
		}
		else
		{
			widgetPos_ = posX;
			widgetSize_ = availableWidth;
			size_.setX(availableWidth);
		}
		
		int height = isRoot() ? 0 : tree->_defaultRowHeight() + floorHeight();
		size_.setY(height);
		int pulledCount = 0;
		int lastIndex = 0;
		for (int i = 0; i < children_.size(); ++i)
		{
			PropertyRow* row = children_[i];
			if(row->visible(tree) && !row->pulledUp())
			{
				row->calculateMinimalSize(tree, posX, availableWidth, force, _extraSizeRemainder, _extraSize, i);
			}
			else
			{
				++pulledCount;
				lastIndex = i;
			}
		}

		if (pulledCount)
		{
			posX = splitterEnd;
			availableWidth = (widgetSize_ - TEXT_VALUE_SPACING * (pulledCount - 1)) / pulledCount;

			if (pulledCount > 1)
			{
				for (int i = 0; i < children_.size(); ++i)
				{
					PropertyRow* row = children_[i];
					if (row->visible(tree) && row->pulledUp() && availableWidth > row->widgetSizeMax(tree))
					{
						availableWidth += (availableWidth - row->widgetSizeMax(tree)) / (pulledCount - 1);
					}
				}
			}

			for (int i = 0; i < children_.size(); ++i)
			{
				PropertyRow* row = children_[i];
				if (row->visible(tree) && row->pulledUp())
				{
					row->calculateMinimalSize(tree, posX, std::min(availableWidth, row->widgetSizeMax(tree)), force, _extraSizeRemainder, _extraSize, i);
					posX += row->widgetSize_ + TEXT_VALUE_SPACING;
				}
			}

			PropertyRow* lastRow = children_[lastIndex];
			lastRow->widgetSize_ = tree->rightBorder() - lastRow->widgetPos_;
		}
		return;
	}

	int minWidgetSize = widgetSizeMin(tree);
	widgetSize_ = minWidgetSize;
	if (_extraSizeRemainder && *_extraSizeRemainder) {
		widgetSize_ += *_extraSizeRemainder;
		*_extraSizeRemainder = 0;
	}

	updateTextSizeInitial(tree, index, force);

	int height = isRoot() ? 0 : tree->_defaultRowHeight() + floorHeight();
	size_.setY(height);

	pos_.setX(posX);
	posX += plusSize_;

	int extraSizeStorage = 0;
	int& extraSize = !pulledUp() || !_extraSize ? extraSizeStorage : *_extraSize;

	int validatorIconsWidth = 0;
	if (validatorHasErrors_)
		validatorIconsWidth += tree->_defaultRowHeight();
	if (validatorHasWarnings_)
		validatorIconsWidth += tree->_defaultRowHeight();

	int freePulledChildren = 0;
	if(!pulledUp()){
		int minTextSize = 0;
		int totalMinimalWidth = 0;
		calcPulledRows(&minTextSize, &freePulledChildren, &totalMinimalWidth, tree, index);
		DEBUG_TRACE_ROW("%s minTextSize: %i, totalMinimalWidth: %i", label(), minTextSize, totalMinimalWidth);
		size_.setX(totalMinimalWidth);
		extraSize = (tree->rightBorder() - tree->leftBorder()) - totalMinimalWidth - posX - validatorIconsWidth;
		DEBUG_TRACE_ROW("%s extraSize 0: %i", label(), extraSize);

		float textScale = 1.0f;
		bool hideOwnText = false;
		if(extraSize < 0){
			// hide container item text first
			if (parent() && parent()->isContainer()){
				extraSize += textSizeInitial_;
				minTextSize -= textSizeInitial_;
				hideOwnText = true;
			}

			textScale = minTextSize ? clamp(1.0f - float(-extraSize) / minTextSize, 0.0f, 1.0f) : 0;
		}
		setTextSize(tree, index, textScale);

		if (hideOwnText) {
			textSize_ = 0;
			DEBUG_TRACE_ROW("%s hideOwnText", label());
		}
	}

	DEBUG_TRACE_ROW("%s extraSize 1: %i", label(), extraSize);

	WidgetPlacement widgetPlace = widgetPlacement();
	if(widgetPlace == WIDGET_AFTER_NAME && !tree->fullRowContainers())
		widgetPlace = WIDGET_VALUE;

	int numChildren = (int)children_.size();

	if(widgetPlace == WIDGET_ICON){
		if (tree->treeStyle().alignLabelsToRight && !pulledUp_ && !pulledBefore_ && !hasPulled_ && numChildren == 0)
			widgetPos_ = widgetSize_ ? tree->leftBorder() + xround((tree->rightBorder() - tree->leftBorder())* (1.f - tree->treeStyle().valueColumnWidth)) : -1000;
		else
			widgetPos_ = widgetSize_ ? posX : -1000;
		posX += widgetSize_;
		if (tree->treeStyle().alignLabelsToRight)
			textPos_ = widgetPos_ + widgetSize_ + TEXT_VALUE_SPACING;
		else
			textPos_ = posX;
		posX += textSize_;
	}

	bool hasPulledBefore = false;
	if (hasPulled_) {
		for (int i = 0; i < numChildren; ++i) {
			PropertyRow* row = children_[i];
			if(row->visible(tree) && row->pulledBefore()){
				row->calculateMinimalSize(tree, posX, availableWidth, force, 0, &extraSize, i);
				posX += row->size_.x();
				hasPulledBefore = true;
			}
		}
		if (hasPulledBefore)
			posX += TEXT_VALUE_SPACING;
	}

	if(widgetPlace != WIDGET_ICON){
		textPos_ = posX;
		posX += textSize_;
	}

	if(widgetPlace == WIDGET_AFTER_NAME){
		if (textSize_)
			posX += TEXT_VALUE_SPACING;
		widgetPos_ = posX;
		posX += widgetSize_;
	}

	if (widgetPlace == WIDGET_INSTEAD_OF_TEXT)
		widgetPos_ = posX;

	if(widgetPlace == WIDGET_VALUE || widgetPlace == WIDGET_AFTER_PULLED || (freePulledChildren > 0)){
		if (textSize_)
			posX += TEXT_VALUE_SPACING;

		if(!pulledUp() && extraSize > 0){
			// align widget value to value column
			if(!isFullRow(tree))
			{
				int oldX = posX;

				bool rightAlignment = tree->treeStyle().alignLabelsToRight && !hasPulledBefore;
				int maxX = rightAlignment ? textSize_ + TEXT_VALUE_SPACING: posX;
				int newX = max(tree->leftBorder() + xround((tree->rightBorder() - tree->leftBorder())* (1.f - tree->treeStyle().valueColumnWidth)), maxX);

				if (rightAlignment) {
					textPos_ = newX - textSize_ - TEXT_VALUE_SPACING;
					widgetPos_ = textPos_ - widgetSize_ - TEXT_VALUE_SPACING;
				}

				int xDelta = newX - oldX;
				if (xDelta <= extraSize)
				{
					extraSize -= xDelta;
					posX = newX;
				}
				else
				{
					posX += extraSize;
					extraSize = 0;
				}
			}
		}
	}

	int extraSizeRemainder = 0;
	if (freePulledChildren > 0) {
			extraSizeRemainder = extraSize % freePulledChildren;
			extraSize = extraSize / freePulledChildren;
	}

	if (widgetPlace == WIDGET_VALUE || widgetPlace == WIDGET_INSTEAD_OF_TEXT) {
		if(minWidgetSize && !isWidgetFixed() && extraSize > 0) {
			DEBUG_TRACE_ROW("%s widget extraSize: %i+%d", label(), extraSize, extraSizeRemainder);
			widgetSize_ += extraSize + extraSizeRemainder;
			extraSizeRemainder = 0;
		}

		if (widgetPlace != WIDGET_INSTEAD_OF_TEXT)
		widgetPos_ = posX;
		DEBUG_TRACE_ROW("textSize: %i widgetPos: %i", int(textSize_), int(widgetPos_));
		posX += widgetSize_;
	}

	size_.setX(textSize_ + (textSize_ ? TEXT_VALUE_SPACING : 0) + widgetSize_ + validatorIconsWidth);
	
	int childrenLeft = nonPulled->pos_.x();
	if (parent() != 0){
		if (parent()->parent() == 0) {
			if (!tree->treeStyle().doNotIndentSecondLevel)
				childrenLeft += int(tree->treeStyle().firstLevelIndent * tree->_defaultRowHeight());
		}
		else
			childrenLeft += int(tree->treeStyle().levelIndent * tree->_defaultRowHeight());
	}

	int checkBoxChildren = 0;
	for (int i = 0; i < numChildren; ++i) {
		PropertyRow* row = children_[i];
		if(!row->visible(tree)) {
			DEBUG_TRACE_ROW("skipping invisible child: %s", row->label());
			continue;
		}
		if(row->pulledUp()){
			if(!row->pulledBefore()){
				row->calculateMinimalSize(tree, posX, availableWidth, force, &extraSizeRemainder, &extraSize, i);
				posX += row->size_.x();
				posX += TEXT_VALUE_SPACING;
			}
			size_.setX(size_.x() + TEXT_VALUE_SPACING + row->size_.x());
			size_.setY(max(size_.y(), row->size_.y()));
		}
		else if(expanded()){
			row->calculateMinimalSize(tree, childrenLeft, availableWidth, force, 0, &extraSize, i);
			if (row->widgetPlacement() == WIDGET_ICON && row->count() == 0)
				++checkBoxChildren;
		}
	}

	// align checkboxes into two columns
	if (tree->packCheckboxes() || userPackCheckboxes_) {
		if (expanded() && checkBoxChildren > 0 && hasVisibleChildren(tree)) {
			int widthTotal = tree->rightBorder() - 16 - childrenLeft - plusSize_;
			int widthNextToLastCheckbox = 0;
			int left = childrenLeft + plusSize_;
			PropertyRow* previousCheckbox = nullptr;

			std::vector<PropertyRow*> checkboxesToRealign;
			bool hasChanges = false;

			for (int i = 0; i < numChildren; ++i) {
				PropertyRow* row = children_[i];
				if(!row->visible(tree))
					continue;
				if(row->widgetPlacement() != WIDGET_ICON || row->count() > 0) {
					previousCheckbox = 0;
					continue;
				}
				if(!row->pulledUp()){
					int checkboxWidth = row->textSize_ + tree->_defaultRowHeight()/* + TEXT_VALUE_SPACING*/;

					if (previousCheckbox && widthNextToLastCheckbox >= widthTotal / 2 && checkboxWidth < widthTotal / 2) {
						row->packedAfterPreviousRow_ = true;
						widthNextToLastCheckbox = 0;

						row->pos_.setX(left + widthTotal / 2);
						row->widgetPos_ = row->pos_.x();
						row->textPos_ = row->pos_.x() + row->widgetSize_;
						row->size_.setX(widthTotal / 2);

						previousCheckbox->size_.setX(widthTotal / 2);
						previousCheckbox->pos_.setX(left);
						previousCheckbox->widgetPos_ = left;
						previousCheckbox->textPos_ = left + previousCheckbox->widgetSize_;
						row->size_.setX(widthTotal / 2);
						previousCheckbox = 0;
						hasChanges = true;
					}
					else {
						row->packedAfterPreviousRow_ = false;
						widthNextToLastCheckbox = widthTotal - checkboxWidth;
						previousCheckbox = row;
					}

					if (previousCheckbox && tree->treeStyle().alignLabelsToRight)
						checkboxesToRealign.push_back(previousCheckbox);
				}
			}

			if (hasChanges) {
				for (int i = 0; i < checkboxesToRealign.size(); ++i) {
					PropertyRow* row = checkboxesToRealign[i];
					row->size_.setX(widthTotal / 2);
					row->pos_.setX(left);
					row->widgetPos_ = left;
					row->textPos_ = left + row->widgetSize_;
				}
			}
		}
	}

	if (widgetPlace == WIDGET_AFTER_PULLED)
	{
		posX += TEXT_VALUE_SPACING;
		widgetPos_ = posX;
	}

	if(!pulledUp())
		size_.setX(tree->rightBorder() - pos_.x());
	DEBUG_TRACE_ROW("calculateMinimalSize: '%s' %i %i (%s)", label(), size_.x(), size_.y(), isRoot() ? "root" : "non-root");
	layoutChanged_ = false;

	validatorsHeight_ = 0;
	if (!pulledUp() && !pulledBefore() && (validatorCount_ != 0 || hasPulled_)) {
		int padding = int(0.1f * tree->_defaultRowHeight());
		auto calculateValidatorHeight = [&](PropertyRow* row) {
			const ValidatorEntry* entries = tree->_validatorBlock()->getEntry(row->validatorIndex_, row->validatorCount_);
			if (entries) {
				for (int i = 0; i < row->validatorCount_; ++i) {
					int startPos = pos_.x() + plusSize_;
					int height = tree->ui()->textHeight(availableWidth - startPos - tree->_defaultRowHeight() - padding * 2, entries[i].message.c_str(), property_tree::FONT_NORMAL);
					validatorsHeight_ += max(tree->_defaultRowHeight(), height+ padding * 2) + padding * 3;
				}
			}
		};
		calculateValidatorHeight(this);
		visitPulledRows(this, calculateValidatorHeight);
	}

	size_.setY(size_.y() + validatorsHeight_);
}

void PropertyRow::adjustVerticalPosition(const PropertyTree* tree, int& totalHeight)
{
	int defaultRowHeight = tree->_defaultRowHeight();
	pos_.setY(totalHeight);
	int rowHeight = size_.y() + int(defaultRowHeight * (tree->treeStyle().rowSpacing - 1.0f) + 0.5f);

	if (packedAfterPreviousRow_)
		pos_.setY(totalHeight - rowHeight);
	else
		pos_.setY(totalHeight);

	if(!pulledUp()) {
		if (!packedAfterPreviousRow_)
			totalHeight += rowHeight;
	}
	else{
		pos_.setY(parent()->pos_.y());
		expanded_ = parent()->expanded();
	}
	PropertyRow* nonPulled = nonPulledParent();

	DEBUG_TRACE_ROW("adjustRect: %s %i %i %i %i %s", label(), pos_.x(), pos_.y(), size_.x(), size_.y(), pulledUp() ? "pulled" : "");

	if (expanded_ || hasPulled_) {
		for(PropertyRows::iterator it = children_.begin(); it != children_.end(); ++it){
			PropertyRow* row = *it;
			if(row->visible(tree) && (nonPulled->expanded() || row->pulledUp()))
				row->adjustVerticalPosition(tree, totalHeight);
		}
	}
	int delta = totalHeight - pos_.y();
	if (delta > USHRT_MAX)
		delta = USHRT_MAX;
	heightIncludingChildren_ = delta;
}

void PropertyRow::setTextSize(const PropertyTree* tree, int index, float mult)
{
	updateTextSizeInitial(tree, index, false);

	textSize_ = int(textSizeInitial_ * mult);

	size_t numChildren = children_.size();
	for (size_t i = 0; i < numChildren; ++i) {
		PropertyRow* row = children_[i];
		if(row->pulledUp())
			row->setTextSize(tree, 0, mult);
	}
}

void PropertyRow::calcPulledRows(int* minTextSize, int* freePulledChildren, int* minimalWidth, const PropertyTree *tree, int index) 
{
	updateTextSizeInitial(tree, index, false);

	*minTextSize += textSizeInitial_;
	WidgetPlacement widgetPlace = widgetPlacement();
	if((widgetPlace == WIDGET_VALUE || widgetPlace == WIDGET_INSTEAD_OF_TEXT || widgetPlace == WIDGET_AFTER_PULLED) && !isWidgetFixed())
		*freePulledChildren += 1;
	*minimalWidth += textSizeInitial_ + widgetSizeMin(tree); // spacing
	bool hasWidget = widgetPlace == WIDGET_VALUE || 
									 widgetPlace == WIDGET_INSTEAD_OF_TEXT ||
									 widgetPlace == WIDGET_AFTER_PULLED;
	if (textSizeInitial_ && (hasWidget || hasPulled_))
			*minimalWidth += TEXT_VALUE_SPACING;
	if (hasWidget && hasPulled_)
		*minimalWidth += TEXT_VALUE_SPACING;

	size_t numChildren = children_.size();
	int pulledCount = 0;
	for (size_t i = 0; i < numChildren; ++i) {
		PropertyRow* row = children_[i];
		if(row->pulledUp())
		{
			++pulledCount;
			row->calcPulledRows(minTextSize, freePulledChildren, minimalWidth, tree, index);
	}
	}
	if (hasPulled_)
		*minimalWidth += (pulledCount - 1) * TEXT_VALUE_SPACING;
}

PropertyRow* PropertyRow::findSelected()
{
    if(selected())
        return this;
    iterator it;
    for(it = children_.begin(); it != children_.end(); ++it){
        PropertyRow* result = (*it)->findSelected();
        if(result)
            return result;
    }
    return 0;
}

PropertyRow* PropertyRow::find(const char* name, const char* nameAlt, const char* typeName)
{
	iterator it;
	for(it = children_.begin(); it != children_.end(); ++it){
		PropertyRow* row = *it;
		if(((row->name() == name) || strcmp(row->name(), name) == 0) &&
		   ((nameAlt == 0) || (row->label() != 0 && strcmp(row->label(), nameAlt) == 0)) &&
		   ((typeName == 0) || (row->typeName() != 0 && strcmp(row->typeName(), typeName) == 0)))
			return row;
	}
	return 0;
}

PropertyRow* PropertyRow::findFromIndex(int* outIndex, const char* name, const char* typeName, int startIndex) const
{
	int numChildren = (int)children_.size();
	startIndex = min(startIndex, numChildren);

	for (int i = startIndex; i < numChildren; ++i) {
		PropertyRow* row = children_[i];
		if(((row->name() == name) || strcmp(row->name(), name) == 0) &&
			((row->typeName() == typeName || strcmp(row->typeName(), typeName) == 0))) {
			*outIndex = i;
			return row;
		}
	}

	for (int i = 0; i < startIndex; ++i) {
		PropertyRow* row = children_[i];
		if(((row->name() == name) || strcmp(row->name(), name) == 0) &&
			((row->typeName() == typeName || strcmp(row->typeName(), typeName) == 0))) {
			*outIndex = i;
			return row;
		}
	}

	*outIndex = -1;
	return 0;
}

const PropertyRow* PropertyRow::find(const char* name, const char* nameAlt, const char* typeName) const
{
	return const_cast<PropertyRow* const>(this)->find(name, nameAlt, typeName);
}


bool PropertyRow::onKeyDown(PropertyTree* tree, const KeyEvent* ev)
{
	using namespace property_tree;
	if(parent() && parent()->isContainer()){
		PropertyRowContainer* container = static_cast<PropertyRowContainer*>(parent());
		ContainerMenuHandler menuHandler(tree, container);
		menuHandler.element = this;
		if(ev->key() == KEY_DELETE && ev->modifiers() == 0) {
			menuHandler.onMenuChildRemove();
			return true;
		}
		else if(ev->key() == KEY_INSERT && ev->modifiers() == MODIFIER_SHIFT){
			menuHandler.onMenuChildInsertBefore();
			return true;
		}
	}
	return false;
}

struct ExpansionMenuHandler : PropertyRowMenuHandler
{
	PropertyTree* tree;
	PropertyRow* row;

	ExpansionMenuHandler(PropertyTree* tree, PropertyRow* row)
	: tree(tree), row(row)
	{
	}

	void onMenuExpand()
	{
		tree->expandChildren(row);
	}
	void onMenuCollapse()
	{
		tree->collapseChildren(row);
	}
};

bool PropertyRow::onContextMenu(property_tree::IMenu &menu, PropertyTree* tree)
{
	if(parent() && parent()->isContainer()){
		PropertyRowContainer* container = static_cast<PropertyRowContainer*>(parent());
		ContainerMenuHandler* handler = new ContainerMenuHandler(tree, container);
		handler->element = this;
		tree->addMenuHandler(handler);
		if(!container->isFixedSize()){
		    if(!menu.isEmpty())
			    menu.addSeparator();

				menu.addAction("Insert Before", "Shift+Insert",
					container->userReadOnly() ? MENU_DISABLED : 0, handler, &ContainerMenuHandler::onMenuChildInsertBefore);
				
				menu.addAction("Remove", "Shift+Delete",
					container->userReadOnly() ? MENU_DISABLED : 0, handler, &ContainerMenuHandler::onMenuChildRemove);
		}
	}

	if(hasVisibleChildren(tree)){
		if(!menu.isEmpty())
			menu.addSeparator();

		ExpansionMenuHandler* handler = new ExpansionMenuHandler(tree, this);
		menu.addAction(isRoot() ? "Expand All" : "Expand", 0, handler, &ExpansionMenuHandler::onMenuExpand);
		menu.addAction(isRoot() ? "Collapse All" : "Collapse", 0, handler, &ExpansionMenuHandler::onMenuCollapse);
		tree->addMenuHandler(handler);
	}

	return !menu.isEmpty();
}

bool PropertyRow::onDataDragMove(PropertyTree* tree)
{
	// By default, do not accept any drag&drop operations.
	return false;
}

bool PropertyRow::onDataDrop(PropertyTree* tree)
{
	// By default, do not accept any drag&drop operations.
	return false;
}

int PropertyRow::level() const
{
    int result = 0;
    const PropertyRow* row = this;
    while(row){
        row = row->parent();
        ++result;
    }
    return result;
}

PropertyRow* PropertyRow::nonPulledParent()
{
	PropertyRow* row = this;
	while(row->pulledUp())
		row = row->parent();
	return row;
}

bool PropertyRow::pulledSelected() const
{
	if(selected())
		return true;
	const PropertyRow* row = this;
	while(row->pulledUp()){
		row = row->parent();
		if(row->selected())
			return true;
	}
	return false;
}


Font PropertyRow::rowFont(const PropertyTree* tree) const
{
	return (hasVisibleChildren(tree) || (isContainer() && !static_cast<const PropertyRowContainer*>(this)->isInlined())) ? property_tree::FONT_BOLD : property_tree::FONT_NORMAL;
}

Rect PropertyRow::rectIncludingChildren(const PropertyTree* tree) const
{
	Rect r = rect();
	if (expanded())
		for (size_t i = 0; i < children_.size(); ++i)
			if (children_[i]->visible(tree))
				r = r.united(children_[i]->rectIncludingChildren(tree));
	return r;
}

bool PropertyRow::isGroupParent(const PropertyTree* tree) const
{
	if (!parent() || !parent()->isRoot())
		return false;
	
	if (isContainer())
		return true;

	return hasVisibleChildren(tree, true);
}

void PropertyRow::drawRow(IDrawContext& context, const PropertyTree* tree, int index, bool selectionPass)
{
	Rect rowRect = rect();
	Rect selectionRect;

	if (selectionPass)
	{
		if (tree->treeStyle().groupRectangle)
		{
			if (isGroupParent(tree))
			{
				Rect groupRect = widgetRect(tree);
				int headerHeight = groupRect.h;
				groupRect.x = 0;
				groupRect.w = tree->rightBorder();
				groupRect.h = heightIncludingChildren();
				context.drawGroupBox(groupRect, headerHeight);
			}
		}
	}
	else
	{
		if (tree->treeStyle().propertySplitter)
		{
			if (isGroupParent(tree) && expanded())
			{
				int splitterHeight = heightIncludingChildren();
				Rect splitterRect = widgetRect(tree);
				splitterRect.x = tree->getPropertySplitterPos() - tree->treeStyle().propertySplitterHalfWidth;
				splitterRect.w = 1 + tree->treeStyle().propertySplitterHalfWidth * 2;

				if (tree->treeStyle().groupRectangle)
				{
					splitterRect.y += splitterRect.h;
					splitterHeight -= splitterRect.h;
				}
				splitterRect.h = splitterHeight;

				context.drawSplitter(splitterRect);
			}
		}
	}
	
	bool showSelection = true;
	if (tree->treeStyle().selectionRectangle) {
		if(!pulledUp())
			selectionRect = rowRect.adjusted(plusSize_ - (tree->compact() ? 1 : 2), -2, 1, 1);
		else {
			selectionRect = rowRect.adjusted(-1, 0, 1, -1);
		}
	}
	else {
		selectionRect = widgetRect(tree).adjusted(-1, -1, 1, 1);
		if (widgetPlacement() == WIDGET_ICON)
			showSelection = labelUndecorated()[0] == '\0';
		else
			showSelection = widgetPlacement() != WIDGET_NONE;
	}

	if (selectionPass) {
		if (selected()) {
			// drawing a selection rectangle
			if (showSelection)
				context.drawSelection(selectionRect, false);
		}
		else{
			bool pulledChildrenSelected = false;

			for (size_t i = 0; i < children_.size(); ++i) {
				PropertyRow* child = children_[i];
				if (!child)
					continue;
				if ((child->pulledBefore() || child->pulledUp()) && child->selected())
					pulledChildrenSelected = true;
			}

			if (pulledChildrenSelected) {
				if (tree->treeStyle().selectionRectangle)
					context.drawSelection(selectionRect, true);
				// draw rectangle around parent of selected pulled row
			}
		}
	}
	else{
		context.widgetRect = widgetRect(tree);
		context.lineRect = floorRect(tree);
		context.captured = tree->_isCapturedRow(this);
		context.pressed = tree->_pressedRow() == this;


		// drawing a horizontal line
		int lineSize = widgetPos_ - textPos_ - 2;
		if(lineSize > textSize_){
			Rect rect(textPos_, rowRect.bottom() - 3, lineSize, 1);

			if (tree->treeStyle().horizontalLines)
				context.drawRowLine(rect);
		}


		if(!tree->compact() || (parent() && !parent()->isRoot())){
			if(hasVisibleChildren(tree)){
				context.drawPlus(plusRect(tree), expanded(), selected(), expanded());
			}
		}

		// custom drawing
		if(!isStatic() && context.widgetRect.isValid())
			redraw(context);

		bool bReadOnly =  userReadOnly() || userReadOnlyRecurse();

		if(textSize_ > 0 && !isRoot()){
			char containerLabel[16] = "";
			yasli::string text = rowText(containerLabel, tree, index);
			context.drawLabel(text.c_str(), rowFont(tree), textRect(tree), pulledSelected(), bReadOnly);
		}

		if (hasTitleIcon_)
		{
			context.drawIcon(titleIconRect(tree), titleIcon_);
		}

		if (validatorHasWarnings_) {
			context.drawValidatorWarningIcon(validatorWarningIconRect(tree));
		}
		if (validatorHasErrors_) {
			context.drawValidatorErrorIcon(validatorErrorIconRect(tree));
		}

		if (!selectionPass && validatorsHeight_ > 0)
		{
			Rect totalRect = validatorRect(tree);

			context.drawValidators(this, totalRect);
		}
	}
}

bool PropertyRow::visible(const PropertyTree* tree) const
{
	if (tree->_isDragged(this))
		return false;
	return (visible_ && (matchFilter_ || belongsToFilteredRow_));
}

bool PropertyRow::canBeToggled(const PropertyTree* tree) const
{
	if(!visible(tree))
		return false;
	if((tree->treeStyle().compact && (parent() && parent()->isRoot())) || (isContainer() && pulledUp()) || !hasVisibleChildren(tree))
		return false;
	return !empty();
}

bool PropertyRow::canBeDragged() const
{
	if(parent()){
		if(parent()->isContainer())
			return true;
	}
	return false;
}

bool PropertyRow::canBeDroppedOn(const PropertyRow* parentRow, const PropertyRow* beforeChild, const PropertyTree* tree) const
{
	YASLI_ASSERT(parentRow);

	if(parentRow->pulledContainer())
		parentRow = parentRow->pulledContainer();

	if(parentRow->isContainer()){
		const PropertyRowContainer* container = static_cast<const PropertyRowContainer*>(parentRow);
				
		if((container->isFixedSize() || container->userReadOnly()) && parent() != parentRow)
			return false;

		if(beforeChild && beforeChild->parent() != parentRow)
			return false;

		const PropertyRow* defaultRow = container->defaultRow(tree->model());
		if(defaultRow && strcmp(defaultRow->typeName(), typeName()) == 0)
			return true;
	}
	return false;	
}

void PropertyRow::dropInto(PropertyRow* parentRow, PropertyRow* cursorRow, PropertyTree* tree, bool before)
{
	SharedPtr<PropertyRow> ref(this);

	PropertyTreeModel* model = tree->model();
	PropertyTreeModel::UpdateLock lock = model->lockUpdate();
	if(parentRow->pulledContainer())
		parentRow = parentRow->pulledContainer();
	if(parentRow->isContainer()){
		tree->model()->rowAboutToBeChanged(tree->model()->root()); // FIXME: select optimal row
		setSelected(false);
		PropertyRowContainer* container = static_cast<PropertyRowContainer*>(parentRow);
		PropertyRow* oldParent = parent();
		TreePath oldParentPath = tree->model()->pathFromRow(oldParent);
		oldParent->erase(this);
		if(before)
			parentRow->addBefore(this, cursorRow);
		else
			parentRow->addAfter(this, cursorRow);
		model->selectRow(this, true);
		TreePath thisPath = tree->model()->pathFromRow(this);
		TreePath parentRowPath = tree->model()->pathFromRow(parentRow);
		oldParent = tree->model()->rowFromPath(oldParentPath);
		if (oldParent)
			model->rowChanged(oldParent); // after this call we can get invalid this
		if(PropertyRow* newThis = tree->model()->rowFromPath(thisPath)) {
			TreeSelection selection;
			selection.push_back(thisPath);
			model->setSelection(selection);

			// we use path to obtain new row
			tree->ensureVisible(newThis);
			model->rowChanged(newThis); // after this call row pointers are invalidated
		}
		parentRow = tree->model()->rowFromPath(parentRowPath);
		if (parentRow)
			model->rowChanged(parentRow); // after this call row pointers are invalidated
	}
}

void PropertyRow::intersect(const PropertyRow* row)
{
	setMultiValue(multiValue() || row->multiValue() || valueAsString() != row->valueAsString());


	int indexSource = 0;
	for(int i = 0; i < int(children_.size()); ++i)
	{
		PropertyRow* testRow = children_[i];
		PropertyRow* matchingRow = row->findFromIndex(&indexSource, testRow->name_, testRow->typeName_, indexSource);
		++indexSource;
		if (matchingRow == 0) {
			children_.erase(children_.begin() + i);
			--i;
		}	
		else {
			children_[i]->intersect(matchingRow);
		}
	}
}

const char* PropertyRow::rowText(char (&containerLabelBuffer)[16], const PropertyTree* tree, int index) const
{
	if(parent() && parent()->isContainer() && !pulledUp()){
		if (tree->showContainerIndices()) {
            sprintf_s(containerLabelBuffer, " %i.", index);
			return containerLabelBuffer;
		}
		else
			return "";
	}
	else
		return labelUndecorated() ? labelUndecorated() : "";
}

bool PropertyRow::hasVisibleChildren(const PropertyTree* tree, bool internalCall) const
{
	if(empty() || (!internalCall && pulledUp()))
		return false;

	PropertyRow::const_iterator it;
	for(it = children_.begin(); it != children_.end(); ++it){
		const PropertyRow* child = *it;
		if(child->pulledUp()){
            if(child->hasVisibleChildren(tree, true))
                return true;
        }
        else if(child->visible(tree))
                return true;
	}
	return false;
}

const PropertyRow* PropertyRow::hit(const PropertyTree* tree, Point point) const
{
  return const_cast<PropertyRow*>(this)->hit(tree, point);
}

PropertyRow* PropertyRow::hit(const PropertyTree* tree, Point point)
{
    bool expanded = this->expanded();
    if(isContainer() && pulledUp())
        expanded = parent() ? parent()->expanded() : true;
    bool onlyPulled = !expanded;
    PropertyRow::const_iterator it;
    for(it = children_.begin(); it != children_.end(); ++it){
        PropertyRow* child = *it;
		if (!child->visible(tree))
			continue;
        if(!onlyPulled || child->pulledUp())
            if(PropertyRow* result = child->hit(tree, point))
                return result;
    }
	if (Rect(pos_.x(), pos_.y(), size_.x(), size_.y()).contains(point))
        return this;
    return 0;
}

PropertyRow* PropertyRow::findByAddress(const void* addr)
{
    if(searchHandle() == addr)
        return this;
    else{
        Rows::iterator it;
        for(it = children_.begin(); it != children_.end(); ++it){
            PropertyRow* result = it->get()->findByAddress(addr);
            if(result)
                return result;
        }
    }
    return 0;
}

const void* PropertyRow::searchHandle() const
{
	return serializer_.pointer();
}


struct GetVerticalIndexOp{
    int index_;
    const PropertyRow* row_;

    GetVerticalIndexOp(const PropertyRow* row) : row_(row), index_(0) {}

    ScanResult operator()(PropertyRow* row, PropertyTree* tree, int index)
    {
        if(row == row_)
            return SCAN_FINISHED;
				if(row->visible(tree) && row->isSelectable() && !row->pulledUp() && !row->packedAfterPreviousRow())
            ++index_;
        return row->expanded() ? SCAN_CHILDREN_SIBLINGS : SCAN_SIBLINGS;
    }
};

int PropertyRow::verticalIndex(PropertyTree* tree, PropertyRow* row)
{
    GetVerticalIndexOp op(row);
	scanChildren(op, tree);
	return op.index_;
}


struct RowByVerticalIndexOp{
    int index_;
    PropertyRow* row_;

    RowByVerticalIndexOp(int index) : row_(0), index_(index) {}

    ScanResult operator()(PropertyRow* row, PropertyTree* tree, int index)
		{
			if(row->visible(tree) && !row->pulledUp() && row->isSelectable() && !row->packedAfterPreviousRow()){
				row_ = row;
				if(index_-- <= 0)
					return SCAN_FINISHED;
			}
			return row->expanded() ? SCAN_CHILDREN_SIBLINGS : SCAN_SIBLINGS;
		}
};

PropertyRow* PropertyRow::rowByVerticalIndex(PropertyTree* tree, int index)
{
    RowByVerticalIndexOp op(index);
	scanChildren(op, tree);
	return op.row_;
}

struct HorizontalIndexOp{
    int index_;
    PropertyRow* row_;
    bool pulledBefore_;

    HorizontalIndexOp(PropertyRow* row) : row_(row), index_(0), pulledBefore_(row->pulledBefore()) {}

    ScanResult operator()(PropertyRow* row, PropertyTree* tree, int index)
    {
        if(!row->pulledUp())
            return SCAN_SIBLINGS;
        if(row->visible(tree) && row->isSelectable() && row->pulledUp() && row->pulledBefore() == pulledBefore_){
            index_ += pulledBefore_ ? -1 : 1;
            if(row == row_)
                return SCAN_FINISHED;
        }
        return SCAN_CHILDREN_SIBLINGS;
    }
};

int PropertyRow::horizontalIndex(PropertyTree* tree, PropertyRow* row)
{
	if(row == this)
		return 0;
    HorizontalIndexOp op(row);
	if(row->pulledBefore())
		scanChildrenReverse(op, tree);
	else
		scanChildren(op, tree);
	return op.index_;
}

struct RowByHorizontalIndexOp{
    int index_;
    PropertyRow* row_;
    bool pulledBefore_;

    RowByHorizontalIndexOp(int index) : row_(0), index_(index), pulledBefore_(index < 0) {}

    ScanResult operator()(PropertyRow* row, PropertyTree* tree, int index)
    {
        if(!row->pulledUp())
            return SCAN_SIBLINGS;
        if(row->visible(tree) && row->isSelectable() && row->pulledUp() && row->pulledBefore() == pulledBefore_){
            row_ = row;
            if(pulledBefore_ ? ++index_ >= 0 : --index_ <= 0)
                return SCAN_FINISHED;
        }
        return SCAN_CHILDREN_SIBLINGS;
    }
};

PropertyRow* PropertyRow::rowByHorizontalIndex(PropertyTree* tree, int index)
{
	if(!index)
		return this;
    RowByHorizontalIndexOp op(index);
	if(index < 0)
		scanChildrenReverse(op, tree);
	else
		scanChildren(op, tree);
	return op.row_ ? op.row_ : this;
}

void PropertyRow::redraw(IDrawContext& context)
{

}

bool PropertyRow::isFullRow(const PropertyTree* tree) const
{
    if (tree->fullRowMode())
		return true;
	if (parent() && parent()->isContainer() && tree->fullRowContainers())
		return true;
	return userFullRow();
}

Rect PropertyRow::textRect(const PropertyTree* tree) const
{
	return Rect(textPos_, pos_.y(), textSize_ < textSizeInitial_ ? textSize_ - 1 : textSize_, tree->_defaultRowHeight());
}

Rect PropertyRow::widgetRect(const PropertyTree* tree) const
{
	return Rect(widgetPos_, pos_.y(), widgetSize_, tree->_defaultRowHeight());
}

Rect PropertyRow::validatorRect(const PropertyTree* tree) const
{
	return Rect(pos_.x() + plusSize_, pos_.y() + size_.y() - validatorsHeight_, size_.x() - plusSize_, validatorsHeight_);
}

Rect PropertyRow::validatorErrorIconRect(const PropertyTree* tree) const
{
	int rowHeight = tree->_defaultRowHeight();
	int width = validatorHasErrors_ && !expanded_ ? rowHeight : 0;
	int normalX = pos_.x() + size_.x() - width;
	int minimalX = max(widgetPos_ + widgetSize_, textPos_ + textSize_);
	return Rect(max(minimalX, normalX), pos_.y(), width, rowHeight);
}

Rect PropertyRow::validatorWarningIconRect(const PropertyTree* tree) const
{
	Rect r = validatorErrorIconRect(tree);	
	int width = validatorHasWarnings_ && !expanded_ ? r.height() : 0;
	return Rect(r.left() - width, pos_.y(), width, r.height());
}

Rect PropertyRow::plusRect(const PropertyTree* tree) const
{
	return Rect(pos_.x(), pos_.y(), plusSize_, tree->_defaultRowHeight());
}

Rect PropertyRow::floorRect(const PropertyTree* tree) const
{
	return Rect(textPos_, pos_.y() + tree->_defaultRowHeight(), size_.x() - (textPos_ - pos_.x()) , size_.y() - tree->_defaultRowHeight());
}

void PropertyRow::setCallback(yasli::CallbackInterface* callback)
{
	callback_ = callback;
}

void PropertyRow::setTitleIcon(const char *icon)
{
	if (icon)
	{
		titleIcon_ = Icon(icon);
		hasTitleIcon_ = true;
	}
	else
	{
		hasTitleIcon_ = false;
	}
}

Rect PropertyRow::titleIconRect(const PropertyTree* tree) const
{
	int iconSize = tree->_defaultRowHeight();
	return Rect(pos_.x()+plusSize_, pos_.y(), iconSize, tree->_defaultRowHeight());
}

YASLI_CLASS_NAME(PropertyRow, PropertyRow, "PropertyRow", "Structure");

PROPERTY_TREE_API PropertyRowFactory& globalPropertyRowFactory()
{
	return PropertyRowFactory::the();
}

PROPERTY_TREE_API yasli::ClassFactory<PropertyRow>& globalPropertyRowClassFactory()
{
	return yasli::ClassFactory<PropertyRow>::the();
}

// ---------------------------------------------------------------------------

int RowWidthCache::getOrUpdate(const PropertyTree* tree, const PropertyRow* rowForValue, int extraSpace)
{
	yasli::string value = rowForValue->valueAsString();
	const Font font = rowForValue->rowFont(tree);
	unsigned int newHash = calculateHash(value.c_str());
	newHash = calculateHash(font, newHash);
	if (newHash != valueHash)
	{
		width = tree->ui()->textWidth(value.c_str(), font) + 6 + extraSpace;
		if (width < 24)
			width = 24;
		valueHash = newHash;
	}
	return width;
}

FORCE_SEGMENT(PropertyRowNumber)
FORCE_SEGMENT(PropertyRowButton)
FORCE_SEGMENT(PropertyRowHorizontalLine)

#ifndef IS_EDITOR_BUILD
FORCE_SEGMENT(PropertyRowStringList)
#endif

/*
FORCE_SEGMENT(PropertyRowIconXPM)
FORCE_SEGMENT(PropertyRowFileOpen)
FORCE_SEGMENT(PropertyRowFileSave)
FORCE_SEGMENT(PropertyRowColor)
FORCE_SEGMENT(PropertyRowBitVector)
FORCE_SEGMENT(PropertyRowDecorators)
FORCE_SEGMENT(PropertyRowFileSelector)
FORCE_SEGMENT(PropertyRowHotkey)
FORCE_SEGMENT(PropertyRowSlider)
*/


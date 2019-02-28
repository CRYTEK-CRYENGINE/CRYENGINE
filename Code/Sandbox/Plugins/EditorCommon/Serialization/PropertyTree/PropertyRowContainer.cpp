/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include "PropertyRowContainer.h"
#include "PropertyRowPointer.h"
#include "PropertyTree.h"
#include "PropertyTreeModel.h"
#include "IDrawContext.h"
#include "Serialization.h"
#include "PropertyRowPointer.h"

#include "IMenu.h"
#include "IUIFacade.h"
#include <stdlib.h>

// ---------------------------------------------------------------------------

ContainerMenuHandler::ContainerMenuHandler(PropertyTree* tree, PropertyRowContainer* container)
: element()
, container(container)
, tree(tree)
, pointerIndex(-1)
{
}



// ---------------------------------------------------------------------------
YASLI_CLASS_NAME(PropertyRow, PropertyRowContainer, "PropertyRowContainer", "Container");

PropertyRowContainer::PropertyRowContainer()
: fixedSize_(false)
, elementTypeName_("")
, inlined_(false)
{
	buttonLabel_[0] = '\0';

}

struct ClassMenuItemAdderRowContainer : ClassMenuItemAdder
{
	ClassMenuItemAdderRowContainer(PropertyRowContainer* row, PropertyTree* tree, bool insert = false) 
	: row_(row)
	, tree_(tree)
	, insert_(insert) {}    

	void addAction(property_tree::IMenu& menu, const char* text, int index) override
	{
		ContainerMenuHandler* handler = new ContainerMenuHandler(tree_, row_);
		tree_->addMenuHandler(handler);
		handler->pointerIndex = index;

		menu.addAction(text, 0, handler, &ContainerMenuHandler::onMenuAppendPointerByIndex);
	}
protected:
	PropertyRowContainer* row_;
	PropertyTree* tree_;
	bool insert_;
};

void PropertyRowContainer::redraw(IDrawContext& context)
{
	Rect widgetRect = context.widgetRect;
	if (widgetRect.width() == 0 || inlined_)
		return;
	Rect rt = widgetRect.adjusted(0, 1, -1, -1);

	const char* text = multiValue() ? "..." : buttonLabel_;
	using namespace property_tree; // BUTTON_
	int buttonFlags = (context.pressed ? BUTTON_PRESSED : 0) |
					  (userReadOnly() ? BUTTON_DISABLED : 0) | BUTTON_DROP_DOWN | BUTTON_CENTER_TEXT;
	context.drawControlButton(rt, text, buttonFlags, FONT_NORMAL);
}


bool PropertyRowContainer::onActivate(const PropertyActivationEvent& e)
{
	if (e.reason == e.REASON_RELEASE)
		return false;
	if(userReadOnly())
		return false;
	if (inlined_)
		return false;
	std::auto_ptr<property_tree::IMenu> menu(e.tree->ui()->createMenu());
	generateMenu(*menu, e.tree, true);
	e.tree->_setPressedRow(this);
	menu->exec(Point(widgetPos_, pos_.y() + e.tree->_defaultRowHeight()));
	e.tree->_setPressedRow(0);
	return true;
}


void PropertyRowContainer::generateMenu(property_tree::IMenu& menu, PropertyTree* tree, bool addActions)
{
	ContainerMenuHandler* handler = new ContainerMenuHandler(tree, this);
	tree->addMenuHandler(handler);

	if (fixedSize_) {
		if (!inlined_)
			menu.addAction("[ Fixed Size Container ]", MENU_DISABLED);
	}
	else if(userReadOnly()) {
		menu.addAction("[ Read Only Container ]", MENU_DISABLED);
	}
	else
	{
		if (addActions) {
			PropertyRow* row = defaultRow(tree->model());
			if (row && row->isPointer()) {
				property_tree::IMenu* createItem = menu.addMenu("Add");
				menu.addSeparator();

				PropertyRowPointer* pointerRow = static_cast<PropertyRowPointer*>(row);
				ClassMenuItemAdderRowContainer(this, tree).generateMenu(*createItem, tree->model()->typeStringList(pointerRow->baseType()));
			}
			else {

				menu.addAction("Insert", "Insert", 0, handler, &ContainerMenuHandler::onMenuInsertElement);
				menu.addAction("Add", "", 0, handler, &ContainerMenuHandler::onMenuAppendElement);
			}
		}

		if (!menu.isEmpty())
			menu.addSeparator();

		menu.addAction(pulledUp() ? "Remove Children" : "Remove All", "Shift+Delete", userReadOnly() ? MENU_DISABLED : 0,
			handler, &ContainerMenuHandler::onMenuRemoveAll);
	}
}

bool PropertyRowContainer::onContextMenu(IMenu& menu, PropertyTree* tree)
{
	if(!menu.isEmpty())
		menu.addSeparator();

	generateMenu(menu, tree, true);

	if(pulledUp())
		return !menu.isEmpty();

	return PropertyRow::onContextMenu(menu, tree);
}


void ContainerMenuHandler::onMenuRemoveAll()
{
	tree->model()->rowAboutToBeChanged(container);
	container->clear();
	tree->model()->rowChanged(container);
}

PropertyRow* PropertyRowContainer::defaultRow(PropertyTreeModel* model)
{
	PropertyRow* defaultType = model->defaultType(elementTypeName_);
	//YASLI_ASSERT(defaultType);
	//YASLI_ASSERT(defaultType->numRef() == 1);
	return defaultType;
}

const PropertyRow* PropertyRowContainer::defaultRow(const PropertyTreeModel* model) const
{
	const PropertyRow* defaultType = model->defaultType(elementTypeName_);
	return defaultType;
}

void ContainerMenuHandler::onMenuInsertElement()
{
	container->addElement(tree, false);
}

void ContainerMenuHandler::onMenuAppendElement()
{
	container->addElement(tree, true);
}

PropertyRow* PropertyRowContainer::addElement(PropertyTree* tree, bool append)
{
	tree->model()->rowAboutToBeChanged(this);
	PropertyRow* defaultType = defaultRow(tree->model());
	YASLI_ESCAPE(defaultType != 0, return 0);
	SharedPtr<PropertyRow> clonedRow = defaultType->clone(tree->model()->constStrings());
	if(count() == 0)
		tree->expandRow(this);
	if (append)
		add(clonedRow);
	else
		addBefore(clonedRow, 0);
	clonedRow->setHideChildren(tree->outlineMode());
	clonedRow->setLabelChanged();
	clonedRow->setLabelChangedToChildren();
	setMultiValue(false);
	if(expanded())
		tree->model()->selectRow(clonedRow, true);
	tree->expandRow(clonedRow);
	TreePath path = tree->model()->pathFromRow(clonedRow);
	tree->model()->rowChanged(clonedRow);
	clonedRow = tree->model()->rowFromPath(path);
	tree->repaint(); 
	clonedRow = tree->model()->rowFromPath(path);
	if (clonedRow) {
		PropertyTreeModel::Selection sel;
		sel.push_back(path);
		tree->model()->setSelection(sel);
		if(clonedRow->activateOnAdd())
		{
			PropertyActivationEvent e;
			e.tree = tree;
			e.reason = e.REASON_NEW_ELEMENT;
			clonedRow->onActivate(e);
		}
	}
	return clonedRow;
}

void ContainerMenuHandler::onMenuAppendPointerByIndex()
{
	PropertyRow* defaultType = container->defaultRow(tree->model());
	PropertyRowPointer* defaultTypePointer = static_cast<PropertyRowPointer*>(defaultType);
	SharedPtr<PropertyRow> clonedRow = defaultType->clone(tree->model()->constStrings());
	if(container->count() == 0)
		tree->expandRow(container);
	container->add(clonedRow);
	clonedRow->setLabelChanged();
	clonedRow->setLabelChangedToChildren();
	clonedRow->setHideChildren(tree->outlineMode());
	container->setMultiValue(false);
	PropertyRowPointer* clonedRowPointer = static_cast<PropertyRowPointer*>(clonedRow.get());
	clonedRowPointer->setDerivedType(defaultTypePointer->derivedTypeName(), defaultTypePointer->factory());
	clonedRowPointer->setBaseType(defaultTypePointer->baseType());
	clonedRowPointer->setFactory(defaultTypePointer->factory());
	if(container->expanded())
		tree->model()->selectRow(clonedRow, true);
	tree->expandRow(clonedRowPointer);
	PropertyTreeModel::Selection sel = tree->model()->selection();

	CreatePointerMenuHandler handler;
	handler.tree = tree;
	handler.row = clonedRowPointer;
	handler.index = pointerIndex;
	handler.onMenuCreateByIndex();
	tree->model()->setSelection(sel);
	tree->repaint(); 
}

void ContainerMenuHandler::onMenuChildInsertBefore()
{
	tree->model()->rowAboutToBeChanged(container);
	PropertyRow* defaultType = tree->model()->defaultType(container->elementTypeName());
	if(!defaultType)
		return;
	SharedPtr<PropertyRow> clonedRow = defaultType->clone(tree->model()->constStrings());
	clonedRow->setHideChildren(tree->outlineMode());
	element->setSelected(false);
	container->addBefore(clonedRow, element);
	container->setMultiValue(false);
	tree->model()->selectRow(clonedRow, true);
	PropertyTreeModel::Selection sel = tree->model()->selection();
	tree->model()->rowChanged(clonedRow);
	tree->model()->setSelection(sel);
	tree->repaint(); 
	clonedRow = tree->selectedRow();
	if(clonedRow->activateOnAdd())
	{
		PropertyActivationEvent e;
		e.tree = tree;
		e.reason = PropertyActivationEvent::REASON_NEW_ELEMENT;
		clonedRow->onActivate(e);
	}
}

void ContainerMenuHandler::onMenuChildRemove()
{
	tree->model()->rowAboutToBeChanged(container);
	int index = container->childIndex(element);
	container->erase(element);
	container->setMultiValue(false);
	tree->model()->deselectAll();
	tree->updateAttachedPropertyTree(false);
	tree->model()->rowChanged(container);
	PropertyRow* newSelectedRow = container->childByIndex(index);
	if (newSelectedRow == 0)
		newSelectedRow = container;
	tree->model()->selectRow(newSelectedRow, true, true);
	tree->updateAttachedPropertyTree(false);
}


void PropertyRowContainer::labelChanged()
{
	sprintf_s(buttonLabel_, "%i", (int)count());
	userFixedWidget_ = true;
}

void PropertyRowContainer::serializeValue(yasli::Archive& ar)
{
	ar(ConstStringWrapper(constStrings_, elementTypeName_), "elementTypeName", "ElementTypeName");
	ar(fixedSize_, "fixedSize", "fixedSize");
}

yasli::string PropertyRowContainer::valueAsString() const
{
	char buf[32] = { 0 };
	sprintf_s(buf, "%d", (int)children_.size());
	return yasli::string(buf);
}

const char* PropertyRowContainer::typeNameForFilter(PropertyTree* tree) const 
{
	const PropertyRow* defaultType = defaultRow(tree->model());
	if (defaultType)
		return defaultType->typeNameForFilter(tree);
	else
		return elementTypeName_;
}

bool PropertyRowContainer::onKeyDown(PropertyTree* tree, const KeyEvent* ev)
{
	ContainerMenuHandler handler(tree, this);
	if(ev->key() == KEY_DELETE && ev->modifiers() == MODIFIER_SHIFT){
		handler.onMenuRemoveAll();
		return true;
	}
	if(ev->key() == KEY_INSERT && ev->modifiers() == 0){
		handler.onMenuAppendElement();
		return true;
	}
    return PropertyRow::onKeyDown(tree, ev);
}

int PropertyRowContainer::widgetSizeMin(const PropertyTree* tree) const
{
	return inlined_ ? 0 : (userWidgetSize() >=0 ? userWidgetSize() : int(tree->_defaultRowHeight() * 1.7f));
}



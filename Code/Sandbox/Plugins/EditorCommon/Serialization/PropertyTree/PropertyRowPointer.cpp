/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include "PropertyRowPointer.h"
#include "PropertyTree.h"
#include "PropertyTreeModel.h"
#include "IDrawContext.h"
#include "Serialization.h"
#include "Unicode.h"
#include "IMenu.h"
#include "IUIFacade.h"
#include "Rect.h"

// ---------------------------------------------------------------------------

void ClassMenuItemAdder::generateMenu(IMenu& createItem, const StringList& comboStrings)
{
	StringList::const_iterator it;
	int index = 0;
	for (it = comboStrings.begin(); it != comboStrings.end(); ++it) {
		StringList path;
		splitStringList(&path, it->c_str(), '\\');
		int level = 0;
		IMenu* item = &createItem;

		for(int level = 0; level < int(path.size()); ++level){
			const char* leaf = path[level].c_str();
			if(level == path.size() - 1){
				addAction(*item, leaf, index++);
			}
			else{
				if (property_tree::IMenu* menu = item->findMenu(leaf))
					item = menu;
				else
					item = addMenu(*item, leaf);
			}
		}
	}
}

void ClassMenuItemAdder::addAction(IMenu& menu, const char* text, int index)
{
	menu.addAction(text, MENU_DISABLED);
}

IMenu* ClassMenuItemAdder::addMenu(IMenu& menu, const char* text)
{
	IMenu* result = menu.addMenu(text);
	return result;
}


// ---------------------------------------------------------------------------

YASLI_CLASS_NAME(PropertyRow, PropertyRowPointer, "PropertyRowPointer", "SharedPtr");

PropertyRowPointer::PropertyRowPointer()
: factory_(0)
, searchHandle_(0)
, colorOverride_(0,0,0,0)
{
}

void PropertyRowPointer::setDerivedType(const char* typeName, yasli::ClassFactoryBase* factory)
{
	if (!factory) {
		derivedTypeName_.clear();
		return;
	}
	derivedTypeName_ = typeName;
}

bool PropertyRowPointer::assignTo(yasli::PointerInterface &ptr)
{
	if (derivedTypeName_ != ptr.registeredTypeName()) {
		ptr.create(derivedTypeName_.c_str());
	}

	return true;
}


void CreatePointerMenuHandler::onMenuCreateByIndex()
{
	tree->model()->rowAboutToBeChanged(row);
	if(index < 0){ // NULL value
		row->setDerivedType("", 0);
		row->clear();
	}
	else{
		const PropertyDefaultDerivedTypeValue* defaultValue = tree->model()->defaultType(row->baseType(), index);
		SharedPtr<PropertyRow> clonedDefault = defaultValue->root->clone(tree->model()->constStrings());
		if (defaultValue && defaultValue->root) {
			YASLI_ASSERT(defaultValue->root->refCount() == 1);
			if(useDefaultValue){
				row->clear();
				row->swapChildren(clonedDefault, 0);
			}
			row->setDerivedType(defaultValue->registeredName.c_str(), row->factory());
			row->setLabelChanged();
			row->setLabelChangedToChildren();
			tree->expandRow(row);
		}
		else{
			row->setDerivedType("", 0);
			row->clear();
		}
	}
	tree->model()->rowChanged(row);
}


yasli::string PropertyRowPointer::valueAsString() const
{
	yasli::string result;
	const yasli::TypeDescription* desc = 0;
	if (factory_)
		desc = factory_->descriptionByRegisteredName(derivedTypeName_.c_str());
	if (desc)
		result = desc->label();
	else
		result = derivedTypeName_;

	return result;
}

yasli::string PropertyRowPointer::generateLabel() const
{
	if(multiValue())
		return "...";

	yasli::string str;
	if(!derivedTypeName_.empty()){
		const char* textStart = derivedTypeName_.c_str();
		if (factory_) {
			const yasli::TypeDescription* desc = factory_->descriptionByRegisteredName(derivedTypeName_.c_str());

			if (desc)
				textStart = desc->label();
		}
		const char* p = textStart + strlen(textStart);
		while(p > textStart){
			if(*(p - 1) == '\\')
				break;
			--p;
		}
		str = p;
		if(p != textStart){
			str += " (";
			str.append(textStart, p - 1);
			str += ")";
		}
	}
	else
	{
		if (factory_)
			str = factory_->nullLabel() ? factory_->nullLabel() : "[ null ]";
		else
			str = "[ null ]";

	}
	return str;
}

void PropertyRowPointer::redraw(IDrawContext& context)
{
	Rect widgetRect = context.widgetRect;
	Rect rt = widgetRect.adjusted(-1, 0, 0, 1);
	yasli::string str = generateLabel();
	using namespace property_tree;
	property_tree::Font font = derivedTypeName_.empty() ? FONT_NORMAL : FONT_BOLD;
	context.drawControlButton(rt, str.c_str(),
		(context.pressed ? BUTTON_PRESSED : 0) | (userReadOnly() ? BUTTON_DISABLED : 0) | BUTTON_DROP_DOWN,
		font, colorOverride_.a != 0 ? &colorOverride_ : 0);
}

struct ClassMenuItemAdderRowPointer : ClassMenuItemAdder{
	ClassMenuItemAdderRowPointer(PropertyRowPointer* row, PropertyTree* tree) : row_(row), tree_(tree) {}    
	void addAction(property_tree::IMenu& menu, const char* text, int index)
	{
		CreatePointerMenuHandler* handler = new CreatePointerMenuHandler;
		tree_->addMenuHandler(handler);
		handler->row = row_;
		handler->tree = tree_;
		handler->index = index;
		handler->useDefaultValue = !tree_->config().immediateUpdate;

		menu.addAction(text, 0, handler, &CreatePointerMenuHandler::onMenuCreateByIndex);
	}
protected:
	PropertyRowPointer* row_;
	PropertyTree* tree_;
};


bool PropertyRowPointer::onActivate(const PropertyActivationEvent& ev)
{
	if(userReadOnly())
			return false;
	std::auto_ptr<property_tree::IMenu> menu(ev.tree->ui()->createMenu());
	ClassMenuItemAdderRowPointer(this, ev.tree).generateMenu(*menu, ev.tree->model()->typeStringList(baseType()));
	ev.tree->_setPressedRow(this);
	menu->exec(Point(widgetPos_, pos_.y() + ev.tree->_defaultRowHeight()));
	ev.tree->_setPressedRow(0);
	return true;
}

bool PropertyRowPointer::onMouseDown(PropertyTree* tree, Point point, bool& changed) 
{
	if(widgetRect(tree).contains(point)){
		PropertyActivationEvent ev;
		ev.tree = tree;
		ev.clickPoint = point;
		if(onActivate(ev))
			changed = true;
	}
	return false; 
}

bool PropertyRowPointer::onContextMenu(IMenu &menu, PropertyTree* tree)
{
	if(!menu.isEmpty())
		menu.addSeparator();
		if(!userReadOnly()){
			IMenu* createItem = menu.addMenu("Set");
			ClassMenuItemAdderRowPointer(this, tree).generateMenu(*createItem, tree->model()->typeStringList(baseType()));
		}
	return PropertyRow::onContextMenu(menu, tree);
}

void PropertyRowPointer::serializeValue(yasli::Archive& ar)
{
	ar(derivedTypeName_, "derivedTypeName", "Derived Type Name");
}

int PropertyRowPointer::widgetSizeMin(const PropertyTree* tree) const
{
	if (userWidgetSize() >= 0)
	{
		return userWidgetSize();
	}

	property_tree::Font font = derivedTypeName_.empty() ? property_tree::FONT_NORMAL : property_tree::FONT_BOLD;
	std::string text = generateLabel();
	return tree->ui()->textWidth(text.c_str(), font) + 18;
}

static Color parseColorString(const char* str)
{
	unsigned int color = 0;
	if (sscanf(str, "%x", &color) != 1)
		return Color(0,0,0,0);
	Color result((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff, 255);
	return result;
}

void PropertyRowPointer::setValueAndContext(const yasli::PointerInterface& ptr, yasli::Archive& ar)
{
	baseType_ = ptr.baseType();
	factory_ = ptr.factory();
	serializer_ = ptr.serializer();
	pointerType_ = ptr.pointerType();
	searchHandle_ = ptr.handle();

	const char* colorString = factory_->findAnnotation(ptr.registeredTypeName(), "color");
	if (colorString[0] != '\0')
		colorOverride_ = parseColorString(colorString);
	else
		colorOverride_ = Color(0,0,0,0);

	const yasli::TypeDescription* desc = factory_->descriptionByRegisteredName(ptr.registeredTypeName());
	if (desc)
		derivedTypeName_ = desc->name();
	else
		derivedTypeName_.clear();
}

// vim:ts=4 sw=4:


// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CryCore/Platform/platform.h>

#include <Serialization/PropertyTree/PropertyTree.h>
#include <Serialization/PropertyTree/PropertyRowImpl.h>
#include <Serialization/PropertyTree/IDrawContext.h>
#include <Serialization/PropertyTree/PropertyTreeModel.h>
#include <Serialization/PropertyTree/Unicode.h>
#include <CrySerialization/Color.h>

#include <CrySerialization/ClassFactory.h>
#include <Serialization/Decorators/EditToolButton.h>
#include "Serialization.h"

using Serialization::SEditToolButton;
using Serialization::SEditToolButtonPtr;

class PropertyRowSEditToolButton : public PropertyRow, public IEditorNotifyListener
{
public:
	PropertyRowSEditToolButton() : minimalWidth_()
	{
		IEditor* editor = GetIEditor();
		if (editor)
			editor->RegisterNotifyListener(this);
	}

	~PropertyRowSEditToolButton()
	{
		IEditor* editor = GetIEditor();
		if (editor)
			editor->UnregisterNotifyListener(this);
	}

	void OnEditorNotifyEvent(EEditorNotifyEvent event)
	{
		if (event == eNotify_OnEditToolEndChange)
		{
			if (value_)
				value_->DetermineCheckedState();
		}
	}

	bool isLeaf() const override       { return true; }
	bool isStatic() const override     { return false; }
	bool isSelectable() const override { return true; }

	bool onActivate(const PropertyActivationEvent& e) override
	{
		if (e.reason == PropertyActivationEvent::REASON_KEYBOARD)
		{
			value_->OnClicked();
		}
		return true;
	}

	bool onMouseDown(PropertyTree* tree, Point point, bool& changed) override
	{
		if (userReadOnly())
			return false;
		if (widgetRect(tree).contains(point))
		{
			value_->OnClicked();
			tree->repaint();
			changed = true;
			return true;
		}
		return false;
	}

	void setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar) override
	{
		value_ = static_cast<SEditToolButton*>(ser.pointer())->Clone();
		const char* icon = value_->Icon();
		icon_ = icon && icon[0] ? property_tree::Icon(icon) : property_tree::Icon();
	}

	bool            assignTo(const Serialization::SStruct& ser) const override { return true; }
	wstring         valueAsWString() const override                            { return L""; }
	WidgetPlacement widgetPlacement() const override                           { return WIDGET_INSTEAD_OF_TEXT; }
	void            serializeValue(Serialization::IArchive& ar) override       {}

	int             widgetSizeMin(const PropertyTree* tree) const override
	{
		if (minimalWidth_ == 0)
			minimalWidth_ = (int)tree->ui()->textWidth(labelUndecorated(), property_tree::FONT_NORMAL) + 6 + (icon_.isNull() ? 0 : 18);
		return minimalWidth_;
	}

	void redraw(IDrawContext& context) override
	{
		using namespace property_tree;
		Rect rect = context.widgetRect.adjusted(-1, -1, 1, 1);

		// silly to initialize PropertyTree here with const crappiness even, but it's the only reliable place that will get called early
		value_->SetPropertyTree(const_cast<PropertyTree*>(context.tree));

		int buttonFlags = value_->GetButtonFlags();

		if (icon_.isNull())
		{
			context.drawButton(rect, labelUndecorated(), buttonFlags | BUTTON_CENTER_TEXT, property_tree::FONT_NORMAL);
		}
		else
		{
			context.drawButtonWithIcon(icon_, rect, labelUndecorated(), buttonFlags, property_tree::FONT_NORMAL);
		}
	}
	bool isFullRow(const PropertyTree* tree) const override
	{
		if (PropertyRow::isFullRow(tree))
			return true;
		return !userFixedWidget();
	}
protected:
	mutable int         minimalWidth_;
	property_tree::Icon icon_;
	SEditToolButtonPtr  value_;
};

REGISTER_PROPERTY_ROW(SEditToolButton, PropertyRowSEditToolButton);


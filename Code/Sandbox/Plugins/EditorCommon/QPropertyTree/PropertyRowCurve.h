// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Serialization/PropertyTree/PropertyRow.h>
#include <CurveEditorContent.h>
#include <Serialization.h>

// A row for editing one or more curves.
// Displays the curves inline. Clicking redirects to a Curve Editor panel

class PropertyRowCurve : public PropertyRow
{
public:
	PropertyRowCurve(bool multiCurve = false)
		: multiCurve_(multiCurve)
		, editorContent_(nullptr)
	{}
	~PropertyRowCurve()
	{
		delete editorContent_;
	}

	// PropertyRow
	bool            isLeaf() const override                                { return true; }
	bool            isStatic() const override                              { return false; }
	WidgetPlacement widgetPlacement() const override                       { return WIDGET_VALUE; }
	int             widgetSizeMin(const PropertyTree* tree) const override { return userWidgetSize() >= 0 ? userWidgetSize() : 48; }

	void            setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar) override;
	bool            assignTo(const Serialization::SStruct& ser) const override;
	void            serializeValue(yasli::Archive& ar) override;
	yasli::string   valueAsString() const override;

	void            closeNonLeaf(const Serialization::SStruct& ser, Serialization::IArchive& ar) override { setValueAndContext(ser, ar); }
	bool            onActivate(const PropertyActivationEvent& e) override;

	void            redraw(property_tree::IDrawContext& context) override;

	bool            onContextMenu(property_tree::IMenu& menu, PropertyTree* tree) override;

	// additional
	void          editCurve(PropertyTree* tree);
	PropertyTree* tree() const { return tree_; }

private:
	// Accessor used for lazy initialization of editor content. This is necessary because this PropertyRow is created statically by yasli
	// before the application is created, which leads to QObject constructor to be called before the application is created. QObject constuctor in turn
	// gets a handle of the main thread, which will populate global main thread info if it hasn't been initialized yet by using the current thread.
	// This is problematic when launching sandbox as a child process of another application (like Renderdoc) since it results in Qt setting the
	// main thread id to something that is not really the main thread id (the case that was evaluated Qt would think that a kernel thread was the main thread)
	SCurveEditorContent*       GetEditorContent();
	const SCurveEditorContent* const GetEditorContent() const;

	TCurveEditorCurves&       curves()       { return GetEditorContent()->m_curves; }
	TCurveEditorCurves const& curves() const { return GetEditorContent()->m_curves; }

private:
	bool                 multiCurve_;
	string               curveDomain_;
	SCurveEditorContent* editorContent_;
	PropertyTree*        tree_ = 0;
};

struct CurveMenuHandler : PropertyRowMenuHandler
{
	PropertyTree*     tree;
	PropertyRowCurve* propertyRowCurve;

	CurveMenuHandler(PropertyTree* tree, PropertyRowCurve* propertyRowCurve)
		: propertyRowCurve(propertyRowCurve), tree(tree) {}

	void onMenuEditCurve()
	{
		propertyRowCurve->editCurve(tree);
	}
};

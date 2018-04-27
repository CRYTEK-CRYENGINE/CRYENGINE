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
	{}

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
	void                      editCurve(PropertyTree* tree);
	TCurveEditorCurves&       curves()       { return editorContent_.m_curves; }
	TCurveEditorCurves const& curves() const { return editorContent_.m_curves; }
	PropertyTree*             tree() const   { return tree_; }

private:
	bool                 multiCurve_;
	string               curveDomain_;
	SCurveEditorContent  editorContent_;
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


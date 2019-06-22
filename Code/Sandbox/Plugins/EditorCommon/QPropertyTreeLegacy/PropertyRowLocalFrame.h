// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Serialization/Decorators/IGizmoSink.h"
#include <Serialization/PropertyTreeLegacy/PropertyRowField.h>
#include <Serialization/PropertyTreeLegacy/sigslot.h>
#include "Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h"

struct IGizmoSink;

class PropertyRowLocalFrameBase : public PropertyRow
{
public:
	PropertyRowLocalFrameBase();
	~PropertyRowLocalFrameBase();

	bool            isLeaf() const override   { return m_reset; }
	bool            isStatic() const override { return false; }

	bool            onActivate(const PropertyActivationEvent& e) override;

	WidgetPlacement widgetPlacement() const override                       { return WIDGET_AFTER_PULLED; }
	int             widgetSizeMin(const PropertyTreeLegacy* tree) const override { return tree->_defaultRowHeight(); }

	string          valueAsString() const override;
	bool            onContextMenu(property_tree::IMenu& menu, PropertyTreeLegacy* tree) override;
	const void*     searchHandle() const override { return m_handle; }
	void            redraw(property_tree::IDrawContext& context) override;

	void            reset(PropertyTreeLegacy* tree);
protected:
	Serialization::IGizmoSink*        m_sink;
	const void*                       m_handle;
	int                               m_gizmoIndex;
	mutable Serialization::GizmoFlags m_gizmoFlags;
	bool                              m_reset;
};

struct LocalFrameMenuHandler : PropertyRowMenuHandler
{
	PropertyTreeLegacy*              tree;
	PropertyRowLocalFrameBase* self;

	LocalFrameMenuHandler(PropertyTreeLegacy* tree, PropertyRowLocalFrameBase* self) : tree(tree), self(self) {}

	void onMenuReset();
};

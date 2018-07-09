// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Serialization/Decorators/IGizmoSink.h"
#include <Serialization/PropertyTree/PropertyRowField.h>
#include <Serialization/PropertyTree/sigslot.h>
#include "Serialization/QPropertyTree/QPropertyTree.h"

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
	int             widgetSizeMin(const PropertyTree* tree) const override { return tree->_defaultRowHeight(); }

	string          valueAsString() const override;
	bool            onContextMenu(property_tree::IMenu& menu, PropertyTree* tree) override;
	const void*     searchHandle() const override { return m_handle; }
	void            redraw(property_tree::IDrawContext& context) override;

	void            reset(PropertyTree* tree);
protected:
	Serialization::IGizmoSink*        m_sink;
	const void*                       m_handle;
	int                               m_gizmoIndex;
	mutable Serialization::GizmoFlags m_gizmoFlags;
	bool                              m_reset;
};

struct LocalFrameMenuHandler : PropertyRowMenuHandler
{
	PropertyTree*              tree;
	PropertyRowLocalFrameBase* self;

	LocalFrameMenuHandler(PropertyTree* tree, PropertyRowLocalFrameBase* self) : tree(tree), self(self) {}

	void onMenuReset();
};


// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <StdAfx.h>
#include <Serialization/PropertyTree/IDrawContext.h>
#include <Serialization/PropertyTree/PropertyRow.h>
#include <Serialization/PropertyTree/PropertyTreeModel.h>
#include "Serialization/QPropertyTree/QPropertyTree.h"
#include "Serialization.h"
#include "CrySerialization/Math.h"

class PropertyRowUniformScale : public PropertyRow
{
public:
	PropertyRowUniformScale();
	bool            isLeaf() const override   { return true; }
	bool            isStatic() const override { return false; }

	bool            onActivate(const PropertyActivationEvent& e) override;
	bool            assignTo(const Serialization::SStruct& ser) const override;
	void            serializeValue(Serialization::IArchive& ar);
	WidgetPlacement widgetPlacement() const override                       { return WIDGET_AFTER_PULLED; }
	int             widgetSizeMax(const PropertyTree* tree) const override { return 10; }
	void            redraw(IDrawContext& context) override;

private:
	bool m_uniform;
};

PropertyRowUniformScale::PropertyRowUniformScale()
	: m_uniform(false)
{
	if (GetIEditor() != nullptr)
	{
		GetIEditor()->GetPersonalizationManager()->GetProperty("BaseObject", "UniformScale", m_uniform);
	}
}

bool PropertyRowUniformScale::onActivate(const PropertyActivationEvent& e)
{
	if (e.REASON_PRESS == e.reason)
	{
		e.tree->model()->rowAboutToBeChanged(this);
		m_uniform = !m_uniform;
		e.tree->model()->rowChanged(this);
		return true;
	}
	return false;
}

void PropertyRowUniformScale::redraw(IDrawContext& context)
{
	Rect rect = widgetRect(context.tree);
	const char* iconName = m_uniform ? "icons:General/Uniform.ico" : "icons:General/NonUniform.ico";
	context.drawIcon(rect, iconName);
}

bool PropertyRowUniformScale::assignTo(const Serialization::SStruct& ser) const
{
	Serialization::SUniformScale* pValue = ((Serialization::SUniformScale*)ser.pointer());
	pValue->uniform = m_uniform;
	GetIEditor()->GetPersonalizationManager()->SetProperty("BaseObject", "UniformScale", m_uniform);
	return true;
}

void PropertyRowUniformScale::serializeValue(Serialization::IArchive& ar)
{
	ar(m_uniform, "uniform");
}

namespace Serialization
{
REGISTER_PROPERTY_ROW(SUniformScale, PropertyRowUniformScale);
}


// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PropertyRowLocalFrame.h"
#include <QAction>
#include <QMenu>
#include <Serialization/PropertyTree/PropertyTreeModel.h>
#include <Serialization/PropertyTree/IMenu.h>
#include <Serialization/PropertyTree/IDrawContext.h>
#include <Serialization/Decorators/IGizmoSink.h>
#include <CrySerialization/Decorators/LocalFrame.h>
#include <CrySerialization/ClassFactory.h>
#include "Serialization.h"
#include <CrySerialization/Decorators/LocalFrameImpl.h>

using Serialization::LocalPosition;

void LocalFrameMenuHandler::onMenuReset()
{
	self->reset(tree);
}

PropertyRowLocalFrameBase::PropertyRowLocalFrameBase()
	: m_sink(0)
	, m_gizmoIndex(-1)
	, m_handle(0)
	, m_reset(false)
{
}

PropertyRowLocalFrameBase::~PropertyRowLocalFrameBase()
{
	m_sink = 0;
}

bool PropertyRowLocalFrameBase::onActivate(const PropertyActivationEvent& e)
{
	if (e.reason == e.REASON_RELEASE)
		return false;
	return false;
}

string PropertyRowLocalFrameBase::valueAsString() const
{
	return string();
}

bool PropertyRowLocalFrameBase::onContextMenu(IMenu& menu, PropertyTree* tree)
{
	yasli::SharedPtr<PropertyRow> selfPointer(this);

	LocalFrameMenuHandler* handler = new LocalFrameMenuHandler(tree, this);

	menu.addAction("Reset", 0, handler, &LocalFrameMenuHandler::onMenuReset);
	tree->addMenuHandler(handler);
	return true;
}

void PropertyRowLocalFrameBase::reset(PropertyTree* tree)
{
	tree->model()->rowAboutToBeChanged(this);
	m_reset = true;
	tree->model()->rowChanged(this);
}

void PropertyRowLocalFrameBase::redraw(property_tree::IDrawContext& context)
{
	context.drawIcon(context.widgetRect.adjusted(1, 1, 1, 1), property_tree::Icon("icons:Navigation/Coordinates_Local.ico"));
}

static void ResetTransform(Serialization::LocalPosition* l)    { *l->value = ZERO; }
static void ResetTransform(Serialization::LocalOrientation* l) { *l->value = IDENTITY; }
static void ResetTransform(Serialization::LocalFrame* l)       { *l->position = ZERO; *l->rotation = IDENTITY; }

template<class TLocal>
class PropertyRowLocalFrameImpl : public PropertyRowLocalFrameBase
{
public:
	void setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar) override
	{
		serializer_ = ser;

		TLocal* value = (TLocal*)ser.pointer();
		m_handle = value->handle;
		m_reset = false;

		if (label() && label()[0])
		{
			m_sink = ar.context<Serialization::IGizmoSink>();
			if (m_sink)
				m_gizmoIndex = m_sink->Write(*value, m_gizmoFlags, m_handle);
		}
	}

	void closeNonLeaf(const Serialization::SStruct& ser, Serialization::IArchive& ar) override
	{
		if (label() && label()[0] && ar.isInput())
		{
			TLocal& value = *((TLocal*)ser.pointer());
			if (m_sink)
			{
				if (m_sink->CurrentGizmoIndex() == m_gizmoIndex)
					m_sink->Read(&value, &m_gizmoFlags, m_handle);
				else
					m_sink->SkipRead();
			}

		}
	}

	bool assignTo(const Serialization::SStruct& ser) const
	{
		if (m_reset)
		{
			TLocal& value = *((TLocal*)ser.pointer());
			ResetTransform(&value);
		}
		return false;
	}

};

typedef PropertyRowLocalFrameImpl<Serialization::LocalPosition>    PropertyRowLocalPosition;
typedef PropertyRowLocalFrameImpl<Serialization::LocalOrientation> PropertyRowLocalOrientation;
typedef PropertyRowLocalFrameImpl<Serialization::LocalFrame>       PropertyRowLocalFrame;

REGISTER_PROPERTY_ROW(Serialization::LocalPosition, PropertyRowLocalPosition);
REGISTER_PROPERTY_ROW(Serialization::LocalOrientation, PropertyRowLocalOrientation);
REGISTER_PROPERTY_ROW(Serialization::LocalFrame, PropertyRowLocalFrame);


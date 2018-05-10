// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ComponentsModel.h"

#include "ScriptBrowserUtils.h"
#include "DetailWidget.h"

#include <CrySchematyc/Utils/Any.h>
#include <CrySchematyc/SerializationUtils/ISerializationContext.h>

#include <CryIcon.h>

#include "QtUtil.h"

namespace CrySchematycEditor {

CComponentItem::CComponentItem(Schematyc::IScriptComponentInstance& componentInstance, CAbstractComponentsModel& model)
	: m_componentInstance(componentInstance)
	, m_model(model)
{
	m_name = m_componentInstance.GetName();
}

CComponentItem::~CComponentItem()
{

}

void CComponentItem::SetName(QString name)
{
	Schematyc::CStackString uniqueName = QtUtil::ToString(name).c_str();
	Schematyc::ScriptBrowserUtils::MakeScriptElementNameUnique(uniqueName, m_model.GetScriptElement());

	m_componentInstance.SetName(uniqueName.c_str());
	m_name = m_componentInstance.GetName();
}

QString CComponentItem::GetDescription() const
{
	return "";
}

CryIcon CComponentItem::GetIcon() const
{
	return CryIcon(Schematyc::ScriptBrowserUtils::GetScriptElementIcon(m_componentInstance));
}

void CComponentItem::Serialize(Serialization::IArchive& archive)
{
	Schematyc::SSerializationContextParams serializationParams(archive, Schematyc::ESerializationPass::Edit);
	Schematyc::ISerializationContextPtr pSerializationContext = gEnv->pSchematyc->CreateSerializationContext(serializationParams);
	m_componentInstance.Serialize(archive);
	if (archive.isInput())
	{
		gEnv->pSchematyc->GetScriptRegistry().ElementModified(m_componentInstance);
	}
}

}


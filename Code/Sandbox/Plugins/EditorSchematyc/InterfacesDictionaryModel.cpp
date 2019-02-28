// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "InterfacesDictionaryModel.h"

#include "ScriptBrowserUtils.h"

#include <CrySchematyc/Reflection/TypeDesc.h>

#include <CrySchematyc/Script/IScriptView.h>
#include <CrySchematyc/Script/IScriptRegistry.h>
#include <CrySchematyc/Script/Elements/IScriptComponentInstance.h>

#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/IEnvElement.h>
#include <CrySchematyc/Env/Elements/IEnvComponent.h>
#include "ComponentsDictionaryModel.h"

namespace CrySchematycEditor {

CInterfaceDictionaryEntry::CInterfaceDictionaryEntry()
{

}

CInterfaceDictionaryEntry::~CInterfaceDictionaryEntry()
{

}

QVariant CInterfaceDictionaryEntry::GetColumnValue(int32 columnIndex) const
{
	switch (columnIndex)
	{
	case CComponentsDictionary::Column_Name:
		return QVariant::fromValue(m_name);
	default:
		break;
	}

	return QVariant();
}

QString CInterfaceDictionaryEntry::GetToolTip() const
{
	return m_description;
}

CInterfacesDictionary::CInterfacesDictionary(const Schematyc::IScriptElement* pScriptScope)
{
	if (pScriptScope)
	{
		m_interfaces.reserve(100);

		Schematyc::IScriptViewPtr pScriptView = gEnv->pSchematyc->CreateScriptView(pScriptScope->GetGUID());
		auto visitEnvInterface = [this, &pScriptView](const Schematyc::IEnvInterface& envInterface) -> Schematyc::EVisitStatus
		{
			Schematyc::CStackString fullName;
			pScriptView->QualifyName(envInterface, fullName);

			CInterfaceDictionaryEntry entry;
			entry.m_identifier = envInterface.GetGUID();
			entry.m_name = envInterface.GetName();
			entry.m_fullName = fullName.c_str();
			entry.m_description = envInterface.GetDescription();
			entry.m_domain = Schematyc::EDomain::Env;

			m_interfaces.emplace_back(entry);
			return Schematyc::EVisitStatus::Continue;
		};
		pScriptView->VisitEnvInterfaces(visitEnvInterface);
	}
}

CInterfacesDictionary::~CInterfacesDictionary()
{

}

const CAbstractDictionaryEntry* CInterfacesDictionary::GetEntry(int32 index) const
{
	if (index < m_interfaces.size())
	{
		return &m_interfaces[index];
	}

	return nullptr;
}

QString CInterfacesDictionary::GetColumnName(int32 index) const
{
	switch (index)
	{
	case Column_Name:
		return QString("Name");
	default:
		break;
	}

	return QString();
}

}


// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EntityObject.h"

#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>

class CEntityObjectWithComponent : public CEntityObject
{
public:
	DECLARE_DYNCREATE(CEntityObjectWithComponent)

	// CEntityObject
	virtual bool Init(CBaseObject* prev, const string& file) override;
	virtual bool CreateGameObject() override;
	// ~CEntityObject

protected:
	CryGUID m_componentGUID;
};

/*!
* Class Description of Entity with a default component
*/
class CEntityWithComponentClassDesc : public CObjectClassDesc
{
public:
	ObjectType          GetObjectType() { return OBJTYPE_ENTITY; }
	const char*         ClassName() { return "EntityWithComponent"; }
	const char*         Category() { return "Components"; }
	CRuntimeClass*      GetRuntimeClass() { return RUNTIME_CLASS(CEntityObjectWithComponent); }
	const char*         GetFileSpec() { return ""; }
	virtual const char* GetDataFilesFilterString() override { return ""; }
	void                EnumerateObjects(IObjectEnumerator* pEnumerator)
	{
		Schematyc::IEnvRegistry& registry = gEnv->pSchematyc->GetEnvRegistry();

		auto visitComponentsLambda = [pEnumerator](const Schematyc::IEnvComponent& component)
		{
			if (strlen(component.GetDesc().GetLabel()) == 0)
			{
				return Schematyc::EVisitStatus::Continue;
			}

			if (component.GetDesc().GetComponentFlags().Check(EEntityComponentFlags::HideFromInspector))
			{
				return Schematyc::EVisitStatus::Continue;
			}

			const char* szCategory = component.GetDesc().GetEditorCategory();
			if (szCategory == nullptr || szCategory[0] == '\0')
			{
				szCategory = "General";
			}

			char buffer[37];
			component.GetDesc().GetGUID().ToString(buffer);

			string sLabel = szCategory;
			sLabel.append("/");
			sLabel.append(component.GetDesc().GetLabel());

			pEnumerator->AddEntry(sLabel, buffer);

			return Schematyc::EVisitStatus::Continue;
		};

		registry.VisitComponents(visitComponentsLambda);
	}
};

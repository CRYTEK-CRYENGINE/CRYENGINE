// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EntityObjectWithComponent.h"

class CEntityObjectWithStaticMeshComponent : public CEntityObject
{
public:
	DECLARE_DYNCREATE(CEntityObjectWithStaticMeshComponent)

	// CEntityObject
	virtual bool Init(CBaseObject* prev, const string& file) override;
	virtual bool CreateGameObject() override;
	virtual bool ConvertFromObject(CBaseObject* object) override;
	// ~CEntityObject

protected:
	string m_file;
};

/*!
* Class Description of Entity with a default component
*/
class CEntityWithStaticMeshComponentClassDesc : public CObjectClassDesc
{
public:
	ObjectType          GetObjectType() { return OBJTYPE_ENTITY; }
	const char*         ClassName() { return "EntityWithStaticMeshComponent"; }
	const char*         Category() { return "Static Mesh Entity"; }
	CRuntimeClass*      GetRuntimeClass() { return RUNTIME_CLASS(CEntityObjectWithStaticMeshComponent); }
	const char*         GetFileSpec() { return "*.cgf"; }
	virtual const char* GetDataFilesFilterString() override { return GetFileSpec(); }
};


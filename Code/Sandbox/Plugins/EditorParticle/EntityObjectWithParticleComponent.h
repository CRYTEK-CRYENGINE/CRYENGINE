// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Objects/EntityObject.h"

class CEntityObjectWithParticleComponent : public CEntityObject
{
public:
	DECLARE_DYNCREATE(CEntityObjectWithParticleComponent)

	// CEntityObject
	virtual bool Init(CBaseObject* prev, const string& file) override;
	virtual bool CreateGameObject() override;
	// ~CEntityObject

protected:
	string m_file;
};

/*!
* Class Description of Entity with a default component
*/
class CEntityObjectWithParticleComponentClassDesc : public CObjectClassDesc
{
public:
	ObjectType          GetObjectType() { return OBJTYPE_ENTITY; }
	const char*         ClassName() { return "EntityWithParticleComponent"; }
	const char*         Category() { return "Particle Emitter"; }
	CRuntimeClass*      GetRuntimeClass() { return RUNTIME_CLASS(CEntityObjectWithParticleComponent); }
	const char*         GetFileSpec() { return "*.pfx;*.pfx2"; }
	virtual const char* GetDataFilesFilterString() override { return GetFileSpec(); }
	// Disallow creation from the Create Object panel, only use the Asset Browser
	virtual bool        IsCreatable() const { return false; }
};


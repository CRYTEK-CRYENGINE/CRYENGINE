// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EntityObject.h"

class CProximityTrigger : public CEntityObject
{
public:
	CProximityTrigger();
	DECLARE_DYNCREATE(CProximityTrigger)

	virtual void Display(CObjectRenderHelper& objRenderHelper) override;

protected:
	void  DeleteThis() { delete this; };
};

class CProximityTriggerClassDesc : public CObjectClassDesc
{
public:
	ObjectType          GetObjectType()   { return OBJTYPE_ENTITY; };
	const char*         ClassName()       { return "Entity::ProximityTrigger"; };
	const char*         Category()        { return ""; };
	CRuntimeClass*      GetRuntimeClass() { return RUNTIME_CLASS(CProximityTrigger); };
	virtual const char* GetTextureIcon()  { return "%EDITOR%/ObjectIcons/proximitytrigger.bmp"; };
	virtual bool   IsCreatable() const override { return gEnv->pEntitySystem->GetClassRegistry()->FindClass("ProximityTrigger") != nullptr; }
};

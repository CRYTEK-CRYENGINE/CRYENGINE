// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __aiwave_h__
#define __aiwave_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "EntityObject.h"

class CAIWaveObject : public CEntityObject
{
public:
	DECLARE_DYNCREATE(CAIWaveObject)

	virtual void InitVariables() {}

	void         SetName(const string& newName);

protected:
	//! Ctor must be protected.
	CAIWaveObject();

	void DeleteThis() { delete this; };
};

class CAIWaveObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_ENTITY; };
	const char*    ClassName()         { return "Entity::AIWave"; };
	const char*    Category()          { return ""; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CAIWaveObject); };
};

#endif // __aiwave_h__


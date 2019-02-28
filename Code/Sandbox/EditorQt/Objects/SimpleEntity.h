// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EntityObject.h"

class CSimpleEntity : public CEntityObject
{
public:
	DECLARE_DYNCREATE(CSimpleEntity)

	//////////////////////////////////////////////////////////////////////////
	bool    Init(CBaseObject* prev, const string& file);
	bool    ConvertFromObject(CBaseObject* object);

	void    Validate();
	bool    IsSimilarObject(CBaseObject* pObject);

	string GetGeometryFile() const;
	void    SetGeometryFile(const string& filename);

private:
	void OnFileChange(string filename);
};

/*!
 * Class Description of Entity
 */
class CSimpleEntityClassDesc : public CObjectClassDesc
{
public:
	ObjectType          GetObjectType()                     { return OBJTYPE_ENTITY; };
	const char*         ClassName()                         { return "SimpleEntity"; };
	const char*         Category()                          { return ""; };
	CRuntimeClass*      GetRuntimeClass()                   { return RUNTIME_CLASS(CSimpleEntity); };
	const char*         GetFileSpec()                       { return "*.cgf;*.chr;*.cga;*.cdf"; };
	virtual const char* GetDataFilesFilterString() override { return GetFileSpec(); }
};


// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __protentityobject_h__
#define __protentityobject_h__
#pragma once

#include "EntityObject.h"

class CEntityPrototype;
/*!
 *	Prototype entity object.
 *
 */
class CProtEntityObject : public CEntityObject
{
public:
	DECLARE_DYNCREATE(CProtEntityObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool       Init(CBaseObject* prev, const string& file);
	void       Done();
	bool       CreateGameObject();

	string    GetTypeDescription() const { return m_prototypeName; };
	string    GetLibrary() const         { return m_prototypeLibrary; }

	bool       HasMeasurementAxis() const { return true;  }

	void       Serialize(CObjectArchive& ar);

	XmlNodeRef Export(const string& levelPath, XmlNodeRef& xmlNode);
	//////////////////////////////////////////////////////////////////////////

	void SetPrototype(CryGUID guid, bool bForceReload = false);
	void SetPrototype(CEntityPrototype* prototype, bool bForceReload, bool bOnlyDisabled = false);

protected:
	//! Dtor must be protected.
	CProtEntityObject();

	virtual void SpawnEntity();

	virtual void PostSpawnEntity();

	//! Callback called by prototype when its updated.
	void OnPrototypeUpdate();
	void SyncVariablesFromPrototype(bool bOnlyDisabled = false);

	//! Entity prototype name.
	string m_prototypeName;
	//! Library that contains the prototype.
	string m_prototypeLibrary;
	//! Full prototype name.
	CryGUID m_prototypeGUID;
};

/*!
 * Class Description of StaticObject
 */
class CProtEntityObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()   { return OBJTYPE_ENTITY; };
	const char*    ClassName()       { return "EntityArchetype"; };
	const char*    Category()        { return "Archetype Entity"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CProtEntityObject); };

	//! Select all entity prototypes.
	//! ObjectTreeBrowser object can recognize this hardcoded name.
	const char*          GetFileSpec()                       { return "*EntityArchetype"; };
	virtual const char*  GetDataFilesFilterString() override { return ""; }
	virtual void         EnumerateObjects(IObjectEnumerator* pEnumerator) override;
	virtual bool         IsPreviewable() const override      { return true; }
	virtual SPreviewDesc GetPreviewDesc(const char* id) const override;
	virtual bool         IsCreatable() const override;
};

#endif // __protentityobject_h__


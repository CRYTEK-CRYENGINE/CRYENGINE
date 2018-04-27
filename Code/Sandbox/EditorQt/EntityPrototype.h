// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __centityprototype_h__
#define __centityprototype_h__
#pragma once

#include "BaseLibraryItem.h"
#include "Objects/EntityScript.h"

class CEntityPrototypeLibrary;
class CEntityScript;
struct IEntityArchetype;

//////////////////////////////////////////////////////////////////////////
/** Prototype of entity, contain specified entity properties.
 */
class SANDBOX_API CEntityPrototype : public CBaseLibraryItem
{
public:
	typedef Functor0 UpdateCallback;

	CEntityPrototype();
	~CEntityPrototype();

	virtual EDataBaseItemType GetType() const { return EDB_TYPE_ENTITY_ARCHETYPE; };

	//! Set prototype description.
	void          SetDescription(const string& description) { m_description = description; };
	//! Get prototype description.
	const string& GetDescription() const                    { return m_description; };

	//! Set class name of entity.
	void              SetEntityClassName(const string& className);
	//! Get class name of entity.
	const string&     GetEntityClassName() const  { return m_className; }

	IEntityArchetype* GetIEntityArchetype() const { return m_pArchetype; }

	//! Reload entity class.
	void Reload();

	//! Return properties of entity.
	CVarBlock*     GetProperties();
	//! Get entity script of this prototype.
	CEntityScript* GetScript();

	CVarBlock*     GetObjectVarBlock() { return m_pObjectVarBlock; };

	QString        GetMaterial() const;
	QString        GetVisualObject() const;

	//////////////////////////////////////////////////////////////////////////
	//! Serialize prototype to xml.
	virtual void Serialize(SerializeContext& ctx);
	void         SerializePrototype(SerializeContext& ctx, bool bRecreateArchetype);

	//////////////////////////////////////////////////////////////////////////
	// Update callback.
	//////////////////////////////////////////////////////////////////////////

	void AddUpdateListener(UpdateCallback cb);
	void RemoveUpdateListener(UpdateCallback cb);

	//! Called after prototype is updated.
	void Update();

private:
	//! Name of entity class name.
	string       m_className;
	//! Description of this prototype.
	string       m_description;
	//! Entity properties.
	CVarBlockPtr m_properties;

	CVarBlockPtr m_pObjectVarBlock;

	// Pointer to entity script.
	std::shared_ptr<CEntityScript> m_script;

	//! List of update callbacks.
	std::list<UpdateCallback> m_updateListeners;

	IEntityArchetype*         m_pArchetype;
};

TYPEDEF_AUTOPTR(CEntityPrototype);

#endif // __centityprototype_h__


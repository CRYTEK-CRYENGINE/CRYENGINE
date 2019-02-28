// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SandboxAPI.h"
#include <CryEntitySystem/IEntityClass.h>
#include <CryScriptSystem/IScriptSystem.h>

#define TSmartPtr             _smart_ptr

#define PROPERTIES_TABLE      "Properties"
#define PROPERTIES2_TABLE     "PropertiesInstance"
#define COMMONPARAMS_TABLE    "CommonParams"
#define FIRST_ENTITY_CLASS_ID 200

// forward declaration
class CEntityObject;
struct IScriptTable;
struct IScriptObject;

#define EVENT_PREFIX "Event_"

//////////////////////////////////////////////////////////////////////////
enum EEntityScriptFlags
{
	ENTITY_SCRIPT_SHOWBOUNDS     = 0x0001,
	ENTITY_SCRIPT_ABSOLUTERADIUS = 0x0002,
	ENTITY_SCRIPT_DISPLAY_ARROW  = 0x0008,
	ENTITY_SCRIPT_ISNOTSCALABLE  = 0x0010,
	ENTITY_SCRIPT_ISNOTROTATABLE = 0x0020,
};

/*!
 *  CEntityScript holds information about Entity lua script.
 */
class SANDBOX_API CEntityScript
{
public:
	CEntityScript(IEntityClass* pClass);
	virtual ~CEntityScript();

	//////////////////////////////////////////////////////////////////////////
	void SetClass(IEntityClass* pClass);

	//////////////////////////////////////////////////////////////////////////
	IEntityClass* GetClass() const { return m_pClass; }

	//! Get name of entity script.
	string GetName() const;
	// Get file of script.
	string GetFile() const;

	//////////////////////////////////////////////////////////////////////////
	// Flags.
	int           GetFlags() const           { return m_nFlags; }

	int           GetMethodCount() const     { return m_methods.size(); }
	const string& GetMethod(int index) const { return m_methods[index]; }

	//////////////////////////////////////////////////////////////////////////
	int         GetEventCount();
	const char* GetEvent(int i);

	//////////////////////////////////////////////////////////////////////////
	int           GetEmptyLinkCount();
	const string& GetEmptyLink(int i);

	bool          Load();
	void          Reload();
	bool          IsValid() const { return m_bValid; }

	//! Marks script not valid, must be loaded on next access.
	void Invalidate() { m_bValid = false; }

	//! Get default properties of this script.
	CVarBlock* GetDefaultProperties()  { return m_pDefaultProperties; }
	CVarBlock* GetDefaultProperties2() { return m_pDefaultProperties2; }

	//! Takes current values of var block and copies them to entity script table.
	void CopyPropertiesToScriptTable(IEntity* pEntity, CVarBlock* pVarBlock, bool bCallUpdate);
	void CopyProperties2ToScriptTable(IEntity* pEntity, CVarBlock* pVarBlock, bool bCallUpdate);
	void CallOnPropertyChange(IEntity* pEntity);

	// Takes current values from script table and copies it to respective properties var block
	void CopyPropertiesFromScriptTable(IEntity* pEntity, CVarBlock* pVarBlock);
	void CopyProperties2FromScriptTable(IEntity* pEntity, CVarBlock* pVarBlock);

	//! Setup entity target events table
	void SetEventsTable(CEntityObject* pEntity);

	//! Run method.
	void RunMethod(IEntity* pEntity, const string& method);
	void SendEvent(IEntity* pEntity, const string& event);

	// Edit methods.
	void GotoMethod(const string& method);
	void AddMethod(const string& method);

	// Updates the texture icon with any overrides from the entity.  Should be called
	// after any property change.
	void UpdateTextureIcon(IEntity* pEntity);

	//! Check if entity of this class can be used in editor.
	bool IsUsable() const { return m_usable; }

	// Set class as placeable or not.
	void SetUsable(bool usable) { m_usable = usable; }

	// Get client override for display path in editor entity treeview
	const string& GetDisplayPath();

private:
	void CopyPropertiesToScriptInternal(IEntity* pEntity, CVarBlock* pVarBlock, bool bCallUpdate, const char* tableKey);
	void CopyPropertiesFromScriptInternal(IEntity* pEntity, CVarBlock* pVarBlock, const char* tableKey);

	bool ParseScript();
	int  FindLineNum(const string& line);

	//! Copy variable to script table
	void VarToScriptTable(IVariable* pVariable, IScriptTable* pScriptTable);

	//! Get variable from script table
	void ScriptTableToVar(IScriptTable* pScriptTable, IVariable* pVariable);

	IEntityClass* m_pClass;

	bool          m_bValid;

	bool          m_haveEventsTable;

	//! True if entity script have update entity
	bool   m_bUpdatePropertiesImplemented;

	int    m_visibilityMask;

	bool   m_usable;

	//! Array of methods in this script.
	std::vector<string> m_methods;

	//! Array of events supported by this script.
	std::vector<string> m_events;

	// Create empty links.
	std::vector<string>  m_emptyLinks;

	int                  m_nFlags;
	TSmartPtr<CVarBlock> m_pDefaultProperties;
	TSmartPtr<CVarBlock> m_pDefaultProperties2;

	HSCRIPTFUNCTION      m_pOnPropertyChangedFunc;
	string               m_userPath;
};

/*!
 *	CEntityScriptRegistry	manages all known CEntityScripts instances.
 */
class CEntityScriptRegistry : public IEntityClassRegistryListener, public IEditorNotifyListener
{
public:
	CEntityScriptRegistry();
	~CEntityScriptRegistry();

	// IEntityClassRegistryListener
	void OnEntityClassRegistryEvent(EEntityClassRegistryEvent event, const IEntityClass* pEntityClass);
	// ~IEntityClassRegistryListener

	// IEditorNotifyListener
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override;
	// ~IEditorNotifyListener

	std::shared_ptr<CEntityScript> Find(const char* szName) const;
	std::shared_ptr<CEntityScript> Insert(IEntityClass* pClass, const char* szAlias = "");

	void                           LoadScripts();
	void                           Reload();

	//! Get all scripts as array.
	void                          IterateOverScripts(std::function<void(CEntityScript&)> callback);

	static CEntityScriptRegistry* Instance();
	static void                   Release();

	CCrySignal<void(CEntityScript*)> m_scriptAdded;
	CCrySignal<void(CEntityScript*)> m_scriptChanged;
	CCrySignal<void(CEntityScript*)> m_scriptRemoved;

private:

	void SetClassCategory(CEntityScript* script);

	bool m_needsScriptReload;

	std::unordered_map<string, std::shared_ptr<CEntityScript>, stl::hash_strcmp<string>> m_scripts;

	static CEntityScriptRegistry* m_instance;
};


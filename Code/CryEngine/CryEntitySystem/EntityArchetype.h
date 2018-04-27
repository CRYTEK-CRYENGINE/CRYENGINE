// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryScriptSystem/IScriptSystem.h>

class CEntityClass;

//////////////////////////////////////////////////////////////////////////
class CEntityArchetype : public IEntityArchetype, public _i_reference_target_t
{
public:
	explicit CEntityArchetype(IEntityClass* pClass);

	//////////////////////////////////////////////////////////////////////////
	// IEntityArchetype
	//////////////////////////////////////////////////////////////////////////
	virtual IEntityClass* GetClass() const override { return m_pClass; }
	virtual const char*   GetName() const override  { return m_name.c_str(); }
	virtual IScriptTable* GetProperties() override  { return m_pProperties; }
	virtual XmlNodeRef    GetObjectVars() override  { return m_ObjectVars; }
	virtual void          LoadFromXML(XmlNodeRef& propertiesNode, XmlNodeRef& objectVarsNode) override;
	//////////////////////////////////////////////////////////////////////////

	void SetName(const string& sName) { m_name = sName; };

private:
	string           m_name;
	SmartScriptTable m_pProperties;
	XmlNodeRef       m_ObjectVars;
	IEntityClass*    m_pClass;
};

//////////////////////////////////////////////////////////////////////////
// Manages collection of the entity archetypes.
//////////////////////////////////////////////////////////////////////////
class CEntityArchetypeManager
{
public:
	CEntityArchetypeManager();

	IEntityArchetype*                 CreateArchetype(IEntityClass* pClass, const char* sArchetype);
	IEntityArchetype*                 FindArchetype(const char* sArchetype);
	IEntityArchetype*                 LoadArchetype(const char* sArchetype);
	void                              UnloadArchetype(const char* sArchetype);

	void                              Reset();

	void                              SetEntityArchetypeManagerExtension(IEntityArchetypeManagerExtension* pEntityArchetypeManagerExtension);
	IEntityArchetypeManagerExtension* GetEntityArchetypeManagerExtension() const;

private:
	bool   LoadLibrary(const string& library);
	string GetLibraryFromName(const string& sArchetypeName);

	typedef std::map<const char*, _smart_ptr<CEntityArchetype>, stl::less_stricmp<const char*>> ArchetypesNameMap;
	ArchetypesNameMap                 m_nameToArchetypeMap;

	DynArray<string>                  m_loadedLibs;

	IEntityArchetypeManagerExtension* m_pEntityArchetypeManagerExtension;
};

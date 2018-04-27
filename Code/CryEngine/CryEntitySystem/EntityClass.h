// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntityClass.h>

#include <CrySchematyc/Runtime/IRuntimeRegistry.h>

//////////////////////////////////////////////////////////////////////////
// Description:
//    Standard implementation of the IEntityClass interface.
//////////////////////////////////////////////////////////////////////////
class CEntityClass : public IEntityClass
{
public:
	CEntityClass();
	virtual ~CEntityClass();

	//////////////////////////////////////////////////////////////////////////
	// IEntityClass interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void                      Release() override                { delete this; };

	virtual uint32                    GetFlags() const override         { return m_nFlags; };
	virtual void                      SetFlags(uint32 nFlags) override  { m_nFlags = nFlags; };

	virtual const char*               GetName() const override          { return m_sName.c_str(); }
	virtual CryGUID                   GetGUID() const final             { return m_guid; };
	virtual const char*               GetScriptFile() const override    { return m_sScriptFile.c_str(); }

	virtual IEntityScript*            GetIEntityScript() const override { return m_pEntityScript; }
	virtual IScriptTable*             GetScriptTable() const override;
	virtual bool                      LoadScript(bool bForceReload) override;
	virtual UserProxyCreateFunc       GetUserProxyCreateFunc() const override { return m_pfnUserProxyCreate; };
	virtual void*                     GetUserProxyData() const override       { return m_pUserProxyUserData; };

	virtual IEntityEventHandler*      GetEventHandler() const override;
	virtual IEntityScriptFileHandler* GetScriptFileHandler() const override;

	virtual const SEditorClassInfo&  GetEditorClassInfo() const override;
	virtual void                     SetEditorClassInfo(const SEditorClassInfo& editorClassInfo) override;

	virtual int                      GetEventCount() override;
	virtual IEntityClass::SEventInfo GetEventInfo(int nIndex) override;
	virtual bool                     FindEventInfo(const char* sEvent, SEventInfo& event) override;

	virtual const OnSpawnCallback&   GetOnSpawnCallback() const override { return m_onSpawnCallback; };

	//////////////////////////////////////////////////////////////////////////

	void                             SetClassDesc(const IEntityClassRegistry::SEntityClassDesc& classDesc);

	void                             SetName(const char* sName);
	void                             SetGUID(const CryGUID& guid);
	void                             SetScriptFile(const char* sScriptFile);
	void                             SetEntityScript(IEntityScript* pScript);

	void                             SetUserProxyCreateFunc(UserProxyCreateFunc pFunc, void* pUserData = NULL);
	void                             SetEventHandler(IEntityEventHandler* pEventHandler);
	void                             SetScriptFileHandler(IEntityScriptFileHandler* pScriptFileHandler);

	void                             SetOnSpawnCallback(const OnSpawnCallback& callback);

	Schematyc::IRuntimeClassConstPtr GetSchematycRuntimeClass() const;

	void                             GetMemoryUsage(ICrySizer* pSizer) const override
	{
		pSizer->AddObject(m_sName);
		pSizer->AddObject(m_sScriptFile);
		pSizer->AddObject(m_pEntityScript);
		pSizer->AddObject(m_pEventHandler);
		pSizer->AddObject(m_pScriptFileHandler);
	}
private:
	uint32                                   m_nFlags;
	string                                   m_sName;
	CryGUID                                  m_guid;
	string                                   m_sScriptFile;
	IEntityScript*                           m_pEntityScript;

	UserProxyCreateFunc                      m_pfnUserProxyCreate;
	void*                                    m_pUserProxyUserData;

	bool                                     m_bScriptLoaded;

	IEntityEventHandler*                     m_pEventHandler;
	IEntityScriptFileHandler*                m_pScriptFileHandler;

	SEditorClassInfo                         m_EditorClassInfo;

	OnSpawnCallback                          m_onSpawnCallback;
	CryGUID                                  m_schematycRuntimeClassGuid;

	IFlowNodeFactory*                        m_pIFlowNodeFactory = nullptr;

	mutable Schematyc::IRuntimeClassConstPtr m_pSchematycRuntimeClass = nullptr;
};

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryFlowGraph/IFlowGraphModuleManager.h>
#include <CryGame/IGameFramework.h>
#include <CryCore/Containers/CryListenerSet.h>
#include <CryCore/Containers/VectorMap.h>

struct SActivationInfo;
class CFlowGraphModule;
class CFlowModuleCallNode;

class CFlowGraphModuleManager : public IFlowGraphModuleManager, public ISystemEventListener, public IGameFrameworkListener
{
public:
	CFlowGraphModuleManager();
	virtual ~CFlowGraphModuleManager();
	CFlowGraphModuleManager(CFlowGraphModuleManager const&) = delete;
	CFlowGraphModuleManager& operator=(CFlowGraphModuleManager const&) = delete;
	CFlowGraphModuleManager(CFlowGraphModuleManager &&) = delete;
	CFlowGraphModuleManager& operator=(CFlowGraphModuleManager&& other) = delete;

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	// ~ISystemEventListener

	// IGameFrameworkListener
	virtual void OnPostUpdate(float fDeltaTime);
	virtual void OnSaveGame(ISaveGame* pSaveGame)         {}
	virtual void OnLoadGame(ILoadGame* pLoadGame)         {}
	virtual void OnLevelEnd(const char* nextLevel)        {}
	virtual void OnActionEvent(const SActionEvent& event) {}
	// ~IGameFrameworkListener

	// IFlowGraphModuleManager
	virtual bool               RegisterListener(IFlowGraphModuleListener* pListener, const char* name);
	virtual void               UnregisterListener(IFlowGraphModuleListener* pListener);

	virtual bool               DeleteModuleXML(const char* moduleName);
	virtual bool               RenameModuleXML(const char* moduleName, const char* newName);

	virtual const char*        GetModulePath(const char* name);

	virtual bool               SaveModule(const char* moduleName, XmlNodeRef saveTo);
	IFlowGraphModule*          LoadModuleFile(const char* moduleName, const char* fileName, bool bGlobal);

	virtual IFlowGraphModule*  GetModule(IFlowGraphPtr pFlowgraph) const;
	virtual IFlowGraphModule*  GetModule(const char* moduleName) const;
	virtual IFlowGraphModule*  GetModule(TModuleId id) const;

	virtual const char*        GetStartNodeName(const char* moduleName) const;
	virtual const char*        GetReturnNodeName(const char* moduleName) const;
	virtual const char*        GetCallerNodeName(const char* moduleName) const;

	virtual void               ClearModules();
	virtual void               ClearLevelModules();
	virtual void               ScanAndReloadModules(bool bScanGlobalModules, bool bScanLevelModules, const char* szLoadedLevelName);


	virtual bool               CreateModuleNodes(const char* moduleName, TModuleId moduleId);

	virtual IModuleIteratorPtr CreateModuleIterator();
	// ~IFlowGraphModuleManager

	void Shutdown();

	void DestroyActiveModuleInstances();
	void RemoveCompletedModuleInstances(); // cleanup at end of update
	void ClearModuleRequestedInstances();

	void BroadcastModuleInstanceStarted(IFlowGraphModule* module, TModuleInstanceId instanceID);
	void BroadcastModuleInstanceFinished(IFlowGraphModule* module, TModuleInstanceId instanceID);

	DeclareStaticConstIntCVar(CV_fg_debugmodules, 0);
	ICVar* fg_debugmodules_filter;

private:
	typedef VectorMap<TModuleId, CFlowGraphModule*> TModuleMap;

	void              DestroyModule(TModuleMap::iterator& itModule);

	void              RescanModuleNames(bool bGlobal, const char* szLoadedLevelName);
	void              ScanFolder(const string& folderName, bool bGlobal);

	CFlowGraphModule* PreLoadModuleFile(const char* moduleName, const char* fileName, bool bGlobal);
	void              LoadModuleGraph(const char* moduleName, const char* fileName, IFlowGraphModuleListener::ERootGraphChangeReason rootGraphChangeReason);

	// Loaded modules
	TModuleMap m_Modules;
	TModuleId  m_nextModuleId;

	// Module name-ID map
	typedef std::unordered_map<string, TModuleId, stl::hash_strcmp<string>> TModuleIdMap;
	TModuleIdMap m_ModuleIds;

	// Module name-path map
	typedef std::unordered_map<string, string, stl::hash_strcmp<string>> TModulesPathInfo;
	TModulesPathInfo m_ModulesPathInfo;

	// Listeners
	CListenerSet<IFlowGraphModuleListener*> m_listeners;

	// Module Iterator
	class CModuleIterator : public IFlowGraphModuleIterator
	{
	public:
		CModuleIterator(CFlowGraphModuleManager* pMM)
		{
			m_pModuleManager = pMM;
			m_cur = m_pModuleManager->m_Modules.begin();
			m_nRefs = 0;
		}

		void AddRef()
		{
			++m_nRefs;
		}

		void Release()
		{
			if (--m_nRefs <= 0)
			{
				this->~CModuleIterator();
				m_pModuleManager->m_iteratorPool.push_back(this);
			}
		}

		IFlowGraphModule* Next();

		size_t            Count()
		{
			return m_pModuleManager->m_Modules.size();
		}

		CFlowGraphModuleManager* m_pModuleManager;
		TModuleMap::iterator     m_cur;
		int                      m_nRefs;
	};

	std::vector<CModuleIterator*> m_iteratorPool;

};

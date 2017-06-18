// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ISystem.h>
#include <CryExtension/ClassWeaver.h>
#include <CryExtension/ICryUnknown.h>
#include <CryExtension/ICryFactoryRegistryImpl.h>
#include <CryFlowGraph/IFlowBaseNode.h>

struct SSystemInitParams;

struct IPluginUpdateListener
{
	enum EPluginUpdateType
	{
		EUpdateType_NoUpdate         = BIT(0),
		EUpdateType_PrePhysicsUpdate = BIT(1),
		EUpdateType_Update           = BIT(2)
	};

	virtual ~IPluginUpdateListener() {}
	virtual void OnPluginUpdate(EPluginUpdateType updateType) = 0;
};

struct ICryPlugin : public ICryUnknown, IPluginUpdateListener, IAutoCleanup
{
	friend class CCryPluginManager;

	enum EPluginPriority : int16
	{
		EPluginPriority_CryUnload = -100,	// Lowest priority
		EPluginPriority_Default = 0,
		EPluginPriority_CryLoad = 100	// Highest priority
	};

	CRYINTERFACE_DECLARE(ICryPlugin, 0xF491A0DB38634FCA, 0xB6E6BCFE2D98EEA2);

	virtual ~ICryPlugin() {}

	//! Retrieve name of the plugin.
	virtual const char* GetName() const = 0;

	//! Retrieve category of the plugin.
	virtual const char* GetCategory() const = 0;

	//! This is called to initialize the new plugin.
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) = 0;

	virtual bool RegisterFlowNodes() { return false; }
	virtual bool UnregisterFlowNodes() { return false; }

	uint8 GetUpdateFlags() const { return m_updateFlags; }
	
	void SetUpdateFlags(uint8 flags) { m_updateFlags = flags; }

protected:
	uint8 m_updateFlags = IPluginUpdateListener::EUpdateType_NoUpdate;
	std::vector<TFlowNodeTypeId> m_registeredFlowNodeIds;
	int16 m_loadPriority = EPluginPriority_Default;
	int16 m_unloadPriority = EPluginPriority_Default;
};

#ifndef _LIB
	#define PLUGIN_FLOWNODE_REGISTER                                                                    \
	  virtual bool RegisterFlowNodes() override                                                         \
	  {                                                                                                 \
	    m_registeredFlowNodeIds.clear();                                                                \
	                                                                                                    \
	    if (auto pFlowSystem = gEnv->pFlowSystem)                                                       \
		{                                                                                               \
	      CAutoRegFlowNodeBase* pFactory = CAutoRegFlowNodeBase::s_pFirst;                              \
	      for (auto pFactory = CAutoRegFlowNodeBase::s_pFirst; pFactory; pFactory = pFactory->m_pNext)  \
	      {                                                                                             \
	        TFlowNodeTypeId nodeId = pFlowSystem->RegisterType(pFactory->m_szClassName, pFactory);      \
	        m_registeredFlowNodeIds.push_back(nodeId);                                                  \
	        CryLog("Successfully registered flownode '%s'", pFactory->m_szClassName);                   \
	      }                                                                                             \
	    }                                                                                               \
	                                                                                                    \
	    return m_registeredFlowNodeIds.empty();                                                         \
	  }
#else
	#define PLUGIN_FLOWNODE_REGISTER
#endif

#ifndef _LIB
	#define PLUGIN_FLOWNODE_UNREGISTER                                         \
	  virtual bool UnregisterFlowNodes() override                              \
	  {                                                                        \
	    bool bSuccess = true;                                                  \
	    const size_t numFlownodes = m_registeredFlowNodeIds.size();            \
	                                                                           \
	    if (auto pFlowSystem = gEnv->pFlowSystem)                              \
	    {                                                                      \
	      for (auto nodeId : m_registeredFlowNodeIds)                          \
	      {                                                                    \
	        const char* szNameBuffer = gEnv->pFlowSystem->GetTypeName(nodeId); \
	        CryLog("Unregistering flownode '%s'", szNameBuffer);               \
	        if (!pFlowSystem->UnregisterType(szNameBuffer))                    \
	        {                                                                  \
	          bSuccess = false;                                                \
	          CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR,             \
	                     "Error unregistering flownode '%s'", szNameBuffer);   \
	        }                                                                  \
	      }                                                                    \
	    }                                                                      \
	    m_registeredFlowNodeIds.clear();                                       \
	    return bSuccess;                                                       \
	  }
#else
	#define PLUGIN_FLOWNODE_UNREGISTER
#endif

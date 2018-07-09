// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Replace CRY_ASSERT with SCHEMATYC2_SYSTEM_ASSERT.

#include "StdAfx.h"
#include "EnvRegistry.h"

#include <CrySerialization/IArchiveHost.h>
#include <CrySchematyc2/IActionFactory.h>
#include <CrySchematyc2/IComponentFactory.h>
#include <CrySchematyc2/EnvTypeDesc.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>
#include <CrySchematyc2/Env/IEnvFunctionDescriptor.h>

#include "AbstractInterface.h"
#include "Foundation.h"
#include "Signal.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	bool CEnvRegistry::RegisterTypeDesc(const IEnvTypeDescPtr& pTypeDesc)
	{
		CRY_ASSERT(pTypeDesc);
		if(pTypeDesc)
		{
			const EnvTypeId id = pTypeDesc->GetEnvTypeId();
			const SGUID     guid = pTypeDesc->GetGUID();
			CRY_ASSERT((id != GetEnvTypeId<void>()) && guid);
			if((id != GetEnvTypeId<void>()) && guid)
			{
				if(!GetTypeDesc(id) && !GetTypeDesc(guid))
				{
					m_typeDescsById.insert(TypeDescByIdMap::value_type(id, pTypeDesc));
					m_typeDescsByGUID.insert(TypeDescByGUIDMap::value_type(guid, pTypeDesc));
					return true;
				}
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	const IEnvTypeDesc* CEnvRegistry::GetTypeDesc(const EnvTypeId& id) const
	{
		TypeDescByIdMap::const_iterator itTypeDesc = m_typeDescsById.find(id);
		return itTypeDesc != m_typeDescsById.end() ? itTypeDesc->second.get() : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	const IEnvTypeDesc* CEnvRegistry::GetTypeDesc(const SGUID& guid) const
	{
		TypeDescByGUIDMap::const_iterator itTypeDesc = m_typeDescsByGUID.find(guid);
		return itTypeDesc != m_typeDescsByGUID.end() ? itTypeDesc->second.get() : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::VisitTypeDescs(const EnvTypeDescVisitor& visitor) const
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(const TypeDescByIdMap::value_type& typeDesc : m_typeDescsById)
			{
				if(visitor(*typeDesc.second) != EVisitStatus::Continue)
				{
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	ISignalPtr CEnvRegistry::CreateSignal(const SGUID& guid)
	{
		if(GetSignal(guid) == NULL)
		{
			ISignalPtr	pSignal(new CSignal(guid));
			m_signals.insert(TSignalMap::value_type(guid, pSignal));
			return pSignal;
		}
		return ISignalPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	ISignalConstPtr CEnvRegistry::GetSignal(const SGUID& guid) const
	{
		TSignalMap::const_iterator	iSignal = m_signals.find(guid);
		return iSignal != m_signals.end() ? iSignal->second : ISignalConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::VisitSignals(const EnvSignalVisitor& visitor) const
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TSignalMap::const_iterator iSignal = m_signals.begin(), iEndSignal = m_signals.end(); iSignal != iEndSignal; ++ iSignal)
			{
				if(visitor(iSignal->second) != EVisitStatus::Continue)
				{
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CEnvRegistry::RegisterGlobalFunction(const IGlobalFunctionPtr& pFunction)
	{
		CRY_ASSERT(pFunction != NULL);
		if(pFunction != NULL)
		{
			const SGUID	guid = pFunction->GetGUID();
			CRY_ASSERT(guid);
			if(guid && !GetGlobalFunction(guid))
			{
				m_globalFunctions.insert(TGlobalFunctionMap::value_type(guid, pFunction));
				return true;
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	IGlobalFunctionConstPtr CEnvRegistry::GetGlobalFunction(const SGUID& guid) const
	{
		TGlobalFunctionMap::const_iterator	iGlobalFunction = m_globalFunctions.find(guid);
		return iGlobalFunction != m_globalFunctions.end() ? iGlobalFunction->second : IGlobalFunctionConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::VisitGlobalFunctions(const EnvGlobalFunctionVisitor& visitor) const
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TGlobalFunctionMap::const_iterator iGlobalFunction = m_globalFunctions.begin(), iEndGlobalFunction = m_globalFunctions.end(); iGlobalFunction != iEndGlobalFunction; ++ iGlobalFunction)
			{
				if(visitor(iGlobalFunction->second) != EVisitStatus::Continue)
				{
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::RegisterFunction(const IEnvFunctionDescriptorPtr& pFunctionDescriptor)
	{
		SCHEMATYC2_SYSTEM_ASSERT_FATAL(pFunctionDescriptor);
		const SGUID guid = pFunctionDescriptor->GetGUID();
		SCHEMATYC2_SYSTEM_ASSERT_FATAL(!GetFunction(guid));
		m_functions.insert(Functions::value_type(guid, pFunctionDescriptor));
	}

	//////////////////////////////////////////////////////////////////////////
	const IEnvFunctionDescriptor* CEnvRegistry::GetFunction(const SGUID& guid) const
	{
		Functions::const_iterator itFunction = m_functions.find(guid);
		return itFunction != m_functions.end() ? itFunction->second.get() : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::VisitFunctions(const EnvFunctionVisitor& visitor) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(const Functions::value_type& function : m_functions)
			{
				if(visitor(*function.second) != EVisitStatus::Continue)
				{
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	IFoundationPtr CEnvRegistry::CreateFoundation(const SGUID& guid, const char* name)
	{
		if(GetFoundation(guid) == NULL)
		{
			IFoundationPtr	pFoundation(new CFoundation(guid, name));
			m_foundations.insert(TFoundationMap::value_type(guid, pFoundation));
			return pFoundation;
		}
		return IFoundationPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	IFoundationConstPtr CEnvRegistry::GetFoundation(const SGUID& guid) const
	{
		TFoundationMap::const_iterator itFoundation = m_foundations.find(guid);
		return itFoundation != m_foundations.end() ? itFoundation->second : IFoundationConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::VisitFoundations(const EnvFoundationVisitor& visitor, EVisitFlags flags) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(const TFoundationMap::value_type& foundation : m_foundations)
			{
				if(((flags & EVisitFlags::IgnoreBlacklist) != 0) || !IsBlacklistedElement(foundation.first))
				{
					if(visitor(foundation.second) != EVisitStatus::Continue)
					{
						break;
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	IAbstractInterfacePtr CEnvRegistry::CreateAbstractInterface(const SGUID& guid)
	{
		return IAbstractInterfacePtr(new CAbstractInterface(guid));
	}

	//////////////////////////////////////////////////////////////////////////
	bool CEnvRegistry::RegisterAbstractInterface(const IAbstractInterfacePtr& pAbstractInterface)
	{
		CRY_ASSERT(pAbstractInterface);
		if(pAbstractInterface)
		{
			const SGUID	guid = pAbstractInterface->GetGUID();
			CRY_ASSERT(guid);
			if(guid && !GetAbstractInterface(guid))
			{
				bool					error = false;
				const size_t	functionCount = pAbstractInterface->GetFunctionCount();
				for(size_t iFunction = 0; iFunction < functionCount; ++ iFunction)
				{
					IAbstractInterfaceFunctionConstPtr	pFunction = pAbstractInterface->GetFunction(iFunction);
					if(!pFunction || GetAbstractInterfaceFunction(pAbstractInterface->GetFunction(iFunction)->GetGUID()) || (pFunction->GetOwnerGUID() != guid))
					{
						error = true;
						break;
					}
				}
				if(!error)
				{
					m_abstractInterfaces.insert(TAbstractInterfaceMap::value_type(guid, pAbstractInterface));
					for(size_t iFunction = 0; iFunction < functionCount; ++ iFunction)
					{
						IAbstractInterfaceFunctionConstPtr	pFunction = pAbstractInterface->GetFunction(iFunction);
						m_abstractInterfaceFunctions.insert(TAbstractInterfaceFunctionMap::value_type(pFunction->GetGUID(), pFunction));
					}
					return true;
				}
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	IAbstractInterfaceConstPtr CEnvRegistry::GetAbstractInterface(const SGUID& guid) const
	{
		TAbstractInterfaceMap::const_iterator	iAbstractInterface = m_abstractInterfaces.find(guid);
		return iAbstractInterface != m_abstractInterfaces.end() ? iAbstractInterface->second : IAbstractInterfaceConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::VisitAbstractInterfaces(const EnvAbstractInterfaceVisitor& visitor) const
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TAbstractInterfaceMap::const_iterator iAbstractInterface = m_abstractInterfaces.begin(), iEndAbstractInterface = m_abstractInterfaces.end(); iAbstractInterface != iEndAbstractInterface; ++ iAbstractInterface)
			{
				if(visitor(iAbstractInterface->second) != EVisitStatus::Continue)
				{
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	IAbstractInterfaceFunctionConstPtr CEnvRegistry::GetAbstractInterfaceFunction(const SGUID& guid) const
	{
		TAbstractInterfaceFunctionMap::const_iterator	iFunction = m_abstractInterfaceFunctions.find(guid);
		return iFunction != m_abstractInterfaceFunctions.end() ? iFunction->second : IAbstractInterfaceFunctionConstPtr();
	}
	
	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::VisitAbstractInterfaceFunctions(const EnvAbstractInterfaceFunctionVisitor& visitor) const
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TAbstractInterfaceFunctionMap::const_iterator iFunction = m_abstractInterfaceFunctions.begin(), iEndFunction = m_abstractInterfaceFunctions.end(); iFunction != iEndFunction; ++ iFunction)
			{
				if(visitor(iFunction->second) != EVisitStatus::Continue)
				{
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CEnvRegistry::RegisterComponentFactory(const IComponentFactoryPtr& pComponentFactory)
	{
		CRY_ASSERT(pComponentFactory != NULL);
		if(pComponentFactory != NULL)
		{
			const SGUID	guid = pComponentFactory->GetComponentGUID();
			CRY_ASSERT(guid);
			if(guid && !GetComponentFactory(guid))
			{
				m_componentFactories.insert(TComponentFactoryMap::value_type(guid, pComponentFactory));
				return true;
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	IComponentFactoryConstPtr CEnvRegistry::GetComponentFactory(const SGUID& guid) const
	{
		TComponentFactoryMap::const_iterator iComponentFactory = m_componentFactories.find(guid);
		return iComponentFactory != m_componentFactories.end() ? iComponentFactory->second : IComponentFactoryConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::VisitComponentFactories(const EnvComponentFactoryVisitor& visitor) const
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TComponentFactoryMap::const_iterator iComponentFactory = m_componentFactories.begin(), iEndComponentFactory = m_componentFactories.end(); iComponentFactory != iEndComponentFactory; ++ iComponentFactory)
			{
				if(visitor(iComponentFactory->second) != EVisitStatus::Continue)
				{
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CEnvRegistry::RegisterComponentMemberFunction(const IComponentMemberFunctionPtr& pFunction)
	{
		CRY_ASSERT(pFunction != NULL);
		if(pFunction != NULL)
		{
			const SGUID	guid = pFunction->GetGUID();
			CRY_ASSERT(guid);
			if(guid && !GetComponentMemberFunction(guid))
			{
				m_componentMemberFunctions.insert(TComponentMemberFunctionMap::value_type(guid, pFunction));
				return true;
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	IComponentMemberFunctionConstPtr CEnvRegistry::GetComponentMemberFunction(const SGUID& guid) const
	{
		TComponentMemberFunctionMap::const_iterator	iComponentMemberFunction = m_componentMemberFunctions.find(guid);
		return iComponentMemberFunction != m_componentMemberFunctions.end() ? iComponentMemberFunction->second : IComponentMemberFunctionConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::VisitComponentMemberFunctions(const EnvComponentMemberFunctionVisitor& visitor) const
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TComponentMemberFunctionMap::const_iterator iComponentMemberFunction = m_componentMemberFunctions.begin(), iEndComponentMemberFunction = m_componentMemberFunctions.end(); iComponentMemberFunction != iEndComponentMemberFunction; ++ iComponentMemberFunction)
			{
				if(visitor(iComponentMemberFunction->second) != EVisitStatus::Continue)
				{
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CEnvRegistry::RegisterActionFactory(const IActionFactoryPtr& pActionFactory)
	{
		CRY_ASSERT(pActionFactory != NULL);
		if(pActionFactory != NULL)
		{
			const SGUID	guid = pActionFactory->GetActionGUID();
			CRY_ASSERT(guid);
			if(guid && !GetActionFactory(guid))
			{
				m_actionFactories.insert(TActionFactoryMap::value_type(guid, pActionFactory));
				return true;
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	IActionFactoryConstPtr CEnvRegistry::GetActionFactory(const SGUID& guid) const
	{
		TActionFactoryMap::const_iterator iActionFactory = m_actionFactories.find(guid);
		return iActionFactory != m_actionFactories.end() ? iActionFactory->second : IActionFactoryConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::VisitActionFactories(const EnvActionFactoryVisitor& visitor) const
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TActionFactoryMap::const_iterator iActionFactory = m_actionFactories.begin(), iEndActionFactory = m_actionFactories.end(); iActionFactory != iEndActionFactory; ++ iActionFactory)
			{
				if(visitor(iActionFactory->second) != EVisitStatus::Continue)
				{
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CEnvRegistry::RegisterActionMemberFunction(const IActionMemberFunctionPtr& pFunction)
	{
		CRY_ASSERT(pFunction != NULL);
		if(pFunction != NULL)
		{
			const SGUID	guid = pFunction->GetGUID();
			CRY_ASSERT(guid);
			if(guid && !GetActionMemberFunction(guid))
			{
				m_actionMemberFunctions.insert(TActionMemberFunctionMap::value_type(guid, pFunction));
				return true;
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	IActionMemberFunctionConstPtr CEnvRegistry::GetActionMemberFunction(const SGUID& guid) const
	{
		TActionMemberFunctionMap::const_iterator	iActionMemberFunction = m_actionMemberFunctions.find(guid);
		return iActionMemberFunction != m_actionMemberFunctions.end() ? iActionMemberFunction->second : IActionMemberFunctionConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::VisitActionMemberFunctions(const EnvActionMemberFunctionVisitor& visitor) const
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TActionMemberFunctionMap::const_iterator iActionMemberFunction = m_actionMemberFunctions.begin(), iEndActionMemberFunction = m_actionMemberFunctions.end(); iActionMemberFunction != iEndActionMemberFunction; ++ iActionMemberFunction)
			{
				if(visitor(iActionMemberFunction->second) != EVisitStatus::Continue)
				{
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::BlacklistElement(const SGUID& guid)
	{
		m_blacklist.insert(guid);
	}

	//////////////////////////////////////////////////////////////////////////
	bool CEnvRegistry::IsBlacklistedElement(const SGUID& guid) const
	{
		return m_blacklist.find(guid) != m_blacklist.end();
	}

	//////////////////////////////////////////////////////////////////////////
	bool CEnvRegistry::RegisterSettings(const char* name, const IEnvSettingsPtr& pSettings)
	{
		return m_settings.insert(TSettingsMap::value_type(name, pSettings)).second;
	}

	//////////////////////////////////////////////////////////////////////////
	IEnvSettingsPtr CEnvRegistry::GetSettings(const char* name) const
	{
		TSettingsMap::const_iterator	iSettings = m_settings.find(name);
		return iSettings != m_settings.end() ? iSettings->second : IEnvSettingsPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::VisitSettings(const EnvSettingsVisitor& visitor) const
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TSettingsMap::const_iterator iSettings = m_settings.begin(), iEndSettings = m_settings.end(); iSettings != iEndSettings; ++ iSettings)
			{
				if(visitor(iSettings->first, iSettings->second) != EVisitStatus::Continue)
				{
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::LoadAllSettings()
	{
		LOADING_TIME_PROFILE_SECTION;
		const char* szSettingsFolder = gEnv->pSchematyc2->GetSettingsFolder();
		const char* szSettingsExtension = gEnv->pSchematyc2->GetSettingsExtension();
		for(TSettingsMap::const_iterator iSettings = m_settings.begin(), iEndSettings = m_settings.end(); iSettings != iEndSettings; ++ iSettings)
		{
			const char*  szName = iSettings->first.c_str();
			stack_string fileName = szSettingsFolder;
			fileName.append("/");
			fileName.append(szName);
			fileName.append(".");
			fileName.append(szSettingsExtension);
			fileName.MakeLower();
			Serialization::LoadXmlFile(*iSettings->second, fileName);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::SaveAllSettings()
	{
		const char* szSttingsFolder = gEnv->pSchematyc2->GetSettingsFolder();
		const char* szSettingsExtension = gEnv->pSchematyc2->GetSettingsExtension();
		for(TSettingsMap::const_iterator iSettings = m_settings.begin(), iEndSettings = m_settings.end(); iSettings != iEndSettings; ++ iSettings)
		{
			const char* szName = iSettings->first.c_str();
			stack_string fileName = szSttingsFolder;
			fileName.append("/");
			fileName.append(szName);
			fileName.append(".");
			fileName.append(szSettingsExtension);
			fileName.MakeLower();
			Serialization::SaveXmlFile(fileName, *iSettings->second, szName);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::Validate() const
	{
		ValidateComponentDependencies();
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::Clear()
	{
		m_typeDescsById.clear();
		m_typeDescsByGUID.clear();
		m_signals.clear();
		m_globalFunctions.clear();
		m_foundations.clear();
		m_abstractInterfaces.clear();
		m_componentFactories.clear();
		m_componentMemberFunctions.clear();
		m_actionFactories.clear();
		m_actionMemberFunctions.clear();
	}

	//////////////////////////////////////////////////////////////////////////
	void CEnvRegistry::ValidateComponentDependencies() const
	{
		for(TComponentFactoryMap::const_iterator iComponentFactory = m_componentFactories.begin(), iEndComponentFactory = m_componentFactories.end(); iComponentFactory != iEndComponentFactory; ++ iComponentFactory)
		{
			const SGUID								componentGUID = iComponentFactory->first;
			const IComponentFactory&	componentFactory = *iComponentFactory->second;
			for(size_t iComponentDependency = 0, componentDependencyCount = componentFactory.GetDependencyCount(); iComponentDependency < componentDependencyCount; ++ iComponentDependency)
			{
				const SGUID								dependencyComponentGUID = componentFactory.GetDependencyGUID(iComponentDependency);
				IComponentFactoryConstPtr	pDependencyComponentFactory = GetComponentFactory(dependencyComponentGUID);
				if(pDependencyComponentFactory)
				{
					if((pDependencyComponentFactory->GetFlags() & EComponentFlags::Singleton) == 0)
					{
						CryFatalError("%s : Non-singleton %s component detected as dependency of %s!", __FUNCTION__, pDependencyComponentFactory->GetName(), componentFactory.GetName());
					}
					if(EnvComponentUtils::IsDependency(*pDependencyComponentFactory, componentGUID))
					{
						CryFatalError("%s : Circular dependency detected between %s and %s components!", __FUNCTION__, componentFactory.GetName(), pDependencyComponentFactory->GetName());
					}
				}
				else
				{
					char	stringBuffer[StringUtils::s_guidStringBufferSize];
					CryFatalError("%s : Unable to resolve dependency %s for %s component!", __FUNCTION__, StringUtils::SysGUIDToString(dependencyComponentGUID.sysGUID, stringBuffer), componentFactory.GetName());
				}
			}
		}
	}
}

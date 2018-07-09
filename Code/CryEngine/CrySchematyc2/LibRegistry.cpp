// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LibRegistry.h"

#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/ILib.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	ISignalConstPtr CLibRegistry::GetSignal(const SGUID& guid) const
	{
		TSignalMap::const_iterator	iSignal = m_signals.find(guid);
		return iSignal != m_signals.end() ? iSignal->second : ISignalConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibRegistry::VisitSignals(const LibSignalVisitor& visitor) const
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
	ILibAbstractInterfaceConstPtr CLibRegistry::GetAbstractInterface(const SGUID& guid) const
	{
		TAbstractInterfaceMap::const_iterator	iAbstractInterface = m_abstractInterfaces.find(guid);
		return iAbstractInterface != m_abstractInterfaces.end() ? iAbstractInterface->second : ILibAbstractInterfaceConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibRegistry::VisitAbstractInterfaces(const ILibAbstractInterfaceVisitor& visitor)
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
	ILibAbstractInterfaceFunctionConstPtr CLibRegistry::GetAbstractInterfaceFunction(const SGUID& guid) const
	{
		TAbstractInterfaceFunctionMap::const_iterator	iAbstractInterfaceFunction = m_abstractInterfaceFunctions.find(guid);
		return iAbstractInterfaceFunction != m_abstractInterfaceFunctions.end() ? iAbstractInterfaceFunction->second : ILibAbstractInterfaceFunctionConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibRegistry::VisitAbstractInterfaceFunctions(const ILibAbstractInterfaceFunctionVisitor& visitor)
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TAbstractInterfaceFunctionMap::const_iterator iAbstractInterfaceFunction = m_abstractInterfaceFunctions.begin(), iEndAbstractInterfaceFunction = m_abstractInterfaceFunctions.end(); iAbstractInterfaceFunction != iEndAbstractInterfaceFunction; ++ iAbstractInterfaceFunction)
			{
				if(visitor(iAbstractInterfaceFunction->second) != EVisitStatus::Continue)
				{
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibRegistry::FindClass(const char* name) const
	{
		CRY_ASSERT(name != NULL);
		if(name != NULL)
		{
			for(TClassMap::const_iterator iClass = m_classes.begin(), iEndClass = m_classes.end(); iClass != iEndClass; ++ iClass)
			{
				const ILibClass&	libClass = *iClass->second;
				if(strcmp(libClass.GetName(), name) == 0)
				{
					return libClass.GetGUID();
				}
			}
		}
		return SGUID();
	}

	//////////////////////////////////////////////////////////////////////////
	ILibClassConstPtr CLibRegistry::GetClass(const SGUID& guid) const
	{
		TClassMap::const_iterator	iClass = m_classes.find(guid);
		return iClass != m_classes.end() ? iClass->second : ILibClassConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibRegistry::VisitClasses(const ILibClassVisitor& visitor) const
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TClassMap::const_iterator iClass = m_classes.begin(), iEndClass = m_classes.end(); iClass != iEndClass; ++ iClass)
			{
				if(visitor(iClass->second) != EVisitStatus::Continue)
				{
					return;
				}
			}		
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CLibRegistry::RegisterLib(const ILibPtr& pLib)
	{
		CRY_ASSERT(pLib);
		if(pLib)
		{
			const char*				libName = pLib->GetName();
			TLibMap::iterator	iLib = m_libs.find(libName);
			if(iLib != m_libs.end())
			{
				if(iLib->second.unique())
				{
					ReleaseLib(iLib);
				}
				else
				{
					return false;
				}
			}
			pLib->VisitSignals(LibSignalVisitor::FromMemberFunction<CLibRegistry, &CLibRegistry::VisitAndRegisterSignal>(*this));
			pLib->VisitAbstractInterfaces(LibAbstractInterfaceVisitor::FromMemberFunction<CLibRegistry, &CLibRegistry::VisitAndRegisterAbstractInterface>(*this));
			pLib->VisitAbstractInterfaceFunctions(LibAbstractInterfaceFunctionVisitor::FromMemberFunction<CLibRegistry, &CLibRegistry::VisitAndRegisterAbstractInterfaceFunction>(*this));
			pLib->VisitClasses(LibClassVisitor::FromMemberFunction<CLibRegistry, &CLibRegistry::VisitAndRegisterClass>(*this));
			m_libs.insert(TLibMap::value_type(libName, pLib));
			return true;
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	ILibPtr CLibRegistry::GetLib(const char* szName)
	{
		return stl::find_in_map(m_libs, CONST_TEMP_STRING(szName), nullptr);
	}

	//////////////////////////////////////////////////////////////////////////
	SLibRegistrySignals& CLibRegistry::Signals()
	{
		return m_registrySignals;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibRegistry::Clear()
	{
		m_classes.clear();
		m_libs.clear();
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CLibRegistry::VisitAndRegisterSignal(const ISignalConstPtr& pSignal)
	{
		m_signals[pSignal->GetGUID()] = pSignal;
		return EVisitStatus::Continue;
	}
	
	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CLibRegistry::VisitAndReleaseSignal(const ISignalConstPtr& pSignal)
	{
		m_signals.erase(pSignal->GetGUID());
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CLibRegistry::VisitAndRegisterAbstractInterface(const ILibAbstractInterfaceConstPtr& pAbstractInterface)
	{
		m_abstractInterfaces[pAbstractInterface->GetGUID()] = pAbstractInterface;
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CLibRegistry::VisitAndReleaseAbstractInterface(const ILibAbstractInterfaceConstPtr& pAbstractInterface)
	{
		m_abstractInterfaces.erase(pAbstractInterface->GetGUID());
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CLibRegistry::VisitAndRegisterAbstractInterfaceFunction(const ILibAbstractInterfaceFunctionConstPtr& pAbstractInterfaceFunction)
	{
		m_abstractInterfaceFunctions[pAbstractInterfaceFunction->GetGUID()] = pAbstractInterfaceFunction;
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CLibRegistry::VisitAndReleaseAbstractInterfaceFunction(const ILibAbstractInterfaceFunctionConstPtr& pAbstractInterfaceFunction)
	{
		m_abstractInterfaceFunctions.erase(pAbstractInterfaceFunction->GetGUID());
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CLibRegistry::VisitAndRegisterClass(const ILibClassConstPtr& pClass)
	{
		m_classes[pClass->GetGUID()] = pClass;
		m_registrySignals.classRegistration.Send(pClass);
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CLibRegistry::VisitAndReleaseClass(const ILibClassConstPtr& pClass)
	{
		m_classes.erase(pClass->GetGUID());
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibRegistry::ReleaseLib(const TLibMap::iterator& iLib)
	{
		const ILibPtr&	pLib = iLib->second;
		pLib->VisitSignals(LibSignalVisitor::FromMemberFunction<CLibRegistry, &CLibRegistry::VisitAndReleaseSignal>(*this));
		pLib->VisitAbstractInterfaces(LibAbstractInterfaceVisitor::FromMemberFunction<CLibRegistry, &CLibRegistry::VisitAndReleaseAbstractInterface>(*this));
		pLib->VisitAbstractInterfaceFunctions(LibAbstractInterfaceFunctionVisitor::FromMemberFunction<CLibRegistry, &CLibRegistry::VisitAndReleaseAbstractInterfaceFunction>(*this));
		pLib->VisitClasses(LibClassVisitor::FromMemberFunction<CLibRegistry, &CLibRegistry::VisitAndReleaseClass>(*this));
		m_libs.erase(iLib);
	}
}

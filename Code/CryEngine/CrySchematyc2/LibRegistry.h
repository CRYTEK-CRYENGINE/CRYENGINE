// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/ILibRegistry.h>

#include "AbstractInterface.h"

namespace Schematyc2
{
	// Library registry.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CLibRegistry : public ILibRegistry
	{
	public:

		// ILibRegistry
		virtual ISignalConstPtr GetSignal(const SGUID& guid) const override;
		virtual void VisitSignals(const LibSignalVisitor& visitor) const override;
		virtual ILibAbstractInterfaceConstPtr GetAbstractInterface(const SGUID& guid) const override;
		virtual void VisitAbstractInterfaces(const ILibAbstractInterfaceVisitor& visitor) override;
		virtual ILibAbstractInterfaceFunctionConstPtr GetAbstractInterfaceFunction(const SGUID& guid) const override;
		virtual void VisitAbstractInterfaceFunctions(const ILibAbstractInterfaceFunctionVisitor& visitor) override;
		virtual SGUID FindClass(const char* name) const override;
		virtual ILibClassConstPtr GetClass(const SGUID& guid) const override;
		virtual void VisitClasses(const ILibClassVisitor& visitor) const override;
		virtual bool RegisterLib(const ILibPtr& pLib) override;
		virtual ILibPtr GetLib(const char* szName) override;
		virtual SLibRegistrySignals& Signals() override;
		// ~ILibRegistry

		void Clear();

	private:

		typedef std::unordered_map<SGUID, ISignalConstPtr>                       TSignalMap;
		typedef std::unordered_map<SGUID, ILibAbstractInterfaceConstPtr>         TAbstractInterfaceMap;
		typedef std::unordered_map<SGUID, ILibAbstractInterfaceFunctionConstPtr> TAbstractInterfaceFunctionMap;
		typedef std::unordered_map<SGUID, ILibClassConstPtr>                     TClassMap;
		typedef std::map<string, ILibPtr>                                        TLibMap;

		EVisitStatus VisitAndRegisterSignal(const ISignalConstPtr& pSignal);
		EVisitStatus VisitAndReleaseSignal(const ISignalConstPtr& pSignal);
		EVisitStatus VisitAndRegisterAbstractInterface(const ILibAbstractInterfaceConstPtr& pAbstractInterface);
		EVisitStatus VisitAndReleaseAbstractInterface(const ILibAbstractInterfaceConstPtr& pAbstractInterface);
		EVisitStatus VisitAndRegisterAbstractInterfaceFunction(const ILibAbstractInterfaceFunctionConstPtr& pAbstractInterfaceFunction);
		EVisitStatus VisitAndReleaseAbstractInterfaceFunction(const ILibAbstractInterfaceFunctionConstPtr& pAbstractInterfaceFunction);
		EVisitStatus VisitAndRegisterClass(const ILibClassConstPtr& pClass);
		EVisitStatus VisitAndReleaseClass(const ILibClassConstPtr& pClass);
		void ReleaseLib(const TLibMap::iterator& iLib);

		TSignalMap                    m_signals;
		TAbstractInterfaceMap         m_abstractInterfaces;
		TAbstractInterfaceFunctionMap m_abstractInterfaceFunctions;
		TClassMap                     m_classes;
		TLibMap                       m_libs;
		SLibRegistrySignals           m_registrySignals;
	};
}

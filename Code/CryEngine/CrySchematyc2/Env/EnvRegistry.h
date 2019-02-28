// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Env/IEnvRegistry.h>

namespace Schematyc2
{
	class CEnvRegistry : public IEnvRegistry
	{
	public:

		// IEnvRegistry

		virtual bool RegisterTypeDesc(const IEnvTypeDescPtr& pTypeDesc) override;
		virtual const IEnvTypeDesc* GetTypeDesc(const EnvTypeId& id) const override;
		virtual const IEnvTypeDesc* GetTypeDesc(const SGUID& guid) const override;
		virtual void VisitTypeDescs(const EnvTypeDescVisitor& visitor) const override;
		virtual ISignalPtr CreateSignal(const SGUID& guid) override;
		virtual ISignalConstPtr GetSignal(const SGUID& guid) const override;
		virtual void VisitSignals(const EnvSignalVisitor& visitor) const override;
		virtual bool RegisterGlobalFunction(const IGlobalFunctionPtr& pFunction) override;
		virtual IGlobalFunctionConstPtr GetGlobalFunction(const SGUID& guid) const override;
		virtual void VisitGlobalFunctions(const EnvGlobalFunctionVisitor& visitor) const override;
		virtual void RegisterFunction(const IEnvFunctionDescriptorPtr& pFunctionDescriptor) override;
		virtual const IEnvFunctionDescriptor* GetFunction(const SGUID& guid) const override;
		virtual void VisitFunctions(const EnvFunctionVisitor& visitor) const override;
		virtual IFoundationPtr CreateFoundation(const SGUID& guid, const char* name) override;
		virtual IFoundationConstPtr GetFoundation(const SGUID& guid) const override;
		virtual void VisitFoundations(const EnvFoundationVisitor& visitor, EVisitFlags flags = EVisitFlags::None) const override;
		virtual IAbstractInterfacePtr CreateAbstractInterface(const SGUID& guid) override;
		virtual bool RegisterAbstractInterface(const IAbstractInterfacePtr& pAbstractInterface) override;
		virtual IAbstractInterfaceConstPtr GetAbstractInterface(const SGUID& guid) const override;
		virtual void VisitAbstractInterfaces(const EnvAbstractInterfaceVisitor& visitor) const override;
		virtual IAbstractInterfaceFunctionConstPtr GetAbstractInterfaceFunction(const SGUID& guid) const override;
		virtual void VisitAbstractInterfaceFunctions(const EnvAbstractInterfaceFunctionVisitor& visitor) const override;
		virtual bool RegisterComponentFactory(const IComponentFactoryPtr& pComponentFactory) override;
		virtual IComponentFactoryConstPtr GetComponentFactory(const SGUID& guid) const override;
		virtual void VisitComponentFactories(const EnvComponentFactoryVisitor& visitor) const override;
		virtual bool RegisterComponentMemberFunction(const IComponentMemberFunctionPtr& pFunction) override;
		virtual IComponentMemberFunctionConstPtr GetComponentMemberFunction(const SGUID& guid) const override;
		virtual void VisitComponentMemberFunctions(const EnvComponentMemberFunctionVisitor& visitor) const override;
		virtual bool RegisterActionFactory(const IActionFactoryPtr& pActionFactory) override;
		virtual IActionFactoryConstPtr GetActionFactory(const SGUID& guid) const override;
		virtual void VisitActionFactories(const EnvActionFactoryVisitor& visitor) const override;
		virtual bool RegisterActionMemberFunction(const IActionMemberFunctionPtr& pFunction) override;
		virtual IActionMemberFunctionConstPtr GetActionMemberFunction(const SGUID& guid) const override;
		virtual void VisitActionMemberFunctions(const EnvActionMemberFunctionVisitor& visitor) const override;

		virtual void BlacklistElement(const SGUID& guid) override;
		virtual bool IsBlacklistedElement(const SGUID& guid) const override;

		virtual bool RegisterSettings(const char* name, const IEnvSettingsPtr& pSettings) override;
		virtual IEnvSettingsPtr GetSettings(const char* name) const override;
		virtual void VisitSettings(const EnvSettingsVisitor& visitor) const override;
		virtual void LoadAllSettings() override;
		virtual void SaveAllSettings() override;

		virtual void Validate() const override;

		// ~IEnvRegistry

		void Clear();

	private:

		DECLARE_SHARED_POINTERS(IEnvFunctionDescriptor)

		typedef std::unordered_map<EnvTypeId, IEnvTypeDescPtr>                TypeDescByIdMap;
		typedef std::unordered_map<SGUID, IEnvTypeDescPtr>                    TypeDescByGUIDMap;
		typedef std::unordered_map<SGUID, ISignalPtr>                         TSignalMap;
		typedef std::unordered_map<SGUID, IGlobalFunctionPtr>                 TGlobalFunctionMap;
		typedef std::unordered_map<SGUID, IEnvFunctionDescriptorPtr>          Functions;
		typedef std::unordered_map<SGUID, IFoundationPtr>                     TFoundationMap;
		typedef std::unordered_map<SGUID, IAbstractInterfacePtr>              TAbstractInterfaceMap;
		typedef std::unordered_map<SGUID, IAbstractInterfaceFunctionConstPtr> TAbstractInterfaceFunctionMap;
		typedef std::unordered_map<SGUID, IComponentFactoryPtr>               TComponentFactoryMap;
		typedef std::unordered_map<SGUID, IComponentMemberFunctionPtr>        TComponentMemberFunctionMap;
		typedef std::unordered_map<SGUID, IActionFactoryPtr>                  TActionFactoryMap;
		typedef std::unordered_map<SGUID, IActionMemberFunctionPtr>           TActionMemberFunctionMap;
		typedef std::unordered_set<SGUID>                                     Blacklist;
		typedef std::map<string, IEnvSettingsPtr>                             TSettingsMap;

		void ValidateComponentDependencies() const;

		TypeDescByIdMap               m_typeDescsById;
		TypeDescByGUIDMap             m_typeDescsByGUID;
		TSignalMap                    m_signals;
		TGlobalFunctionMap            m_globalFunctions;
		Functions                     m_functions;
		TFoundationMap                m_foundations;
		TAbstractInterfaceMap         m_abstractInterfaces;
		TAbstractInterfaceFunctionMap m_abstractInterfaceFunctions;
		TComponentFactoryMap          m_componentFactories;
		TComponentMemberFunctionMap   m_componentMemberFunctions;
		TActionFactoryMap             m_actionFactories;
		TActionMemberFunctionMap      m_actionMemberFunctions;
		Blacklist                     m_blacklist;
		TSettingsMap                  m_settings;
	};
}

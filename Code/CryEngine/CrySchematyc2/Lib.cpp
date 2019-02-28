// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Lib.h"

#include <CrySerialization/BlackBox.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CrySchematyc2/ILibRegistry.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Utils/StringUtils.h>

SERIALIZATION_ENUM_BEGIN_NESTED2(Schematyc2, CLibClassProperties, EInternalOverridePolicy, "Schematyc Library Class Property Override Policy")
	SERIALIZATION_ENUM(Schematyc2::CLibClassProperties::EInternalOverridePolicy::UseDefault, "UseDefault", "Default")
	SERIALIZATION_ENUM(Schematyc2::CLibClassProperties::EInternalOverridePolicy::OverrideDefault, "OverrideDefault", "Override")
SERIALIZATION_ENUM_END()

namespace Schematyc2
{
	const size_t CLibFunction::MIN_CAPACITY  = 512;
	const size_t CLibFunction::GROWTH_FACTOR = 8;

	struct SComponentInstanceSortRef
	{
		inline SComponentInstanceSortRef(size_t _iComponentInstance)
			: iComponentInstance(_iComponentInstance)
			, priority(0)
		{}

		size_t       iComponentInstance;
		int32        priority;
		TSizeTVector dependencies;
	};

	typedef std::vector<SComponentInstanceSortRef> TComponentInstanceSortRefVector;

	struct SComponentInstanceSortPredicate
	{
		inline bool operator () (const SComponentInstanceSortRef& lhs, const SComponentInstanceSortRef& rhs) const
		{
			if (lhs.priority != rhs.priority)
			{
				return lhs.priority > rhs.priority;
			}
			return lhs.iComponentInstance < rhs.iComponentInstance;
		}
	};

	//////////////////////////////////////////////////////////////////////////
	CLibAbstractInterfaceFunction::CLibAbstractInterfaceFunction(const SGUID& guid, const char* szName)
		: m_guid(guid)
		, m_name(szName)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibAbstractInterfaceFunction::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibAbstractInterfaceFunction::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	TVariantConstArray CLibAbstractInterfaceFunction::GetVariantInputs() const
	{
		return m_variantInputs;
	}

	//////////////////////////////////////////////////////////////////////////
	TVariantConstArray CLibAbstractInterfaceFunction::GetVariantOutputs() const
	{
		return m_variantOutputs;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibAbstractInterfaceFunction::AddInput(const IAny& value)
	{
		CVariantVectorOutputArchive archive(m_variantInputs);
		archive(value, "", "");
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibAbstractInterfaceFunction::AddOutput(const IAny& value)
	{
		CVariantVectorOutputArchive archive(m_variantOutputs);
		archive(value, "", "");
	}

	//////////////////////////////////////////////////////////////////////////
	CLibAbstractInterface::CLibAbstractInterface(const SGUID& guid, const char* szName)
		: m_guid(guid)
		, m_name(szName)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibAbstractInterface::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibAbstractInterface::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	CLibStateMachine::CLibStateMachine(const SGUID& guid, const char* szName, ELibStateMachineLifetime lifetime)
		: m_guid(guid)
		, m_name(szName)
		, m_lifetime(lifetime)
		, m_iPartner(INVALID_INDEX)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibStateMachine::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibStateMachine::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	ELibStateMachineLifetime CLibStateMachine::GetLifetime() const
	{
		return m_lifetime;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibStateMachine::GetPartner() const
	{
		return m_iPartner;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibStateMachine::GetListenerCount() const
	{
		return m_listeners.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibStateMachine::GetListener(size_t iListener) const
	{
		return iListener < m_listeners.size() ? m_listeners[iListener] : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibStateMachine::GetVariableCount() const
	{
		return m_variables.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibStateMachine::GetVariable(const size_t iVariable) const
	{
		return iVariable < m_variables.size() ? m_variables[iVariable] : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibStateMachine::GetContainerCount() const
	{
		return m_containers.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibStateMachine::GetContainer(const size_t iContainer) const
	{
		return iContainer < m_containers.size() ? m_containers[iContainer] : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibStateMachine::SetBeginFunction(const LibFunctionId& functionId)
	{
		m_beginFunctionId = functionId;
	}

	//////////////////////////////////////////////////////////////////////////
	LibFunctionId CLibStateMachine::GetBeginFunction() const
	{
		return m_beginFunctionId;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibStateMachine::SetPartner(size_t iPartner)
	{
		m_iPartner = iPartner;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibStateMachine::AddListener(size_t iListener)
	{
		m_listeners.push_back(iListener);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibStateMachine::AddVariable(const size_t iVariable)
	{
		m_variables.push_back(iVariable);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibStateMachine::AddContainer(const size_t iContainer)
	{
		m_containers.push_back(iContainer);
	}

	//////////////////////////////////////////////////////////////////////////
	CLibState::CLibState(const SGUID& guid, const char* szName)
		: m_guid(guid)
		, m_name(szName)
		, m_iParent(INVALID_INDEX)
		, m_iPartner(INVALID_INDEX)
		, m_iStateMachine(INVALID_INDEX)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibState::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibState::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetParent() const
	{
		return m_iParent;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetPartner() const
	{
		return m_iPartner;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetStateMachine() const
	{
		return m_iStateMachine;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetVariableCount() const
	{
		return m_variables.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetVariable(size_t iVariable) const
	{
		return iVariable < m_variables.size() ? m_variables[iVariable] : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetContainerCount() const
	{
		return m_containers.size();
	}
	
	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetContainer(size_t iContainer) const
	{
		return iContainer < m_containers.size() ? m_containers[iContainer] : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetTimerCount() const
	{
		return m_timers.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetTimer(size_t iTimer) const
	{
		return iTimer < m_timers.size() ? m_timers[iTimer] : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetActionInstanceCount() const
	{
		return m_actionInstances.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetActionInstance(size_t iActionInstance) const
	{
		return iActionInstance < m_actionInstances.size() ? m_actionInstances[iActionInstance] : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetConstructorCount() const
	{
		return m_constructors.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetConstructor(size_t iConstructor) const
	{
		return iConstructor < m_constructors.size() ? m_constructors[iConstructor] : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetDestructorCount() const
	{
		return m_destructors.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetDestructor(size_t iDestructor) const
	{
		return iDestructor < m_destructors.size() ? m_destructors[iDestructor] : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetSignalReceiverCount() const
	{
		return m_signalReceivers.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetSignalReceiver(size_t iSignalReceiver) const
	{
		return iSignalReceiver < m_signalReceivers.size() ? m_signalReceivers[iSignalReceiver] : INVALID_INDEX;
	}

	////////////////////////////////////////a//////////////////////////////////
	size_t CLibState::GetTransitionCount() const
	{
		return m_transitions.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibState::GetTransition(size_t iTransition) const
	{
		return iTransition < m_transitions.size() ? m_transitions[iTransition] : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibState::SetParent(size_t iParent)
	{
		m_iParent = iParent;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibState::SetPartner(size_t iPartner)
	{
		m_iPartner = iPartner;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibState::SetStateMachine(size_t iStateMachine)
	{
		m_iStateMachine = iStateMachine;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibState::AddVariable(size_t iVariable)
	{
		m_variables.push_back(iVariable);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibState::AddContainer(size_t iContainer)
	{
		m_containers.push_back(iContainer);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibState::AddTimer(size_t iTimer)
	{
		m_timers.push_back(iTimer);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibState::AddActionInstance(size_t iActionInstance)
	{
		m_actionInstances.push_back(iActionInstance);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibState::AddConstructor(size_t iConstructor)
	{
		m_constructors.push_back(iConstructor);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibState::AddDestructor(size_t iDestructor)
	{
		m_destructors.push_back(iDestructor);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibState::AddSignalReceiver(size_t iSignalReceiver)
	{
		m_signalReceivers.push_back(iSignalReceiver);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibState::AddTransition(size_t iTransition)
	{
		m_transitions.push_back(iTransition);
	}

	//////////////////////////////////////////////////////////////////////////
	CLibVariable::CLibVariable(const SGUID& guid, const char* szName, EOverridePolicy overridePolicy, const IAny& value, size_t variantPos, size_t variantCount, ELibVariableFlags flags)
		: m_guid(guid)
		, m_name(szName)
		, m_overridePolicy(overridePolicy)
		, m_pValue(value.Clone())
		, m_variantPos(variantPos)
		, m_variantCount(variantCount)
		, m_flags(flags)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibVariable::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibVariable::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	EOverridePolicy CLibVariable::GetOverridePolicy() const
	{
		return m_overridePolicy;
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CLibVariable::GetValue() const
	{
		return m_pValue;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibVariable::GetVariantPos() const
	{
		return m_variantPos;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibVariable::GetVariantCount() const
	{
		return m_variantCount;
	}

	//////////////////////////////////////////////////////////////////////////
	ELibVariableFlags CLibVariable::GetFlags() const
	{
		return m_flags;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibVariable::SetOverridePolicy(EOverridePolicy overridePolicy)
	{
		m_overridePolicy = overridePolicy;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibContainer::CLibContainer(const SGUID& guid, const char* name, const SGUID& typeGUID)
		: m_guid(guid)
		, m_name(name)
		, m_typeGUID(typeGUID)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibContainer::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibContainer::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibContainer::GetTypeGUID() const
	{
		return m_typeGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibTimer::CLibTimer(const SGUID& guid, const char* szName, const STimerParams& params)
		: m_guid(guid)
		, m_name(szName)
		, m_params(params)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibTimer::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibTimer::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	const STimerParams& CLibTimer::GetParams() const
	{
		return m_params;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibAbstractInterfaceImplementation::CLibAbstractInterfaceImplementation(const SGUID& guid, const char* name, const SGUID& interfaceGUID)
		: m_guid(guid)
		, m_name(name)
		, m_interfaceGUID(interfaceGUID)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibAbstractInterfaceImplementation::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibAbstractInterfaceImplementation::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibAbstractInterfaceImplementation::GetInterfaceGUID() const
	{
		return m_interfaceGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibAbstractInterfaceImplementation::GetFunctionCount() const
	{
		return m_functions.size();
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibAbstractInterfaceImplementation::GetFunctionGUID(size_t iFunction) const
	{
		return iFunction < m_functions.size() ? m_functions[iFunction].guid : SGUID();
	}

	//////////////////////////////////////////////////////////////////////////
	LibFunctionId CLibAbstractInterfaceImplementation::GetFunctionId(size_t iFunction) const
	{
		return iFunction < m_functions.size() ? m_functions[iFunction].functionId : LibFunctionId();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibAbstractInterfaceImplementation::AddFunction(const SGUID& guid, const LibFunctionId& functionId)
	{
		m_functions.push_back(SFunction(guid, functionId));
	}

	//////////////////////////////////////////////////////////////////////////
	CLibAbstractInterfaceImplementation::SFunction::SFunction(const SGUID& _guid, const LibFunctionId& _functionId)
		: guid(_guid)
		, functionId(_functionId)
	{}

	//////////////////////////////////////////////////////////////////////////
	CLibComponentInstance::CLibComponentInstance(const SGUID& guid, const char* szName, const SGUID& componentGUID, const IPropertiesConstPtr& pProperties, uint32 propertyFunctionIdx, uint32 parentIdx)
		: m_guid(guid)
		, m_name(szName)
		, m_componentGUID(componentGUID)
		, m_pProperties(pProperties)
		, m_propertyFunctionIdx(propertyFunctionIdx)
		, m_parentIdx(parentIdx)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibComponentInstance::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibComponentInstance::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibComponentInstance::GetComponentGUID() const
	{
		return m_componentGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	IPropertiesConstPtr CLibComponentInstance::GetProperties() const
	{
		return m_pProperties;
	}

	//////////////////////////////////////////////////////////////////////////
	uint32 CLibComponentInstance::GetPropertyFunctionIdx() const
	{
		return m_propertyFunctionIdx;
	}

	//////////////////////////////////////////////////////////////////////////
	uint32 CLibComponentInstance::GetParentIdx() const
	{
		return m_parentIdx;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibActionInstance::CLibActionInstance(const SGUID& guid, const char* name, const SGUID& actionGUID, size_t iComponentInstance, const IPropertiesPtr& pProperties)
		: m_guid(guid)
		, m_name(name)
		, m_actionGUID(actionGUID)
		, m_iComponentInstance(iComponentInstance)
		, m_pProperties(pProperties)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibActionInstance::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibActionInstance::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibActionInstance::GetActionGUID() const
	{
		return m_actionGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibActionInstance::GetComponentInstance() const
	{
		return m_iComponentInstance;
	}

	//////////////////////////////////////////////////////////////////////////
	IPropertiesConstPtr CLibActionInstance::GetProperties() const
	{
		return m_pProperties;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibConstructor::CLibConstructor(const SGUID& guid, const LibFunctionId& functionId)
		: m_guid(guid)
		, m_functionId(functionId)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibConstructor::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	LibFunctionId CLibConstructor::GetFunctionId() const
	{
		return m_functionId;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibDestructor::CLibDestructor(const SGUID& guid, const LibFunctionId& functionId)
		: m_guid(guid)
		, m_functionId(functionId)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibDestructor::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	LibFunctionId CLibDestructor::GetFunctionId() const
	{
		return m_functionId;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibSignalReceiver::CLibSignalReceiver(const SGUID& guid, const SGUID& contextGUID, const LibFunctionId& functionId)
		: m_guid(guid)
		, m_contextGUID(contextGUID)
		, m_functionId(functionId)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibSignalReceiver::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibSignalReceiver::GetContextGUID() const
	{
		return m_contextGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	LibFunctionId CLibSignalReceiver::GetFunctionId() const
	{
		return m_functionId;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibTransition::CLibTransition(const SGUID& guid, const SGUID& contextGUID, const LibFunctionId& functionId)
		: m_guid(guid)
		, m_contextGUID(contextGUID)
		, m_functionId(functionId)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibTransition::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibTransition::GetContextGUID() const
	{
		return m_contextGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	LibFunctionId CLibTransition::GetFunctionId() const
	{
		return m_functionId;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibFunction::CLibFunction()
		: m_capacity(0)
		, m_size(0)
		, m_lastOpPos(INVALID_INDEX)
		, m_pBegin(NULL)
	{}

	//////////////////////////////////////////////////////////////////////////
	CLibFunction::~CLibFunction()
	{
		Release();
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibFunction::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibFunction::GetClassGUID() const
	{
		return m_classGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibFunction::SetName(const char* name)
	{
		m_name = name;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibFunction::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibFunction::SetScope(const char* scope)
	{
		m_scope = scope;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibFunction::GetScope() const
	{
		return m_scope.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibFunction::SetFileName(const char* fileName)
	{
		m_fileName = fileName;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibFunction::GetFileName() const
	{
		return m_fileName.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibFunction::SetAuthor(const char* author)
	{
		m_author = author;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibFunction::GetAuthor() const
	{
		return m_author.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibFunction::SetDescription(const char* szDescription)
	{
		m_textDesc = szDescription;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibFunction::GetDescription() const
	{
		return m_textDesc.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibFunction::AddInput(const char* name, const char* textDesc, const IAny& value)
	{
		m_inputs.push_back(SParam(name, textDesc));
		CVariantVectorOutputArchive	archive(m_variantInputs);
		archive(const_cast<IAny&>(value), "", "");
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibFunction::GetInputCount() const
	{
		return m_inputs.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibFunction::GetInputName(size_t iInput) const
	{
		return iInput < m_inputs.size() ? m_inputs[iInput].name.c_str() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibFunction::GetInputTextDesc(size_t iInput) const
	{
		return iInput < m_inputs.size() ? m_inputs[iInput].textDesc.c_str() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	TVariantConstArray CLibFunction::GetVariantInputs() const
	{
		return m_variantInputs;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibFunction::AddOutput(const char* name, const char* textDesc, const IAny& value)
	{
		m_outputs.push_back(SParam(name, textDesc));
		CVariantVectorOutputArchive	archive(m_variantOutputs);
		archive(const_cast<IAny&>(value), "", "");
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibFunction::GetOutputCount() const
	{
		return m_outputs.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibFunction::GetOutputName(size_t iOutput) const
	{
		return iOutput < m_outputs.size() ? m_outputs[iOutput].name.c_str() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibFunction::GetOutputTextDesc(size_t iOutput) const
	{
		return iOutput < m_outputs.size() ? m_outputs[iOutput].textDesc.c_str() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	TVariantConstArray CLibFunction::GetVariantOutputs() const
	{
		return m_variantOutputs;
	}

	//////////////////////////////////////////////////////////////////////////
	TVariantConstArray CLibFunction::GetVariantConsts() const
	{
		return m_variantConsts;
	}

	//////////////////////////////////////////////////////////////////////////
	GlobalFunctionConstTable CLibFunction::GetGlobalFunctionTable() const
	{
		return m_globalFunctionTable;
	}

	//////////////////////////////////////////////////////////////////////////
	ComponentMemberFunctionConstTable CLibFunction::GetComponentMemberFunctionTable() const
	{
		return m_componentMemberFunctionTable;
	}

	//////////////////////////////////////////////////////////////////////////
	ActionMemberFunctionConstTable CLibFunction::GetActionMemberFunctionTable() const
	{
		return m_actionMemberFunctionTable;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibFunction::GetSize() const
	{
		return m_size;
	}

	//////////////////////////////////////////////////////////////////////////
	const SVMOp* CLibFunction::GetOp(size_t pos) const
	{
		CRY_ASSERT(pos < m_size);
		return pos < m_size ? reinterpret_cast<const SVMOp*>(m_pBegin + pos) : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibFunction::SetGUID(const SGUID& guid)
	{
		m_guid = guid;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibFunction::SetClassGUID(const SGUID& classGUID)
	{
		m_classGUID = classGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibFunction::AddConstValue(const CVariant& value)
	{
		TVariantVector::iterator	iBeginConst = m_variantConsts.begin();
		TVariantVector::iterator	iEndConst = m_variantConsts.end();
		TVariantVector::iterator	iConst = std::find(iBeginConst, iEndConst, value);
		if(iConst != iEndConst)
		{
			return iConst - iBeginConst;
		}
		else
		{
			m_variantConsts.push_back(value);
			return m_variantConsts.size() - 1;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibFunction::AddGlobalFunction(const IGlobalFunctionConstPtr& pGlobalFunction)
	{
		GlobalFunctionTable::iterator	iBeginGlobalFunction = m_globalFunctionTable.begin();
		GlobalFunctionTable::iterator	iEndGlobalFunction = m_globalFunctionTable.end();
		GlobalFunctionTable::iterator	iGlobalFunction = std::find(iBeginGlobalFunction, iEndGlobalFunction, pGlobalFunction);
		if(iGlobalFunction != iEndGlobalFunction)
		{
			return iGlobalFunction - iBeginGlobalFunction;
		}
		else
		{
			m_globalFunctionTable.push_back(pGlobalFunction);
			return m_globalFunctionTable.size() - 1;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibFunction::AddComponentMemberFunction(const IComponentMemberFunctionConstPtr& pComponentMemberFunction)
	{
		ComponentMemberFunctionTable::iterator	iBeginComponentMemberFunction = m_componentMemberFunctionTable.begin();
		ComponentMemberFunctionTable::iterator	iEndComponentMemberFunction = m_componentMemberFunctionTable.end();
		ComponentMemberFunctionTable::iterator	iComponentMemberFunction = std::find(iBeginComponentMemberFunction, iEndComponentMemberFunction, pComponentMemberFunction);
		if(iComponentMemberFunction != iEndComponentMemberFunction)
		{
			return iComponentMemberFunction - iBeginComponentMemberFunction;
		}
		else
		{
			m_componentMemberFunctionTable.push_back(pComponentMemberFunction);
			return m_componentMemberFunctionTable.size() - 1;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibFunction::AddActionMemberFunction(const IActionMemberFunctionConstPtr& pActionMemberFunction)
	{
		ActionMemberFunctionTable::iterator	iBeginActionMemberFunction = m_actionMemberFunctionTable.begin();
		ActionMemberFunctionTable::iterator	iEndActionMemberFunction = m_actionMemberFunctionTable.end();
		ActionMemberFunctionTable::iterator	iActionMemberFunction = std::find(iBeginActionMemberFunction, iEndActionMemberFunction, pActionMemberFunction);
		if(iActionMemberFunction != iEndActionMemberFunction)
		{
			return iActionMemberFunction - iBeginActionMemberFunction;
		}
		else
		{
			m_actionMemberFunctionTable.push_back(pActionMemberFunction);
			return m_actionMemberFunctionTable.size() - 1;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibFunction::AddOp(const SVMOp& op)
	{
		if(op.size > (m_capacity - m_size))
		{
			m_capacity	= /*std::*/max(m_capacity * GROWTH_FACTOR, /*std::*/max(op.size, MIN_CAPACITY));
			m_pBegin		= static_cast<uint8*>(realloc(m_pBegin, m_capacity));
		}
		memcpy(m_pBegin + m_size, &op, op.size);
		m_lastOpPos = m_size;
		m_size += op.size;
		return m_lastOpPos;
	}

	//////////////////////////////////////////////////////////////////////////
	SVMOp* CLibFunction::GetOp(size_t pos)
	{
		CRY_ASSERT(pos < m_size);
		return pos < m_size ? reinterpret_cast<SVMOp*>(m_pBegin + pos) : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	SVMOp* CLibFunction::GetLastOp()
	{
		return m_lastOpPos != INVALID_INDEX ? GetOp(m_lastOpPos) : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibFunction::Release()
	{
		if(m_pBegin)
		{
			free(m_pBegin);
		}
		m_capacity	= 0;
		m_size			= 0;
		m_lastOpPos	= INVALID_INDEX;
		m_pBegin		= NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibFunction::SParam::SParam(const char* _name, const char* _textDesc)
		: name(_name)
		, textDesc(_textDesc)
	{}

	//////////////////////////////////////////////////////////////////////////
	CLibClassProperties::SProperty::SProperty()
		: overridePolicy(EInternalOverridePolicy::UseDefault)
	{}

	//////////////////////////////////////////////////////////////////////////
	CLibClassProperties::SProperty::SProperty(const char* _szLabel, const IAnyPtr& _pValue, EInternalOverridePolicy _overridePolicy)
			: label(_szLabel)
			, pValue(_pValue)
			, overridePolicy(_overridePolicy)
	{}

	//////////////////////////////////////////////////////////////////////////
	CLibClassProperties::SProperty::SProperty(const SProperty& rhs)
		: label(rhs.label)
		, pValue(rhs.pValue->Clone())
		, overridePolicy(rhs.overridePolicy)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CLibClassProperties::SProperty::Serialize(Serialization::IArchive& archive)
	{
		if(archive.isEdit())
		{
			archive(overridePolicy, "overridePolicy", "Override");
			if(overridePolicy == EInternalOverridePolicy::OverrideDefault)
			{
				archive(*pValue, "value", "Value");
			}
		}
		else
		{
			archive(*pValue, "value");
			archive(overridePolicy, "overridePolicy");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CLibClassProperties::CLibClassProperties(const LibVariables& variables)
	{
		for(const CLibVariable& variable : variables)
		{
			if(variable.GetOverridePolicy() != EOverridePolicy::Finalize)
			{
				if((variable.GetFlags() & ELibVariableFlags::ClassProperty) != 0)
				{
					IAnyConstPtr pVariableValue = variable.GetValue();
					if(pVariableValue)
					{
						m_properties.insert(Properties::value_type(variable.GetGUID(), SProperty(variable.GetName(), pVariableValue->Clone(), EInternalOverridePolicy::UseDefault)));
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CLibClassProperties::CLibClassProperties(const CLibClassProperties& rhs)
		: m_properties(rhs.m_properties)
	{}

	//////////////////////////////////////////////////////////////////////////
	ILibClassPropertiesPtr CLibClassProperties::Clone() const
	{
		return std::make_shared<CLibClassProperties>(*this);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibClassProperties::Serialize(Serialization::IArchive& archive)
	{
		if(archive.isEdit())
		{
			PropertiesByLabel propertiesByLabel;
			for(Properties::value_type& property : m_properties)
			{
				propertiesByLabel.insert(PropertiesByLabel::value_type(property.second.label.c_str(), property.second));
			}
			for(PropertiesByLabel::value_type& property : propertiesByLabel)
			{
				archive(property.second, property.first, property.first);
			}
		}
		else if(archive.isInput() && !archive.caps(Serialization::IArchive::BINARY)) // Check for binary archive because binary archives do not currently support black box serialization.
		{
			typedef std::map<SGUID, Serialization::SBlackBox> BlackBoxProperties;

			BlackBoxProperties blackBoxProperties;
			archive(blackBoxProperties, "properties");
			for(BlackBoxProperties::value_type& blackBoxProperty : blackBoxProperties)
			{
				Properties::iterator itProperty = m_properties.find(blackBoxProperty.first);
				if(itProperty != m_properties.end())
				{
					Serialization::LoadBlackBox(itProperty->second, blackBoxProperty.second);
				}
			}
		}
		else
		{
			 archive(m_properties, "properties");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CLibClassProperties::GetProperty(const SGUID& guid) const
	{
		Properties::const_iterator itProperty = m_properties.find(guid);
		if((itProperty != m_properties.end()))
		{
			if(itProperty->second.overridePolicy == EInternalOverridePolicy::OverrideDefault)
			{
				return itProperty->second.pValue;
			}
		}
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibClassProperties::VisitProperties(const LibClassPropertyVisitor& visitor) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(const Properties::value_type& property : m_properties)
			{
				visitor(property.first, property.second.label.c_str(), property.second.pValue);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibClassProperties::OverrideProperty(const SGUID& guid, const IAny& value)
	{
		Properties::iterator itProperty = m_properties.find(guid);
		if((itProperty != m_properties.end()))
		{
			SProperty& property = itProperty->second;
			if(property.pValue)
			{
				*property.pValue        = value;
				property.overridePolicy = EInternalOverridePolicy::OverrideDefault;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CLibClass::CLibClass(const ILib& lib, const SGUID& guid, const char* szName, const SGUID& foundationGUID, const IPropertiesConstPtr& pFoundationProperties)
		: m_lib(lib)
		, m_guid(guid)
		, m_name(szName)
		, m_foundationGUID(foundationGUID)
		, m_pFoundationProperties(pFoundationProperties ? pFoundationProperties->Clone() : IPropertiesPtr())
		, m_nextFunctionId(1)
	{}

	//////////////////////////////////////////////////////////////////////////
	const ILib& CLibClass::GetLib() const
	{
		return m_lib;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibClass::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CLibClass::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CLibClass::GetFoundationGUID() const
	{
		return m_foundationGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	IPropertiesConstPtr CLibClass::GetFoundationProperties() const
	{
		return m_pFoundationProperties;
	}

	//////////////////////////////////////////////////////////////////////////
	ILibClassPropertiesPtr CLibClass::CreateProperties() const
	{
		return std::make_shared<CLibClassProperties>(m_variables);
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetStateMachineCount() const
	{
		return m_stateMachines.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const ILibStateMachine* CLibClass::GetStateMachine(size_t iStateMachine) const
	{
		return iStateMachine < m_stateMachines.size() ? &m_stateMachines[iStateMachine] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetStateCount() const
	{
		return m_states.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const ILibState* CLibClass::GetState(size_t iState) const
	{
		return iState < m_states.size() ? &m_states[iState] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetVariableCount() const
	{
		return m_variables.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const ILibVariable* CLibClass::GetVariable(size_t iVariable) const
	{
		return iVariable < m_variables.size() ? &m_variables[iVariable] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	TVariantConstArray CLibClass::GetVariants() const
	{
		return m_variants;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetContainerCount() const
	{
		return m_containers.size();
	}
	
	//////////////////////////////////////////////////////////////////////////
	const ILibContainer* CLibClass::GetContainer(size_t iContainer) const
	{
		return iContainer < m_containers.size() ? &m_containers[iContainer] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetTimerCount() const
	{
		return m_timers.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const ILibTimer* CLibClass::GetTimer(size_t iTimer) const
	{
		return iTimer < m_timers.size() ? &m_timers[iTimer] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetPersistentTimerCount() const
	{
		return m_persistentTimers.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetPersistentTimer(size_t iPersistentTimer) const
	{
		return iPersistentTimer < m_persistentTimers.size() ? m_persistentTimers[iPersistentTimer] : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetAbstractInterfaceImplementationCount() const
	{
		return m_abstractInterfaceImplementations.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const ILibAbstractInterfaceImplementation* CLibClass::GetAbstractInterfaceImplementation(size_t iAbstractInterfaceImplementation) const
	{
		return iAbstractInterfaceImplementation < m_abstractInterfaceImplementations.size() ? &m_abstractInterfaceImplementations[iAbstractInterfaceImplementation] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetComponentInstanceCount() const
	{
		return m_componentInstances.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const ILibComponentInstance* CLibClass::GetComponentInstance(size_t iComponentInstance) const
	{
		return iComponentInstance < m_componentInstances.size() ? &m_componentInstances[iComponentInstance] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetActionInstanceCount() const
	{
		return m_actionInstances.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const ILibActionInstance* CLibClass::GetActionInstance(size_t iActionInstance) const
	{
		return iActionInstance < m_actionInstances.size() ? &m_actionInstances[iActionInstance] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetPersistentActionInstanceCount() const
	{
		return m_persistentActionInstances.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetPersistentActionInstance(size_t iPersistentActionInstance) const
	{
		return iPersistentActionInstance < m_persistentActionInstances.size() ? m_persistentActionInstances[iPersistentActionInstance] : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetConstructorCount() const
	{
		return m_constructors.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const ILibConstructor* CLibClass::GetConstructor(size_t iConstructor) const
	{
		return iConstructor < m_constructors.size() ? &m_constructors[iConstructor] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetPersistentConstructorCount() const
	{
		return m_persistentConstructors.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetPersistentConstructor(size_t iPersistentConstructor) const
	{
		return iPersistentConstructor < m_persistentConstructors.size() ? m_persistentConstructors[iPersistentConstructor] : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetDestructorCount() const
	{
		return m_destructors.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const ILibDestructor* CLibClass::GetDestructor(size_t iDestructor) const
	{
		return iDestructor < m_destructors.size() ? &m_destructors[iDestructor] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetPersistentDestructorCount() const
	{
		return m_persistentDestructors.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetPersistentDestructor(size_t iPersistentDestructor) const
	{
		return iPersistentDestructor < m_persistentDestructors.size() ? m_persistentDestructors[iPersistentDestructor] : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetSignalReceiverCount() const
	{
		return m_signalReceivers.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const ILibSignalReceiver* CLibClass::GetSignalReceiver(size_t iSignalReceiver) const
	{
		return iSignalReceiver < m_signalReceivers.size() ? &m_signalReceivers[iSignalReceiver] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetPersistentSignalReceiverCount() const
	{
		return m_persistentSignalReceivers.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetPersistentSignalReceiver(size_t iPersistentSignalReceiver) const
	{
		return iPersistentSignalReceiver < m_persistentSignalReceivers.size() ? m_persistentSignalReceivers[iPersistentSignalReceiver] : INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetTransitionCount() const
	{
		return m_transitions.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const ILibTransition* CLibClass::GetTransition(size_t iTransition) const
	{
		return iTransition < m_transitions.size() ? &m_transitions[iTransition] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	LibFunctionId CLibClass::GetFunctionId(const SGUID& guid) const
	{
		TFunctionIdByGUIDMap::const_iterator	iFunctionId = m_functionIdsByGUID.find(guid);
		return iFunctionId != m_functionIdsByGUID.end() ? iFunctionId->second : LibFunctionId::s_invalid;
	}

	//////////////////////////////////////////////////////////////////////////
	const ILibFunction* CLibClass::GetFunction(const LibFunctionId& functionId) const
	{
		TFunctionMap::const_iterator	iFunction = m_functions.find(functionId);
		return iFunction != m_functions.end() ? &iFunction->second.function : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	const CRuntimeFunction* CLibClass::GetFunction_New(uint32 functionIdx) const
	{
		return functionIdx < m_functions_New.size() ? m_functions_New[functionIdx].get() : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibClass::PreviewGraphFunctions(const SGUID& graphGUID, const StringStreamOutCallback& stringStreamOutCallback) const
	{
		for(TFunctionMap::const_iterator iFunction = m_functions.begin(), iEndFunction = m_functions.end(); iFunction != iEndFunction; ++ iFunction)
		{
			const SFunction&	function = iFunction->second;
			if(function.graphGUID == graphGUID)
			{
				PreviewFunction(function.function, stringStreamOutCallback);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddStateMachine(const SGUID& guid, const char* name, ELibStateMachineLifetime lifetime)
	{
		m_stateMachines.push_back(CLibStateMachine(guid, name, lifetime));
		return m_stateMachines.size() - 1;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibStateMachine* CLibClass::GetStateMachine(size_t iStateMachine)
	{
		return iStateMachine < m_stateMachines.size() ? &m_stateMachines[iStateMachine] : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddState(const SGUID& guid, const char* name)
	{
		m_states.push_back(CLibState(guid, name));
		return m_states.size() - 1;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibState* CLibClass::GetState(size_t iState)
	{
		return iState < m_states.size() ? &m_states[iState] : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddVariable(const SGUID& guid, const char* szName, EOverridePolicy overridePolicy, const IAny& value, size_t variantPos, size_t variantCount, ELibVariableFlags flags)
	{
		m_variables.push_back(CLibVariable(guid, szName, overridePolicy, value, variantPos, variantCount, flags));
		return m_variables.size() - 1;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibVariable* CLibClass::GetVariable(size_t iVariable)
	{
		return iVariable < m_variables.size() ? &m_variables[iVariable] : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddContainer(const SGUID& guid, const char* szName, const SGUID& typeGUID)
	{
		m_containers.push_back(CLibContainer(guid, szName, typeGUID));
		return m_containers.size() - 1;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibContainer* CLibClass::GetContainer(size_t iContainer)
	{
		return iContainer < m_containers.size() ? &m_containers[iContainer] : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	TVariantVector& CLibClass::GetVariants()
	{
		return m_variants;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddTimer(const SGUID& guid, const char* szName, const STimerParams& params)
	{
		m_timers.push_back(CLibTimer(guid, szName, params));
		return m_timers.size() - 1;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddPersistentTimer(const SGUID& guid, const char* szName, const STimerParams& params)
	{
		m_persistentTimers.push_back(m_timers.size());
		return AddTimer(guid, szName, params);
	}

	//////////////////////////////////////////////////////////////////////////
	CLibTimer* CLibClass::GetTimer(size_t iTimer)
	{
		return iTimer < m_timers.size() ? &m_timers[iTimer] : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddAbstractInterfaceImplementation(const SGUID& guid, const char* szName, const SGUID& interfaceGUID)
	{
		m_abstractInterfaceImplementations.push_back(CLibAbstractInterfaceImplementation(guid, szName, interfaceGUID));
		return m_abstractInterfaceImplementations.size() - 1;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibAbstractInterfaceImplementation* CLibClass::GetAbstractInterfaceImplementation(size_t iAbstractInterfaceImplementation)
	{
		return iAbstractInterfaceImplementation < m_abstractInterfaceImplementations.size() ? &m_abstractInterfaceImplementations[iAbstractInterfaceImplementation] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddComponentInstance(const SGUID& guid, const char* szName, const SGUID& componentGUID, const IPropertiesPtr& pProperties, uint32 propertyFunctionIdx, uint32 parentIdx)
	{
		m_componentInstances.push_back(CLibComponentInstance(guid, szName, componentGUID, pProperties, propertyFunctionIdx, parentIdx));
		return m_componentInstances.size() - 1;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibComponentInstance* CLibClass::GetComponentInstance(size_t iComponentInstance)
	{
		return iComponentInstance < m_componentInstances.size() ? &m_componentInstances[iComponentInstance] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibClass::SortComponentInstances()
	{
		IEnvRegistry&										envRegistry = gEnv->pSchematyc2->GetEnvRegistry();
		const size_t										componentInstanceCount = m_componentInstances.size();
		TComponentInstanceSortRefVector	componentInstanceSortRefs;
		componentInstanceSortRefs.reserve(componentInstanceCount);
		for(size_t iComponentInstance = 0; iComponentInstance < componentInstanceCount; ++ iComponentInstance)
		{
			const CLibComponentInstance&	componentInstance = m_componentInstances[iComponentInstance];
			IComponentFactoryConstPtr			pComponentFactory = envRegistry.GetComponentFactory(componentInstance.GetComponentGUID());
			CRY_ASSERT(pComponentFactory);
			if(pComponentFactory)
			{
				componentInstanceSortRefs.push_back(SComponentInstanceSortRef(iComponentInstance));

				SComponentInstanceSortRef&	componentInstanceSortRef = componentInstanceSortRefs.back();
				const size_t								componentDependencyCount = pComponentFactory->GetDependencyCount();
				componentInstanceSortRef.dependencies.reserve(componentDependencyCount);
				for(size_t iComponentDependency = 0; iComponentDependency < componentDependencyCount; ++ iComponentDependency)
				{
					const SGUID	componentDependencyGUID = pComponentFactory->GetDependencyGUID(iComponentDependency);
					for(size_t iOtherComponentInstance = 0; iOtherComponentInstance < componentInstanceCount; ++ iOtherComponentInstance)
					{
						if(m_componentInstances[iOtherComponentInstance].GetComponentGUID() == componentDependencyGUID)
						{
							componentInstanceSortRef.dependencies.push_back(iOtherComponentInstance);
							break;
						}
					}
				}
			}
		}

		bool	resolveDependencies;
		do
		{
			resolveDependencies = false;
			for(size_t iComponentInstance = 0; iComponentInstance < componentInstanceCount; ++ iComponentInstance)
			{
				SComponentInstanceSortRef&	componentInstanceSortRef = componentInstanceSortRefs[iComponentInstance];
				for(TSizeTVector::const_iterator iComponentDependency = componentInstanceSortRef.dependencies.begin(), iEndComponentDependency = componentInstanceSortRef.dependencies.end(); iComponentDependency != iEndComponentDependency; ++ iComponentDependency)
				{
					SComponentInstanceSortRef&	otherComponentInstanceSortRef = componentInstanceSortRefs[*iComponentDependency];
					if(otherComponentInstanceSortRef.priority <= componentInstanceSortRef.priority)
					{
						otherComponentInstanceSortRef.priority = componentInstanceSortRef.priority + 1;
						resolveDependencies = true;
					}
				}
			}
		} while(resolveDependencies);
		std::sort(componentInstanceSortRefs.begin(), componentInstanceSortRefs.end(), SComponentInstanceSortPredicate());

		TComponentInstanceVector	componentInstances;
		componentInstances.reserve(componentInstanceCount);
		for(size_t iComponentInstance = 0; iComponentInstance < componentInstanceCount; ++ iComponentInstance)
		{
			componentInstances.push_back(m_componentInstances[componentInstanceSortRefs[iComponentInstance].iComponentInstance]);
		}
		std::swap(componentInstances, m_componentInstances);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibClass::ValidateSingletonComponentInstances() const
	{
		IEnvRegistry& envRegistry = gEnv->pSchematyc2->GetEnvRegistry();

		const size_t componentInstanceCount = m_componentInstances.size();

		VectorMap<SGUID, SGUID> singletons;
		singletons.reserve(componentInstanceCount);

		for (size_t iComponentInstance = 0; iComponentInstance < componentInstanceCount; ++iComponentInstance)
		{
			const CLibComponentInstance& componentInstance = m_componentInstances[iComponentInstance];
			const SGUID componentGuid = componentInstance.GetComponentGUID();
			IComponentFactoryConstPtr pComponentFactory = envRegistry.GetComponentFactory(componentGuid);
			if ((pComponentFactory->GetFlags() & EComponentFlags::Singleton) != 0)
			{
				const SGUID instanceGuid = componentInstance.GetGUID();
				auto iter = singletons.find(componentGuid);
				if (singletons.find(componentGuid) == singletons.end())
				{
					singletons[componentGuid] = instanceGuid;
				}
				else
				{
					stack_string guid1, guid2, guid3;
					SCHEMATYC2_COMPILER_ERROR("Singleton component %s '%s' instance %s is repeated several time. First instance %s",
						StringUtils::SysGUIDToString(componentGuid.sysGUID, guid1), 
						componentInstance.GetName(), 
						StringUtils::SysGUIDToString(instanceGuid.sysGUID, guid2),
						StringUtils::SysGUIDToString(iter->second.sysGUID, guid3));
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddActionInstance(const SGUID& guid, const char* name, const SGUID& actionGUID, size_t iComponentInstance, const IPropertiesPtr& pProperties)
	{
		m_actionInstances.push_back(CLibActionInstance(guid, name, actionGUID, iComponentInstance, pProperties));
		return m_actionInstances.size() - 1;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddPersistentActionInstance(const SGUID& guid, const char* name, const SGUID& actionGUID, size_t iComponentInstance, const IPropertiesPtr& pProperties)
	{
		m_persistentActionInstances.push_back(m_actionInstances.size());
		return AddActionInstance(guid, name, actionGUID, iComponentInstance, pProperties);
	}

	//////////////////////////////////////////////////////////////////////////
	CLibActionInstance* CLibClass::GetActionInstance(size_t iActionInstance)
	{
		return iActionInstance < m_actionInstances.size() ? &m_actionInstances[iActionInstance] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddConstructor(const SGUID& guid, const LibFunctionId& functionId)
	{
		m_constructors.push_back(CLibConstructor(guid, functionId));
		return m_constructors.size() - 1;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddPersistentConstructor(const SGUID& guid, const LibFunctionId& functionId)
	{
		m_persistentConstructors.push_back(m_constructors.size());
		return AddConstructor(guid, functionId);
	}

	//////////////////////////////////////////////////////////////////////////
	CLibConstructor* CLibClass::GetConstructor(size_t iConstructor)
	{
		return iConstructor < m_constructors.size() ? &m_constructors[iConstructor] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddDestructor(const SGUID& guid, const LibFunctionId& functionId)
	{
		m_destructors.push_back(CLibDestructor(guid, functionId));
		return m_destructors.size() - 1;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddPersistentDestructor(const SGUID& guid, const LibFunctionId& functionId)
	{
		m_persistentDestructors.push_back(m_destructors.size());
		return AddDestructor(guid, functionId);
	}

	//////////////////////////////////////////////////////////////////////////
	CLibDestructor* CLibClass::GetDestructor(size_t iDestructor)
	{
		return iDestructor < m_destructors.size() ? &m_destructors[iDestructor] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddSignalReceiver(const SGUID& guid, const SGUID& contextGUID, const LibFunctionId& functionId)
	{
		m_signalReceivers.push_back(CLibSignalReceiver(guid, contextGUID, functionId));
		return m_signalReceivers.size() - 1;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddPersistentSignalReceiver(const SGUID& guid, const SGUID& contextGUID, const LibFunctionId& functionId)
	{
		m_persistentSignalReceivers.push_back(m_signalReceivers.size());
		return AddSignalReceiver(guid, contextGUID, functionId);
	}

	//////////////////////////////////////////////////////////////////////////
	CLibSignalReceiver* CLibClass::GetSignalReceiver(size_t iSignalReceiver)
	{
		return iSignalReceiver < m_signalReceivers.size() ? &m_signalReceivers[iSignalReceiver] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::AddTransition(const SGUID& guid, const SGUID& contextGUID, const LibFunctionId& functionId)
	{
		m_transitions.push_back(CLibTransition(guid, contextGUID, functionId));
		return m_transitions.size() - 1;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibTransition* CLibClass::GetTransition(size_t iTransition)
	{
		return iTransition < m_transitions.size() ? &m_transitions[iTransition] : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	LibFunctionId CLibClass::AddFunction(const SGUID& graphGUID)
	{
		LibFunctionId functionId = m_nextFunctionId ++;
		m_functions.insert(TFunctionMap::value_type(functionId, SFunction(functionId, graphGUID)));
		return functionId;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibFunction* CLibClass::GetFunction(const LibFunctionId& functionId)
	{
		TFunctionMap::iterator	iFunction = m_functions.find(functionId);
		return iFunction != m_functions.end() ? &iFunction->second.function : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	uint32 CLibClass::AddFunction_New(const SGUID& guid)
	{
		const uint32        functionIdx = m_functions_New.size();
		CRuntimeFunctionPtr pFunction = std::make_shared<CRuntimeFunction>(guid);
		m_functions_New.push_back(pFunction);
		return functionIdx;
	}

	//////////////////////////////////////////////////////////////////////////
	CRuntimeFunction* CLibClass::GetFunction_New(uint32 functionIdx)
	{
		return functionIdx < m_functions_New.size() ? m_functions_New[functionIdx].get() : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CLibClass::BindFunctionToGUID(const LibFunctionId& functionId, const SGUID& guid)
	{
		return m_functionIdsByGUID.insert(TFunctionIdByGUIDMap::value_type(guid, functionId)).second;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLibClass::AddPrecacheResource(IAnyConstPtr resource)
	{
		m_resourcesToPrecache.emplace_back(resource);
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CLibClass::GetPrecacheResourceCount() const
	{
		return m_resourcesToPrecache.size();
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CLibClass::GetPrecacheResource(size_t ressourceIdx) const
	{
		return ressourceIdx < m_resourcesToPrecache.size() ? m_resourcesToPrecache[ressourceIdx] : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	CLibClass::SFunction::SFunction(const LibFunctionId& _functionId, const SGUID& _graphGUID)
		: functionId(_functionId)
		, graphGUID(_graphGUID)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CLibClass::PreviewFunction(const CLibFunction& function, StringStreamOutCallback stringStreamOutCallback) const
	{
		stringStreamOutCallback("function ");
		stringStreamOutCallback(function.GetName());
		if(function.GetSize() > 0)
		{
			stringStreamOutCallback("\n\n");
			for(size_t functionPos = 0, size = function.GetSize(); functionPos < size; )
			{
				const SVMOp*	pOp = function.GetOp(functionPos);
				{
					size_t	pos = static_cast<size_t>(reinterpret_cast<const uint8*>(pOp) - reinterpret_cast<const uint8*>(function.GetOp(0)));
					char		stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
					stringStreamOutCallback(StringUtils::SizeTToString(pos, stringBuffer));
				}
				switch(pOp->opCode)
				{
				case SVMOp::PUSH:
					{
						const SVMPushOp*	pPushOp = static_cast<const SVMPushOp*>(pOp);
						const CVariant&		value = function.GetVariantConsts()[pPushOp->iConstValue];
						const bool				valueIsString = (value.GetTypeId() == CVariant::STRING) || (value.GetTypeId() == CVariant::POOL_STRING);
						stack_string			stringBuffer;
						StringUtils::VariantToString(value, stringBuffer);
						stringStreamOutCallback("\tPUSH ");
						if(valueIsString == true)
						{
							stringStreamOutCallback("\"");
						}
						stringStreamOutCallback(stringBuffer.c_str());
						if(valueIsString == true)
						{
							stringStreamOutCallback("\"");
						}
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::SET:
					{
						const SVMSetOp*	pSetOp = static_cast<const SVMSetOp*>(pOp);
						const CVariant&	value = function.GetVariantConsts()[pSetOp->iConstValue];
						const bool			valueIsString = (value.GetTypeId() == CVariant::STRING) || (value.GetTypeId() == CVariant::POOL_STRING);
						stack_string		stringBuffer;
						StringUtils::SizeTToString(pSetOp->pos, stringBuffer);
						stringStreamOutCallback("\tSET ");
						stringStreamOutCallback(stringBuffer.c_str());
						stringStreamOutCallback(" ");
						StringUtils::VariantToString(value, stringBuffer);
						if(valueIsString == true)
						{
							stringStreamOutCallback("\"");
						}
						stringStreamOutCallback(stringBuffer.c_str());
						if(valueIsString == true)
						{
							stringStreamOutCallback("\"");
						}
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::COPY:
					{
						const size_t	srcPos = static_cast<const SVMCopyOp*>(pOp)->srcPos;
						const size_t	dstPos = static_cast<const SVMCopyOp*>(pOp)->dstPos;
						char					stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tCOPY ");
						if(srcPos == INVALID_INDEX)
						{
							stringStreamOutCallback("TOP");
						}
						else
						{
							stringStreamOutCallback(StringUtils::SizeTToString(srcPos, stringBuffer));
						}
						stringStreamOutCallback(" ");
						if(dstPos == INVALID_INDEX)
						{
							stringStreamOutCallback("TOP");
						}
						else
						{
							stringStreamOutCallback(StringUtils::SizeTToString(dstPos, stringBuffer));
						}
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::POP:
					{
						const SVMPopOp*	pPopOp = static_cast<const SVMPopOp*>(pOp);
						char						stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tPOP ");
						stringStreamOutCallback(StringUtils::SizeTToString(pPopOp->count, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::COLLAPSE:
					{
						const SVMCollapseOp*	pCollapseOp = static_cast<const SVMCollapseOp*>(pOp);
						char									stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tCOLLAPSE ");
						stringStreamOutCallback(StringUtils::SizeTToString(pCollapseOp->pos, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::LOAD:
					{
						const SVMLoadOp*	pLoadOp = static_cast<const SVMLoadOp*>(pOp);
						const char*				variableName = "?";
						char							stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						for(size_t iVariable = 0, variableCount = GetVariableCount(); iVariable < variableCount; ++ iVariable)
						{
							const ILibVariable&	variable = *GetVariable(iVariable);
							if(variable.GetVariantPos() == pLoadOp->pos)
							{
								variableName = variable.GetName();
								break;
							}
						}
						stringStreamOutCallback("\tLOAD ");
						stringStreamOutCallback(variableName);
						stringStreamOutCallback(" ");
						stringStreamOutCallback(StringUtils::SizeTToString(pLoadOp->count, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::STORE:
					{
						const SVMStoreOp*	pStoreOp = static_cast<const SVMStoreOp*>(pOp);
						const char*				variableName = "?";
						char							stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						for(size_t iVariable = 0, variableCount = GetVariableCount(); iVariable < variableCount; ++ iVariable)
						{
							const ILibVariable&	variable = *GetVariable(iVariable);
							if(variable.GetVariantPos() == pStoreOp->pos)
							{
								variableName = variable.GetName();
								break;
							}
						}
						stringStreamOutCallback("\tSTORE ");
						stringStreamOutCallback(variableName);
						stringStreamOutCallback(" ");
						stringStreamOutCallback(StringUtils::SizeTToString(pStoreOp->count, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::CONTAINER_ADD:
					{
						const SVMContainerAddOp*	pContainerAddOp = static_cast<const SVMContainerAddOp*>(pOp);
						char											stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tCONTAINER_ADD ");
						stringStreamOutCallback(GetContainer(pContainerAddOp->iContainer)->GetName());
						stringStreamOutCallback(" ");
						stringStreamOutCallback(StringUtils::SizeTToString(pContainerAddOp->count, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::CONTAINER_REMOVE_BY_INDEX:
					{
						const SVMContainerRemoveByIndexOp*	pContainerRemoveByIndexOp = static_cast<const SVMContainerRemoveByIndexOp*>(pOp);
						char																stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tCONTAINER_REMOVE_BY_INDEX ");
						stringStreamOutCallback(GetContainer(pContainerRemoveByIndexOp->iContainer)->GetName());
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::CONTAINER_REMOVE_BY_VALUE:
					{
						const SVMContainerRemoveByValueOp*	pContainerRemoveByValueOp = static_cast<const SVMContainerRemoveByValueOp*>(pOp);
						char																stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tCONTAINER_REMOVE_BY_VALUE ");
						stringStreamOutCallback(GetContainer(pContainerRemoveByValueOp->iContainer)->GetName());
						stringStreamOutCallback(" ");
						stringStreamOutCallback(StringUtils::SizeTToString(pContainerRemoveByValueOp->count, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::CONTAINER_CLEAR:
					{
						const SVMContainerClearOp*	pContainerClearOp = static_cast<const SVMContainerClearOp*>(pOp);
						char												stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tCONTAINER_CLEAR ");
						stringStreamOutCallback(GetContainer(pContainerClearOp->iContainer)->GetName());
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::CONTAINER_SIZE:
					{
						const SVMContainerSizeOp*	pContainerSizeOp = static_cast<const SVMContainerSizeOp*>(pOp);
						char											stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tCONTAINER_SIZE ");
						stringStreamOutCallback(GetContainer(pContainerSizeOp->iContainer)->GetName());
						stringStreamOutCallback(" ");
						stringStreamOutCallback(StringUtils::SizeTToString(pContainerSizeOp->divisor, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::CONTAINER_GET:
					{
						const SVMContainerGetOp*	pContainerGetOp = static_cast<const SVMContainerGetOp*>(pOp);
						char											stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tCONTAINER_GET ");
						stringStreamOutCallback(GetContainer(pContainerGetOp->iContainer)->GetName());
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::CONTAINER_SET:
					{
						const SVMContainerSetOp*	pContainerSetOp = static_cast<const SVMContainerSetOp*>(pOp);
						char											stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tCONTAINER_SET ");
						stringStreamOutCallback(GetContainer(pContainerSetOp->iContainer)->GetName());
						stringStreamOutCallback(" ");
						stringStreamOutCallback(StringUtils::SizeTToString(pContainerSetOp->count, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::CONTAINER_FIND_BY_VALUE:
					{
						const SVMContainerFindByValueOp*	pContainerFindByValueOp = static_cast<const SVMContainerFindByValueOp*>(pOp);
						char															stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tCONTAINER_FIND_BY_VALUE ");
						stringStreamOutCallback(GetContainer(pContainerFindByValueOp->iContainer)->GetName());
						stringStreamOutCallback(" ");
						stringStreamOutCallback(StringUtils::SizeTToString(pContainerFindByValueOp->count, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::COMPARE:
					{
						const SVMCompareOp*	pCompareOp = static_cast<const SVMCompareOp*>(pOp);
						char								stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tCOMPARE ");
						stringStreamOutCallback(StringUtils::SizeTToString(pCompareOp->lhsPos, stringBuffer));
						stringStreamOutCallback(" ");
						stringStreamOutCallback(StringUtils::SizeTToString(pCompareOp->rhsPos, stringBuffer));
						stringStreamOutCallback(" ");
						stringStreamOutCallback(StringUtils::SizeTToString(pCompareOp->count, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::INCREMENT_INT32:
					{
						const SVMIncrementInt32Op*	pIncrementInt32Op = static_cast<const SVMIncrementInt32Op*>(pOp);
						char												stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tINCREMENT_INT32 ");
						stringStreamOutCallback(StringUtils::SizeTToString(pIncrementInt32Op->pos, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::LESS_THAN_INT32:
					{
						const SVMLessThanInt32Op*	pLessThanInt32Op = static_cast<const SVMLessThanInt32Op*>(pOp);
						char											stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tLESS_THAN_INT32 ");
						stringStreamOutCallback(StringUtils::SizeTToString(pLessThanInt32Op->lhsPos, stringBuffer));
						stringStreamOutCallback(" ");
						stringStreamOutCallback(StringUtils::SizeTToString(pLessThanInt32Op->rhsPos, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::GREATER_THAN_INT32:
					{
						const SVMGreaterThanInt32Op*	pGreaterThanInt32Op = static_cast<const SVMGreaterThanInt32Op*>(pOp);
						char													stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tGREATER_THAN_INT32 ");
						stringStreamOutCallback(StringUtils::SizeTToString(pGreaterThanInt32Op->lhsPos, stringBuffer));
						stringStreamOutCallback(" ");
						stringStreamOutCallback(StringUtils::SizeTToString(pGreaterThanInt32Op->rhsPos, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::BRANCH:
					{
						const size_t	pos = static_cast<const SVMBranchOp*>(pOp)->pos;
						char					stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tBRANCH ");
						stringStreamOutCallback(StringUtils::SizeTToString(pos, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::BRANCH_IF_ZERO:
					{
						const size_t	pos = static_cast<const SVMBranchIfZeroOp*>(pOp)->pos;
						char					stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tBRANCH_IF_ZERO ");
						stringStreamOutCallback(StringUtils::SizeTToString(pos, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::BRANCH_IF_NOT_ZERO:
					{
						const size_t	pos = static_cast<const SVMBranchIfNotZeroOp*>(pOp)->pos;
						char					stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
						stringStreamOutCallback("\tBRANCH_IF_NOT_ZERO ");
						stringStreamOutCallback(StringUtils::SizeTToString(pos, stringBuffer));
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::GET_OBJECT:
					{
						stringStreamOutCallback("\tGET_OBJECT\n");
						break;
					}
				case SVMOp::START_TIMER:
					{
						// #SchematycTODO : Can we get to the timer's name from here?
						char	guidString[StringUtils::s_guidStringBufferSize] = "";
						StringUtils::SysGUIDToString(static_cast<const SVMStartTimerOp*>(pOp)->guid.sysGUID, guidString);
						stringStreamOutCallback("\tSTART_TIMER ");
						stringStreamOutCallback(guidString);
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::STOP_TIMER:
					{
						// #SchematycTODO : Can we get to the timer's name from here?
						char	guidString[StringUtils::s_guidStringBufferSize] = "";
						StringUtils::SysGUIDToString(static_cast<const SVMStopTimerOp*>(pOp)->guid.sysGUID, guidString);
						stringStreamOutCallback("\tSTOP_TIMER ");
						stringStreamOutCallback(guidString);
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::SEND_SIGNAL:
					{
						const SVMSendSignalOp*	pSendSignalOp = static_cast<const SVMSendSignalOp*>(pOp);
						stringStreamOutCallback("\tSEND_SIGNAL ");
						if(ISignalConstPtr pSignal = gEnv->pSchematyc2->GetLibRegistry().GetSignal(pSendSignalOp->guid))
						{
							stringStreamOutCallback(pSignal->GetName());
							stringStreamOutCallback("\n");
						}
						else
						{
							stringStreamOutCallback("?\n");
						}
						break;
					}
				case SVMOp::BROADCAST_SIGNAL:
					{
						const SVMBroadcastSignalOp*	pBroadcastSignalOp = static_cast<const SVMBroadcastSignalOp*>(pOp);
						stringStreamOutCallback("\tBROADCAST_SIGNAL ");
						if(ISignalConstPtr pSignal = gEnv->pSchematyc2->GetLibRegistry().GetSignal(pBroadcastSignalOp->guid))
						{
							stringStreamOutCallback(pSignal->GetName());
							stringStreamOutCallback("\n");
						}
						else
						{
							stringStreamOutCallback("?\n");
						}
						break;
					}
				case SVMOp::CALL_GLOBAL_FUNCTION:
					{
						const SVMCallGlobalFunctionOp*	pCallGlobalFunctionOp = static_cast<const SVMCallGlobalFunctionOp*>(pOp);
						IGlobalFunctionConstPtr					pFunction = function.GetGlobalFunctionTable()[pCallGlobalFunctionOp->iGlobalFunction];
						stringStreamOutCallback("\tCALL_GLOBAL_FUNCTION ");
						stringStreamOutCallback(pFunction->GetName());
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::CALL_ENV_ABSTRACT_INTERFACE_FUNCTION:
					{
						const SVMCallEnvAbstractInterfaceFunctionOp*	pCallEnvAbstractInterfaceFunctionOp = static_cast<const SVMCallEnvAbstractInterfaceFunctionOp*>(pOp);
						stringStreamOutCallback("\tCALL_ENV_ABSTRACT_INTERFACE_FUNCTION ");
						if(IAbstractInterfaceConstPtr pAbstractInterface = gEnv->pSchematyc2->GetEnvRegistry().GetAbstractInterface(pCallEnvAbstractInterfaceFunctionOp->abstractInterfaceGUID))
						{
							stringStreamOutCallback(pAbstractInterface->GetName());
						}
						else
						{
							stringStreamOutCallback("?");
						}
						stringStreamOutCallback(" ");
						if(IAbstractInterfaceFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetAbstractInterfaceFunction(pCallEnvAbstractInterfaceFunctionOp->functionGUID))
						{
							stringStreamOutCallback(pFunction->GetName());
						}
						else
						{
							stringStreamOutCallback("?");
						}
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::CALL_LIB_ABSTRACT_INTERFACE_FUNCTION:
					{
						const SVMCallLibAbstractInterfaceFunctionOp*	pCallLibAbstractInterfaceFunctionOp = static_cast<const SVMCallLibAbstractInterfaceFunctionOp*>(pOp);
						stringStreamOutCallback("\tCALL_LIB_ABSTRACT_INTERFACE_FUNCTION ");
						if(ILibAbstractInterfaceConstPtr pAbstractInterface = gEnv->pSchematyc2->GetLibRegistry().GetAbstractInterface(pCallLibAbstractInterfaceFunctionOp->abstractInterfaceGUID))
						{
							stringStreamOutCallback(pAbstractInterface->GetName());
						}
						else
						{
							stringStreamOutCallback("?");
						}
						stringStreamOutCallback(" ");
						if(ILibAbstractInterfaceFunctionConstPtr pFunction = gEnv->pSchematyc2->GetLibRegistry().GetAbstractInterfaceFunction(pCallLibAbstractInterfaceFunctionOp->functionGUID))
						{
							stringStreamOutCallback(pFunction->GetName());
						}
						else
						{
							stringStreamOutCallback("?");
						}
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::CALL_COMPONENT_MEMBER_FUNCTION:
					{
						const SVMCallComponentMemberFunctionOp*	pCallComponentMemberFunctionOp = static_cast<const SVMCallComponentMemberFunctionOp*>(pOp);
						IComponentMemberFunctionConstPtr				pFunction = function.GetComponentMemberFunctionTable()[pCallComponentMemberFunctionOp->iComponentMemberFunction];
						stringStreamOutCallback("\tCALL_COMPONENT_MEMBER_FUNCTION ");
						stringStreamOutCallback(pFunction->GetName());
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::CALL_ACTION_MEMBER_FUNCTION:
					{
						const SVMCallActionMemberFunctionOp*	pCallActionMemberFunctionOp = static_cast<const SVMCallActionMemberFunctionOp*>(pOp);
						IActionMemberFunctionConstPtr					pFunction = function.GetActionMemberFunctionTable()[pCallActionMemberFunctionOp->iActionMemberFunction];
						stringStreamOutCallback("\tCALL_ACTION_MEMBER_FUNCTION ");
						stringStreamOutCallback(pFunction->GetName());
						stringStreamOutCallback("\n");
						break;
					}
				case SVMOp::CALL_LIB_FUNCTION:
					{
						stringStreamOutCallback("\tCALL_LIB_FUNCTION ");
						const SVMCallLibFunctionOp*	pCallLibFunctionOp = static_cast<const SVMCallLibFunctionOp*>(pOp);
						if(const ILibFunction* pFunction = GetFunction(pCallLibFunctionOp->functionId))
						{
							stringStreamOutCallback(pFunction->GetName());
							stringStreamOutCallback("\n");
						}
						else
						{
							stringStreamOutCallback("?\n");
						}
						break;
					}
				case SVMOp::RETURN:
					{
						stringStreamOutCallback("\tRETURN\n");
						break;
					}
				}
				functionPos += pOp->size;
			}
			stringStreamOutCallback("\nend\n\n");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CLib::CLib(const char* name)
		: m_name(name)
	{}

	//////////////////////////////////////////////////////////////////////////
	const char* CLib::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	ISignalConstPtr CLib::GetSignal(const SGUID& guid) const
	{
		TSignalMap::const_iterator	iSignal = m_signals.find(guid);
		return iSignal != m_signals.end() ? iSignal->second : ISignalConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLib::VisitSignals(const LibSignalVisitor& visitor)
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TSignalMap::iterator iSignal = m_signals.begin(), iEndSignal = m_signals.end(); iSignal != iEndSignal; ++ iSignal)
			{
				if(visitor(iSignal->second) != EVisitStatus::Continue)
				{
					return;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	ILibAbstractInterfaceConstPtr CLib::GetAbstractInterface(const SGUID& guid) const
	{
		TAbstractInterfaceMap::const_iterator	iAbstractInterface = m_abstractInterfaces.find(guid);
		return iAbstractInterface != m_abstractInterfaces.end() ? iAbstractInterface->second : ILibAbstractInterfaceConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLib::VisitAbstractInterfaces(const ILibAbstractInterfaceVisitor& visitor)
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TAbstractInterfaceMap::iterator iAbstractInterface = m_abstractInterfaces.begin(), iEndAbstractInterface = m_abstractInterfaces.end(); iAbstractInterface != iEndAbstractInterface; ++ iAbstractInterface)
			{
				if(visitor(iAbstractInterface->second) != EVisitStatus::Continue)
				{
					return;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	ILibAbstractInterfaceFunctionConstPtr CLib::GetAbstractInterfaceFunction(const SGUID& guid) const
	{
		TAbstractInterfaceFunctionMap::const_iterator	iAbstractInterfaceFunction = m_abstractInterfaceFunctions.find(guid);
		return iAbstractInterfaceFunction != m_abstractInterfaceFunctions.end() ? iAbstractInterfaceFunction->second : ILibAbstractInterfaceFunctionConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLib::VisitAbstractInterfaceFunctions(const ILibAbstractInterfaceFunctionVisitor& visitor)
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TAbstractInterfaceFunctionMap::iterator iAbstractInterfaceFunction = m_abstractInterfaceFunctions.begin(), iEndAbstractInterfaceFunction = m_abstractInterfaceFunctions.end(); iAbstractInterfaceFunction != iEndAbstractInterfaceFunction; ++ iAbstractInterfaceFunction)
			{
				if(visitor(iAbstractInterfaceFunction->second) != EVisitStatus::Continue)
				{
					return;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	ILibClassConstPtr CLib::GetClass(const SGUID& guid) const
	{
		TClassMap::const_iterator	iClass = m_classes.find(guid);
		return iClass != m_classes.end() ? iClass->second : ILibClassConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLib::VisitClasses(const ILibClassVisitor& visitor) const
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
	void CLib::PreviewGraphFunctions(const SGUID& graphGUID, const StringStreamOutCallback& stringStreamOutCallback) const
	{
		for(TClassMap::const_iterator iClass = m_classes.begin(), iEndClass = m_classes.end(); iClass != iEndClass; ++ iClass)
		{
			iClass->second->PreviewGraphFunctions(graphGUID, stringStreamOutCallback);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CSignalPtr CLib::AddSignal(const SGUID& guid, const SGUID& senderGUID, const char* name)
	{
		if(GetSignal(guid) == NULL)
		{
			CSignalPtr	pSignal(new CSignal(guid, senderGUID, name));
			m_signals.insert(TSignalMap::value_type(guid, pSignal));
			return pSignal;
		}
		return CSignalPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	CSignalPtr CLib::GetSignal(const SGUID& guid)
	{
		TSignalMap::iterator	iSignal = m_signals.find(guid);
		return iSignal != m_signals.end() ? iSignal->second : CSignalPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	CLibAbstractInterfacePtr CLib::AddAbstractInterface(const SGUID& guid, const char* name)
	{
		if(!GetAbstractInterface(guid))
		{
			CLibAbstractInterfacePtr	pAbstractInterface(new CLibAbstractInterface(guid, name));
			m_abstractInterfaces.insert(TAbstractInterfaceMap::value_type(guid, pAbstractInterface));
			return pAbstractInterface;
		}
		return CLibAbstractInterfacePtr();
	}

	//////////////////////////////////////////////////////////////////////////
	CLibAbstractInterfacePtr CLib::GetAbstractInterface(const SGUID& guid)
	{
		TAbstractInterfaceMap::iterator	iAbstractInterface = m_abstractInterfaces.find(guid);
		return iAbstractInterface != m_abstractInterfaces.end() ? iAbstractInterface->second : CLibAbstractInterfacePtr();
	}

	//////////////////////////////////////////////////////////////////////////
	CLibAbstractInterfaceFunctionPtr CLib::AddAbstractInterfaceFunction(const SGUID& guid, const char* name)
	{
		if(!GetAbstractInterfaceFunction(guid))
		{
			CLibAbstractInterfaceFunctionPtr	pAbstractInterfaceFunction(new CLibAbstractInterfaceFunction(guid, name));
			m_abstractInterfaceFunctions.insert(TAbstractInterfaceFunctionMap::value_type(guid, pAbstractInterfaceFunction));
			return pAbstractInterfaceFunction;
		}
		return CLibAbstractInterfaceFunctionPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	CLibAbstractInterfaceFunctionPtr CLib::GetAbstractInterfaceFunction(const SGUID& guid)
	{
		TAbstractInterfaceFunctionMap::iterator	iAbstractInterfaceFunction = m_abstractInterfaceFunctions.find(guid);
		return iAbstractInterfaceFunction != m_abstractInterfaceFunctions.end() ? iAbstractInterfaceFunction->second : CLibAbstractInterfaceFunctionPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	CLibClassPtr CLib::AddClass(const SGUID& guid, const char* name, const SGUID& foundationGUID, const IPropertiesConstPtr& pFoundationProperties)
	{
		if(!GetClass(guid))
		{
			CLibClassPtr pClass(new CLibClass(*this, guid, name, foundationGUID, pFoundationProperties));
			m_classes.insert(TClassMap::value_type(guid, pClass));
			return pClass;
		}
		return CLibClassPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	CLibClassPtr CLib::GetClass(const SGUID& guid)
	{
		TClassMap::iterator	iClass = m_classes.find(guid);
		return iClass != m_classes.end() ? iClass->second : CLibClassPtr();
	}
}

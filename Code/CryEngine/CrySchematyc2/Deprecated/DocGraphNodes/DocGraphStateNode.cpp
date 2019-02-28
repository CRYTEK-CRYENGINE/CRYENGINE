// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Deprecated/DocGraphNodes/DocGraphStateNode.h"

#include <CrySchematyc2/GUIDRemapper.h>
#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>

#include "Deprecated/DocGraphNodes/DocGraphNodeBase.h"
#include "Script/ScriptVariableDeclaration.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CDocGraphStateNode::CDocGraphStateNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "", EScriptGraphNodeType::State, contextGUID, refGUID, pos)
	{}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphStateNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphStateNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStateNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator)
	{
		SEnumerateOptionalOutputs(CDocGraphNodeBase::GetFile(), CDocGraphNodeBase::GetRefGUID(), enumerator);
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphStateNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		m_outputTypeGUIDs.push_back(typeId.AsScriptTypeGUID());
		return CDocGraphNodeBase::AddOutput(szName, flags | EScriptGraphPortFlags::Removable, typeId);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStateNode::RemoveOutput(size_t outputIdx)
	{
		CDocGraphNodeBase::RemoveOutput(outputIdx);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStateNode::Refresh(const SScriptRefreshParams& params)
	{
		CDocGraphNodeBase::Refresh(params);
		
		CDocGraphNodeBase::AddInput("Select", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::EndSequence | EScriptGraphPortFlags::Execute);
		
		IScriptFile&        file = CDocGraphNodeBase::GetFile();
		const SGUID         refGUID = CDocGraphNodeBase::GetRefGUID();
		const IScriptState* pState = file.GetState(refGUID);
		if(pState)
		{
			stack_string name = "State: ";
			name.append(pState->GetName());
			CDocGraphNodeBase::SetName(name.c_str());

			const SGUID         partnerStateGUID = pState->GetPartnerGUID();
			const IScriptState* pPartnerState = file.GetState(partnerStateGUID);
			if(pPartnerState)
			{
				{
					stack_string outputName = pPartnerState->GetName();
					outputName.append(" Failed");
					CDocGraphNodeBase::AddOutput(outputName.c_str(), EScriptGraphPortFlags::BeginSequence | EScriptGraphPortFlags::Execute, CAggregateTypeId::FromScriptTypeGUID(refGUID));
				}
				{
					stack_string outputName = pPartnerState->GetName();
					outputName.append(" Interrupted");
					CDocGraphNodeBase::AddOutput(outputName.c_str(), EScriptGraphPortFlags::BeginSequence | EScriptGraphPortFlags::Execute | EScriptGraphPortFlags::SpacerBelow, CAggregateTypeId::FromScriptTypeGUID(partnerStateGUID));
				}
			}

			//////////
			for(const SGUID& outputTypeGUID : m_outputTypeGUIDs)
			{
				stack_string    outputName;
				ISignalConstPtr pEnvSignal = gEnv->pSchematyc2->GetEnvRegistry().GetSignal(outputTypeGUID);
				if(pEnvSignal)
				{
					if(DocUtils::IsSignalAvailableInScope(file, refGUID, pEnvSignal->GetSenderGUID(), pEnvSignal->GetNamespace()))
					{
						outputName = pEnvSignal->GetName();
					}
				}
				else
				{
					const IScriptElement* pScriptElement = ScriptIncludeRecursionUtils::GetElement(file, outputTypeGUID).second;
					if(pScriptElement)
					{
						switch(pScriptElement->GetElementType())
						{
						case EScriptElementType::Signal:
							{
								const IScriptSignal* pScriptSignal = static_cast<const IScriptSignal*>(pScriptElement);
								// #SchematycTODO : Do we need to verify scope here?
								outputName = pScriptSignal->GetName();
								break;
							}
						case EScriptElementType::Timer:
							{
								const IScriptTimer* pScriptTimer = static_cast<const IScriptTimer*>(pScriptElement);
								if(DocUtils::IsElementInScope(file, refGUID, pScriptTimer->GetScopeGUID()))
								{
									outputName = pScriptTimer->GetName();
								}
								break;
							}
						case EScriptElementType::State:
							{
								const IScriptState* pScriptState = static_cast<const IScriptState*>(pScriptElement);
								if(outputTypeGUID == refGUID)
								{
									outputName = pScriptState->GetName();
									outputName.append(" Released");
								}
								else
								{
									outputName = pScriptState->GetName();
									outputName.append(" Requested");
								}
								break;
							}
						}
					}
				}

				if(!outputName.empty())
				{
					CDocGraphNodeBase::AddOutput(outputName.c_str(), EScriptGraphPortFlags::BeginSequence | EScriptGraphPortFlags::Execute | EScriptGraphPortFlags::Removable, CAggregateTypeId::FromScriptTypeGUID(outputTypeGUID)); // N.B. This is slightly hacky since outputTypeGUID may actually represent an env signal.
				}
			}
			//////////
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStateNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const
	{
		preCompiler.AddFunctionOutput("Result", nullptr, MakeAny(int32(ELibTransitionResult::Continue)));
		preCompiler.AddFunctionOutput("State", nullptr, MakeAny(int32(0)));
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStateNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const
	{
		linker.CreateTransition(CDocGraphNodeBase::GetGUID(), CDocGraphNodeBase::GetRefGUID(), CDocGraphNodeBase::GetOutputTypeId(outputIdx).AsScriptTypeGUID(), functionId);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStateNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
	{
		switch(sequenceStep)
		{
		case EDocGraphSequenceStep::BeginInput:
			{
				if(portIdx == EInput::In)
				{
					SelectState(compiler);
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStateNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Serialize(archive);

		switch(SerializationContext::GetPass(archive))
		{
		case ESerializationPass::PreLoad:
		case ESerializationPass::Save:
			{
				archive(m_outputTypeGUIDs, "outputs");
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStateNode::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		CDocGraphNodeBase::RemapGUIDs(guidRemapper);
		for(SGUID& outputTypeGUID : m_outputTypeGUIDs)
		{
			outputTypeGUID = guidRemapper.Remap(outputTypeGUID);
		}
		// #SchematycTODO : Is it still necessary to remap outputs or are we guaranteed to refresh afterwards?
		for(size_t outputIdx = 0, outputCount = CDocGraphNodeBase::GetOutputCount(); outputIdx < outputCount; ++ outputIdx)
		{
			if((CDocGraphNodeBase::GetOutputFlags(outputIdx) & EScriptGraphPortFlags::Removable) != 0)
			{
				const CAggregateTypeId prevOutputTypeId = CDocGraphNodeBase::GetOutputTypeId(outputIdx);
				const CAggregateTypeId newOutputTypeId = CAggregateTypeId::FromScriptTypeGUID(guidRemapper.Remap(prevOutputTypeId.AsScriptTypeGUID()));
				CDocGraphNodeBase::SetOutputTypeId(outputIdx, newOutputTypeId);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphStateNode::SEnumerateOptionalOutputs::SEnumerateOptionalOutputs(const IScriptFile& _file, const SGUID& _refGUID, const ScriptGraphNodeOptionalOutputEnumerator& _enumerator)
		: file(_file)
		, refGUID(_refGUID)
		, enumerator(_enumerator)
	{
		if(enumerator)
		{
			gEnv->pSchematyc2->GetEnvRegistry().VisitSignals(EnvSignalVisitor::FromMemberFunction<SEnumerateOptionalOutputs, &SEnumerateOptionalOutputs::VisitSignal>(*this));
			ScriptIncludeRecursionUtils::VisitSignals(file, ScriptIncludeRecursionUtils::SignalVisitor::FromMemberFunction<SEnumerateOptionalOutputs, &SEnumerateOptionalOutputs::VisitScriptSignal>(*this), SGUID(), true);
			file.VisitTimers(ScriptTimerConstVisitor::FromMemberFunction<SEnumerateOptionalOutputs, &SEnumerateOptionalOutputs::VisitScriptTimer>(*this), SGUID(), true);

			const IScriptStateMachine* pScriptStateMachine = DocUtils::FindOwnerStateMachine(file, refGUID);
			if(pScriptStateMachine)
			{
				if(DocUtils::IsPartnerStateMachine(file, pScriptStateMachine->GetGUID()))
				{
					file.VisitStates(ScriptStateConstVisitor::FromMemberFunction<SEnumerateOptionalOutputs, &SEnumerateOptionalOutputs::VisitScriptState>(*this), pScriptStateMachine->GetGUID(), true);

					const IScriptState* pState = file.GetState(refGUID);
					if(pState)
					{
						stack_string	name = pState->GetName();
						name.append(" Released");
						stack_string	fullName;
						DocUtils::GetFullElementName(file, pState->GetGUID(), fullName, EScriptElementType::Class);
						fullName.append(" Released");
						enumerator(name.c_str(), fullName.c_str(), EScriptGraphPortFlags::BeginSequence | EScriptGraphPortFlags::Execute, CAggregateTypeId::FromScriptTypeGUID(refGUID));
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CDocGraphStateNode::SEnumerateOptionalOutputs::VisitSignal(const ISignalConstPtr& pSignal)
	{
		if(DocUtils::IsSignalAvailableInScope(file, refGUID, pSignal->GetSenderGUID(), pSignal->GetNamespace()))
		{
			stack_string	fullName;
			EnvRegistryUtils::GetFullName(pSignal->GetName(), pSignal->GetNamespace(), pSignal->GetSenderGUID(), fullName);
			enumerator(pSignal->GetName(), fullName.c_str(), EScriptGraphPortFlags::BeginSequence | EScriptGraphPortFlags::Execute, CAggregateTypeId::FromScriptTypeGUID(pSignal->GetGUID()));
		}
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStateNode::SEnumerateOptionalOutputs::VisitScriptSignal(const IScriptFile& signalFile, const IScriptSignal& signal)
	{
		if(DocUtils::IsElementInScope(signalFile, refGUID, signal.GetScopeGUID()))
		{
			const SGUID  guid = signal.GetGUID();
			stack_string fullName;
			DocUtils::GetFullElementName(signalFile, guid, fullName, EScriptElementType::Class);
			enumerator(signal.GetName(), fullName.c_str(), EScriptGraphPortFlags::BeginSequence | EScriptGraphPortFlags::Execute, CAggregateTypeId::FromScriptTypeGUID(guid));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CDocGraphStateNode::SEnumerateOptionalOutputs::VisitScriptTimer(const IScriptTimer& timer)
	{
		if(DocUtils::IsElementInScope(file, refGUID, timer.GetScopeGUID()))
		{
			const SGUID  guid = timer.GetGUID();
			stack_string fullName;
			DocUtils::GetFullElementName(file, guid, fullName, EScriptElementType::Class);
			enumerator(timer.GetName(), fullName.c_str(), EScriptGraphPortFlags::BeginSequence | EScriptGraphPortFlags::Execute, CAggregateTypeId::FromScriptTypeGUID(guid));
		}
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CDocGraphStateNode::SEnumerateOptionalOutputs::VisitScriptState(const IScriptState& state)
	{
		const SGUID  guid = state.GetGUID();
		stack_string name = state.GetName();
		name.append(" Requested");
		stack_string fullName;
		DocUtils::GetFullElementName(file, state.GetGUID(), fullName, EScriptElementType::Class);
		fullName.append(" Requested");
		enumerator(name.c_str(), fullName.c_str(), EScriptGraphPortFlags::BeginSequence | EScriptGraphPortFlags::Execute, CAggregateTypeId::FromScriptTypeGUID(guid));
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStateNode::SelectState(IDocGraphNodeCompiler& compiler) const
	{
		const size_t libStateIdx = LibUtils::FindStateByGUID(compiler.GetLibClass(), CDocGraphNodeBase::GetRefGUID());
		if(libStateIdx != INVALID_INDEX)
		{
			compiler.Set(0, MakeAny(int32(ELibTransitionResult::ChangeState)));
			compiler.Set(1, MakeAny(int32(libStateIdx)));
			compiler.Return();
		}
	}
}

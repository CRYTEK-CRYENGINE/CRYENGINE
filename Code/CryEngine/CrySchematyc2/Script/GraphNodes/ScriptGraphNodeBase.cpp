// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/GraphNodes/ScriptGraphNodeBase.h"

#include <CrySchematyc2/GUIDRemapper.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>
#include <CrySchematyc2/Services/ILog.h>

namespace Schematyc2
{
	CScriptGraphNodeBase::SPort::SPort(EScriptGraphPortFlags _flags, const char* _szName, const CAggregateTypeId& _typeId)
		: flags(_flags)
		, name(_szName)
		, typeId(_typeId)
	{}

	CScriptGraphNodeBase::CScriptGraphNodeBase()
		: m_pGraph(nullptr)
		, m_pos(ZERO)
	{}

	CScriptGraphNodeBase::CScriptGraphNodeBase(const SGUID& guid)
		: m_pGraph(nullptr)
		, m_guid(guid)
		, m_pos(ZERO)
	{}

	CScriptGraphNodeBase::CScriptGraphNodeBase(const SGUID& guid, const Vec2& pos)
		: m_pGraph(nullptr)
		, m_guid(guid)
		, m_pos(pos)
	{}

	void CScriptGraphNodeBase::Attach(IScriptGraphExtension& graph)
	{
		m_pGraph = &graph;
	}

	void CScriptGraphNodeBase::SetGUID(const SGUID& guid)
	{
		m_guid = guid;
	}

	SGUID CScriptGraphNodeBase::GetGUID() const
	{
		return m_guid;
	}

	void CScriptGraphNodeBase::SetName(const char* szName)
	{
		m_name = szName;
	}

	const char* CScriptGraphNodeBase::GetName() const
	{
		return m_name.c_str();
	}

	EScriptGraphNodeType CScriptGraphNodeBase::GetType() const
	{
		return EScriptGraphNodeType::Unknown;
	}

	SGUID CScriptGraphNodeBase::GetContextGUID() const
	{
		return SGUID();
	}

	SGUID CScriptGraphNodeBase::GetRefGUID() const
	{
		return SGUID();
	}

	void CScriptGraphNodeBase::SetPos(Vec2 pos)
	{
		m_pos = pos;
	}

	Vec2 CScriptGraphNodeBase::GetPos() const
	{
		return m_pos;
	}

	size_t CScriptGraphNodeBase::GetInputCount() const
	{
		return m_inputs.size();
	}

	uint32 CScriptGraphNodeBase::FindInput(const char* szName) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(szName);
		if(szName)
		{
			for(uint32 inputIdx = 0, inputCount = m_inputs.size(); inputIdx < inputCount; ++ inputIdx)
			{
				if(strcmp(m_inputs[inputIdx].name.c_str(), szName) == 0)
				{
					return inputIdx;
				}
			}
		}
		return s_invalidIdx;
	}

	const char* CScriptGraphNodeBase::GetInputName(size_t inputIdx) const
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].name.c_str() : "";
	}

	EScriptGraphPortFlags CScriptGraphNodeBase::GetInputFlags(size_t inputIdx) const
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].flags : EScriptGraphPortFlags::None;
	}

	CAggregateTypeId CScriptGraphNodeBase::GetInputTypeId(size_t inputIdx) const
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].typeId : CAggregateTypeId();
	}

	size_t CScriptGraphNodeBase::GetOutputCount() const
	{
		return m_outputs.size();
	}

	uint32 CScriptGraphNodeBase::FindOutput(const char* szName) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(szName);
		if(szName)
		{
			for(uint32 outputIdx = 0, outputCount = m_outputs.size(); outputIdx < outputCount; ++ outputIdx)
			{
				if(strcmp(m_outputs[outputIdx].name.c_str(), szName) == 0)
				{
					return outputIdx;
				}
			}
		}
		return s_invalidIdx;
	}

	const char* CScriptGraphNodeBase::GetOutputName(size_t outputIdx) const
	{
		return outputIdx < m_outputs.size() ? m_outputs[outputIdx].name.c_str() : "";
	}

	EScriptGraphPortFlags CScriptGraphNodeBase::GetOutputFlags(size_t outputIdx) const
	{
		return outputIdx < m_outputs.size() ? m_outputs[outputIdx].flags : EScriptGraphPortFlags::None;
	}

	CAggregateTypeId CScriptGraphNodeBase::GetOutputTypeId(size_t outputIdx) const
	{
		return outputIdx < m_outputs.size() ? m_outputs[outputIdx].typeId : CAggregateTypeId();
	}

	IAnyConstPtr CScriptGraphNodeBase::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	size_t CScriptGraphNodeBase::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	void CScriptGraphNodeBase::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	size_t CScriptGraphNodeBase::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	void CScriptGraphNodeBase::RemoveOutput(size_t outputIdx)
	{
		SCHEMATYC2_SYSTEM_ASSERT(outputIdx < m_outputs.size());
		if(outputIdx < m_outputs.size())
		{
			m_outputs.erase(m_outputs.begin() + outputIdx);
		}
	}

	void CScriptGraphNodeBase::Refresh(const SScriptRefreshParams& params)
	{
		m_name.clear();
		m_inputs.clear();
		m_outputs.clear();
	}

	void CScriptGraphNodeBase::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		switch(SerializationContext::GetPass(archive))
		{
		case ESerializationPass::PreLoad:
		case ESerializationPass::Save:
			{
				SerializeBasicInfo(archive);
				SerializeDebugLabel(archive);
				break;
			}
		case ESerializationPass::Edit:
			{
				Edit(archive);
				SerializeDebugLabel(archive);
				break;
			}
		}
	}

	void CScriptGraphNodeBase::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		m_guid = guidRemapper.Remap(m_guid);
	}

	void CScriptGraphNodeBase::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	void CScriptGraphNodeBase::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	void CScriptGraphNodeBase::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const {}

	IScriptGraphExtension* CScriptGraphNodeBase::GetGraph()
	{
		return m_pGraph;
	}

	const IScriptGraphExtension* CScriptGraphNodeBase::GetGraph() const
	{
		return m_pGraph;
	}

	const char* CScriptGraphNodeBase::GetDebugLabel() const
	{
		return m_debugLabel.c_str();
	}

	size_t CScriptGraphNodeBase::AddInput(EScriptGraphPortFlags flags, const char* szName, const CAggregateTypeId& typeId)
	{
		SCHEMATYC2_SYSTEM_ASSERT(szName);
		if(szName)
		{
			if(DocUtils::FindGraphNodeInput(*this, szName) == INVALID_INDEX)
			{
				m_inputs.push_back(SPort(flags, szName, typeId));
				return m_inputs.size() - 1;
			}
		}
		return INVALID_INDEX;
	}

	void CScriptGraphNodeBase::SetInputTypeId(size_t inputIdx, const CAggregateTypeId& typeId)
	{
		SCHEMATYC2_SYSTEM_ASSERT(inputIdx < m_inputs.size());
		if(inputIdx < m_inputs.size())
		{
			m_inputs[inputIdx].typeId = typeId;
		}
	}

	void CScriptGraphNodeBase::RemoveInput(size_t inputIdx)
	{
		SCHEMATYC2_SYSTEM_ASSERT(inputIdx < m_inputs.size());
		if(inputIdx < m_inputs.size())
		{
			m_inputs.erase(m_inputs.begin() + inputIdx);
		}
	}

	size_t CScriptGraphNodeBase::AddOutput(EScriptGraphPortFlags flags, const char* szName, const CAggregateTypeId& typeId)
	{
		SCHEMATYC2_SYSTEM_ASSERT(szName);
		if(szName)
		{
			if(DocUtils::FindGraphNodeOutput(*this, szName) == INVALID_INDEX)
			{
				m_outputs.push_back(SPort(flags, szName, typeId));
				return m_outputs.size() - 1;
			}
		}
		return INVALID_INDEX;
	}

	void CScriptGraphNodeBase::SetOutputTypeId(size_t outputIdx, const CAggregateTypeId& typeId)
	{
		SCHEMATYC2_SYSTEM_ASSERT(outputIdx < m_outputs.size());
		if(outputIdx < m_outputs.size())
		{
			m_outputs[outputIdx].typeId = typeId;
		}
	}

	void CScriptGraphNodeBase::SerializeBasicInfo(Serialization::IArchive& archive)
	{
		archive(m_guid, "guid");
		archive(m_pos, "pos");
	}

	void CScriptGraphNodeBase::SerializeDebugLabel(Serialization::IArchive& archive)
	{
		if(archive.openBlock("debug", "Debug"))
		{
			archive(m_debugLabel, "debugLabel", "^Label");
			archive.closeBlock();
		}
	}

	void CScriptGraphNodeBase::Edit(Serialization::IArchive& archive)
	{
		archive(m_name, "name", "!Name");
	}
}

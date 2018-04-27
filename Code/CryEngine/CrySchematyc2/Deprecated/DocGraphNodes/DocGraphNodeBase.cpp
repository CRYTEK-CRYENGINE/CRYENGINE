// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Replace CRY_ASSERT with SCHEMATYC2_SYSTEM_ASSERT.

#include "StdAfx.h"
#include "Deprecated/DocGraphNodes/DocGraphNodeBase.h"

#include <CrySchematyc2/GUIDRemapper.h>
#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>

#include "Script/ScriptVariableDeclaration.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CDocGraphNodeBase::CDocGraphNodeBase(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const char* szName, EScriptGraphNodeType type, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: m_file(file)
		, m_graph(graph)
		, m_guid(guid)
		, m_name(szName)
		, m_type(type)
		, m_contextGUID(contextGUID)
		, m_refGUID(refGUID)
		, m_pos(pos)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeBase::SetGUID(const SGUID& guid)
	{
		m_guid = guid;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CDocGraphNodeBase::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeBase::SetName(const char* szName)
	{
		m_name = szName;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CDocGraphNodeBase::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	EScriptGraphNodeType CDocGraphNodeBase::GetType() const
	{
		return m_type;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CDocGraphNodeBase::GetContextGUID() const
	{
		return m_contextGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CDocGraphNodeBase::GetRefGUID() const
	{
		return m_refGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeBase::SetPos(Vec2 pos)
	{
		m_pos = pos;
	}

	//////////////////////////////////////////////////////////////////////////
	Vec2 CDocGraphNodeBase::GetPos() const
	{
		return m_pos;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeBase::GetInputCount() const
	{
		return m_inputs.size();
	}

	//////////////////////////////////////////////////////////////////////////
	uint32 CDocGraphNodeBase::FindInput(const char* szName) const
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

	//////////////////////////////////////////////////////////////////////////
	const char* CDocGraphNodeBase::GetInputName(size_t inputIdx) const
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].name.c_str() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	EScriptGraphPortFlags CDocGraphNodeBase::GetInputFlags(size_t inputIdx) const
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].flags : EScriptGraphPortFlags::None;
	}

	//////////////////////////////////////////////////////////////////////////
	CAggregateTypeId CDocGraphNodeBase::GetInputTypeId(size_t inputIdx) const
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].typeId : CAggregateTypeId();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeBase::GetOutputCount() const
	{
		return m_outputs.size();
	}

	//////////////////////////////////////////////////////////////////////////
	uint32 CDocGraphNodeBase::FindOutput(const char* szName) const
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

	//////////////////////////////////////////////////////////////////////////
	const char* CDocGraphNodeBase::GetOutputName(size_t outputIdx) const
	{
		return outputIdx < m_outputs.size() ? m_outputs[outputIdx].name.c_str() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	EScriptGraphPortFlags CDocGraphNodeBase::GetOutputFlags(size_t outputIdx) const
	{
		return outputIdx < m_outputs.size() ? m_outputs[outputIdx].flags : EScriptGraphPortFlags::None;
	}

	//////////////////////////////////////////////////////////////////////////
	CAggregateTypeId CDocGraphNodeBase::GetOutputTypeId(size_t outputIdx) const
	{
		return outputIdx < m_outputs.size() ? m_outputs[outputIdx].typeId : CAggregateTypeId();
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeBase::RemoveOutput(size_t outputIdx)
	{
		CRY_ASSERT(outputIdx < m_outputs.size());
		if(outputIdx < m_outputs.size())
		{
			m_outputs.erase(m_outputs.begin() + outputIdx);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeBase::Refresh(const SScriptRefreshParams& params)
	{
		m_inputs.clear();
		m_outputs.clear();
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeBase::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;
		if(archive.isEdit())
		{
			archive(m_name, "name", "!Name");
		}
		if(archive.openBlock("debug", "Debug"))
		{
			archive(m_debugLabel, "debugLabel", "^Label");
			archive.closeBlock();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeBase::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		m_guid        = guidRemapper.Remap(m_guid);
		m_contextGUID = guidRemapper.Remap(m_contextGUID);
		m_refGUID     = guidRemapper.Remap(m_refGUID);
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptFile& CDocGraphNodeBase::GetFile()
	{
		return m_file;
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptFile& CDocGraphNodeBase::GetFile() const
	{
		return m_file;
	}

	//////////////////////////////////////////////////////////////////////////
	IDocGraph& CDocGraphNodeBase::GetGraph()
	{
		return m_graph;
	}

	//////////////////////////////////////////////////////////////////////////
	const IDocGraph& CDocGraphNodeBase::GetGraph() const
	{
		return m_graph;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CDocGraphNodeBase::GetDebugLabel() const
	{
		return m_debugLabel.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeBase::SetContextGUID(const SGUID& contextGUID)
	{
		m_contextGUID = contextGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeBase::SetRefGUID(const SGUID& refGUID)
	{
		m_refGUID = refGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeBase::AddInput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		CRY_ASSERT(szName);
		if(szName)
		{
			if(DocUtils::FindGraphNodeInput(*this, szName) == INVALID_INDEX)
			{
				m_inputs.push_back(SPort(szName, flags, typeId));
				return m_inputs.size() - 1;
			}
		}
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeBase::SetInputTypeId(size_t inputIdx, const CAggregateTypeId& typeId)
	{
		CRY_ASSERT(inputIdx < m_inputs.size());
		if(inputIdx < m_inputs.size())
		{
			m_inputs[inputIdx].typeId = typeId;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeBase::RemoveInput(size_t inputIdx)
	{
		CRY_ASSERT(inputIdx < m_inputs.size());
		if(inputIdx < m_inputs.size())
		{
			m_inputs.erase(m_inputs.begin() + inputIdx);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeBase::AddOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		LOADING_TIME_PROFILE_SECTION;
		CRY_ASSERT(szName);
		if(szName)
		{
			if(DocUtils::FindGraphNodeOutput(*this, szName) == INVALID_INDEX)
			{
				m_outputs.push_back(SPort(szName, flags, typeId));
				return m_outputs.size() - 1;
			}
		}
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeBase::SetOutputTypeId(size_t outputIdx, const CAggregateTypeId& typeId)
	{
		CRY_ASSERT(outputIdx < m_outputs.size());
		if(outputIdx < m_outputs.size())
		{
			m_outputs[outputIdx].typeId = typeId;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphNodeBase::SPort::SPort(const char* _szName, EScriptGraphPortFlags _flags, const CAggregateTypeId& _typeId)
		: name(_szName)
		, flags(_flags)
		, typeId(_typeId)
	{}

	namespace DocGraphNodeUtils
	{
		void CopyInputsToTopOfStack(const CDocGraphNodeBase& node, size_t firstInputIdx, const IAnyPtrVector& inputValues, IDocGraphNodeCompiler& compiler)
		{
			const size_t	inputCount = node.GetInputCount();
			for(size_t iInput = firstInputIdx; iInput < inputCount; ++ iInput)
			{
				// Find input on stack.
				const size_t	stackPos = compiler.FindInputOnStack(node, iInput);
				if(stackPos != INVALID_INDEX)
				{
					compiler.Copy(stackPos, INVALID_INDEX, *inputValues[iInput]);
				}
				else
				{
					// Push default input value.
					compiler.Push(*inputValues[iInput]);
				}
			}
		}

		void CopyInputsToStack(const CDocGraphNodeBase& node, size_t firstInputIdx, const IAnyPtrVector& inputValues, IDocGraphNodeCompiler& compiler)
		{
			// #SchematycTODO : Figure out how we can safely optimize the number of copy operations?
			return CopyInputsToTopOfStack(node, firstInputIdx, inputValues, compiler);
		}

		void CopyInputsToTopOfStack(const CDocGraphNodeBase& node, size_t firstInputIdx, const IAnyPtrVector& inputValues, size_t firstInputValueIdx, IDocGraphNodeCompiler& compiler)
		{
			const size_t	inputCount = node.GetInputCount();
			for(size_t iInput = firstInputIdx; iInput < inputCount; ++ iInput)
			{
				// Find input on stack.
				const size_t	stackPos = compiler.FindInputOnStack(node, iInput);
				const size_t	iInputValue = firstInputValueIdx + (iInput - firstInputIdx);
				if(stackPos != INVALID_INDEX)
				{
					compiler.Copy(stackPos, INVALID_INDEX, *inputValues[iInputValue]);
				}
				else
				{
					// Push default input value.
					compiler.Push(*inputValues[iInputValue]);
				}
			}
		}

		void CopyInputsToStack(const CDocGraphNodeBase& node, size_t firstInputIdx, const IAnyPtrVector& inputValues, size_t firstInputValueIdx, IDocGraphNodeCompiler& compiler)
		{
			// #SchematycTODO : Figure out how we can safely optimize the number of copy operations?
			return CopyInputsToTopOfStack(node, firstInputIdx, inputValues, firstInputValueIdx, compiler);
		}
	}
}

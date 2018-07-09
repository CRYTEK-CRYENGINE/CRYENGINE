// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Compiler.h"

#include <CrySchematyc2/IFramework.h>
#include <CrySchematyc2/ILibRegistry.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>
#include <CrySchematyc2/Deprecated/Variant.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Script/IScriptRegistry.h>
#include <CrySchematyc2/Services/ILog.h>
#include <CrySchematyc2/Utils/StringUtils.h>

#include "CVars.h"
#include "Script/ScriptGraphCompiler.h"

namespace Schematyc2
{
	namespace CompilerUtils
	{
		//////////////////////////////////////////////////////////////////////////
		inline size_t NumberOfDigits(size_t x)
		{
			size_t numberOfDigits = 1;
			while(x > 9)
			{
				x /= 10;
				++ numberOfDigits;
			}
			return numberOfDigits;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphSequencePreCompiler::CDocGraphSequencePreCompiler(CLibClass& libClass, const LibFunctionId& libFunctionId, CLibFunction& libFunction)
		: m_libClass(libClass)
		, m_libFunctionId(libFunctionId)
		, m_libFunction(libFunction)
	{}

	//////////////////////////////////////////////////////////////////////////
	bool CDocGraphSequencePreCompiler::BindFunctionToGUID(const SGUID& guid)
	{
		if(m_libClass.BindFunctionToGUID(m_libFunctionId, guid))
		{
			m_libFunction.SetGUID(guid);
			return true;
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSequencePreCompiler::SetFunctionName(const char* name)
	{
		m_libFunction.SetName(name);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSequencePreCompiler::AddFunctionInput(const char* name, const char* textDesc, const IAny& value)
	{
		m_libFunction.AddInput(name, textDesc, value);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSequencePreCompiler::AddFunctionOutput(const char* name, const char* textDesc, const IAny& value)
	{
		m_libFunction.AddOutput(name, textDesc, value);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSequencePreCompiler::PrecompileSequence(const IScriptGraphNode& node, size_t iNodeOutput)
	{
		node.PreCompileSequence(*this, iNodeOutput);
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphSequenceLinker::CDocGraphSequenceLinker(CLibClass& libClass, CLibStateMachine* pLibStateMachine, CLibState* pLibState)
		: m_libClass(libClass)
		, m_pLibStateMachine(pLibStateMachine)
		, m_pLibState(pLibState)
	{}

	//////////////////////////////////////////////////////////////////////////
	ILibStateMachine* CDocGraphSequenceLinker::GetLibStateMachine()
	{
		return m_pLibStateMachine;
	}

	//////////////////////////////////////////////////////////////////////////
	ILibState* CDocGraphSequenceLinker::GetLibState()
	{
		return m_pLibState;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSequenceLinker::CreateConstructor(const SGUID& guid, const LibFunctionId& libFunctionId)
	{
		if(m_pLibState)
		{
			m_pLibState->AddConstructor(m_libClass.AddConstructor(guid, libFunctionId));
		}
		else
		{
			m_libClass.AddPersistentConstructor(guid, libFunctionId);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSequenceLinker::CreateDestructor(const SGUID& guid, const LibFunctionId& libFunctionId)
	{
		if(m_pLibState)
		{
			m_pLibState->AddDestructor(m_libClass.AddDestructor(guid, libFunctionId));
		}
		else
		{
			m_libClass.AddPersistentDestructor(guid, libFunctionId);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSequenceLinker::CreateSignalReceiver(const SGUID& guid, const SGUID& contextGUID, const LibFunctionId& libFunctionId)
	{
		if(m_pLibState)
		{
			m_pLibState->AddSignalReceiver(m_libClass.AddSignalReceiver(guid, contextGUID, libFunctionId));
		}
		else
		{
			m_libClass.AddPersistentSignalReceiver(guid, contextGUID, libFunctionId);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSequenceLinker::CreateTransition(const SGUID& guid, const SGUID& stateGUID, const SGUID& contextGUID, const LibFunctionId& libFunctionId)
	{
		CLibState*	pLibState = m_libClass.GetState(LibUtils::FindStateByGUID(m_libClass, stateGUID));
		SCHEMATYC2_COMPILER_ASSERT(pLibState);
		if(pLibState)
		{
			pLibState->AddTransition(m_libClass.AddTransition(guid, contextGUID, libFunctionId));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSequenceLinker::LinkSequence(const IScriptGraphNode& node, size_t iNodeOutput, const LibFunctionId& libFunctionId)
	{
		node.LinkSequence(*this, iNodeOutput, libFunctionId);
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphNodeCompiler::CDocGraphNodeCompiler(const IScriptFile& file, const IDocGraph& docGraph, CLib& lib, CLibClass& libClass, CLibFunction& libFunction)
		: m_file(file)
		, m_docGraph(docGraph)
		, m_lib(lib)
		, m_libClass(libClass)
		, m_libFunction(libFunction)
	{
		m_stack.reserve(64);
		m_stackFrames.reserve(32);
		m_markers.reserve(16);
		m_branches.reserve(16);
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_previewLines.reserve(128);
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	ILibClass& CDocGraphNodeCompiler::GetLibClass() const
	{
		return m_libClass;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeCompiler::GetStackSize() const
	{
		return !m_stack.empty() ? m_stack.size() : 0;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeCompiler::GetStackInputPos() const
	{
		return m_libFunction.GetVariantOutputs().size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeCompiler::GetStackOutputPos() const
	{
		return 0;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::CreateStackFrame(const IScriptGraphNode& node, size_t label)
	{
		SCHEMATYC2_COMPILER_ASSERT(GetStackFramePos(node.GetGUID(), label) == INVALID_INDEX);
		m_stackFrames.push_back(SStackFrame(node.GetGUID(), label, m_stack.size()));
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::CollapseStackFrame(const IScriptGraphNode& node, size_t label)
	{
		const SGUID		nodeGUID = node.GetGUID();
		const size_t	stackFramePos = GetStackFramePos(nodeGUID, label);
		SCHEMATYC2_COMPILER_ASSERT(stackFramePos != INVALID_INDEX);
		if(stackFramePos != INVALID_INDEX)
		{
			m_stack.resize(stackFramePos);
			SVMOp*	pLastOp = m_libFunction.GetLastOp();
			if(pLastOp && (pLastOp->opCode == SVMOp::COLLAPSE))
			{
				SVMCollapseOp*	pCollapseOp = static_cast<SVMCollapseOp*>(pLastOp);
				if(stackFramePos < pCollapseOp->pos)
				{
					pCollapseOp->pos = stackFramePos;
				}
			}
			else
			{
				m_libFunction.AddOp(SVMCollapseOp(stackFramePos));
			}
			DestroyStackFrame(nodeGUID, label);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::AddOutputToStack(const IScriptGraphNode& node, size_t iOutput, const IAny& value)
	{
		TVariantVector							variants;
		CVariantVectorOutputArchive	archive(variants);
		archive(value, "", "");
		const size_t	pos = m_stack.size();
		m_stack.resize(pos + variants.size());
		m_stack[pos].outputBindings.push_back(SOutputBinding(node.GetGUID(), iOutput));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		if(m_stack[pos].name.empty())
		{
			m_stack[pos].name = node.GetOutputName(iOutput);
		}
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeCompiler::BindOutputToStack(const IScriptGraphNode& node, size_t pos, size_t iOutput, const IAny& value)
	{
		TVariantVector							variants;
		CVariantVectorOutputArchive	archive(variants);
		archive(value, "", "");
		const size_t	variantSize = variants.size();
		const size_t	stackSize = m_stack.size();
		SCHEMATYC2_COMPILER_ASSERT((pos + variantSize) <= stackSize);
		if((pos + variantSize) <= stackSize)
		{
			m_stack[pos].outputBindings.push_back(SOutputBinding(node.GetGUID(), iOutput));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			if(m_stack[pos].name.empty())
			{
				m_stack[pos].name = node.GetOutputName(iOutput);
			}
#endif
			return variantSize;
		}
		return 0;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeCompiler::FindInputOnStack(const IScriptGraphNode& node, size_t iInput)
	{
		const char*                 inputName = node.GetInputName(iInput);
		TScriptGraphLinkConstVector inputLinks;
		DocUtils::CollectGraphNodeInputLinks(m_docGraph, node.GetGUID(), inputName, inputLinks);
		if(!inputLinks.empty())
		{
			const IScriptGraphLink& link = *inputLinks.front();
			const IScriptGraphNode* pSrcNode = m_docGraph.GetNode(link.GetSrcNodeGUID());
			SCHEMATYC2_COMPILER_ASSERT(pSrcNode);
			if(pSrcNode)
			{
				return FindOutputOnStack(link.GetSrcNodeGUID(), DocUtils::FindGraphNodeOutput(*pSrcNode, link.GetSrcOutputName()));
			}
		}
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeCompiler::FindOutputOnStack(const IScriptGraphNode& node, size_t iOutput)
	{
		return FindOutputOnStack(node.GetGUID(), iOutput);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::BindVariableToStack(const IScriptGraphNode& node, size_t pos, size_t label)
	{
		SCHEMATYC2_COMPILER_ASSERT(pos < m_stack.size());
		if(pos < m_stack.size())
		{
			m_stack[pos].labelBindings.push_back(SLabelBinding(node.GetGUID(), label));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeCompiler::FindVariableOnStack(const IScriptGraphNode& node, size_t label) const
	{
		return FindVariableOnStack(node.GetGUID(), label);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::CreateMarker(const IScriptGraphNode& node, size_t label)
	{
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_markers.push_back(SMarker(node.GetGUID(), label, m_libFunction.GetSize(), m_previewLines.size()));
#else
		m_markers.push_back(SMarker(node.GetGUID(), label, m_libFunction.GetSize()));
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::Branch(const IScriptGraphNode& node, size_t label)
	{
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_branches.push_back(SBranch(node.GetGUID(), label, m_libFunction.AddOp(SVMBranchOp(0)), m_previewLines.size()));
		m_previewLines.push_back(string());
#else
		m_branches.push_back(SBranch(node.GetGUID(), label, m_libFunction.AddOp(SVMBranchOp(0))));
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::BranchIfZero(const IScriptGraphNode& node, size_t label)
	{
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_branches.push_back(SBranch(node.GetGUID(), label, m_libFunction.AddOp(SVMBranchIfZeroOp(0)), m_previewLines.size()));
		m_previewLines.push_back(string());
#else
		m_branches.push_back(SBranch(node.GetGUID(), label, m_libFunction.AddOp(SVMBranchIfZeroOp(0))));
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::BranchIfNotZero(const IScriptGraphNode& node, size_t label)
	{
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_branches.push_back(SBranch(node.GetGUID(), label, m_libFunction.AddOp(SVMBranchIfNotZeroOp(0)), m_previewLines.size()));
		m_previewLines.push_back(string());
#else
		m_branches.push_back(SBranch(node.GetGUID(), label, m_libFunction.AddOp(SVMBranchIfNotZeroOp(0))));
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::GetObject()
	{
		m_stack.push_back(SStackVariable());
		m_libFunction.AddOp(SVMGetObjectOp());
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_previewLines.push_back("GET_OBJECT");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::Push(const IAny& value)
	{
		TVariantVector							variants;
		CVariantVectorOutputArchive	archive(variants);
		archive(value, "", "");
		for(TVariantVector::const_iterator iVariant = variants.begin(), iEndVariant = variants.end(); iVariant != iEndVariant; ++ iVariant)
		{
			m_stack.push_back(SStackVariable());
			m_libFunction.AddOp(SVMPushOp(m_libFunction.AddConstValue(*iVariant)));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			string			previewLine = "PUSH ";
			char				stringBuffer[256] = "";
			const bool	typeIsString = (iVariant->GetTypeId() == CVariant::STRING) || (iVariant->GetTypeId() == CVariant::POOL_STRING);
			if(typeIsString)
			{
				previewLine.append("\"");
			}
			previewLine.append(StringUtils::VariantToString(*iVariant, stringBuffer));
			if(typeIsString)
			{
				previewLine.append("\"");
			}
			m_previewLines.push_back(previewLine);
#endif
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::Set(size_t pos, const IAny& value)
	{
		TVariantVector							variants;
		CVariantVectorOutputArchive	archive(variants);
		archive(value, "", "");
		for(TVariantVector::const_iterator iVariant = variants.begin(), iEndVariant = variants.end(); iVariant != iEndVariant; ++ iVariant)
		{
			m_libFunction.AddOp(SVMSetOp(pos, m_libFunction.AddConstValue(*iVariant)));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			string	previewLine = "SET ";
			char		stringBuffer[256] = "";
			previewLine.append(StringUtils::SizeTToString(pos, stringBuffer));
			previewLine.append(" ");
			previewLine.append(StringUtils::VariantToString(*iVariant, stringBuffer));
			m_previewLines.push_back(previewLine);
#endif
			pos ++;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::Copy(size_t srcPos, size_t dstPos, const IAny& value)
	{
		TVariantVector							variants;
		CVariantVectorOutputArchive	archive(variants);
		archive(value, "", "");
		if(dstPos == INVALID_INDEX)
		{
			m_stack.resize(m_stack.size() + variants.size());
		}
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		string	previewLine = "COPY";
		char		stringBuffer[256] = "";
		previewLine.append("[");
		previewLine.append(StringUtils::SizeTToString(variants.size(), stringBuffer));
		previewLine.append("] ");
		if(srcPos == INVALID_INDEX)
		{
			previewLine.append("TOP");
		}
		else
		{
			SCHEMATYC2_COMPILER_ASSERT(srcPos < m_stack.size());
			if((srcPos < m_stack.size()) && !m_stack[srcPos].name.empty())
			{
				previewLine.append(m_stack[srcPos].name);
			}
			else
			{
				previewLine.append(StringUtils::SizeTToString(srcPos, stringBuffer));
			}
		}
		previewLine.append(" ");
		if(dstPos == INVALID_INDEX)
		{
			previewLine.append("TOP");
		}
		else
		{
			SCHEMATYC2_COMPILER_ASSERT(dstPos < m_stack.size());
			if((dstPos < m_stack.size()) && !m_stack[dstPos].name.empty())
			{
				previewLine.append(m_stack[dstPos].name);
			}
			else
			{
				previewLine.append(StringUtils::SizeTToString(dstPos, stringBuffer));
			}
		}
		m_previewLines.push_back(previewLine.c_str());
#endif
		for(TVariantVector::const_iterator iVariant = variants.begin(), iEndVariant = variants.end(); iVariant != iEndVariant; ++ iVariant)
		{
			m_libFunction.AddOp(SVMCopyOp(srcPos, dstPos));
			++ srcPos;
			if(dstPos != INVALID_INDEX)
			{
				++ dstPos;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::Pop(size_t count)
	{
		m_stack.resize(m_stack.size() - count);
		m_libFunction.AddOp(SVMPopOp(count));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		string	previewLine = "POP ";
		char		stringBuffer[256] = "";
		previewLine.append(StringUtils::SizeTToString(count, stringBuffer));
		m_previewLines.push_back(previewLine);
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeCompiler::Load(const SGUID& guid)
	{
		const size_t	iVariable = LibUtils::FindVariableByGUID(m_libClass, guid);
		SCHEMATYC2_COMPILER_ASSERT(iVariable != INVALID_INDEX);
		if(iVariable != INVALID_INDEX)
		{
			const ILibVariable&	libVariable = *m_libClass.GetVariable(iVariable);
			IAnyConstPtr				pLibVariableValue = libVariable.GetValue();
			SCHEMATYC2_COMPILER_ASSERT(pLibVariableValue);
			if(pLibVariableValue)
			{
				TVariantVector							variants;
				CVariantVectorOutputArchive	archive(variants);
				archive(*pLibVariableValue, "", "");
				m_stack.resize(m_stack.size() + variants.size());
				const size_t	pos = m_libFunction.AddOp(SVMLoadOp(libVariable.GetVariantPos(), variants.size()));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
				m_previewLines.push_back("LOAD ? ?");
#endif
				return pos;
			}
		}
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeCompiler::Store(const SGUID& guid)
	{
		const size_t	iVariable = LibUtils::FindVariableByGUID(m_libClass, guid);
		SCHEMATYC2_COMPILER_ASSERT(iVariable != INVALID_INDEX);
		if(iVariable != INVALID_INDEX)
		{
			const ILibVariable&	libVariable = *m_libClass.GetVariable(iVariable);
			IAnyConstPtr				pLibVariableValue = libVariable.GetValue();
			SCHEMATYC2_COMPILER_ASSERT(pLibVariableValue);
			if(pLibVariableValue)
			{
				TVariantVector							variants;
				CVariantVectorOutputArchive	archive(variants);
				archive(*pLibVariableValue, "", "");
				const size_t	pos = m_libFunction.AddOp(SVMStoreOp(libVariable.GetVariantPos(), variants.size()));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
				m_previewLines.push_back("STORE ? ?");
#endif
				return pos;
			}
		}
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::ContainerAdd(const SGUID& guid, const IAny& value)
	{
		const size_t	iContainer = LibUtils::FindContainerByGUID(m_libClass, guid);
		SCHEMATYC2_COMPILER_ASSERT(iContainer != INVALID_INDEX);
		if(iContainer != INVALID_INDEX)
		{
			TVariantVector							variants;
			CVariantVectorOutputArchive	archive(variants);
			archive(value, "", "");
			m_libFunction.AddOp(SVMContainerAddOp(iContainer, variants.size()));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			m_previewLines.push_back("CONTAINER_ADD ? ?");
#endif
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::ContainerRemoveByIndex(const SGUID& guid)
	{
		const size_t	iContainer = LibUtils::FindContainerByGUID(m_libClass, guid);
		SCHEMATYC2_COMPILER_ASSERT(iContainer != INVALID_INDEX);
		if (iContainer != INVALID_INDEX)
		{
			m_stack.push_back(SStackVariable());
			m_libFunction.AddOp(SVMContainerRemoveByIndexOp(iContainer));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			m_previewLines.push_back("CONTAINER_REMOVE_BY_VALUE ?");
#endif
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::ContainerRemoveByValue(const SGUID& guid, const IAny& value)
	{
		const size_t	iContainer = LibUtils::FindContainerByGUID(m_libClass, guid);
		SCHEMATYC2_COMPILER_ASSERT(iContainer != INVALID_INDEX);
		if(iContainer != INVALID_INDEX)
		{
			m_stack.push_back(SStackVariable());
			TVariantVector							variants;
			CVariantVectorOutputArchive	archive(variants);
			archive(value, "", "");
			m_libFunction.AddOp(SVMContainerRemoveByValueOp(iContainer, variants.size()));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			m_previewLines.push_back("CONTAINER_REMOVE_BY_VALUE ? ?");
#endif
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::ContainerClear(const SGUID& guid)
	{
		const size_t	iContainer = LibUtils::FindContainerByGUID(m_libClass, guid);
		SCHEMATYC2_COMPILER_ASSERT(iContainer != INVALID_INDEX);
		if(iContainer != INVALID_INDEX)
		{
			m_libFunction.AddOp(SVMContainerClearOp(iContainer));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			m_previewLines.push_back("CONTAINER_CLEAR ?");
#endif
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::ContainerSize(const SGUID& guid, const IAny& value)
	{
		const size_t	iContainer = LibUtils::FindContainerByGUID(m_libClass, guid);
		SCHEMATYC2_COMPILER_ASSERT(iContainer != INVALID_INDEX);
		if(iContainer != INVALID_INDEX)
		{
			m_stack.push_back(SStackVariable());
			TVariantVector							variants;
			CVariantVectorOutputArchive	archive(variants);
			archive(value, "", "");
			m_libFunction.AddOp(SVMContainerSizeOp(iContainer, variants.size()));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			m_previewLines.push_back("CONTAINER_SIZE ? ?");
#endif
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::ContainerGet(const SGUID& guid, const IAny& value)
	{
		const size_t	iContainer = LibUtils::FindContainerByGUID(m_libClass, guid);
		SCHEMATYC2_COMPILER_ASSERT(iContainer != INVALID_INDEX);
		if(iContainer != INVALID_INDEX)
		{
			m_stack.push_back(SStackVariable());
			TVariantVector							variants;
			CVariantVectorOutputArchive	archive(variants);
			archive(value, "", "");
			m_libFunction.AddOp(SVMContainerGetOp(iContainer, variants.size()));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			m_previewLines.push_back("CONTAINER_GET ? ?");
#endif
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::ContainerSet(const SGUID& guid, const IAny& value)
	{
		const size_t	iContainer = LibUtils::FindContainerByGUID(m_libClass, guid);
		SCHEMATYC2_COMPILER_ASSERT(iContainer != INVALID_INDEX);
		if (iContainer != INVALID_INDEX)
		{
			m_stack.push_back(SStackVariable());
			TVariantVector							variants;
			CVariantVectorOutputArchive	archive(variants);
			archive(value, "", "");
			m_libFunction.AddOp(SVMContainerSetOp(iContainer, variants.size()));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			m_previewLines.push_back("CONTAINER_SET ? ?");
#endif
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::ContainerFindByValue(const SGUID& guid, const IAny& value)
	{
		const size_t	iContainer = LibUtils::FindContainerByGUID(m_libClass, guid);
		SCHEMATYC2_COMPILER_ASSERT(iContainer != INVALID_INDEX);
		if (iContainer != INVALID_INDEX)
		{
			m_stack.push_back(SStackVariable());
			TVariantVector							variants;
			CVariantVectorOutputArchive	archive(variants);
			archive(value, "", "");
			m_libFunction.AddOp(SVMContainerFindByValueOp(iContainer, variants.size()));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			m_previewLines.push_back("CONTAINER_FIND_BY_VALUE ? ?");
#endif
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::Compare(size_t lhsPos, size_t rhsPos, size_t count)
	{
		m_stack.push_back(SStackVariable());
		m_libFunction.AddOp(SVMCompareOp(lhsPos, rhsPos, count));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_previewLines.push_back("COMPARE ? ? ?");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::IncrementInt32(size_t pos)
	{
		m_libFunction.AddOp(SVMIncrementInt32Op(pos));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_previewLines.push_back("INCREMENT_INT32 ?");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::LessThanInt32(size_t lhsPos, size_t rhsPos)
	{
		m_stack.push_back(SStackVariable());
		m_libFunction.AddOp(SVMLessThanInt32Op(lhsPos, rhsPos));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_previewLines.push_back("LESS_THAN_INT32 ? ?");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::GreaterThanInt32(size_t lhsPos, size_t rhsPos)
	{
		m_stack.push_back(SStackVariable());
		m_libFunction.AddOp(SVMGreaterThanInt32Op(lhsPos, rhsPos));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_previewLines.push_back("GREATER_THAN_INT32 ? ?");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::StartTimer(const SGUID& guid)
	{
		m_libFunction.AddOp(SVMStartTimerOp(guid));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_previewLines.push_back("START_TIMER ?");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::StopTimer(const SGUID& guid)
	{
		m_libFunction.AddOp(SVMStopTimerOp(guid));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_previewLines.push_back("STOP_TIMER ?");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::ResetTimer(const SGUID& guid)
	{
		m_libFunction.AddOp(SVMResetTimerOp(guid));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_previewLines.push_back("RESET_TIMER ?");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::SendSignal(const SGUID& guid)
	{
		m_libFunction.AddOp(SVMSendSignalOp(guid));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_previewLines.push_back("SEND_SIGNAL ?");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::BroadcastSignal(const SGUID& guid)
	{
		m_libFunction.AddOp(SVMBroadcastSignalOp(guid));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_previewLines.push_back("BROADCAST_SIGNAL ?");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::CallGlobalFunction(const SGUID& guid) // #SchematycTODO : Pass by pointer/reference instead?
	{
		IGlobalFunctionConstPtr	pGlobalFunction = gEnv->pSchematyc2->GetEnvRegistry().GetGlobalFunction(guid);
		SCHEMATYC2_COMPILER_ASSERT(pGlobalFunction);
		if(pGlobalFunction)
		{
			m_libFunction.AddOp(SVMCallGlobalFunctionOp(m_libFunction.AddGlobalFunction(pGlobalFunction)));
		}
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		IGlobalFunctionConstPtr	pGlobalFunction = gEnv->pSchematyc2->GetEnvRegistry().GetGlobalFunction(guid);
		string									previewLine = "CALL_GLOBAL_FUNCTION ";
		previewLine.append(pGlobalFunction ? pGlobalFunction->GetName() : "?");
		m_previewLines.push_back(previewLine);
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::CallEnvAbstractInterfaceFunction(const SGUID& abstractInterfaceGUID, const SGUID& functionGUID)
	{
		m_libFunction.AddOp(SVMCallEnvAbstractInterfaceFunctionOp(abstractInterfaceGUID, functionGUID));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_previewLines.push_back("CALL_ENV_ABSTRACT_INTERFACE_FUNCTION ? ?");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::CallLibAbstractInterfaceFunction(const SGUID& abstractInterfaceGUID, const SGUID& functionGUID)
	{
		m_libFunction.AddOp(SVMCallLibAbstractInterfaceFunctionOp(abstractInterfaceGUID, functionGUID));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		ILibAbstractInterfaceConstPtr					pAbstractInterface = m_lib.GetAbstractInterface(abstractInterfaceGUID);
		ILibAbstractInterfaceFunctionConstPtr	pFunction = m_lib.GetAbstractInterfaceFunction(functionGUID);
		string																previewLine = "CALL_LIB_ABSTRACT_INTERFACE_FUNCTION ";
		previewLine.append(pAbstractInterface ? pAbstractInterface->GetName() : "?");
		previewLine.append("::");
		previewLine.append(pFunction ? pFunction->GetName() : "?");
		m_previewLines.push_back(previewLine);
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::CallComponentMemberFunction(const SGUID& guid) // #SchematycTODO : Pass by pointer/reference instead?
	{
		IComponentMemberFunctionConstPtr	pComponentMemberFunction = gEnv->pSchematyc2->GetEnvRegistry().GetComponentMemberFunction(guid);
		SCHEMATYC2_COMPILER_ASSERT(pComponentMemberFunction);
		if(pComponentMemberFunction)
		{
			m_libFunction.AddOp(SVMCallComponentMemberFunctionOp(m_libFunction.AddComponentMemberFunction(pComponentMemberFunction)));
		}
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_previewLines.push_back("CALL_COMPONENT_MEMBER_FUNCTION ?");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::CallActionMemberFunction(const SGUID& guid) // #SchematycTODO : Pass by pointer/reference instead?
	{
		IActionMemberFunctionConstPtr	pActionMemberFunction = gEnv->pSchematyc2->GetEnvRegistry().GetActionMemberFunction(guid);
		SCHEMATYC2_COMPILER_ASSERT(pActionMemberFunction);
		if(pActionMemberFunction)
		{
			m_libFunction.AddOp(SVMCallActionMemberFunctionOp(m_libFunction.AddActionMemberFunction(pActionMemberFunction)));
		}
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_previewLines.push_back("CALL_ACTION_MEMBER_FUNCTION ?");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::CallLibFunction(const LibFunctionId& libFunctionId)
	{
		m_libFunction.AddOp(SVMCallLibFunctionOp(libFunctionId));
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		m_previewLines.push_back("CALL_LIB_FUNCTION ?");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::Return()
	{
		const SVMOp*	pLastOp = m_libFunction.GetLastOp();
		if(!pLastOp || (pLastOp->opCode != SVMOp::RETURN))
		{
			m_libFunction.AddOp(SVMReturnOp());
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
			m_previewLines.push_back("RETURN");
#endif
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::CompileSequence(const IScriptGraphNode& node, size_t iNodeOutput)
	{
		BeginSequence();
		DocUtils::UnrollGraphSequence(m_docGraph, node, iNodeOutput, DocUtils::GraphSequenceCallback::FromMemberFunction<CDocGraphNodeCompiler, &CDocGraphNodeCompiler::VisitSequenceNode>(*this));
		EndSequence();
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		SCHEMATYC2_COMPILER_COMMENT("%s:", m_docGraph.GetName());
		string				format;
		const size_t	previewLineCount = m_previewLines.size();
		format.Format("%s%dd ", "%.", CompilerUtils::NumberOfDigits(previewLineCount));
		for(size_t iPreviewLine = 0, previewLineCount = m_previewLines.size(); iPreviewLine < previewLineCount; ++ iPreviewLine)
		{
			string	prefix;
			prefix.Format(format.c_str(), iPreviewLine);
			m_previewLines[iPreviewLine].insert(0, prefix);
		}
		for(size_t iPreviewLine = 0, previewLineCount = m_previewLines.size(); iPreviewLine < previewLineCount; ++ iPreviewLine)
		{
			SCHEMATYC2_COMPILER_COMMENT(m_previewLines[iPreviewLine].c_str());
		}
		SCHEMATYC2_COMPILER_COMMENT("");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphNodeCompiler::SOutputBinding::SOutputBinding(const SGUID& _nodeGUID, size_t _output)
		: nodeGUID(_nodeGUID)
		, output(_output)
	{}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphNodeCompiler::SLabelBinding::SLabelBinding(const SGUID& _nodeGUID, size_t _label)
		: nodeGUID(_nodeGUID)
		, label(_label)
	{}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphNodeCompiler::SStackVariable::SStackVariable()
	{
		outputBindings.reserve(16);
		labelBindings.reserve(16);
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphNodeCompiler::SStackFrame::SStackFrame(const SGUID& _nodeGUID, size_t _label, size_t _pos)
		: nodeGUID(_nodeGUID)
		, label(_label)
		, pos(_pos)
	{}

	//////////////////////////////////////////////////////////////////////////
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
	CDocGraphNodeCompiler::SMarker::SMarker(const SGUID& _nodeGUID, size_t _label, size_t _opPos, size_t _iPreviewLine)
#else
	CDocGraphNodeCompiler::SMarker::SMarker(const SGUID& _nodeGUID, size_t _label, size_t _opPos)
#endif
		: nodeGUID(_nodeGUID)
		, label(_label)
		, opPos(_opPos)
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		, iPreviewLine(_iPreviewLine)
#endif
	{}

	//////////////////////////////////////////////////////////////////////////
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
	CDocGraphNodeCompiler::SBranch::SBranch(const SGUID& _nodeGUID, size_t _label, size_t _opPos, size_t _iPreviewLine)
#else
	CDocGraphNodeCompiler::SBranch::SBranch(const SGUID& _nodeGUID, size_t _label, size_t _opPos)
#endif
		: nodeGUID(_nodeGUID)
		, label(_label)
		, opPos(_opPos)
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
		, iPreviewLine(_iPreviewLine)
#endif
	{}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::BeginSequence()
	{
		// Add inputs and outputs to stack.
		m_stack.resize(m_libFunction.GetVariantInputs().size() + m_libFunction.GetVariantOutputs().size());
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::EndSequence()
	{
		// End function.
		Return();
		// Resolve branches.
		// #SchematycTODO : Remove branches once they've been resolved?
		for(TMarkerVector::const_iterator iMarker = m_markers.begin(), iEndMarker = m_markers.end(); iMarker != iEndMarker; ++ iMarker)
		{
			const SMarker&	marker = *iMarker;
			for(TBranchVector::const_iterator iBranch = m_branches.begin(), iEndBranch = m_branches.end(); iBranch != iEndBranch; ++ iBranch)
			{
				const	SBranch&	branch = *iBranch;
				if((branch.nodeGUID == marker.nodeGUID) && (branch.label == marker.label))
				{
					SVMOp*	pVMOp = m_libFunction.GetOp(branch.opPos);
					switch(pVMOp->opCode)
					{
					case SVMOp::BRANCH:
						{
							SVMBranchOp*	pVMBranchOp = static_cast<SVMBranchOp*>(pVMOp);
							pVMBranchOp->pos = marker.opPos;
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
							string	previewLine = "BRANCH ";
							char		stringBuffer[256] = "";
							previewLine.append(StringUtils::SizeTToString(marker.iPreviewLine, stringBuffer));
							m_previewLines[branch.iPreviewLine] = previewLine;
#endif
							break;
						}
					case SVMOp::BRANCH_IF_ZERO:
						{
							SVMBranchIfZeroOp*	pVMBranchIfZeroOp = static_cast<SVMBranchIfZeroOp*>(pVMOp);
							pVMBranchIfZeroOp->pos = marker.opPos;
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
							string	previewLine = "BRANCH_IF_ZERO ";
							char		stringBuffer[256] = "";
							previewLine.append(StringUtils::SizeTToString(marker.iPreviewLine, stringBuffer));
							m_previewLines[branch.iPreviewLine] = previewLine;
#endif
							break;
						}
					case SVMOp::BRANCH_IF_NOT_ZERO:
						{
							SVMBranchIfNotZeroOp*	pVMBranchIfNotZeroOp = static_cast<SVMBranchIfNotZeroOp*>(pVMOp);
							pVMBranchIfNotZeroOp->pos = marker.opPos;
#if SCHEMATYC2_COMPILER_PREVIEW_OUTPUT
							string	previewLine = "BRANCH_IF_NOT_ZERO ";
							char		stringBuffer[256] = "";
							previewLine.append(StringUtils::SizeTToString(marker.iPreviewLine, stringBuffer));
							m_previewLines[branch.iPreviewLine] = previewLine;
#endif
							break;
						}
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::VisitSequenceNode(const IScriptGraphNode& scriptGraphNode, EDocGraphSequenceStep sequenceStep, size_t iNodePort)
	{
		scriptGraphNode.Compile(*this, sequenceStep, iNodePort);
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeCompiler::FindOutputOnStack(const SGUID& nodeGUID, size_t iOutput) const
	{
		for(int32 iStackVariable = static_cast<int32>(m_stack.size() - 1); iStackVariable >= 0; -- iStackVariable)
		{
			const SStackVariable& stackVariable = m_stack[iStackVariable];
			for(TOutputBindingVector::const_iterator iOutputBinding = stackVariable.outputBindings.begin(), iEndOutputBinding = stackVariable.outputBindings.end(); iOutputBinding != iEndOutputBinding; ++ iOutputBinding)
			{
				const SOutputBinding&	outputBinding = *iOutputBinding;
				if((outputBinding.nodeGUID == nodeGUID) && (outputBinding.output == iOutput))
				{
					return static_cast<size_t>(iStackVariable);
				}
			}
		}
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeCompiler::FindVariableOnStack(const SGUID& nodeGUID, size_t label) const
	{
		for(size_t iStackVariable = 0, stackVariableCount = m_stack.size(); iStackVariable < stackVariableCount; ++ iStackVariable)
		{
			const SStackVariable& stackVariable = m_stack[iStackVariable];
			for(TLabelBindingVector::const_iterator iLabelBinding = stackVariable.labelBindings.begin(), iEndLabelBinding = stackVariable.labelBindings.end(); iLabelBinding != iEndLabelBinding; ++ iLabelBinding)
			{
				const SLabelBinding&	labelBinding = *iLabelBinding;
				if((labelBinding.nodeGUID == nodeGUID) && (labelBinding.label == label))
				{
					return iStackVariable;
				}
			}
		}
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphNodeCompiler::GetStackFramePos(const SGUID& nodeGUID, size_t label) const
	{
		for(TStackFrameVector::const_iterator iStackFrame = m_stackFrames.begin(), iEndStackFrame = m_stackFrames.end(); iStackFrame != iEndStackFrame; ++ iStackFrame)
		{
			const SStackFrame&	stackFrame = *iStackFrame;
			if((stackFrame.nodeGUID == nodeGUID) && (stackFrame.label == label))
			{
				return stackFrame.pos;
			}
		}
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphNodeCompiler::DestroyStackFrame(const SGUID& nodeGUID, size_t label)
	{
		auto removePredicate = [&nodeGUID, label] (const SStackFrame& rhs)
		{
			return (rhs.nodeGUID == nodeGUID) && (rhs.label == label);
		};
		m_stackFrames.erase(std::remove_if(m_stackFrames.begin(), m_stackFrames.end(), removePredicate), m_stackFrames.end());
	}

	//////////////////////////////////////////////////////////////////////////
	ILibPtr CCompiler::Compile(const IScriptFile& scriptFile)
	{
		LOADING_TIME_PROFILE_SECTION_ARGS(scriptFile.GetFileName());

		const char* szScriptFileName = scriptFile.GetFileName();
		SCHEMATYC2_COMPILER_ASSERT(szScriptFileName);
		if(szScriptFileName)
		{
			SCHEMATYC2_COMPILER_COMMENT("Compiling Script File : %s", szScriptFileName);
			CLibPtr pLib(new CLib(szScriptFileName));
			// Collect and compile signals.
			TScriptSignalConstVector scriptSignals;
			scriptSignals.reserve(100);
			DocUtils::CollectSignals(scriptFile, scriptSignals);
			for(TScriptSignalConstVector::iterator itScriptSignal = scriptSignals.begin(), itEndScriptSignal = scriptSignals.end(); itScriptSignal != itEndScriptSignal; ++ itScriptSignal)
			{
				CompileSignal(scriptFile, *(*itScriptSignal), *pLib);
			}
			// Collect and compile abstract interfaces.
			TScriptAbstractInterfaceConstVector scriptAbstractInterfaces;
			scriptAbstractInterfaces.reserve(100);
			DocUtils::CollectAbstractInterfaces(scriptFile, SGUID(), true, scriptAbstractInterfaces);
			for(TScriptAbstractInterfaceConstVector::iterator itScriptAbstractInterface = scriptAbstractInterfaces.begin(), itEndScriptAbstractInterface = scriptAbstractInterfaces.end(); itScriptAbstractInterface != itEndScriptAbstractInterface; ++ itScriptAbstractInterface)
			{
				CompileAbstractInterface(scriptFile, *(*itScriptAbstractInterface), *pLib);
			}
			// Collect and compile abstract interface functions.
			TScriptAbstractInterfaceFunctionConstVector scriptAbstractInterfaceFunctions;
			scriptAbstractInterfaceFunctions.reserve(100);
			DocUtils::CollectAbstractInterfaceFunctions(scriptFile, SGUID(), true, scriptAbstractInterfaceFunctions);
			for(TScriptAbstractInterfaceFunctionConstVector::iterator itScriptAbstractInterfaceFunction = scriptAbstractInterfaceFunctions.begin(), itEndScriptAbstractInterfaceFunction = scriptAbstractInterfaceFunctions.end(); itScriptAbstractInterfaceFunction != itEndScriptAbstractInterfaceFunction; ++ itScriptAbstractInterfaceFunction)
			{
				CompileAbstractInterfaceFunction(scriptFile, *(*itScriptAbstractInterfaceFunction), *pLib);
			}
			// Collect and compile classes.
			TScriptClassConstVector scriptClasses;
			scriptClasses.reserve(100);
			DocUtils::CollectClasses(scriptFile, scriptClasses);
			TDocGraphSequenceVector	docGraphSequences;
			for(TScriptClassConstVector::iterator itScriptClass = scriptClasses.begin(), itEndScriptClass = scriptClasses.end(); itScriptClass != itEndScriptClass; ++ itScriptClass)
			{
				CompileClass(scriptFile, *(*itScriptClass), *pLib, docGraphSequences);
			}
			// Pre-compile graph sequences.
			for(TDocGraphSequenceVector::iterator iDocGraphSequence = docGraphSequences.begin(), iEndDocGraphSequence = docGraphSequences.end(); iDocGraphSequence != iEndDocGraphSequence; ++ iDocGraphSequence)
			{
				SDocGraphSequence&	docGraphSequence = *iDocGraphSequence;
				docGraphSequence.libFunctionId	= docGraphSequence.pLibClass->AddFunction(docGraphSequence.pDocGraph->GetGUID());
				docGraphSequence.pLibFunction		= docGraphSequence.pLibClass->GetFunction(docGraphSequence.libFunctionId);
				SCHEMATYC2_COMPILER_ASSERT(docGraphSequence.pLibFunction);
				if(docGraphSequence.pLibFunction)
				{
					docGraphSequence.pLibFunction->SetClassGUID(docGraphSequence.pLibClass->GetGUID());
					docGraphSequence.pLibFunction->SetName(docGraphSequence.pDocGraph->GetName());
					docGraphSequence.pLibFunction->SetScope(docGraphSequence.pLibClass->GetName());
					docGraphSequence.pLibFunction->SetFileName(scriptFile.GetFileName());
					CDocGraphSequencePreCompiler sequencePreCompiler(*docGraphSequence.pLibClass, docGraphSequence.libFunctionId, *docGraphSequence.pLibFunction);
					sequencePreCompiler.PrecompileSequence(*docGraphSequence.pScriptGraphNode, docGraphSequence.iDocGraphNodeOutput);
				}
			}
			// Compile graph sequences.
			for(TDocGraphSequenceVector::const_iterator iDocGraphSequence = docGraphSequences.begin(), iEndDocGraphSequence = docGraphSequences.end(); iDocGraphSequence != iEndDocGraphSequence; ++ iDocGraphSequence)
			{
				const SDocGraphSequence&	docGraphSequence = *iDocGraphSequence;
				SCHEMATYC2_COMPILER_ASSERT(docGraphSequence.pLibFunction);
				if(docGraphSequence.pLibFunction)
				{
					CDocGraphNodeCompiler	nodeCompiler(scriptFile, *docGraphSequence.pDocGraph, *pLib, *docGraphSequence.pLibClass, *docGraphSequence.pLibFunction);
					nodeCompiler.CompileSequence(*docGraphSequence.pScriptGraphNode, docGraphSequence.iDocGraphNodeOutput);
				}
			}
			// Link graph sequences.
			for(TDocGraphSequenceVector::const_iterator iDocGraphSequence = docGraphSequences.begin(), iEndDocGraphSequence = docGraphSequences.end(); iDocGraphSequence != iEndDocGraphSequence; ++ iDocGraphSequence)
			{
				const SDocGraphSequence&	docGraphSequence = *iDocGraphSequence;
				CDocGraphSequenceLinker		sequenceLinker(*docGraphSequence.pLibClass, docGraphSequence.pLibClass->GetStateMachine(docGraphSequence.iLibStateMachine), docGraphSequence.pLibClass->GetState(docGraphSequence.iLibState));
				sequenceLinker.LinkSequence(*docGraphSequence.pScriptGraphNode, docGraphSequence.iDocGraphNodeOutput, docGraphSequence.libFunctionId);
			}
			// Link classes.
			for(TScriptClassConstVector::iterator itScriptClass = scriptClasses.begin(), itEndScriptClass = scriptClasses.end(); itScriptClass != itEndScriptClass; ++ itScriptClass)
			{
				const IScriptClass&	scriptClass = *(*itScriptClass);
				LinkClass(scriptFile, scriptClass, *pLib, *pLib->GetClass(scriptClass.GetGUID()));
			}
			return pLib;
		}
		return ILibPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CCompiler::Compile(const IScriptElement& scriptElement) {}

	//////////////////////////////////////////////////////////////////////////
	void CCompiler::CompileAll()
	{
		LOADING_TIME_PROFILE_SECTION;

		gEnv->pSchematyc2->GetScriptRegistry().VisitFiles(ScriptFileVisitor::FromMemberFunction<CCompiler, &CCompiler::VisitAndCompileScriptFile>(*this));
	}

	//////////////////////////////////////////////////////////////////////////
	CCompiler::SDocGraphSequence::SDocGraphSequence(CLibClass* _pLibClass, size_t _iLibStateMachine, size_t _iLibState, const IDocGraph* _pDocGraph, const IScriptGraphNode* _pScriptGraphNode, size_t	_iDocGraphNodeOutput)
		: pLibClass(_pLibClass)
		, iLibStateMachine(_iLibStateMachine)
		, iLibState(_iLibState)
		, pDocGraph(_pDocGraph)
		, pScriptGraphNode(_pScriptGraphNode)
		, iDocGraphNodeOutput(_iDocGraphNodeOutput)
		, pLibFunction(NULL)
	{}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CCompiler::VisitAndCompileScriptFile(IScriptFile& scriptFile)
	{
		if(ILibPtr pLib = Compile(scriptFile))
		{
			gEnv->pSchematyc2->GetLibRegistry().RegisterLib(pLib);
		}
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	void CCompiler::CompileSignal(const IScriptFile& scriptFile, const IScriptSignal& scriptSignal, CLib& lib)
	{
		// Add signal to library.
		CSignalPtr	pSignal = lib.AddSignal(scriptSignal.GetGUID(), SGUID(), scriptSignal.GetName());
		SCHEMATYC2_COMPILER_ASSERT(pSignal);
		if(pSignal)
		{
			for(size_t iInput = 0, inputCount = scriptSignal.GetInputCount(); iInput < inputCount; ++ iInput)
			{
				IAnyConstPtr	pInputValue = scriptSignal.GetInputValue(iInput);
				SCHEMATYC2_COMPILER_ASSERT(pInputValue);
				if(pInputValue)
				{
					pSignal->AddInput(scriptSignal.GetInputName(iInput), "", *pInputValue);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CCompiler::CompileAbstractInterface(const IScriptFile& scriptFile, const IScriptAbstractInterface& scriptAbstractInterface, CLib& lib)
	{
		// Add abstract interface to library.
		lib.AddAbstractInterface(scriptAbstractInterface.GetGUID(), scriptAbstractInterface.GetName());
	}

	//////////////////////////////////////////////////////////////////////////
	void CCompiler::CompileAbstractInterfaceFunction(const IScriptFile& scriptFile, const IScriptAbstractInterfaceFunction& scriptAbstractInterfaceFunction, CLib& lib)
	{
		// Add abstract interface function to library.
		CLibAbstractInterfaceFunctionPtr	pAbstracInterfaceFunction = lib.AddAbstractInterfaceFunction(scriptAbstractInterfaceFunction.GetGUID(), scriptAbstractInterfaceFunction.GetName());
		SCHEMATYC2_COMPILER_ASSERT(pAbstracInterfaceFunction);
		if(pAbstracInterfaceFunction)
		{
			for(size_t iInput = 0, inputCount = scriptAbstractInterfaceFunction.GetInputCount(); iInput < inputCount; ++ iInput)
			{
				IAnyConstPtr	pInputValue = scriptAbstractInterfaceFunction.GetInputValue(iInput);
				SCHEMATYC2_COMPILER_ASSERT(pInputValue);
				if(pInputValue)
				{
					pAbstracInterfaceFunction->AddInput(*pInputValue);
				}
			}
			for(size_t iOutput = 0, outputCount = scriptAbstractInterfaceFunction.GetOutputCount(); iOutput < outputCount; ++ iOutput)
			{
				IAnyConstPtr	pOutputValue = scriptAbstractInterfaceFunction.GetOutputValue(iOutput);
				SCHEMATYC2_COMPILER_ASSERT(pOutputValue);
				if(pOutputValue)
				{
					pAbstracInterfaceFunction->AddOutput(*pOutputValue);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CCompiler::CompileClass(const IScriptFile& scriptFile, const IScriptClass& scriptClass, CLib& lib, TDocGraphSequenceVector& docGraphSequences)
	{
		const SGUID classGUID = scriptClass.GetGUID();
		SGUID       foundationGUID = scriptClass.GetFoundationGUID();
		// Get foundation and base class.
		IFoundationConstPtr                         pFoundation;
		IPropertiesConstPtr                         pFoundationProperties;
		ScriptIncludeRecursionUtils::GetClassResult scriptBaseClass;
		if(!foundationGUID)
		{
			SGUID baseClassGUID;
			auto visitScriptClassBase = [&baseClassGUID] (const IScriptClassBase& scriptClassBase) -> EVisitStatus
			{
				baseClassGUID = scriptClassBase.GetRefGUID();
				return EVisitStatus::Stop;
			};
			scriptFile.VisitClassBases(ScriptClassBaseConstVisitor::FromLambdaFunction(visitScriptClassBase), classGUID, false);

			scriptBaseClass = ScriptIncludeRecursionUtils::GetClass(scriptFile, baseClassGUID);
			if(scriptBaseClass.second)
			{
				foundationGUID        = scriptBaseClass.second->GetFoundationGUID();
				pFoundationProperties = scriptBaseClass.second->GetFoundationProperties();
			}
			else
			{
				SCHEMATYC2_COMPILER_WARNING("Unable to retrieve base class!");
			}
		}
		else
		{
			pFoundationProperties = scriptClass.GetFoundationProperties();
		}
		pFoundation = gEnv->pSchematyc2->GetEnvRegistry().GetFoundation(foundationGUID);
		if(pFoundation)
		{
			// Get full class name.
			stack_string className;
			DocUtils::GetFullElementName(scriptFile, classGUID, className);
			// Add class to library.
			CLibClassPtr pLibClass = lib.AddClass(classGUID, className.c_str(), foundationGUID, pFoundationProperties);
			SCHEMATYC2_COMPILER_ASSERT(pLibClass);
			if(pLibClass)
			{
				if(scriptBaseClass.second)
				{
					CompileClass(*scriptBaseClass.first, *scriptBaseClass.second, lib, *pLibClass, docGraphSequences);
				}
				CompileClass(scriptFile, scriptClass, lib, *pLibClass, docGraphSequences);
			}
		}
		else
		{
			SCHEMATYC2_COMPILER_WARNING("Unable to retrieve foundation!");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CCompiler::CompileClass(const IScriptFile& scriptFile, const IScriptClass& scriptClass, CLib& lib, CLibClass& libClass, TDocGraphSequenceVector& docGraphSequences)
	{
		LOADING_TIME_PROFILE_SECTION_ARGS(scriptClass.GetName());

		struct SPendingGraph // #SchematycTODO : Move this outside of the function!!!
		{
			inline SPendingGraph(const IScriptGraphExtension* _pScriptGraph, uint32 _functionIdx)
				: pScriptGraph(_pScriptGraph)
				, functionIdx(_functionIdx)
			{}

			const IScriptGraphExtension* pScriptGraph;
			uint32                       functionIdx;
		};

		typedef std::vector<SPendingGraph> PendingGraphs;

		PendingGraphs pendingGraphs;
		pendingGraphs.reserve(100);

		const SGUID classGUID = scriptClass.GetGUID();
		// Collect states.
		TScriptStateConstVector scriptStates;
		scriptStates.reserve(100);
		DocUtils::CollectStates(scriptFile, classGUID, true, scriptStates);
		for(TScriptStateConstVector::iterator itScriptState = scriptStates.begin(), itEndScriptState = scriptStates.end(); itScriptState != itEndScriptState; ++ itScriptState)
		{
			const IScriptState& scriptState = *(*itScriptState);
			// Add state to library class.
			libClass.AddState(scriptState.GetGUID(), scriptState.GetName());
		}
		for(TScriptStateConstVector::iterator itScriptState = scriptStates.begin(), itEndScriptState = scriptStates.end(); itScriptState != itEndScriptState; ++ itScriptState)
		{
			const IScriptState& scriptState = *(*itScriptState);
			// Check for parent state.
			const IScriptState* pScriptParentState = DocUtils::FindOwnerState(scriptFile, scriptState.GetScopeGUID());
			if(pScriptParentState)
			{
				const size_t iState = LibUtils::FindStateByGUID(libClass, scriptState.GetGUID());
				const size_t iParentState = LibUtils::FindStateByGUID(libClass, pScriptParentState->GetGUID());
				libClass.GetState(iState)->SetParent(iParentState);
			}
			// Check for partner state.
			const IScriptState* pScriptPartnerState = DocUtils::FindOwnerState(scriptFile, scriptState.GetPartnerGUID());
			if(pScriptPartnerState)
			{
				const size_t iState = LibUtils::FindStateByGUID(libClass, scriptState.GetGUID());
				const size_t iPartnerState = LibUtils::FindStateByGUID(libClass, pScriptPartnerState->GetGUID());
				libClass.GetState(iState)->SetPartner(iPartnerState);
			}
		}
		// Collect state machines.
		TScriptStateMachineConstVector scriptStateMachines;
		scriptStateMachines.reserve(100);
		DocUtils::CollectStateMachines(scriptFile, classGUID, true, scriptStateMachines);
		for(TScriptStateMachineConstVector::iterator itScriptStateMachine = scriptStateMachines.begin(), itEndScriptStateMachine = scriptStateMachines.end(); itScriptStateMachine != itEndScriptStateMachine; ++ itScriptStateMachine)
		{
			const IScriptStateMachine& scriptStateMachine = *(*itScriptStateMachine);
			// Convert state machine's lifetime parameter.
			ELibStateMachineLifetime lifetime = ELibStateMachineLifetime::Unknown;
			switch(scriptStateMachine.GetLifetime())
			{
			case EScriptStateMachineLifetime::Persistent:
				{
					lifetime = ELibStateMachineLifetime::Persistent;
					break;
				}
			case EScriptStateMachineLifetime::Task:
				{
					lifetime = ELibStateMachineLifetime::Task;
					break;
				}
			}
			// Add state machine to library class.
			libClass.AddStateMachine(scriptStateMachine.GetGUID(), scriptStateMachine.GetName(), lifetime);
		}
		for(TScriptStateMachineConstVector::iterator itScriptStateMachine = scriptStateMachines.begin(), itEndScriptStateMachine = scriptStateMachines.end(); itScriptStateMachine != itEndScriptStateMachine; ++ itScriptStateMachine)
		{
			const IScriptStateMachine& scriptStateMachine = *(*itScriptStateMachine);
			// Check for partner state machine.
			const IScriptStateMachine* pScriptPartnerStateMachine = DocUtils::FindOwnerStateMachine(scriptFile, scriptStateMachine.GetPartnerGUID());
			if(pScriptPartnerStateMachine)
			{
				const size_t iStateMachine = LibUtils::FindStateMachineByGUID(libClass, scriptStateMachine.GetGUID());
				const size_t iPartnerStateMachine = LibUtils::FindStateMachineByGUID(libClass, pScriptPartnerStateMachine->GetGUID());
				libClass.GetStateMachine(iStateMachine)->SetPartner(iPartnerStateMachine);
				libClass.GetStateMachine(iPartnerStateMachine)->AddListener(iStateMachine);
			}
		}
		for (TScriptStateConstVector::iterator itScriptState = scriptStates.begin(), itEndScriptState = scriptStates.end(); itScriptState != itEndScriptState; ++itScriptState)
		{
			const IScriptState& scriptState = *(*itScriptState);
			// Check for owner state machine.
			const IScriptStateMachine* pScriptOwnerStateMachine = DocUtils::FindOwnerStateMachine(scriptFile, scriptState.GetScopeGUID());
			if (pScriptOwnerStateMachine)
			{
				const size_t iState = LibUtils::FindStateByGUID(libClass, scriptState.GetGUID());
				const size_t iStateMachine = LibUtils::FindStateMachineByGUID(libClass, pScriptOwnerStateMachine->GetGUID());
				libClass.GetState(iState)->SetStateMachine(iStateMachine);
			}
		}

		// Collect and compile variables.
		TVariantVector&             variants = libClass.GetVariants();
		CVariantVectorOutputArchive archive(variants);
		TScriptVariableConstVector  scriptVariables;
		scriptVariables.reserve(100);
		DocUtils::CollectVariables(scriptFile, classGUID, true, scriptVariables);
		for(TScriptVariableConstVector::iterator itScriptVariable = scriptVariables.begin(), itEndScriptVariable = scriptVariables.end(); itScriptVariable != itEndScriptVariable; ++ itScriptVariable)
		{
			const IScriptVariable& scriptVariable = *(*itScriptVariable);
			const SGUID&           variableGUID = scriptVariable.GetGUID();
			IAnyConstPtr           pVariableValue = scriptVariable.GetValue();
			SCHEMATYC2_COMPILER_ASSERT(pVariableValue);
			if(pVariableValue)
			{
				// Format variable name.
				stack_string	variableName;
				DocUtils::GetFullElementName(scriptFile, scriptVariable, variableName, EScriptElementType::Class);
				// Serialize variants.
				const size_t	variantPos = variants.size();
				archive(*pVariableValue, "", "");
				// Add variable to library class.
				const size_t	iLibVariable = libClass.AddVariable(variableGUID, variableName.c_str(), EOverridePolicy::Default, *pVariableValue, variantPos, variants.size() - variantPos, ELibVariableFlags::None);
				// Check to see if variable belongs to a state machine or state.
				const IScriptStateMachine* pScriptStateMachine = DocUtils::FindOwnerStateMachine(scriptFile, variableGUID);
				if(pScriptStateMachine)
				{
					// Add variable to library state machine.
					const size_t	iLibStateMachine = LibUtils::FindStateMachineByGUID(libClass, pScriptStateMachine->GetGUID());
					SCHEMATYC2_COMPILER_ASSERT(iLibStateMachine != INVALID_INDEX);
					if(iLibStateMachine != INVALID_INDEX)
					{
						libClass.GetStateMachine(iLibStateMachine)->AddVariable(iLibVariable);
					}
				}
				else
				{
					const IScriptState* pScriptState = DocUtils::FindOwnerState(scriptFile, variableGUID);
					if(pScriptState)
					{
						// Add variable to library state.
						const size_t	iLibState = LibUtils::FindStateByGUID(libClass, pScriptState->GetGUID());
						SCHEMATYC2_COMPILER_ASSERT(iLibState != INVALID_INDEX);
						if(iLibState != INVALID_INDEX)
						{
							libClass.GetState(iLibState)->AddVariable(iLibVariable);
						}
					}
				}
			}
		}
		// Collect and compile properties.
		CompileClassProperties(scriptFile, scriptClass, lib, libClass, docGraphSequences);
		// Collect and compile containers.
		TScriptContainerConstVector scriptContainers;
		scriptContainers.reserve(100);
		DocUtils::CollectContainers(scriptFile, classGUID, true, scriptContainers);
		for(TScriptContainerConstVector::iterator itScriptContainer = scriptContainers.begin(), itEndScriptContainer = scriptContainers.end(); itScriptContainer != itEndScriptContainer; ++ itScriptContainer)
		{
			const IScriptContainer& scriptContainer = *(*itScriptContainer);
			const SGUID&            containerGUID = scriptContainer.GetGUID();
			// Format container name.
			stack_string	containerName;
			DocUtils::GetFullElementName(scriptFile, scriptContainer, containerName, EScriptElementType::Class);
			// Add container to library class.
			const size_t	iLibContainer = libClass.AddContainer(containerGUID, containerName.c_str(), scriptContainer.GetTypeGUID());
			// Check to see if container belongs to a state machine or state.
			const IScriptStateMachine* pScriptStateMachine = DocUtils::FindOwnerStateMachine(scriptFile, containerGUID);
			if(pScriptStateMachine)
			{
				// Add container to library state machine.
				const size_t	iLibStateMachine = LibUtils::FindStateMachineByGUID(libClass, pScriptStateMachine->GetGUID());
				SCHEMATYC2_COMPILER_ASSERT(iLibStateMachine != INVALID_INDEX);
				if(iLibStateMachine != INVALID_INDEX)
				{
					libClass.GetStateMachine(iLibStateMachine)->AddVariable(iLibContainer);
				}
			}
			else
			{
				const IScriptState* pScriptState = DocUtils::FindOwnerState(scriptFile, containerGUID);
				if(pScriptState)
				{
					// Add container to library state.
					const size_t	iLibState = LibUtils::FindStateByGUID(libClass, pScriptState->GetGUID());
					SCHEMATYC2_COMPILER_ASSERT(iLibState != INVALID_INDEX);
					if(iLibState != INVALID_INDEX)
					{
						libClass.GetState(iLibState)->AddVariable(iLibContainer);
					}
				}
			}
		}
		// Collect timers.
		TScriptTimerConstVector scriptTimers;
		scriptTimers.reserve(100);
		DocUtils::CollectTimers(scriptFile, classGUID, true, scriptTimers);
		for(TScriptTimerConstVector::iterator itScriptTimer = scriptTimers.begin(), itEndScriptTimer = scriptTimers.end(); itScriptTimer != itEndScriptTimer; ++ itScriptTimer)
		{
			const IScriptTimer& scriptTimer = *(*itScriptTimer);
			const SGUID&        timerGUID = scriptTimer.GetGUID();
			// Check to see if timer is persistent or belongs to a state.
			const IScriptState* pScriptOwnerState = DocUtils::FindOwnerState(scriptFile, timerGUID);
			if(pScriptOwnerState)
			{
				// Add timer to library class.
				const size_t	iLibTimer = libClass.AddTimer(timerGUID, scriptTimer.GetName(), scriptTimer.GetParams());
				// Add timer to library state.
				const size_t	iLibOwnerState = LibUtils::FindStateByGUID(libClass, pScriptOwnerState->GetGUID());
				SCHEMATYC2_COMPILER_ASSERT(iLibOwnerState != INVALID_INDEX);
				if(iLibOwnerState != INVALID_INDEX)
				{
					libClass.GetState(iLibOwnerState)->AddTimer(iLibTimer);
				}
			}
			else
			{
				// Add persistent timer to library class.
				libClass.AddPersistentTimer(timerGUID, scriptTimer.GetName(), scriptTimer.GetParams());
			}
		}
		// Collect abstract interface implementations.
		TScriptAbstractInterfaceImplementationConstVector scriptAbstractInterfaceImplementations;
		scriptAbstractInterfaceImplementations.reserve(100);
		DocUtils::CollectAbstractInterfaceImplementations(scriptFile, classGUID, true, scriptAbstractInterfaceImplementations);
		for(TScriptAbstractInterfaceImplementationConstVector::iterator itScriptAbstractInterfaceImplementation = scriptAbstractInterfaceImplementations.begin(), itEndScriptAbstractInterfaceImplementation = scriptAbstractInterfaceImplementations.end(); itScriptAbstractInterfaceImplementation != itEndScriptAbstractInterfaceImplementation; ++ itScriptAbstractInterfaceImplementation)
		{
			const IScriptAbstractInterfaceImplementation& scriptAbstractInterfaceImplementation = *(*itScriptAbstractInterfaceImplementation);
			// Add instance of abstract interface to library class.
			libClass.AddAbstractInterfaceImplementation(scriptAbstractInterfaceImplementation.GetGUID(), scriptAbstractInterfaceImplementation.GetName(), scriptAbstractInterfaceImplementation.GetRefGUID());
		}
		// Collect component instances.
		TScriptComponentInstanceConstVector scriptComponentInstances;
		scriptComponentInstances.reserve(100);
		DocUtils::CollectComponentInstances(scriptFile, classGUID, true, scriptComponentInstances);
		for(TScriptComponentInstanceConstVector::const_iterator itScriptComponentInstance = scriptComponentInstances.begin(), itEndScriptComponentInstance = scriptComponentInstances.end(); itScriptComponentInstance != itEndScriptComponentInstance; ++ itScriptComponentInstance)
		{
			const IScriptComponentInstance& scriptComponentInstance = *(*itScriptComponentInstance);
			IPropertiesConstPtr             pComponentProperties = scriptComponentInstance.GetProperties();
			if(pComponentProperties)
			{
				const SGUID componentInstanceGUID = scriptComponentInstance.GetGUID();
				uint32      componentPropertyFunctionIdx = s_invalidIdx;
				// Add component property graph to pending graphs.
				const IScriptGraphExtension* pComponentPropertyGraph = static_cast<const IScriptGraphExtension*>(scriptComponentInstance.GetExtensions().QueryExtension(EScriptExtensionId::Graph));
				if(pComponentPropertyGraph)
				{
					componentPropertyFunctionIdx = libClass.AddFunction_New(componentInstanceGUID);
					pendingGraphs.push_back(SPendingGraph(pComponentPropertyGraph, componentPropertyFunctionIdx));
				}
				// Look for parent component instance.
				// N.B. We can safely assume this exists already because of the order in which we visit script component instances. If this order ever changes we will need to map parent indices in a separate pass.
				const uint32 parentIdx = LibUtils::FindComponentInstanceByGUID(libClass, scriptComponentInstance.GetScopeGUID());
				// Add instance of component to library class.
				libClass.AddComponentInstance(componentInstanceGUID, scriptComponentInstance.GetName(), scriptComponentInstance.GetComponentGUID(), pComponentProperties->Clone(), componentPropertyFunctionIdx, parentIdx);
			}
			else
			{
				// #SchematycTODO : Return error?
			}
		}
		libClass.SortComponentInstances(); // #SchematycTODO : Make sure this doesn't screw up order of parents vs children!!!
		libClass.ValidateSingletonComponentInstances();
		// Collect action instances.
		TScriptActionInstanceConstVector scriptActionInstances;
		scriptActionInstances.reserve(100);
		DocUtils::CollectActionInstances(scriptFile, classGUID, true, scriptActionInstances);
		for(TScriptActionInstanceConstVector::const_iterator itScriptActionInstance = scriptActionInstances.begin(), itEndScriptActionInstance = scriptActionInstances.end(); itScriptActionInstance != itEndScriptActionInstance; ++ itScriptActionInstance)
		{
			const IScriptActionInstance& scriptActionInstance = *(*itScriptActionInstance);
			const SGUID                  actionInstanceGUID = scriptActionInstance.GetGUID();
			const size_t                 iComponentInstance = LibUtils::FindComponentInstanceByGUID(libClass, scriptActionInstance.GetComponentInstanceGUID());
			// Check to see if action instance is persistent or belongs to a state/action.
			const IScriptState* pScriptOwnerState = DocUtils::FindOwnerState(scriptFile, actionInstanceGUID);
			if(pScriptOwnerState)
			{
				// Add action instance to library class.
				IPropertiesConstPtr pScriptActionInstanceProperties = scriptActionInstance.GetProperties();
				const size_t        iLibActionInstance = libClass.AddActionInstance(actionInstanceGUID, scriptActionInstance.GetName(), scriptActionInstance.GetActionGUID(), iComponentInstance, pScriptActionInstanceProperties ? pScriptActionInstanceProperties->Clone() : IPropertiesPtr());
				// Add action instance to library state.
				const size_t	iLibOwnerState = LibUtils::FindStateByGUID(libClass, pScriptOwnerState->GetGUID());
				SCHEMATYC2_COMPILER_ASSERT(iLibOwnerState != INVALID_INDEX);
				if(iLibOwnerState != INVALID_INDEX)
				{
					libClass.GetState(iLibOwnerState)->AddActionInstance(iLibActionInstance);
				}
			}
			else
			{
				// Add persistent action instance to library class.
				libClass.AddPersistentActionInstance(actionInstanceGUID, scriptActionInstance.GetName(), scriptActionInstance.GetActionGUID(), iComponentInstance, scriptActionInstance.GetProperties()->Clone());
			}
		}
		// Collect graph sequences for compilation later on.
		TDocGraphConstVector docGraphs;
		docGraphs.reserve(100);
		DocUtils::CollectGraphs(scriptFile, classGUID, true, docGraphs);
		for(TDocGraphConstVector::const_iterator iDocGraph = docGraphs.begin(), iEndGraph = docGraphs.end(); iDocGraph != iEndGraph; ++ iDocGraph)
		{
			const IDocGraph& docGraph = *(*iDocGraph);
			// Find owner state machine.
			size_t                     iStateMachine = INVALID_INDEX;
			const IScriptStateMachine* pScriptStateMachine = DocUtils::FindOwnerStateMachine(scriptFile, docGraph.GetGUID());
			if(pScriptStateMachine)
			{
				iStateMachine = LibUtils::FindStateMachineByGUID(libClass, pScriptStateMachine->GetGUID());
			}
			// Find owner state.
			size_t              iState = INVALID_INDEX;
			const IScriptState* pScriptState = DocUtils::FindOwnerState(scriptFile, docGraph.GetGUID());
			if(pScriptState)
			{
				iState = LibUtils::FindStateByGUID(libClass, pScriptState->GetGUID());
			}
			// Add graph nodes to sequence.
			TScriptGraphNodeConstVector scriptGraphNodes;
			scriptGraphNodes.reserve(100);
			DocUtils::CollectGraphNodes(docGraph, scriptGraphNodes);
			for(TScriptGraphNodeConstVector::const_iterator iScriptGraphNode = scriptGraphNodes.begin(), iEndScriptGraphNode = scriptGraphNodes.end(); iScriptGraphNode != iEndScriptGraphNode; ++ iScriptGraphNode)
			{
				const IScriptGraphNode& scriptGraphNode = *(*iScriptGraphNode);
				for(size_t iDocGraphNodeOutput = 0, scriptGraphNodeOutputCount = scriptGraphNode.GetOutputCount(); iDocGraphNodeOutput < scriptGraphNodeOutputCount; ++ iDocGraphNodeOutput)
				{
					if((scriptGraphNode.GetOutputFlags(iDocGraphNodeOutput) & EScriptGraphPortFlags::BeginSequence) != 0)
					{
						docGraphSequences.push_back(SDocGraphSequence(&libClass, iStateMachine, iState, &docGraph, &scriptGraphNode, iDocGraphNodeOutput));
					}
				}
			}
		}
		// Compile graphs
		for(const SPendingGraph& pendingGraph : pendingGraphs)
		{
			CScriptGraphCompiler graphCompiler;
			graphCompiler.CompileGraph(libClass, *pendingGraph.pScriptGraph, *libClass.GetFunction_New(pendingGraph.functionIdx));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CCompiler::CompileClassProperties(const IScriptFile& scriptFile, const IScriptClass& scriptClass, CLib& lib, CLibClass& libClass, TDocGraphSequenceVector& docGraphSequences)
	{
		TScriptPropertyConstVector scriptProperties;
		scriptProperties.reserve(100);
		DocUtils::CollectProperties(scriptFile, scriptClass.GetGUID(), true, scriptProperties);
		DocUtils::SortProperties(scriptProperties); // Properties must be sorted to preserve order.

		TVariantVector&             variants = libClass.GetVariants();
		CVariantVectorOutputArchive archive(variants);

		for(const IScriptProperty* pScriptProperty : scriptProperties)
		{
			IAnyConstPtr pPropertyValue = pScriptProperty->GetValue();
			SCHEMATYC2_COMPILER_ASSERT(pPropertyValue);
			if(pPropertyValue)
			{
				// Check for base property.
				const SGUID& propertyRefGUID = pScriptProperty->GetRefGUID();
				if(propertyRefGUID)
				{
					const size_t variableIdx = LibUtils::FindVariableByGUID(libClass, propertyRefGUID);
					SCHEMATYC2_COMPILER_ASSERT(variableIdx != INVALID_INDEX);
					if(variableIdx != INVALID_INDEX)
					{
						CLibVariable* pLibVariable = libClass.GetVariable(variableIdx);
						// Check override policy.
						const EOverridePolicy overridePolicy = pScriptProperty->GetOverridePolicy();
						if(overridePolicy != EOverridePolicy::Default)
						{
							pLibVariable->SetOverridePolicy(overridePolicy);
							// Serialize variants.
							const size_t   variantCount = pLibVariable->GetVariantCount();
							TVariantVector overrideVariants;
							overrideVariants.reserve(variantCount);
							CVariantVectorOutputArchive overrideArchive(overrideVariants);
							overrideArchive(*pPropertyValue, "", "");
							// Override base variants.
							SCHEMATYC2_COMPILER_ASSERT(overrideVariants.size() == variantCount);
							if(overrideVariants.size() == variantCount)
							{
								std::copy(overrideVariants.begin(), overrideVariants.end(), variants.begin() + pLibVariable->GetVariantPos());
							}
						}
					}
				}
				else
				{
					// Format variable name.
					stack_string variableName;
					DocUtils::GetFullElementName(scriptFile, *pScriptProperty, variableName, EScriptElementType::Class);
					// Serialize variants.
					const size_t variantPos = variants.size();
					archive(*pPropertyValue, "", "");
					// Add variable to library class.
					const size_t      libVariableIdx = libClass.GetVariableCount();
					ELibVariableFlags libVariableFlags = ELibVariableFlags::ClassProperty;
					// Check to see if variable belongs to a state machine or state.
					const SGUID&               propertyGUID = pScriptProperty->GetGUID();
					const IScriptStateMachine* pScriptStateMachine = DocUtils::FindOwnerStateMachine(scriptFile, propertyGUID);
					if(pScriptStateMachine)
					{
						// Add variable to library state machine.
						const size_t libStateMachineIdx = LibUtils::FindStateMachineByGUID(libClass, pScriptStateMachine->GetGUID());
						SCHEMATYC2_COMPILER_ASSERT(libStateMachineIdx != INVALID_INDEX);
						if(libStateMachineIdx != INVALID_INDEX)
						{
							libClass.GetStateMachine(libStateMachineIdx)->AddVariable(libVariableIdx);
							libVariableFlags = ELibVariableFlags::StateMachineProperty;
						}
					}
					else
					{
						const IScriptState* pScriptState = DocUtils::FindOwnerState(scriptFile, propertyGUID);
						if(pScriptState)
						{
							// Add variable to library state.
							const size_t libStateIdx = LibUtils::FindStateByGUID(libClass, pScriptState->GetGUID());
							SCHEMATYC2_COMPILER_ASSERT(libStateIdx != INVALID_INDEX);
							if(libStateIdx != INVALID_INDEX)
							{
								libClass.GetState(libStateIdx)->AddVariable(libVariableIdx);
								libVariableFlags = ELibVariableFlags::StateProperty;
							}
						}
					}
					// Add variable to library class.
					libClass.AddVariable(propertyGUID, variableName.c_str(), pScriptProperty->GetOverridePolicy(), *pPropertyValue, variantPos, variants.size() - variantPos, libVariableFlags);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CCompiler::LinkClass(const IScriptFile& scriptFile, const IScriptClass& scriptClass, CLib& lib, CLibClass& libClass)
	{
		// Link class's abstract interface implementations.
		TScriptAbstractInterfaceImplementationConstVector scriptAbstractInterfaceImplementations;
		scriptAbstractInterfaceImplementations.reserve(100);
		DocUtils::CollectAbstractInterfaceImplementations(scriptFile, scriptClass.GetGUID(), false, scriptAbstractInterfaceImplementations);
		for(TScriptAbstractInterfaceImplementationConstVector::iterator itScriptAbstractInterfaceImplementation = scriptAbstractInterfaceImplementations.begin(), itEndScriptAbstractInterfaceImplementation = scriptAbstractInterfaceImplementations.end(); itScriptAbstractInterfaceImplementation != itEndScriptAbstractInterfaceImplementation; ++ itScriptAbstractInterfaceImplementation)
		{
			const IScriptAbstractInterfaceImplementation& scriptAbstractInterfaceImplementation = *(*itScriptAbstractInterfaceImplementation);
			const SGUID                                   abstractInterfaceImplementationGUID = scriptAbstractInterfaceImplementation.GetGUID();
			CLibAbstractInterfaceImplementation*          pLibAbstractInterfaceImplementation = libClass.GetAbstractInterfaceImplementation(LibUtils::FindAbstractInterfaceImplementation(libClass, scriptAbstractInterfaceImplementation.GetRefGUID()));
			SCHEMATYC2_COMPILER_ASSERT(pLibAbstractInterfaceImplementation);
			if(pLibAbstractInterfaceImplementation)
			{
				TDocGraphConstVector docGraphs;
				docGraphs.reserve(100);
				DocUtils::CollectGraphs(scriptFile, abstractInterfaceImplementationGUID, false, docGraphs);
				for(TDocGraphConstVector::const_iterator iDocGraph = docGraphs.begin(), iEndDocGraph = docGraphs.end(); iDocGraph != iEndDocGraph; ++ iDocGraph)
				{
					const IDocGraph& docGraph = *(*iDocGraph);
					LibFunctionId    libFunctionId = libClass.GetFunctionId(docGraph.GetGUID());
					SCHEMATYC2_COMPILER_ASSERT(libFunctionId != LibFunctionId::s_invalid);
					if(libFunctionId != LibFunctionId::s_invalid)
					{
						pLibAbstractInterfaceImplementation->AddFunction(docGraph.GetContextGUID(), libFunctionId);	// #SchematycTODO : Verify that the function's signature is correct!!!
					}
				}
			}
		}
	}
}

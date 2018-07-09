// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DocGraphBase.h"

#include <CrySerialization/BlackBox.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CrySchematyc2/GUIDRemapper.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>
#include <CrySchematyc2/Services/ILog.h>

namespace Schematyc2
{
	namespace DocGraphBaseUtils
	{
		struct SNodeHeader
		{
			inline SNodeHeader()
				: type(EScriptGraphNodeType::Unknown)
				, pos(ZERO)
			{}

			inline SNodeHeader(const SGUID& _guid, EScriptGraphNodeType _type, const SGUID& _contextGUID, const SGUID& _refGUID, const Vec2& _pos)
				: guid(_guid)
				, type(_type)
				, contextGUID(_contextGUID)
				, refGUID(_refGUID)
				, pos(_pos)
			{}

			void Serialize(Serialization::IArchive& archive)
			{
				archive(guid, "guid");
				archive(type, "type");
				archive(contextGUID, "context_guid");
				archive(refGUID, "ref_guid");
				archive(pos, "pos");
			}

			SGUID                guid;
			EScriptGraphNodeType type;
			SGUID                contextGUID;
			SGUID                refGUID;
			Vec2                 pos;
		};

		struct SNodeMapValueSerializer
		{
			inline SNodeMapValueSerializer(std::pair<SGUID, SDocGraphNode>& _value)
				: value(_value)
			{}

			void Serialize(Serialization::IArchive& archive)
			{
				CDocGraphBase* pDocGraph = archive.context<CDocGraphBase>();
				CRY_ASSERT(pDocGraph);
				if(pDocGraph)
				{
					pDocGraph->SerializeNodeMapValue(archive, value);
				}
			}

			std::pair<SGUID, SDocGraphNode>& value;
		};
	}
}

namespace std	// In order to resolve ADL, std type serialization overrides must be placed in std namespace.
{
	inline bool Serialize(Serialization::IArchive& archive, pair<Schematyc2::SGUID, Schematyc2::SDocGraphNode>& value, const char* szName, const char* szLabel)
	{
		archive(Schematyc2::DocGraphBaseUtils::SNodeMapValueSerializer(value), szName, szLabel);
		return true;
	}

	/*inline bool Serialize(Serialization::IArchive& archive, shared_ptr<Schematyc2::CScriptGraphLink>& value, const char* szName, const char* szLabel)
	{
		if(!value)
		{
			value = std::make_shared<Schematyc2::CScriptGraphLink>();
		}
		archive(*value, szName, szLabel);
		return true;
	}*/
}

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	SDocGraphNode::SDocGraphNode() {}

	//////////////////////////////////////////////////////////////////////////
	SDocGraphNode::SDocGraphNode(const IScriptGraphNodePtr& _pNode)
		: pNode(_pNode)
	{}

	//////////////////////////////////////////////////////////////////////////
	CScriptGraphLink::CScriptGraphLink(const SGUID& srcNodeGUID, const char* szSrcOutputName, const SGUID& dstNodeGUID, const char* szDstInputName)
		: m_srcNodeGUID(srcNodeGUID)
		, m_srcOutputName(szSrcOutputName)
		, m_dstNodeGUID(dstNodeGUID)
		, m_dstInputName(szDstInputName)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CScriptGraphLink::SetSrcNodeGUID(const SGUID& guid)
	{
		m_srcNodeGUID = guid;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CScriptGraphLink::GetSrcNodeGUID() const
	{
		return m_srcNodeGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CScriptGraphLink::GetSrcOutputName() const
	{
		return m_srcOutputName.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptGraphLink::SetDstNodeGUID(const SGUID& guid)
	{
		m_dstNodeGUID = guid;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CScriptGraphLink::GetDstNodeGUID() const
	{
		return m_dstNodeGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CScriptGraphLink::GetDstInputName() const
	{
		return m_dstInputName.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptGraphLink::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;
		if(!archive.isEdit())
		{
			archive(m_srcNodeGUID, "src_node_guid");
			archive(m_srcOutputName, "src_output_name");
			archive(m_dstNodeGUID, "dst_node_guid");
			archive(m_dstInputName, "dst_input_name");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphBase::CDocGraphBase(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const char* szName, EScriptGraphType type, const SGUID& contextGUID)
		: CScriptElementBase(EScriptElementType::Graph, file)
		, m_guid(guid)
		, m_scopeGUID(scopeGUID)
		, m_name(szName)
		, m_type(type)
		, m_contextGUID(contextGUID)
		, m_pos(ZERO)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CDocGraphBase::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CDocGraphBase::GetScopeGUID() const
	{
		return m_scopeGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CDocGraphBase::SetName(const char* szName)
	{
		m_name = szName;
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CDocGraphBase::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::EnumerateDependencies(const ScriptDependancyEnumerator& enumerator) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(enumerator);
		if(enumerator)
		{
			if(m_contextGUID)
			{
				enumerator(m_contextGUID);
			}
			for(const CScriptVariableDeclaration& input : m_inputs)
			{
				input.EnumerateDependencies(enumerator);
			}
			for(const CScriptVariableDeclaration& output : m_outputs)
			{
				output.EnumerateDependencies(enumerator);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::Refresh(const SScriptRefreshParams& params)
	{
		for(DocGraphNodeMap::value_type& node : m_nodes)
		{
			node.second.pNode->Refresh(params);
		}
		RemoveBrokenLinks();
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		SerializationContext::SetValidatorLink(archive, SValidatorLink(m_guid)); // #SchematycTODO : This should be handled by derived classes!

		switch(SerializationContext::GetPass(archive))
		{
		case ESerializationPass::PreLoad:
			{
				archive(m_pos, "pos");
				switch(m_type) // #SchematycTODO : Ideally it should be the implementation that decides whether or not to serialize inputs and outputs.
				{
				case EScriptGraphType::Function:
				case EScriptGraphType::Condition:
					{
						archive(m_inputs, "inputs", "Inputs");
						archive(m_outputs, "outputs", "Outputs");
						break;
					}
				}
				Serialization::SContext fileContext(archive, static_cast<IScriptFile*>(&CScriptElementBase::GetFile())); // #SchematycTODO : Do we really need this?
				Serialization::SContext docGraphContext(archive, static_cast<CDocGraphBase*>(this)); // #SchematycTODO : Do we really need this?
				archive(m_nodes, "nodes");
				break;
			}
		case ESerializationPass::Load:
			{
				switch(m_type) // #SchematycTODO : Ideally it should be the implementation that decides whether or not to serialize inputs and outputs.
				{
				case EScriptGraphType::Function:
				case EScriptGraphType::Condition:
					{
						archive(m_inputs, "inputs", "Inputs");
						archive(m_outputs, "outputs", "Outputs");
						break;
					}
				}
				break;
			}
		case ESerializationPass::PostLoad:
			{
				Serialization::SContext fileContext(archive, static_cast<IScriptFile*>(&CScriptElementBase::GetFile())); // #SchematycTODO : Do we really need this?
				Serialization::SContext docGraphContext(archive, static_cast<CDocGraphBase*>(this)); // #SchematycTODO : Do we really need this?
				archive(m_nodes, "nodes");
				archive(m_links, "links");
				break;
			}
		case ESerializationPass::Save:
			{
				SInfoSerializer(*this).Serialize(archive);
				SDataSerializer(*this).Serialize(archive);
				SNodeAndLinkSerializer(*this).Serialize(archive);
				break;
			}
		case ESerializationPass::Edit:
			{
				archive(m_name, "name", "!Name");
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		m_guid        = guidRemapper.Remap(m_guid);
		m_scopeGUID   = guidRemapper.Remap(m_scopeGUID);
		m_contextGUID = guidRemapper.Remap(m_contextGUID);
		for(CScriptVariableDeclaration& input : m_inputs)
		{
			input.RemapGUIDs(guidRemapper);
		}
		for(CScriptVariableDeclaration& output : m_outputs)
		{
			output.RemapGUIDs(guidRemapper);
		}
		for(DocGraphNodeMap::value_type& node : m_nodes)
		{
			node.second.pNode->RemapGUIDs(guidRemapper);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	EScriptGraphType CDocGraphBase::GetType() const
	{
		return m_type;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CDocGraphBase::GetContextGUID() const
	{
		return m_contextGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::SetPos(Vec2 pos)
	{
		m_pos = pos;
	}

	//////////////////////////////////////////////////////////////////////////
	Vec2 CDocGraphBase::GetPos() const
	{
		return m_pos;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBase::GetInputCount() const
	{
		return m_inputs.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CDocGraphBase::GetInputName(size_t inputIdx) const
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].GetName() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphBase::GetInputValue(size_t inputIdx) const
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].GetValue() : IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBase::GetOutputCount() const
	{
		return m_outputs.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CDocGraphBase::GetOutputName(size_t outputIdx) const
	{
		return outputIdx < m_outputs.size() ? m_outputs[outputIdx].GetName() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphBase::GetOutputValue(size_t outputIdx) const
	{
		return outputIdx < m_outputs.size() ? m_outputs[outputIdx].GetValue() : IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBase::GetAvailableNodeCount()
	{
		return m_availableNodes.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CDocGraphBase::GetAvailableNodeName(size_t availableNodeIdx) const
	{
		return availableNodeIdx < m_availableNodes.size() ? m_availableNodes[availableNodeIdx].name.c_str() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	EScriptGraphNodeType CDocGraphBase::GetAvailableNodeType(size_t availableNodeIdx) const
	{
		return availableNodeIdx < m_availableNodes.size() ? m_availableNodes[availableNodeIdx].type : EScriptGraphNodeType::Unknown;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CDocGraphBase::GetAvailableNodeContextGUID(size_t availableNodeIdx) const
	{
		return availableNodeIdx < m_availableNodes.size() ? m_availableNodes[availableNodeIdx].contextGUID : SGUID();
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CDocGraphBase::GetAvailableNodeRefGUID(size_t availableNodeIdx) const
	{
		return availableNodeIdx < m_availableNodes.size() ? m_availableNodes[availableNodeIdx].refGUID : SGUID();
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptGraphNode* CDocGraphBase::AddNode(EScriptGraphNodeType type, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetNode(guid))
			{
				IScriptGraphNodePtr pNode = CreateNode(guid, type, contextGUID, refGUID, pos);
				CRY_ASSERT(pNode);
				if(pNode)
				{
					m_nodes.insert(DocGraphNodeMap::value_type(guid, pNode));
					return pNode.get();
				}
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::RemoveNode(const SGUID& guid)
	{
		IScriptGraphNode* pNode = GetNode(guid);
		SCHEMATYC2_SYSTEM_ASSERT(pNode);
		if(pNode)
		{
			m_nodes.erase(guid);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptGraphNode* CDocGraphBase::GetNode(const SGUID& guid)
	{
		DocGraphNodeMap::iterator itNode = m_nodes.find(guid);
		return itNode != m_nodes.end() ? itNode->second.pNode.get() : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptGraphNode* CDocGraphBase::GetNode(const SGUID& guid) const
	{
		DocGraphNodeMap::const_iterator itNode = m_nodes.find(guid);
		return itNode != m_nodes.end() ? itNode->second.pNode.get() : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::VisitNodes(const ScriptGraphNodeVisitor& visitor)
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(DocGraphNodeMap::value_type& node : m_nodes)
			{
				if(visitor(*node.second.pNode) != EVisitStatus::Continue)
				{
					return;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::VisitNodes(const ScriptGraphNodeConstVisitor& visitor) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(const DocGraphNodeMap::value_type& node : m_nodes)
			{
				if(visitor(*node.second.pNode) != EVisitStatus::Continue)
				{
					return;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CDocGraphBase::CanAddLink(const SGUID& srcNodeGUID, const char* szScOutputName, const SGUID& dstNodeGUID, const char* szDstInputName) const
	{
		const IScriptGraphNode* pSrcNode = CDocGraphBase::GetNode(srcNodeGUID);
		const IScriptGraphNode* pDstNode = CDocGraphBase::GetNode(dstNodeGUID);
		SCHEMATYC2_SYSTEM_ASSERT(pSrcNode && pDstNode);
		if(pSrcNode && pDstNode)
		{
			const size_t srcOutputIdx = DocUtils::FindGraphNodeOutput(*pSrcNode, szScOutputName);
			const size_t dstInputIdx = DocUtils::FindGraphNodeInput(*pDstNode, szDstInputName);
			SCHEMATYC2_SYSTEM_ASSERT((srcOutputIdx != INVALID_INDEX) && (dstInputIdx != INVALID_INDEX));
			if((srcOutputIdx != INVALID_INDEX) && (dstInputIdx != INVALID_INDEX))
			{
				const EScriptGraphPortFlags srcOutputFlags = pSrcNode->GetOutputFlags(srcOutputIdx);
				const EScriptGraphPortFlags dstInputFlags = pDstNode->GetInputFlags(dstInputIdx);
				const CAggregateTypeId      srcOutputTypeId = pSrcNode->GetOutputTypeId(srcOutputIdx);
				const CAggregateTypeId      dstInputTypeId = pDstNode->GetInputTypeId(dstInputIdx);
				if((((srcOutputFlags & EScriptGraphPortFlags::Execute) != 0) && ((dstInputFlags & EScriptGraphPortFlags::Execute) != 0)) || (srcOutputTypeId == dstInputTypeId))
				{
					if((pSrcNode->GetOutputFlags(srcOutputIdx) & EScriptGraphPortFlags::MultiLink) == 0)
					{
						TScriptGraphLinkConstVector srcOutputLinks;
						DocUtils::CollectGraphNodeOutputLinks(*this, srcNodeGUID, pSrcNode->GetOutputName(srcOutputIdx), srcOutputLinks);
						if(!srcOutputLinks.empty())
						{
							return false;
						}
					}
					if((pDstNode->GetInputFlags(dstInputIdx) & EScriptGraphPortFlags::MultiLink) == 0)
					{
						TScriptGraphLinkConstVector dstInputLinks;
						DocUtils::CollectGraphNodeInputLinks(*this, dstNodeGUID, pDstNode->GetInputName(dstInputIdx), dstInputLinks);
						if(!dstInputLinks.empty())
						{
							return false;
						}
					}
					if(CDocGraphBase::FindLink(srcNodeGUID, pSrcNode->GetOutputName(srcOutputIdx), dstNodeGUID, pDstNode->GetInputName(dstInputIdx)) != INVALID_INDEX)
					{
						return false;
					}
					// Trace back along sequence to make sure we're not creating a loop.
					if(DocUtils::IsPrecedingNodeInGraphSequence(*this, *pSrcNode, dstNodeGUID))
					{
						return false;
					}
					return true;
				}
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptGraphLink* CDocGraphBase::AddLink(const SGUID& srcNodeGUID, const char* szSrcOutputName, const SGUID& dstNodeGUID, const char* szDstInputName)
	{
		if(FindLink(srcNodeGUID, szSrcOutputName, dstNodeGUID, szDstInputName))
		{
			CScriptGraphLinkPtr pLink(new CScriptGraphLink(srcNodeGUID, szSrcOutputName, dstNodeGUID, szDstInputName));
			m_links.push_back(pLink);
			return pLink.get();
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::RemoveLink(size_t linkIdx)
	{
		const size_t linkCount = m_links.size();
		SCHEMATYC2_SYSTEM_ASSERT(linkIdx < linkCount);
		if(linkIdx < linkCount)
		{
			m_signals.linkRemoved.Send(*m_links[linkIdx]);
			m_links.erase(m_links.begin() + linkIdx);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::RemoveLinks(const SGUID& nodeGUID)
	{
		// #SchematycTODO : We can probably optimize this a bit.
		for(size_t linkIdx = 0, linkCount = m_links.size(); linkIdx < linkCount; )
		{
			if((m_links[linkIdx]->GetSrcNodeGUID() == nodeGUID) || (m_links[linkIdx]->GetDstNodeGUID() == nodeGUID))
			{
				m_signals.linkRemoved.Send(*m_links[linkIdx]);
				m_links.erase(m_links.begin() + linkIdx);
				-- linkCount;
			}
			else
			{
				++ linkIdx;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBase::GetLinkCount() const
	{
		return m_links.size();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBase::FindLink(const SGUID& srcGUID, const char* szSrcOutputName, const SGUID& dstNodeGUID, const char* szDstInputName) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(szSrcOutputName && szDstInputName);
		if(szSrcOutputName && szDstInputName)
		{
			for(LinkVector::const_iterator iBeginLink = m_links.begin(), iEndLink = m_links.end(), iLink = iBeginLink; iLink != iEndLink; ++ iLink)
			{
				const CScriptGraphLink&	link = *(*iLink);
				if((link.GetSrcNodeGUID() == srcGUID) && (link.GetDstNodeGUID() == dstNodeGUID) && (strcmp(link.GetSrcOutputName(), szSrcOutputName) == 0) && (strcmp(link.GetDstInputName(), szDstInputName) == 0))
				{
					return iLink - iBeginLink;
				}
			}
		}
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptGraphLink* CDocGraphBase::GetLink(size_t linkIdx)
	{
		return linkIdx < m_links.size() ? m_links[linkIdx].get() : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptGraphLink* CDocGraphBase::GetLink(size_t linkIdx) const
	{
		return linkIdx < m_links.size() ? m_links[linkIdx].get() : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::VisitLinks(const ScriptGraphLinkVisitor& visitor)
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(CScriptGraphLinkPtr& pLink : m_links)
			{
				if(visitor(*pLink) != EVisitStatus::Continue)
				{
					return;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::VisitLinks(const ScriptGraphLinkConstVisitor& visitor) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(const CScriptGraphLinkPtr& pLink : m_links)
			{
				if(visitor(*pLink) != EVisitStatus::Continue)
				{
					return;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::RemoveBrokenLinks()
	{
		size_t linkCount = m_links.size();
		for(size_t linkIdx = 0; linkIdx < linkCount; )
		{
			CScriptGraphLink& link = *m_links[linkIdx];
			if(IsBrokenLink(link))
			{
				m_signals.linkRemoved.Send(link);
				-- linkCount;
				if(linkIdx < linkCount)
				{
					m_links[linkIdx] = m_links[linkCount];
				}
			}
			else
			{
				++ linkIdx;
			}
		}
		m_links.resize(linkCount);
	}

	//////////////////////////////////////////////////////////////////////////
	SScriptGraphSignals& CDocGraphBase::Signals()
	{
		return m_signals;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::SerializeNodeMapValue(Serialization::IArchive& archive, std::pair<SGUID, SDocGraphNode>& value)
	{
		SerializationContext::SetValidatorLink(archive, SValidatorLink(m_guid));

		switch(SerializationContext::GetPass(archive))
		{
		case ESerializationPass::PreLoad:
			{
				DocGraphBaseUtils::SNodeHeader nodeHeader;
				archive(nodeHeader, "header");
				IScriptGraphNodePtr pNode = CreateNode(nodeHeader.guid, nodeHeader.type, nodeHeader.contextGUID, nodeHeader.refGUID, nodeHeader.pos);
				SCHEMATYC2_SYSTEM_ASSERT(pNode);
				if(pNode)
				{
					value.first  = nodeHeader.guid;
					value.second = pNode;
					// Hack to fix cloning of state nodes.
					if(nodeHeader.type == EScriptGraphNodeType::State)
					{
						archive(*value.second.pNode, "detail");
					}
				}
				break;
			}
		case ESerializationPass::PostLoad:
			{
				archive(*value.second.pNode, "detail");
				break;
			}
		case ESerializationPass::Save:
			{
				IScriptGraphNode&              node = *value.second.pNode;
				DocGraphBaseUtils::SNodeHeader nodeHeader(node.GetGUID(), node.GetType(), node.GetContextGUID(), node.GetRefGUID(), node.GetPos());
				archive(nodeHeader, "header");
				archive(node, "detail");
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::AddAvailableNode(const char* szName, EScriptGraphNodeType type, const SGUID& contextGUID, const SGUID& refGUID)
	{
		m_availableNodes.push_back(SAvailableNode(szName, type, contextGUID, refGUID));
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::ClearAvailablNodes()
	{
		m_availableNodes.clear();
	}

	//////////////////////////////////////////////////////////////////////////
	ScriptVariableDeclarationVector& CDocGraphBase::GetInputs()
	{
		return m_inputs;
	}

	//////////////////////////////////////////////////////////////////////////
	ScriptVariableDeclarationVector& CDocGraphBase::GetOutputs()
	{
		return m_outputs;
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphBase::SInfoSerializer::SInfoSerializer(CDocGraphBase& _graph)
		: graph(_graph)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::SInfoSerializer::Serialize(Serialization::IArchive& archive)
	{
		archive(graph.m_pos, "pos");
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphBase::SDataSerializer::SDataSerializer(CDocGraphBase& _graph)
		: graph(_graph)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::SDataSerializer::Serialize(Serialization::IArchive& archive)
	{
		// #SchematycTODO : Ideally it should be the implementation that decides whether or not to serialize inputs and outputs.
		switch(graph.m_type)
		{
		case EScriptGraphType::Function:
		case EScriptGraphType::Condition:
			{
				Serialization::SContext fileContext(archive, static_cast<IScriptFile*>(&graph.GetFile())); // #SchematycTODO : Do we still need this?
				Serialization::SContext docGraphContext(archive, static_cast<CDocGraphBase*>(&graph)); // #SchematycTODO : Do we still need this?
				archive(graph.m_inputs, "inputs", "Inputs");
				archive(graph.m_outputs, "outputs", "Outputs");
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphBase::SNodeAndLinkSerializer::SNodeAndLinkSerializer(CDocGraphBase& _graph)
		: graph(_graph)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBase::SNodeAndLinkSerializer::Serialize(Serialization::IArchive& archive)
	{
		Serialization::SContext fileContext(archive, static_cast<IScriptFile*>(&graph.GetFile())); // #SchematycTODO : Do we still need this?
		Serialization::SContext docGraphContext(archive, static_cast<CDocGraphBase*>(&graph)); // #SchematycTODO : Do we still need this?
		archive(graph.m_nodes, "nodes");

		std::sort(graph.m_links.begin(), graph.m_links.end(), [](const CScriptGraphLinkPtr a, const CScriptGraphLinkPtr b)
		{
			return a->GetSrcNodeGUID() > b->GetSrcNodeGUID();
		});

		archive(graph.m_links, "links");
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphBase::SAvailableNode::SAvailableNode(const char* _szName, EScriptGraphNodeType _type, const SGUID& _contextGUID, const SGUID& _refGUID)
		: name(_szName)
		, type(_type)
		, contextGUID(_contextGUID)
		, refGUID(_refGUID)
	{}

	//////////////////////////////////////////////////////////////////////////
	bool CDocGraphBase::IsBrokenLink(const CScriptGraphLink& link) const
	{
		const IScriptGraphNode* pSrcNode = GetNode(link.GetSrcNodeGUID());
		if(!pSrcNode || (DocUtils::FindGraphNodeOutput(*pSrcNode, link.GetSrcOutputName()) == INVALID_INDEX))
		{
			return true;
		}
		const IScriptGraphNode* pDstNode = GetNode(link.GetDstNodeGUID());
		if(!pDstNode || (DocUtils::FindGraphNodeInput(*pDstNode, link.GetDstInputName()) == INVALID_INDEX))
		{
			return true;
		}
		return false;
	}
}

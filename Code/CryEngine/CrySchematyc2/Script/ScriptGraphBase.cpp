// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/ScriptGraphBase.h"

#include <CrySchematyc2/Serialization/ISerializationContext.h>

#include "DomainContext.h"
#include "Script/GraphNodes/ScriptGraphCommentNode.h"    // #SchematycTODO : Where should script graph node creators be registered?
#include "Script/GraphNodes/ScriptGraphBeginNode.h"    // #SchematycTODO : Where should script graph node creators be registered?
#include "Script/GraphNodes/ScriptGraphFunctionNode.h" // #SchematycTODO : Where should script graph node creators be registered?
#include "Script/GraphNodes/ScriptGraphGetNode.h"      // #SchematycTODO : Where should script graph node creators be registered?
#include "Script/GraphNodes/ScriptGraphNodeFactory.h"  // #SchematycTODO : Where should the script graph node factory live?
#include "Script/GraphNodes/ScriptGraphSwitchNode.h"   // #SchematycTODO : Where should script graph node creators be registered?

namespace Schematyc2
{
	static CScriptGraphNodeFactory g_scriptGraphNodeFactory;
}

namespace std // In order to resolve ADL, std type serialization overrides must be placed in std namespace.
{
	inline bool Serialize(Serialization::IArchive& archive, shared_ptr<Schematyc2::IScriptGraphNode>& value, const char* szName, const char* szLabel)
	{
		if(!value && archive.isInput())
		{
			Schematyc2::SGUID typeGUID;
			archive(typeGUID, "typeGUID");
			value = Schematyc2::g_scriptGraphNodeFactory.CreateNode(typeGUID);
		}
		if(value)
		{
			if(archive.isOutput() && !archive.isEdit())
			{
				Schematyc2::SGUID typeGUID = value->GetTypeGUID();
				archive(typeGUID, "typeGUID");
			}
			archive(*value, szName, szLabel);
		}
		return true;
	}

	/*bool Serialize(Serialization::IArchive& archive, shared_ptr<Schematyc2::CScriptGraphLink>& value, const char* szName, const char* szLabel)
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
	void RegisterScriptGraphNodeCreators()
	{
		static bool bRegister = true;
		if(bRegister)
		{
			CScriptGraphCommentNode::RegisterCreator(g_scriptGraphNodeFactory);
			CScriptGraphBeginNode::RegisterCreator(g_scriptGraphNodeFactory);
			CScriptGraphFunctionNode::RegisterCreator(g_scriptGraphNodeFactory);
			CScriptGraphGetNode::RegisterCreator(g_scriptGraphNodeFactory);
			//CScriptGraphSwitchNode::RegisterCreator(g_scriptGraphNodeFactory);
			bRegister = false;
		}
	}

	CScriptGraphBase::CScriptGraphBase(IScriptElement& element)
		: m_element(element)
		, m_pos(ZERO)
	{
		RegisterScriptGraphNodeCreators();
	}

	EScriptExtensionId CScriptGraphBase::GetId_New() const
	{
		return EScriptExtensionId::Graph;
	}

	void CScriptGraphBase::Refresh_New(const SScriptRefreshParams& params)
	{
		for(Nodes::value_type& node : m_nodes)
		{
			node.second->Refresh(params);
		}
		RemoveBrokenLinks();
	}

	void CScriptGraphBase::Serialize_New(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		switch(SerializationContext::GetPass(archive))
		{
		case ESerializationPass::PreLoad:
			{
				archive(m_pos, "pos");
				archive(m_nodes, "nodes");
				for(Nodes::value_type node : m_nodes)
				{
					node.second->Attach(*this);
				}
				break;
			}
		case ESerializationPass::PostLoad:
			{
				archive(m_nodes, "nodes");
				archive(m_links, "links");
				break;
			}
		case ESerializationPass::Save:
			{
				archive(m_pos, "pos");
				archive(m_nodes, "nodes");
				archive(m_links, "links");
				break;
			}
		case ESerializationPass::Validate:
			{
				archive(m_nodes, "nodes");
				break;
			}
		}
	}

	void CScriptGraphBase::RemapGUIDs_New(IGUIDRemapper& guidRemapper)
	{
		for(Nodes::value_type node : m_nodes)
		{
			node.second->RemapGUIDs(guidRemapper);
		}
	}

	SGUID CScriptGraphBase::GetContextGUID() const
	{
		return SGUID();
	}

	IScriptElement& CScriptGraphBase::GetElement_New()
	{
		return m_element;
	}

	const IScriptElement& CScriptGraphBase::GetElement_New() const
	{
		return m_element;
	}

	void CScriptGraphBase::SetPos(Vec2 pos)
	{
		m_pos = pos;
	}

	Vec2 CScriptGraphBase::GetPos() const
	{
		return m_pos;
	}

	size_t CScriptGraphBase::GetInputCount() const
	{
		return 0;
	}

	const char* CScriptGraphBase::GetInputName(size_t inputIdx) const
	{
		return "";
	}

	IAnyConstPtr CScriptGraphBase::GetInputValue(size_t inputIdx) const
	{
		return IAnyConstPtr();
	}

	size_t CScriptGraphBase::GetOutputCount() const
	{
		return 0;
	}

	const char* CScriptGraphBase::GetOutputName(size_t outputIdx) const
	{
		return "";
	}

	IAnyConstPtr CScriptGraphBase::GetOutputValue(size_t outputIdx) const
	{
		return IAnyConstPtr();
	}

	void CScriptGraphBase::RefreshAvailableNodes(const CAggregateTypeId& inputTypeId) {}

	size_t CScriptGraphBase::GetAvailableNodeCount()
	{
		return 0;
	}

	const char* CScriptGraphBase::GetAvailableNodeName(size_t availableNodeIdx) const
	{
		return "";
	}

	EScriptGraphNodeType CScriptGraphBase::GetAvailableNodeType(size_t availableNodeIdx) const
	{
		return EScriptGraphNodeType::Unknown;
	}

	SGUID CScriptGraphBase::GetAvailableNodeContextGUID(size_t availableNodeIdx) const
	{
		return SGUID();
	}

	SGUID CScriptGraphBase::GetAvailableNodeRefGUID(size_t availableNodeIdx) const
	{
		return SGUID();
	}

	IScriptGraphNode* CScriptGraphBase::AddNode(EScriptGraphNodeType type, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
	{
		return nullptr;
	}

	void CScriptGraphBase::RemoveNode(const SGUID& guid)
	{
		IScriptGraphNode* pNode = GetNode(guid);
		SCHEMATYC2_SYSTEM_ASSERT(pNode);
		if(pNode)
		{
			m_nodes.erase(guid);
		}
	}

	IScriptGraphNode* CScriptGraphBase::GetNode(const SGUID& guid)
	{
		Nodes::iterator itNode = m_nodes.find(guid);
		return itNode != m_nodes.end() ? itNode->second.get() : nullptr;
	}

	const IScriptGraphNode* CScriptGraphBase::GetNode(const SGUID& guid) const
	{
		Nodes::const_iterator itNode = m_nodes.find(guid);
		return itNode != m_nodes.end() ? itNode->second.get() : nullptr;
	}

	void CScriptGraphBase::VisitNodes(const ScriptGraphNodeVisitor& visitor)
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(Nodes::value_type& node : m_nodes)
			{
				if(visitor(*node.second) != EVisitStatus::Continue)
				{
					return;
				}
			}
		}
	}

	void CScriptGraphBase::VisitNodes(const ScriptGraphNodeConstVisitor& visitor) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(const Nodes::value_type& node : m_nodes)
			{
				if(visitor(*node.second) != EVisitStatus::Continue)
				{
					return;
				}
			}
		}
	}

	bool CScriptGraphBase::CanAddLink(const SGUID& srcNodeGUID, const char* szScOutputName, const SGUID& dstNodeGUID, const char* szDstInputName) const
	{
		const IScriptGraphNode* pSrcNode = GetNode(srcNodeGUID);
		const IScriptGraphNode* pDstNode = GetNode(dstNodeGUID);
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
					if(FindLink(srcNodeGUID, pSrcNode->GetOutputName(srcOutputIdx), dstNodeGUID, pDstNode->GetInputName(dstInputIdx)) != INVALID_INDEX)
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

	IScriptGraphLink* CScriptGraphBase::AddLink(const SGUID& srcNodeGUID, const char* szSrcOutputName, const SGUID& dstNodeGUID, const char* szDstInputName)
	{
		if(FindLink(srcNodeGUID, szSrcOutputName, dstNodeGUID, szDstInputName))
		{
			CScriptGraphLinkPtr pLink(new CScriptGraphLink(srcNodeGUID, szSrcOutputName, dstNodeGUID, szDstInputName));
			m_links.push_back(pLink);
			return pLink.get();
		}
		return nullptr;
	}

	void CScriptGraphBase::RemoveLink(size_t linkIdx)
	{
		const size_t linkCount = m_links.size();
		SCHEMATYC2_SYSTEM_ASSERT(linkIdx < linkCount);
		if(linkIdx < linkCount)
		{
			m_signals.linkRemoved.Send(*m_links[linkIdx]);
			m_links.erase(m_links.begin() + linkIdx);
		}
	}

	void CScriptGraphBase::RemoveLinks(const SGUID& nodeGUID)
	{
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

	size_t CScriptGraphBase::GetLinkCount() const
	{
		return m_links.size();
	}

	size_t CScriptGraphBase::FindLink(const SGUID& srcGUID, const char* szSrcOutputName, const SGUID& dstNodeGUID, const char* szDstInputName) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(szSrcOutputName && szDstInputName);
		if(szSrcOutputName && szDstInputName)
		{
			for(Links::const_iterator itBeginLink = m_links.begin(), itEndLink = m_links.end(), itLink = itBeginLink; itLink != itEndLink; ++ itLink)
			{
				const CScriptGraphLink& link = *(*itLink);
				if((link.GetSrcNodeGUID() == srcGUID) && (link.GetDstNodeGUID() == dstNodeGUID) && (strcmp(link.GetSrcOutputName(), szSrcOutputName) == 0) && (strcmp(link.GetDstInputName(), szDstInputName) == 0))
				{
					return itLink - itBeginLink;
				}
			}
		}
		return INVALID_INDEX;
	}

	IScriptGraphLink* CScriptGraphBase::GetLink(size_t linkIdx)
	{
		return linkIdx < m_links.size() ? m_links[linkIdx].get() : nullptr;
	}

	const IScriptGraphLink* CScriptGraphBase::GetLink(size_t linkIdx) const
	{
		return linkIdx < m_links.size() ? m_links[linkIdx].get() : nullptr;
	}

	void CScriptGraphBase::VisitLinks(const ScriptGraphLinkVisitor& visitor)
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

	void CScriptGraphBase::VisitLinks(const ScriptGraphLinkConstVisitor& visitor) const
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

	void CScriptGraphBase::RemoveBrokenLinks() {}

	SScriptGraphSignals& CScriptGraphBase::Signals()
	{
		return m_signals;
	}

	uint32 CScriptGraphBase::GetNodeCount() const
	{
		return m_nodes.size();
	}

	bool CScriptGraphBase::AddNode_New(const IScriptGraphNodePtr& pNode)
	{
		if(!pNode)
		{
			return false;
		}
		const SGUID guid = pNode->GetGUID();
		if(!guid || GetNode(guid))
		{
			return false;
		}
		m_nodes.insert(Nodes::value_type(guid, pNode));
		pNode->Attach(*this);
		return true;
	}

	void CScriptGraphBase::PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu)
	{
		const IScriptElement& element = CScriptGraphBase::GetElement_New();
		CDomainContext        domainContext(SDomainContextScope(&element.GetFile(), element.GetGUID()));
		g_scriptGraphNodeFactory.PopulateNodeCreationMenu(nodeCreationMenu, domainContext, *this);
	}

	void CScriptGraphBase::VisitInputLinks(const ScriptGraphLinkVisitor& visitor, const SGUID& dstNodeGUID, const char* szDstInputName)
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor && szDstInputName);
		if(visitor && szDstInputName)
		{
			for(CScriptGraphLinkPtr& pLink : m_links)
			{
				if((pLink->GetDstNodeGUID() == dstNodeGUID) && (strcmp(pLink->GetDstInputName(), szDstInputName) == 0))
				{
					if(visitor(*pLink) != EVisitStatus::Continue)
					{
						return;
					}
				}
			}
		}
	}

	void CScriptGraphBase::VisitInputLinks(const ScriptGraphLinkConstVisitor& visitor, const SGUID& dstNodeGUID, const char* szDstInputName) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor && szDstInputName);
		if(visitor && szDstInputName)
		{
			for(const CScriptGraphLinkPtr& pLink : m_links)
			{
				if((pLink->GetDstNodeGUID() == dstNodeGUID) && (strcmp(pLink->GetDstInputName(), szDstInputName) == 0))
				{
					if(visitor(*pLink) != EVisitStatus::Continue)
					{
						return;
					}
				}
			}
		}
	}

	void CScriptGraphBase::VisitOutputLinks(const ScriptGraphLinkVisitor& visitor, const SGUID& srcNodeGUID, const char* szSrcOutputName)
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor && szSrcOutputName);
		if(visitor && szSrcOutputName)
		{
			for(CScriptGraphLinkPtr& pLink : m_links)
			{
				if((pLink->GetSrcNodeGUID() == srcNodeGUID) && (strcmp(pLink->GetSrcOutputName(), szSrcOutputName) == 0))
				{
					if(visitor(*pLink) != EVisitStatus::Continue)
					{
						return;
					}
				}
			}
		}
	}

	void CScriptGraphBase::VisitOutputLinks(const ScriptGraphLinkConstVisitor& visitor, const SGUID& srcNodeGUID, const char* szSrcOutputName) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor && szSrcOutputName);
		if(visitor && szSrcOutputName)
		{
			for(const CScriptGraphLinkPtr& pLink : m_links)
			{
				if((pLink->GetSrcNodeGUID() == srcNodeGUID) && (strcmp(pLink->GetSrcOutputName(), szSrcOutputName) == 0))
				{
					if(visitor(*pLink) != EVisitStatus::Continue)
					{
						return;
					}
				}
			}
		}
	}

	bool CScriptGraphBase::GetLinkSrc(const IScriptGraphLink& link, IScriptGraphNode*& pNode, uint32& outputIdx)
	{
		pNode = GetNode(link.GetSrcNodeGUID());
		if(pNode)
		{
			outputIdx = pNode->FindOutput(link.GetSrcOutputName());
			return outputIdx != s_invalidIdx;
		}
		return false;
	}

	bool CScriptGraphBase::GetLinkSrc(const IScriptGraphLink& link, const IScriptGraphNode*& pNode, uint32& outputIdx) const
	{
		pNode = GetNode(link.GetSrcNodeGUID());
		if(pNode)
		{
			outputIdx = pNode->FindOutput(link.GetSrcOutputName());
			return outputIdx != s_invalidIdx;
		}
		return false;
	}

	bool CScriptGraphBase::GetLinkDst(const IScriptGraphLink& link, IScriptGraphNode*& pNode, uint32& inputIdx)
	{
		pNode = GetNode(link.GetDstNodeGUID());
		if(pNode)
		{
			inputIdx = pNode->FindInput(link.GetDstInputName());
			return inputIdx != s_invalidIdx;
		}
		return false;
	}

	bool CScriptGraphBase::GetLinkDst(const IScriptGraphLink& link, const IScriptGraphNode*& pNode, uint32& inputIdx) const
	{
		pNode = GetNode(link.GetDstNodeGUID());
		if(pNode)
		{
			inputIdx = pNode->FindInput(link.GetDstInputName());
			return inputIdx != s_invalidIdx;
		}
		return false;
	}
}

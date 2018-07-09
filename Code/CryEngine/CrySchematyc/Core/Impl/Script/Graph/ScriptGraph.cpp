// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ScriptGraph.h"

#include <CrySerialization/Math.h>
#include <CrySchematyc/SerializationUtils/ISerializationContext.h>
#include <CrySchematyc/Utils/Assert.h>
#include <CrySchematyc/Utils/StackString.h>

#include "Script/ScriptView.h"
#include "Script/Graph/ScriptGraphNode.h"
#include "Script/Graph/ScriptGraphNodeFactory.h" // #SchematycTODO : Where should the script graph node factory live?
#include "Script/Graph/ScriptGraphNodeModel.h"

namespace Schematyc
{

static CScriptGraphNodeFactory g_scriptGraphNodeFactory;

} // Schematyc

namespace std // In order to resolve ADL, std type serialization overrides must be placed in std namespace.
{

inline bool Serialize(Serialization::IArchive& archive, shared_ptr<Schematyc::IScriptGraphNode>& value, const char* szName, const char* szLabel)
{
	if (!value && archive.isInput())
	{
		CryGUID typeGUID;
		archive(typeGUID, "typeGUID");
		value = Schematyc::g_scriptGraphNodeFactory.CreateNode(typeGUID);
		if (!value)
		{
			Schematyc::CStackString temp;
			Schematyc::GUID::ToString(temp, typeGUID);
			SCHEMATYC_CORE_ERROR("Filed to create script graph node: guid = %s", temp.c_str());
		}
	}
	if (value)
	{
		if (archive.isOutput() && !archive.isEdit())
		{
			CryGUID typeGUID = value->GetTypeGUID();
			archive(typeGUID, "typeGUID");
		}
		archive(*value, szName, szLabel);
	}
	return true;
}

bool Serialize(Serialization::IArchive& archive, shared_ptr<Schematyc::CScriptGraphLink>& value, const char* szName, const char* szLabel)
{
	if (!value)
	{
		value = std::make_shared<Schematyc::CScriptGraphLink>();
	}
	archive(*value, szName, szLabel);
	return true;
}

} // std

namespace Schematyc
{

void RegisterScriptGraphNodeCreators()
{
	static bool bRegister = true;
	if (bRegister)
	{
		CScriptGraphNodeRegistrar::Process(g_scriptGraphNodeFactory);
		bRegister = false;
	}
}

CScriptGraphLink::CScriptGraphLink() {}

CScriptGraphLink::CScriptGraphLink(const CryGUID& srcNodeGUID, const char* szSrcOutputName, const CryGUID& dstNodeGUID, const char* szDstInputName)
	: m_srcNodeGUID(srcNodeGUID)
	, m_srcOutputName(szSrcOutputName)
	, m_dstNodeGUID(dstNodeGUID)
	, m_dstInputName(szDstInputName)
{}

CScriptGraphLink::CScriptGraphLink(const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId)
	: m_srcNodeGUID(srcNodeGUID)
	, m_srcOutputId(srcOutputId)
	, m_dstNodeGUID(dstNodeGUID)
	, m_dstInputId(dstInputId)
{}

void CScriptGraphLink::SetSrcNodeGUID(const CryGUID& guid)
{
	m_srcNodeGUID = guid;
}

CryGUID CScriptGraphLink::GetSrcNodeGUID() const
{
	return m_srcNodeGUID;
}

CUniqueId CScriptGraphLink::GetSrcOutputId() const
{
	return m_srcOutputId;
}

void CScriptGraphLink::SetDstNodeGUID(const CryGUID& guid)
{
	m_dstNodeGUID = guid;
}

CryGUID CScriptGraphLink::GetDstNodeGUID() const
{
	return m_dstNodeGUID;
}

CUniqueId CScriptGraphLink::GetDstInputId() const
{
	return m_dstInputId;
}

void CScriptGraphLink::Serialize(Serialization::IArchive& archive)
{
	if (!archive.isEdit())
	{
		archive(m_srcNodeGUID, "srcNodeGUID");
		archive(m_srcOutputId, "srcOutputId");
		archive(m_dstNodeGUID, "dstNodeGUID");
		archive(m_dstInputId, "dstInputId");
	}
}

void CScriptGraphLink::SetSrcOutputId(const CUniqueId& id)
{
	m_srcOutputId = id;
}

const char* CScriptGraphLink::GetSrcOutputName() const
{
	return m_srcOutputName.c_str();
}

void CScriptGraphLink::SetDstInputId(const CUniqueId& id)
{
	m_dstInputId = id;
}

const char* CScriptGraphLink::GetDstInputName() const
{
	return m_dstInputName.c_str();
}

CScriptGraph::CScriptGraph(IScriptElement& element, EScriptGraphType type)
	: m_element(element)
	, m_type(type)
	, m_pos(ZERO)
{
	RegisterScriptGraphNodeCreators();
}

void CScriptGraph::EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const
{
	for (const Nodes::value_type& node : m_nodes)
	{
		if (node.second)
		{
			node.second->EnumerateDependencies(enumerator, type);
		}
	}
}

void CScriptGraph::RemapDependencies(IGUIDRemapper& guidRemapper)
{
	for (Nodes::value_type node : m_nodes)
	{
		if (node.second)
		{
			node.second->RemapDependencies(guidRemapper);
		}
	}
}

void CScriptGraph::ProcessEvent(const SScriptEvent& event)
{
	switch (event.id)
	{
	case EScriptEventId::FileLoad:
		{
			PatchLinks();
			break;
		}
	}

	for (Nodes::value_type& node : m_nodes)
	{
		if (node.second)
		{
			node.second->ProcessEvent(event);
		}
	}

	RemoveBrokenLinks();
}

void CScriptGraph::Serialize(Serialization::IArchive& archive)
{
	switch (SerializationContext::GetPass(archive))
	{
	case ESerializationPass::LoadDependencies:
		{
			archive(m_pos, "pos");
			archive(m_nodes, "nodes");
			// #SchematycTODO : Strip broken nodes or replace with special 'error' nodes?
			for (Nodes::value_type node : m_nodes)
			{
				if (node.second)
				{
					node.second->Attach(*this);
				}
			}
			break;
		}
	case ESerializationPass::Load:
		{
			archive(m_nodes, "nodes");
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

EScriptGraphType CScriptGraph::GetType() const
{
	return m_type;
}

IScriptElement& CScriptGraph::GetElement()
{
	return m_element;
}

const IScriptElement& CScriptGraph::GetElement() const
{
	return m_element;
}

void CScriptGraph::SetPos(Vec2 pos)
{
	m_pos = pos;
}

Vec2 CScriptGraph::GetPos() const
{
	return m_pos;
}

void CScriptGraph::PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu)
{
	const IScriptElement& element = CScriptGraph::GetElement();
	CScriptView scriptView(element.GetGUID());
	g_scriptGraphNodeFactory.PopulateNodeCreationMenu(nodeCreationMenu, scriptView, *this);
}

bool CScriptGraph::AddNode(const IScriptGraphNodePtr& pNode)
{
	if (!pNode)
	{
		return false;
	}
	const CryGUID guid = pNode->GetGUID();
	if (GUID::IsEmpty(guid) || GetNode(guid))
	{
		return false;
	}
	m_nodes.insert(Nodes::value_type(guid, pNode));
	pNode->Attach(*this);
	return true;
}

IScriptGraphNodePtr CScriptGraph::AddNode(const CryGUID& typeGUID)
{
	const IScriptElement& element = CScriptGraph::GetElement();
	CScriptView scriptView(element.GetGUID());

	if (g_scriptGraphNodeFactory.CanCreateNode(typeGUID, scriptView, *this))
	{
		IScriptGraphNodePtr pNode = g_scriptGraphNodeFactory.CreateNode(typeGUID, gEnv->pSchematyc->CreateGUID());
		if (AddNode(pNode))
		{
			return pNode;
		}
	}
	return IScriptGraphNodePtr();
}

void CScriptGraph::RemoveNode(const CryGUID& guid)
{
	IScriptGraphNode* pNode = GetNode(guid);
	SCHEMATYC_CORE_ASSERT(pNode && !pNode->GetFlags().Check(EScriptGraphNodeFlags::NotRemovable));
	if (pNode && !pNode->GetFlags().Check(EScriptGraphNodeFlags::NotRemovable))
	{
		m_nodes.erase(guid);
	}
}

uint32 CScriptGraph::GetNodeCount() const
{
	return m_nodes.size();
}

IScriptGraphNode* CScriptGraph::GetNode(const CryGUID& guid)
{
	Nodes::iterator itNode = m_nodes.find(guid);
	return itNode != m_nodes.end() ? itNode->second.get() : nullptr;
}

const IScriptGraphNode* CScriptGraph::GetNode(const CryGUID& guid) const
{
	Nodes::const_iterator itNode = m_nodes.find(guid);
	return itNode != m_nodes.end() ? itNode->second.get() : nullptr;
}

void CScriptGraph::VisitNodes(const ScriptGraphNodeVisitor& visitor)
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (Nodes::value_type& node : m_nodes)
		{
			if (node.second)
			{
				if (visitor(*node.second) == EVisitStatus::Stop)
				{
					return;
				}
			}
		}
	}
}

void CScriptGraph::VisitNodes(const ScriptGraphNodeConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const Nodes::value_type& node : m_nodes)
		{
			if (node.second)
			{
				if (visitor(*node.second) == EVisitStatus::Stop)
				{
					return;
				}
			}
		}
	}
}

bool CScriptGraph::CanAddLink(const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId) const
{
	const CScriptGraphLink link(srcNodeGUID, srcOutputId, dstNodeGUID, dstInputId);

	const IScriptGraphNode* pSrcNode = nullptr;
	uint32 srcOutputIdx = InvalidIdx;
	if (!GetLinkSrc(link, pSrcNode, srcOutputIdx))
	{
		return false;
	}

	const IScriptGraphNode* pDstNode = nullptr;
	uint32 dstInputIdx = InvalidIdx;
	if (!GetLinkDst(link, pDstNode, dstInputIdx))
	{
		return false;
	}

	const ScriptGraphPortFlags srcOutputFlags = pSrcNode->GetOutputFlags(srcOutputIdx);
	const ScriptGraphPortFlags dstInputFlags = pDstNode->GetInputFlags(dstInputIdx);

	if (srcOutputFlags.Check(EScriptGraphPortFlags::Ptr) != dstInputFlags.Check(EScriptGraphPortFlags::Ptr))
	{
		return false;
	}
	if (srcOutputFlags.Check(EScriptGraphPortFlags::Array) != dstInputFlags.Check(EScriptGraphPortFlags::Array))
	{
		return false;
	}
	if (srcOutputFlags.CheckAny({ EScriptGraphPortFlags::Signal, EScriptGraphPortFlags::Flow }) != dstInputFlags.CheckAny({ EScriptGraphPortFlags::Signal, EScriptGraphPortFlags::Flow }))
	{
		return false;
	}
	if (!srcOutputFlags.Check(EScriptGraphPortFlags::Signal) && srcOutputFlags.Check(EScriptGraphPortFlags::Data) && !dstInputFlags.Check(EScriptGraphPortFlags::Data))
	{
		return false;
	}
	if (dstInputFlags.CheckAny({ EScriptGraphPortFlags::Signal, EScriptGraphPortFlags::Data }) && (pSrcNode->GetOutputTypeGUID(srcOutputIdx) != pDstNode->GetInputTypeGUID(dstInputIdx)))
	{
		return false;
	}

	if (!srcOutputFlags.Check(EScriptGraphPortFlags::MultiLink))
	{
		bool bLinked = false;
		auto visitOutputLink = [&bLinked](const IScriptGraphLink& outputLink) -> EVisitStatus
		{
			bLinked = true;
			return EVisitStatus::Stop;
		};
		VisitOutputLinks(visitOutputLink, srcNodeGUID, srcOutputId);

		if (bLinked)
		{
			return false;
		}
	}

	if (!dstInputFlags.Check(EScriptGraphPortFlags::MultiLink))
	{
		bool bLinked = false;
		auto visitInputLink = [&bLinked](const IScriptGraphLink& outputLink) -> EVisitStatus
		{
			bLinked = true;
			return EVisitStatus::Stop;
		};
		VisitInputLinks(visitInputLink, dstNodeGUID, dstInputId);

		if (bLinked)
		{
			return false;
		}
	}

	if (FindLink(srcNodeGUID, pSrcNode->GetOutputId(srcOutputIdx), dstNodeGUID, pDstNode->GetInputId(dstInputIdx)) != InvalidIdx)
	{
		return false;
	}

	// #SchematycTODO : Trace back to make sure we're not creating a loop.
	//if (((dstInputFlags & EScriptGraphPortFlags::End) == 0) && DocUtils::IsPrecedingNodeInGraphSequence(*this, *pSrcNode, dstNodeGUID))
	//{
	//	return false;
	//}

	return true;
}

IScriptGraphLink* CScriptGraph::AddLink(const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId)
{
	if (FindLink(srcNodeGUID, srcOutputId, dstNodeGUID, dstInputId) == InvalidIdx)
	{
		CScriptGraphLinkPtr pLink(new CScriptGraphLink(srcNodeGUID, srcOutputId, dstNodeGUID, dstInputId));
		m_links.push_back(pLink);
		return pLink.get();
	}
	return nullptr;
}

void CScriptGraph::RemoveLink(uint32 linkIdx)
{
	const uint32 linkCount = m_links.size();
	SCHEMATYC_CORE_ASSERT(linkIdx < linkCount);
	if (linkIdx < linkCount)
	{
		m_signals.linkRemoved.Send(*m_links[linkIdx]);
		m_links.erase(m_links.begin() + linkIdx);
	}
}

void CScriptGraph::RemoveLinks(const CryGUID& nodeGUID)
{
	auto isConnectedToNode = [&nodeGUID](const CScriptGraphLink& link) -> bool
	{
		return (link.GetSrcNodeGUID() == nodeGUID) || (link.GetDstNodeGUID() == nodeGUID);
	};

	RemoveLinks(isConnectedToNode);
}

uint32 CScriptGraph::GetLinkCount() const
{
	return m_links.size();
}

IScriptGraphLink* CScriptGraph::GetLink(uint32 linkIdx)
{
	return linkIdx < m_links.size() ? m_links[linkIdx].get() : nullptr;
}

const IScriptGraphLink* CScriptGraph::GetLink(uint32 linkIdx) const
{
	return linkIdx < m_links.size() ? m_links[linkIdx].get() : nullptr;
}

uint32 CScriptGraph::FindLink(const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId) const
{
	for (Links::const_iterator itBeginLink = m_links.begin(), itEndLink = m_links.end(), itLink = itBeginLink; itLink != itEndLink; ++itLink)
	{
		const CScriptGraphLink& link = *(*itLink);
		if ((link.GetSrcNodeGUID() == srcNodeGUID) && (link.GetDstNodeGUID() == dstNodeGUID) && (link.GetSrcOutputId() == srcOutputId) && (link.GetDstInputId() == dstInputId))
		{
			return static_cast<uint32>(itLink - itBeginLink);
		}
	}
	return InvalidIdx;
}

EVisitResult CScriptGraph::VisitLinks(const ScriptGraphLinkVisitor& visitor)
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (CScriptGraphLinkPtr& pLink : m_links)
		{
			const EVisitStatus status = visitor(*pLink);
			switch (status)
			{
			case EVisitStatus::Stop:
				{
					return EVisitResult::Stopped;
				}
			case EVisitStatus::Error:
				{
					return EVisitResult::Error;
				}
			}
		}
	}
	return EVisitResult::Complete;
}

EVisitResult CScriptGraph::VisitLinks(const ScriptGraphLinkConstVisitor& visitor) const
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const CScriptGraphLinkPtr& pLink : m_links)
		{
			const EVisitStatus status = visitor(*pLink);
			switch (status)
			{
			case EVisitStatus::Stop:
				{
					return EVisitResult::Stopped;
				}
			case EVisitStatus::Error:
				{
					return EVisitResult::Error;
				}
			}
		}
	}
	return EVisitResult::Complete;
}

EVisitResult CScriptGraph::VisitInputLinks(const ScriptGraphLinkVisitor& visitor, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId)
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (CScriptGraphLinkPtr& pLink : m_links)
		{
			if ((pLink->GetDstNodeGUID() == dstNodeGUID) && (pLink->GetDstInputId() == dstInputId))
			{
				const EVisitStatus status = visitor(*pLink);
				switch (status)
				{
				case EVisitStatus::Stop:
					{
						return EVisitResult::Stopped;
					}
				case EVisitStatus::Error:
					{
						return EVisitResult::Error;
					}
				}
			}
		}
	}
	return EVisitResult::Complete;
}

EVisitResult CScriptGraph::VisitInputLinks(const ScriptGraphLinkConstVisitor& visitor, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId) const
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const CScriptGraphLinkPtr& pLink : m_links)
		{
			if ((pLink->GetDstNodeGUID() == dstNodeGUID) && (pLink->GetDstInputId() == dstInputId))
			{
				const EVisitStatus status = visitor(*pLink);
				switch (status)
				{
				case EVisitStatus::Stop:
					{
						return EVisitResult::Stopped;
					}
				case EVisitStatus::Error:
					{
						return EVisitResult::Error;
					}
				}
			}
		}
	}
	return EVisitResult::Complete;
}

EVisitResult CScriptGraph::VisitOutputLinks(const ScriptGraphLinkVisitor& visitor, const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId)
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (CScriptGraphLinkPtr& pLink : m_links)
		{
			if ((pLink->GetSrcNodeGUID() == srcNodeGUID) && (pLink->GetSrcOutputId() == srcOutputId))
			{
				const EVisitStatus status = visitor(*pLink);
				switch (status)
				{
				case EVisitStatus::Stop:
					{
						return EVisitResult::Stopped;
					}
				case EVisitStatus::Error:
					{
						return EVisitResult::Error;
					}
				}
			}
		}
	}
	return EVisitResult::Complete;
}

EVisitResult CScriptGraph::VisitOutputLinks(const ScriptGraphLinkConstVisitor& visitor, const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId) const
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const CScriptGraphLinkPtr& pLink : m_links)
		{
			if ((pLink->GetSrcNodeGUID() == srcNodeGUID) && (pLink->GetSrcOutputId() == srcOutputId))
			{
				const EVisitStatus status = visitor(*pLink);
				switch (status)
				{
				case EVisitStatus::Stop:
					{
						return EVisitResult::Stopped;
					}
				case EVisitStatus::Error:
					{
						return EVisitResult::Error;
					}
				}
			}
		}
	}
	return EVisitResult::Complete;
}

bool CScriptGraph::GetLinkSrc(const IScriptGraphLink& link, IScriptGraphNode*& pNode, uint32& outputIdx)
{
	pNode = GetNode(link.GetSrcNodeGUID());
	if (pNode)
	{
		outputIdx = pNode->FindOutputById(link.GetSrcOutputId());
		return outputIdx != InvalidIdx;
	}
	return false;
}

bool CScriptGraph::GetLinkSrc(const IScriptGraphLink& link, const IScriptGraphNode*& pNode, uint32& outputIdx) const
{
	pNode = GetNode(link.GetSrcNodeGUID());
	if (pNode)
	{
		outputIdx = pNode->FindOutputById(link.GetSrcOutputId());
		return outputIdx != InvalidIdx;
	}
	return false;
}

bool CScriptGraph::GetLinkDst(const IScriptGraphLink& link, IScriptGraphNode*& pNode, uint32& inputIdx)
{
	pNode = GetNode(link.GetDstNodeGUID());
	if (pNode)
	{
		inputIdx = pNode->FindInputById(link.GetDstInputId());
		return inputIdx != InvalidIdx;
	}
	return false;
}

bool CScriptGraph::GetLinkDst(const IScriptGraphLink& link, const IScriptGraphNode*& pNode, uint32& inputIdx) const
{
	pNode = GetNode(link.GetDstNodeGUID());
	if (pNode)
	{
		inputIdx = pNode->FindInputById(link.GetDstInputId());
		return inputIdx != InvalidIdx;
	}
	return false;
}

void CScriptGraph::RemoveBrokenLinks()
{
	auto isBrokenLink = [this](const CScriptGraphLink& link) -> bool
	{
		Nodes::const_iterator itSrcNode = m_nodes.find(link.GetSrcNodeGUID());
		if ((itSrcNode == m_nodes.end()) || !itSrcNode->second)
		{
			return true;
		}

		if (itSrcNode->second->FindOutputById(link.GetSrcOutputId()) == InvalidIdx)
		{
			return true;
		}

		Nodes::const_iterator itDstNode = m_nodes.find(link.GetDstNodeGUID());
		if ((itDstNode == m_nodes.end()) || !itDstNode->second)
		{
			return true;
		}

		if (itDstNode->second->FindInputById(link.GetDstInputId()) == InvalidIdx)
		{
			return true;
		}

		return false;
	};

	RemoveLinks(isBrokenLink);
}

ScriptGraphLinkRemovedSignal::Slots& CScriptGraph::GetLinkRemovedSignalSlots()
{
	return m_signals.linkRemoved.GetSlots();
}

void CScriptGraph::FixMapping(IScriptGraphNode& node)
{
	for (auto itr = m_nodes.begin(); itr != m_nodes.end(); ++itr)
	{
		if (itr->second.get() == &node)
		{
			IScriptGraphNodePtr pNode = itr->second;
			m_nodes.erase(itr);
			m_nodes.emplace(node.GetGUID(), pNode);
			return;
		}
	}
}

void CScriptGraph::PatchLinks()
{
	for (CScriptGraphLinkPtr& pLink : m_links)
	{
		{
			const CUniqueId srcOutputId = pLink->GetSrcOutputId();
			if (srcOutputId.IsEmpty())
			{
				Nodes::const_iterator itSrcNode = m_nodes.find(pLink->GetSrcNodeGUID());
				if (itSrcNode != m_nodes.end() && itSrcNode->second)
				{
					const CScriptGraphNode& srcNode = static_cast<const CScriptGraphNode&>(*itSrcNode->second);
					const uint32 srcOutputIdx = srcNode.FindOutputByName(pLink->GetSrcOutputName());
					if (srcOutputIdx != InvalidIdx)
					{
						pLink->SetSrcOutputId(srcNode.GetOutputId(srcOutputIdx));
					}
				}
			}
#if SCHEMATYC_PATCH_UNIQUE_IDS
			else if (srcOutputId.IsIdx())
			{
				Nodes::const_iterator itSrcNode = m_nodes.find(pLink->GetSrcNodeGUID());
				if (itSrcNode != m_nodes.end() && itSrcNode->second)
				{
					const uint32 srcOutputIdx = srcOutputId.GetIdx();
					const CUniqueId trueOutputId = itSrcNode->second->GetOutputId(srcOutputIdx);
					if (srcOutputId != trueOutputId)
					{
						pLink->SetSrcOutputId(trueOutputId);
					}
				}
			}
#endif
		}

		{
			const CUniqueId dstInputId = pLink->GetDstInputId();
			if (dstInputId.IsEmpty())
			{
				Nodes::const_iterator itDstNode = m_nodes.find(pLink->GetDstNodeGUID());
				if (itDstNode != m_nodes.end())
				{
					const CScriptGraphNode& dstNode = static_cast<const CScriptGraphNode&>(*itDstNode->second);
					const uint32 dstInputIdx = dstNode.FindInputByName(pLink->GetDstInputName());
					if (dstInputIdx != InvalidIdx)
					{
						pLink->SetDstInputId(dstNode.GetInputId(dstInputIdx));
					}
				}
			}
#if SCHEMATYC_PATCH_UNIQUE_IDS
			else if (dstInputId.IsIdx())
			{
				Nodes::const_iterator itDstNode = m_nodes.find(pLink->GetDstNodeGUID());
				if (itDstNode != m_nodes.end())
				{
					const uint32 dstInputIdx = dstInputId.GetIdx();
					const CUniqueId trueInputId = itDstNode->second->GetInputId(dstInputIdx);
					if (dstInputId != trueInputId)
					{
						pLink->SetDstInputId(trueInputId);
					}
				}
			}
#endif
		}
	}
}

} // Schematyc

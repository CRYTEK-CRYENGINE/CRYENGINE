// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/IScriptGraph.h>

#include "Script/ScriptElementBase.h"

namespace Schematyc
{

class CScriptGraphLink : public IScriptGraphLink
{
public:

	CScriptGraphLink();
	CScriptGraphLink(const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId);
	CScriptGraphLink(const CryGUID& srcNodeGUID, const char* szSrcOutputName, const CryGUID& dstNodeGUID, const char* szDstInputName);

	// IScriptGraphLink
	virtual void      SetSrcNodeGUID(const CryGUID& guid) override;
	virtual CryGUID   GetSrcNodeGUID() const override;
	virtual CUniqueId GetSrcOutputId() const override;
	virtual void      SetDstNodeGUID(const CryGUID& guid) override;
	virtual CryGUID   GetDstNodeGUID() const override;
	virtual CUniqueId GetDstInputId() const override;
	virtual void      Serialize(Serialization::IArchive& archive) override;
	// ~IScriptGraphLink

	void        SetSrcOutputId(const CUniqueId& id);
	const char* GetSrcOutputName() const;
	void        SetDstInputId(const CUniqueId& id);
	const char* GetDstInputName() const;

private:

	CryGUID   m_srcNodeGUID;
	CUniqueId m_srcOutputId;
	string    m_srcOutputName;
	CryGUID   m_dstNodeGUID;
	CUniqueId m_dstInputId;
	string    m_dstInputName;
};

DECLARE_SHARED_POINTERS(CScriptGraphLink)

class CScriptGraph : public IScriptGraph
{
private:

	typedef std::map<CryGUID, IScriptGraphNodePtr> Nodes;
	typedef std::vector<CScriptGraphLinkPtr>       Links;

	struct SSignals
	{
		ScriptGraphLinkRemovedSignal linkRemoved;
	};

public:

	CScriptGraph(IScriptElement& element, EScriptGraphType type);

	// IScriptExtension
	virtual void EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override;
	virtual void RemapDependencies(IGUIDRemapper& guidRemapper) override;
	virtual void ProcessEvent(const SScriptEvent& event) override;
	virtual void Serialize(Serialization::IArchive& archive) override;
	// ~IScriptExtension

	// IScriptGraph
	virtual EScriptGraphType                     GetType() const override;
	virtual IScriptElement&                      GetElement() override;
	virtual const IScriptElement&                GetElement() const override;

	virtual void                                 SetPos(Vec2 pos) override;
	virtual Vec2                                 GetPos() const override;

	virtual void                                 PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu) override;
	virtual bool                                 AddNode(const IScriptGraphNodePtr& pNode) override;
	virtual IScriptGraphNodePtr                  AddNode(const CryGUID& typeGUID) override;
	virtual void                                 RemoveNode(const CryGUID& guid) override;
	virtual uint32                               GetNodeCount() const override;
	virtual IScriptGraphNode*                    GetNode(const CryGUID& guid) override;
	virtual const IScriptGraphNode*              GetNode(const CryGUID& guid) const override;
	virtual void                                 VisitNodes(const ScriptGraphNodeVisitor& visitor) override;
	virtual void                                 VisitNodes(const ScriptGraphNodeConstVisitor& visitor) const override;

	virtual bool                                 CanAddLink(const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId) const override;
	virtual IScriptGraphLink*                    AddLink(const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId) override;
	virtual void                                 RemoveLink(uint32 linkIdx) override;
	virtual void                                 RemoveLinks(const CryGUID& nodeGUID) override;
	virtual uint32                               GetLinkCount() const override;
	virtual IScriptGraphLink*                    GetLink(uint32 linkIdx) override;
	virtual const IScriptGraphLink*              GetLink(uint32 linkIdx) const override;
	virtual uint32                               FindLink(const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId) const override;
	virtual EVisitResult                         VisitLinks(const ScriptGraphLinkVisitor& visitor) override;
	virtual EVisitResult                         VisitLinks(const ScriptGraphLinkConstVisitor& visitor) const override;
	virtual EVisitResult                         VisitInputLinks(const ScriptGraphLinkVisitor& visitor, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId) override;
	virtual EVisitResult                         VisitInputLinks(const ScriptGraphLinkConstVisitor& visitor, const CryGUID& dstNodeGUID, const CUniqueId& dstInputId) const override;
	virtual EVisitResult                         VisitOutputLinks(const ScriptGraphLinkVisitor& visitor, const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId) override;
	virtual EVisitResult                         VisitOutputLinks(const ScriptGraphLinkConstVisitor& visitor, const CryGUID& srcNodeGUID, const CUniqueId& srcOutputId) const override;
	virtual bool                                 GetLinkSrc(const IScriptGraphLink& link, IScriptGraphNode*& pNode, uint32& outputIdx) override;
	virtual bool                                 GetLinkSrc(const IScriptGraphLink& link, const IScriptGraphNode*& pNode, uint32& outputIdx) const override;
	virtual bool                                 GetLinkDst(const IScriptGraphLink& link, IScriptGraphNode*& pNode, uint32& inputIdx) override;
	virtual bool                                 GetLinkDst(const IScriptGraphLink& link, const IScriptGraphNode*& pNode, uint32& inputIdx) const override;

	virtual void                                 RemoveBrokenLinks() override;
	virtual ScriptGraphLinkRemovedSignal::Slots& GetLinkRemovedSignalSlots() override;

	virtual void                                 FixMapping(IScriptGraphNode& node) override;
	// ~IScriptGraph

private:

	template<typename PREDICATE> inline void RemoveLinks(PREDICATE predicate)
	{
		for (uint32 linkIdx = 0, linkCount = m_links.size(); linkIdx < linkCount; )
		{
			const CScriptGraphLink& link = *m_links[linkIdx];
			if (predicate(link))
			{
				m_signals.linkRemoved.Send(link);
				m_links.erase(m_links.begin() + linkIdx);
				--linkCount;
			}
			else
			{
				++linkIdx;
			}
		}
	}

	void PatchLinks();

private:

	IScriptElement&  m_element;
	EScriptGraphType m_type;
	Vec2             m_pos;
	Nodes            m_nodes;
	Links            m_links;
	SSignals         m_signals;
};

DECLARE_SHARED_POINTERS(CScriptGraph)

} // Schematyc

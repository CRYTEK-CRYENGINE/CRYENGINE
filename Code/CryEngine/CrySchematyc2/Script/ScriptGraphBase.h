// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptGraph.h>

#include "Deprecated/DocGraphBase.h"
#include "Script/ScriptElementBase.h"
#include "Script/ScriptVariableDeclaration.h"

namespace Schematyc2
{
	class CScriptGraphBase : public IScriptGraphExtension
	{
	public:

		CScriptGraphBase(IScriptElement& element);

		// IScriptExtension
		virtual EScriptExtensionId GetId_New() const override;
		virtual void Refresh_New(const SScriptRefreshParams& params) override;
		virtual void Serialize_New(Serialization::IArchive& archive) override;
		virtual void RemapGUIDs_New(IGUIDRemapper& guidRemapper) override;
		// ~IScriptExtension

		// IScriptGraph
		virtual SGUID GetContextGUID() const override;
		virtual IScriptElement& GetElement_New() override;
		virtual const IScriptElement& GetElement_New() const override;
		virtual void SetPos(Vec2 pos) override;
		virtual Vec2 GetPos() const override;
		virtual size_t GetInputCount() const override;
		virtual const char* GetInputName(size_t inputIdx) const override;
		virtual IAnyConstPtr GetInputValue(size_t inputIdx) const override;
		virtual size_t GetOutputCount() const override;
		virtual const char* GetOutputName(size_t outputIdx) const override;
		virtual IAnyConstPtr GetOutputValue(size_t outputIdx) const override;
		virtual void RefreshAvailableNodes(const CAggregateTypeId& inputTypeId) override;
		virtual size_t GetAvailableNodeCount() override;
		virtual const char* GetAvailableNodeName(size_t availableNodeIdx) const override;
		virtual EScriptGraphNodeType GetAvailableNodeType(size_t availableNodeIdx) const override;
		virtual SGUID GetAvailableNodeContextGUID(size_t availableNodeIdx) const override;
		virtual SGUID GetAvailableNodeRefGUID(size_t availableNodeIdx) const override;
		virtual IScriptGraphNode* AddNode(EScriptGraphNodeType type, const SGUID& contextGUID = SGUID(), const SGUID& refGUID = SGUID(), Vec2 pos = Vec2()) override;
		virtual void RemoveNode(const SGUID& guid) override;
		virtual IScriptGraphNode* GetNode(const SGUID& guid) override;
		virtual const IScriptGraphNode* GetNode(const SGUID& guid) const override;
		virtual void VisitNodes(const ScriptGraphNodeVisitor& visitor) override;
		virtual void VisitNodes(const ScriptGraphNodeConstVisitor& visitor) const override;
		virtual bool CanAddLink(const SGUID& srcNodeGUID, const char* szScOutputName, const SGUID& dstNodeGUID, const char* szDstInputName) const override;
		virtual IScriptGraphLink* AddLink(const SGUID& srcNodeGUID, const char* szScOutputName, const SGUID& dstNodeGUID, const char* szDstInputName) override;
		virtual void RemoveLink(size_t linkIdx) override;
		virtual void RemoveLinks(const SGUID& nodeGUID) override;
		virtual size_t GetLinkCount() const override;
		virtual size_t FindLink(const SGUID& srcGUID, const char* szSrcOutputName, const SGUID& dstNodeGUID, const char* szDstInputName) const override;
		virtual IScriptGraphLink* GetLink(size_t linkIdx) override;
		virtual const IScriptGraphLink* GetLink(size_t linkIdx) const override;
		virtual void VisitLinks(const ScriptGraphLinkVisitor& visitor) override;
		virtual void VisitLinks(const ScriptGraphLinkConstVisitor& visitor) const override;
		virtual void RemoveBrokenLinks() override;
		virtual SScriptGraphSignals& Signals() override;
		// ~IScriptGraph

		// IScriptGraphExtension
		virtual uint32 GetNodeCount() const override;
		virtual bool AddNode_New(const IScriptGraphNodePtr& pNode) override;
		virtual void PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu) override;
		virtual void VisitInputLinks(const ScriptGraphLinkVisitor& visitor, const SGUID& dstNodeGUID, const char* szDstInputName) override;
		virtual void VisitInputLinks(const ScriptGraphLinkConstVisitor& visitor, const SGUID& dstNodeGUID, const char* szDstInputName) const override;
		virtual void VisitOutputLinks(const ScriptGraphLinkVisitor& visitor, const SGUID& srcNodeGUID, const char* szSrcOutputName) override;
		virtual void VisitOutputLinks(const ScriptGraphLinkConstVisitor& visitor, const SGUID& srcNodeGUID, const char* szSrcOutputName) const override;
		virtual bool GetLinkSrc(const IScriptGraphLink& link, IScriptGraphNode*& pNode, uint32& outputIdx) override;
		virtual bool GetLinkSrc(const IScriptGraphLink& link, const IScriptGraphNode*& pNode, uint32& outputIdx) const override;
		virtual bool GetLinkDst(const IScriptGraphLink& link, IScriptGraphNode*& pNode, uint32& inputIdx) override;
		virtual bool GetLinkDst(const IScriptGraphLink& link, const IScriptGraphNode*& pNode, uint32& inputIdx) const override;
		// ~IScriptGraphExtension

	private:

		typedef std::map<SGUID, IScriptGraphNodePtr> Nodes;
		typedef std::vector<CScriptGraphLinkPtr>     Links;

		IScriptElement&     m_element;
		Vec2                m_pos;
		Nodes               m_nodes;
		Links               m_links;
		SScriptGraphSignals m_signals;
	};
}

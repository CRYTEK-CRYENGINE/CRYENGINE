// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph/AbstractNodeGraphViewModel.h>

#include <NodeGraph/AbstractPinItem.h>
#include "NodeGraph/AbstractNodeItem.h"
#include "NodeGraph/AbstractConnectionItem.h"

#include "Pins/SubstanceInPinItem.h"
#include "Pins/SubstanceOutPinItem.h"
#include "Nodes/SubstanceOutputNodeBase.h"

#include "SubstanceCommon.h"


namespace EditorSubstance
{
	namespace OutputEditor
	{

		class CSubstanceOutputNodeBase;
		class CVirtualOutputNode;
		class CSubstanceConnectionItem;

		class CGraphViewModel : public CryGraphEditor::CNodeGraphViewModel
		{
			Q_OBJECT
				typedef std::unordered_map<uint32 /* name crc */, CSubstanceOutputNodeBase*> NodeItemByNameCrc;
		public:
			CGraphViewModel(const std::vector<SSubstanceOutput>& originalOutputs, const std::vector<SSubstanceOutput>& customOutputs, bool showPreviews = true);

			virtual CryGraphEditor::INodeGraphRuntimeContext& GetRuntimeContext() override;
			virtual QString                                   GetGraphName() override;
			virtual uint32                                    GetNodeItemCount() const override;
			virtual CryGraphEditor::CAbstractNodeItem*        GetNodeItemByIndex(uint32 index) const override;
			virtual CryGraphEditor::CAbstractNodeItem*        GetNodeItemById(QVariant id) const override;
			virtual CryGraphEditor::CAbstractNodeItem*        CreateNode(QVariant identifier, const QPointF& position = QPointF()) override;
			virtual bool                                      RemoveNode(CryGraphEditor::CAbstractNodeItem& node) override;

			virtual uint32                                    GetConnectionItemCount() const override;
			virtual CryGraphEditor::CAbstractConnectionItem*  GetConnectionItemByIndex(uint32 index) const override;
			virtual CryGraphEditor::CAbstractConnectionItem*  GetConnectionItemById(QVariant id) const override;
			virtual CryGraphEditor::CAbstractConnectionItem*  CreateConnection(CryGraphEditor::CAbstractPinItem& sourcePin, CryGraphEditor::CAbstractPinItem& targetPin) override;
			virtual bool                                      RemoveConnection(CryGraphEditor::CAbstractConnectionItem& connection) override;

			CSubstanceOutputNodeBase*                       CreateNode(ESubstanceGraphNodeType nodeType, const QPointF& position);
			CSubstanceConnectionItem*                 CreateConnection(CSubstanceOutPinItem& sourcePin, CSubstanceInPinItem& targetPin);
			CSubstanceOutputNodeBase*                       GetNodeItemById(string identifier) const;
			void                             UpdateNodeCrc(const char* szOldName, const char* szNewName, const ESubstanceGraphNodeType& nodeType);
			std::vector<SSubstanceOutput*> GetOutputs() const;
			bool GetShowPreviews() { return m_showPreviews; }
			bool IsNameUnique(const string& name, const ESubstanceGraphNodeType& nodeType);
		Q_SIGNALS:
			void SignalOutputsChanged();

		private:
			static const uint32 GetNodeCRC(const string& name, const ESubstanceGraphNodeType& nodeType);
			static const uint32 GetNodeCRC(const string& name);
			NodeItemByNameCrc             m_nodeItemByNameCrc;
			std::unique_ptr<CryGraphEditor::INodeGraphRuntimeContext>		      m_pRuntimeContext;
			std::vector<CSubstanceOutputNodeBase*>       m_nodes;
			std::vector<CryGraphEditor::CAbstractConnectionItem*> m_connections;
			bool m_showPreviews;
		};
	}
}

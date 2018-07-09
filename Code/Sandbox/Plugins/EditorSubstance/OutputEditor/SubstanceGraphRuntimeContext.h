// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Controls/DictionaryWidget.h>
#include "NodeGraph/ICryGraphEditor.h"

namespace EditorSubstance
{
	namespace OutputEditor
	{


		class CNodesDictionaryNode : public CAbstractDictionaryEntry
		{
		public:
			CNodesDictionaryNode(QString name, QString identifier)
				: CAbstractDictionaryEntry()
				, m_name(name)
				, m_identifier(identifier)
			{}
			virtual ~CNodesDictionaryNode() {}

			// CAbstractDictionaryEntry
			virtual uint32                          GetType() const override { return Type_Entry; }
			virtual QVariant                        GetColumnValue(int32 columnIndex) const override;
			virtual QVariant                        GetIdentifier() const override { return QVariant::fromValue(m_identifier); }
			// ~CAbstractDictionaryEntry

			const QString& GetName() const { return m_name; }

		private:
			QString                   m_name;
			QString                   m_identifier;
		};


		class CVirtualOutputsNodesDictionary : public CAbstractDictionary
		{
		public:
			enum EColumn : int32
			{
				eColumn_Name,
				eColumn_Identifier,

				eColumn_COUNT
			};

		public:
			CVirtualOutputsNodesDictionary();
			virtual ~CVirtualOutputsNodesDictionary();

			// CryGraphEditor::CAbstractDictionary
			virtual int32                           GetNumEntries() const override { return m_nodes.size(); }
			virtual const CAbstractDictionaryEntry* GetEntry(int32 index) const override;

			virtual int32                           GetNumColumns() const { return 1; }
			// ~CryGraphEditor::CAbstractDictionary

		private:
			std::vector<CNodesDictionaryNode*>     m_nodes;
		};


		class CSubstanceOutputsGraphRuntimeContext : public CryGraphEditor::INodeGraphRuntimeContext
		{
		public:
			CSubstanceOutputsGraphRuntimeContext();
			~CSubstanceOutputsGraphRuntimeContext();

			// CryGraphEditor::INodeGraphRuntimeContext
			virtual const char*                                GetTypeName() const override { return "SubstanceOutputs"; }
			virtual CAbstractDictionary*                       GetAvailableNodesDictionary() override { return &m_nodeTreeModel; }
			virtual const CryGraphEditor::CNodeGraphViewStyle* GetStyle() const { return m_pStyle; }
			// ~CryGraphEditor::INodeGraphRuntimeContext

		private:
			CVirtualOutputsNodesDictionary                   m_nodeTreeModel;
			CryGraphEditor::CNodeGraphViewStyle* m_pStyle;
		};


	}
}

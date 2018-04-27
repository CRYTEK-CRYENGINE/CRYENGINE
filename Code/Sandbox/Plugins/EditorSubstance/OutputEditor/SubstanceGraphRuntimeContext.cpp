// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubstanceGraphRuntimeContext.h"

#include "NodeGraph/NodeWidgetStyle.h"
#include "NodeGraph/NodeHeaderWidgetStyle.h"
#include "NodeGraph/NodeGraphViewStyle.h"
#include "NodeGraph/ConnectionWidgetStyle.h"
#include "NodeGraph/NodePinWidgetStyle.h"

namespace EditorSubstance
{
	namespace OutputEditor
	{

		CVirtualOutputsNodesDictionary::CVirtualOutputsNodesDictionary()
		{
			m_nodes.emplace_back(new CNodesDictionaryNode("Texture Output", "VirtualOutput"));
			m_nodes.emplace_back(new CNodesDictionaryNode("Substance Graph Output", "OriginalOutput"));
		}

		CVirtualOutputsNodesDictionary::~CVirtualOutputsNodesDictionary()
		{

		}

		const CAbstractDictionaryEntry* CVirtualOutputsNodesDictionary::GetEntry(int32 index) const
		{
			return m_nodes[index];
		}

		QVariant CNodesDictionaryNode::GetColumnValue(int32 columnIndex) const
		{
			switch (columnIndex)
			{
			case CVirtualOutputsNodesDictionary::eColumn_Name:
			{
				return QVariant::fromValue(m_name);
			}
			case CVirtualOutputsNodesDictionary::eColumn_Identifier:
			{
				return GetIdentifier();
			}
			default:
				break;
			}

			return QVariant();
		}


		void AddNodeStyle(CryGraphEditor::CNodeGraphViewStyle& viewStyle, const char* szStyleId, const char* szIcon, QColor color)
		{
			CryGraphEditor::CNodeWidgetStyle* pStyle = new CryGraphEditor::CNodeWidgetStyle(szStyleId, viewStyle);

			CryGraphEditor::CNodeHeaderWidgetStyle& headerStyle = pStyle->GetHeaderWidgetStyle();

			headerStyle.SetNodeIconMenuColor(color);


			headerStyle.SetNameColor(color);
			headerStyle.SetLeftColor(QColor(26, 26, 26));
			headerStyle.SetRightColor(QColor(26, 26, 26));
			headerStyle.SetNodeIconViewDefaultColor(color);

			CryIcon icon(szIcon, {
				{ QIcon::Mode::Normal, QColor(255, 255, 255) }
			});
			headerStyle.SetNodeIcon(icon);
		}

		void AddConnectionStyle(CryGraphEditor::CNodeGraphViewStyle& viewStyle, const char* szStyleId, float width)
		{
			CryGraphEditor::CConnectionWidgetStyle* pStyle = new CryGraphEditor::CConnectionWidgetStyle(szStyleId, viewStyle);
			pStyle->SetWidth(width);
		}

		void AddPinStyle(CryGraphEditor::CNodeGraphViewStyle& viewStyle, const char* szStyleId, const char* szIcon, QColor color)
		{
			CryIcon icon(szIcon, {
				{ QIcon::Mode::Normal, color }
			});

			CryGraphEditor::CNodePinWidgetStyle* pStyle = new CryGraphEditor::CNodePinWidgetStyle(szStyleId, viewStyle);
			pStyle->SetIcon(icon);
			pStyle->SetColor(color);
		}


		CSubstanceOutputsGraphRuntimeContext::CSubstanceOutputsGraphRuntimeContext()
		{
			CryGraphEditor::CNodeGraphViewStyle* pViewStyle = new CryGraphEditor::CNodeGraphViewStyle("Substance");

			AddNodeStyle(*pViewStyle, "Node::Output", "icons:MaterialEditor/Material_Editor.ico", QColor(255, 0, 0));
			AddNodeStyle(*pViewStyle, "Node::Input", "icons:ObjectTypes/node_track_expression.ico", QColor(0, 255, 0));

			AddConnectionStyle(*pViewStyle, "Connection::Substance", 2.0);

			AddPinStyle(*pViewStyle, "Pin::Substance", "icons:Graph/Node_connection_arrow_R.ico", QColor(200, 200, 200));
			m_pStyle = pViewStyle;
		}

		CSubstanceOutputsGraphRuntimeContext::~CSubstanceOutputsGraphRuntimeContext()
		{
			m_pStyle->deleteLater();
		}



	}
}

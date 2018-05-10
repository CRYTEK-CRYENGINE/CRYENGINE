// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "SubstanceVirtualOutputNode.h"
#include "OutputEditor/Items/SubstanceConnectionItem.h"
#include "OutputEditor/Pins/SubstanceBasePinItem.h"
#include "OutputEditor/Pins/SubstanceInPinItem.h"
#include "OutputEditor/Pins/SubstanceOutPinItem.h"
#include "OutputEditor/Nodes/SubstanceOriginalOutputNode.h"
#include "OutputEditor/SubstanceNodeContentWidget.h"
#include "OutputEditor/GraphViewModel.h"

#include "NodeGraph/NodeWidget.h"
#include "CrySerialization/StringList.h"

#include "SubstanceCommon.h"
#include "EditorSubstanceManager.h"

namespace EditorSubstance
{
	namespace OutputEditor
	{


		CVirtualOutputNode::CVirtualOutputNode(const SSubstanceOutput& output, CryGraphEditor::CNodeGraphViewModel& viewModel)
			: CSubstanceOutputNodeBase(output, viewModel)
		{
			m_outputType = CSubstanceNodeContentWidget::Virtual;
			SetAcceptsRenaming(true);
			for each (auto var in pinNameMap)
			{
				m_pins.push_back(new CSubstanceInPinItem(*this, var.first));
			}
			SetDeactivated(!m_pOutput.enabled);
			SignalDeactivatedChanged.Connect([=](bool deactivated)
			{
				m_pOutput.enabled = !deactivated;
			}
			);
		}

		void CVirtualOutputNode::HandleIndividualConnection(int pinIndex)
		{
			ConnectionInfo connectionInfo = GetPinConnectionInfo(pinIndex);
			string outputName(connectionInfo.node->NameAsString());
			int sourceId = 0;
			auto it = std::find(m_pOutput.sourceOutputs.begin(), m_pOutput.sourceOutputs.end(), outputName);
			if (it == m_pOutput.sourceOutputs.end())
			{
				sourceId = m_pOutput.sourceOutputs.size();
				m_pOutput.sourceOutputs.push_back(outputName);
			}
			else
			{
				sourceId = std::distance(m_pOutput.sourceOutputs.begin(), it);
			}

			int srcPinIndex = connectionInfo.node->GetPinItemIndex(*connectionInfo.pin);
			if (srcPinIndex >= PIN_INDEX_R)
			{
				m_pOutput.channels[pinIndex - PIN_INDEX_R].sourceId = sourceId;
				m_pOutput.channels[pinIndex - PIN_INDEX_R].channel = srcPinIndex == pinIndex ? -1 : srcPinIndex - PIN_INDEX_R;
			}
		}


		ConnectionInfo CVirtualOutputNode::GetPinConnectionInfo(int index)
		{
			CSubstanceConnectionItem* conitem = static_cast<CSubstanceConnectionItem*>(*(m_pins[index]->GetConnectionItems().begin()));
			ConnectionInfo conInfo;
			conInfo.node = &static_cast<COriginalOutputNode&>(conitem->GetSourcePinItem().GetNodeItem());
			conInfo.pin = &static_cast<CSubstanceOutPinItem&>(conitem->GetSourcePinItem());
			return conInfo;
		}

		void CVirtualOutputNode::Serialize(Serialization::IArchive& ar)
		{
			if (ar.isEdit())
			{
				Serialization::StringList presets;
				for each (string var in CManager::Instance()->GetTexturePresetsForFile(""))
				{
					presets.push_back(var);
				}

				Serialization::StringList resolutions;
				string texToSelect("");
				for each (auto& var in resolutionNamesMap)
				{
					if (var.second == m_pOutput.resolution)
						texToSelect = var.first;
					resolutions.push_back(var.first);
				}
				Serialization::StringListValue dropDown(presets, m_pOutput.preset);
				Serialization::StringListValue dropDownRes(resolutions, texToSelect);
				ar(dropDown, "preset", "Preset");
				ar(dropDownRes, "resolution", "Resolution");
				ar(m_pOutput.enabled, "enabled", "Enabled");
				SetDeactivated(!m_pOutput.enabled);
				if (ar.isInput())
				{
					m_pOutput.preset = dropDown.c_str();
					string newRes(dropDownRes.c_str());
					for each (auto& var in resolutionNamesMap)
					{
						if (newRes == var.first)
						{
							m_pOutput.resolution = var.second;
							break;
						}
					}
					if (m_pNodeContentWidget)
						m_pNodeContentWidget->Update();
					EditorSubstance::OutputEditor::CGraphViewModel* model = static_cast<EditorSubstance::OutputEditor::CGraphViewModel*>(&GetViewModel());
					model->SignalOutputsChanged();
				}
				return;
			}
		}

		void CVirtualOutputNode::UpdatePinState()
		{

			for each (auto var in m_pins)
			{
				var->SetDeactivated(false);
			}

			bool individualConnected = m_pins[PIN_INDEX_R]->IsConnected() || m_pins[PIN_INDEX_G]->IsConnected() || m_pins[PIN_INDEX_B]->IsConnected();
			if (individualConnected)
			{
				m_pins[PIN_INDEX_RGBA]->SetDeactivated(true);
				m_pins[PIN_INDEX_RGB]->SetDeactivated(true);
			}
			else if (m_pins[PIN_INDEX_RGBA]->IsConnected())
			{
				m_pins[PIN_INDEX_RGB]->SetDeactivated(true);
				m_pins[PIN_INDEX_R]->SetDeactivated(true);
				m_pins[PIN_INDEX_G]->SetDeactivated(true);
				m_pins[PIN_INDEX_B]->SetDeactivated(true);
				m_pins[PIN_INDEX_A]->SetDeactivated(true);
			}
			else if (m_pins[PIN_INDEX_RGB]->IsConnected())
			{
				m_pins[PIN_INDEX_RGBA]->SetDeactivated(true);
				m_pins[PIN_INDEX_R]->SetDeactivated(true);
				m_pins[PIN_INDEX_G]->SetDeactivated(true);
				m_pins[PIN_INDEX_B]->SetDeactivated(true);
			}
			else if (m_pins[PIN_INDEX_A]->IsConnected())
			{
				m_pins[PIN_INDEX_RGBA]->SetDeactivated(true);
			}
		}

		void CVirtualOutputNode::PropagateNetworkToOutput()
		{
			m_pOutput.SetAllChannels(-1);
			m_pOutput.SetAllSources(-1);
			m_pOutput.sourceOutputs.clear();
			if (m_pins[PIN_INDEX_RGBA]->IsConnected())
			{
				ConnectionInfo connectionInfo = GetPinConnectionInfo(PIN_INDEX_RGBA);
				m_pOutput.sourceOutputs.push_back(connectionInfo.node->NameAsString());
				for (size_t i = 0; i < 4; i++)
				{
					m_pOutput.channels[i].sourceId = 0;
					m_pOutput.channels[i].channel = -1;
				}
			}
			else if (m_pins[PIN_INDEX_RGB]->IsConnected())
			{
				ConnectionInfo connectionInfo = GetPinConnectionInfo(PIN_INDEX_RGB);
				m_pOutput.sourceOutputs.push_back(connectionInfo.node->NameAsString());
				for (size_t i = 0; i < 3; i++)
				{
					m_pOutput.channels[i].sourceId = 0;
					m_pOutput.channels[i].channel = -1;
				}
				if (m_pins[PIN_INDEX_A]->IsConnected())
				{
					HandleIndividualConnection(PIN_INDEX_A);
				}
			}
			else {
				for (size_t i = PIN_INDEX_R; i <= PIN_INDEX_A; i++)
				{
					if (m_pins[i]->IsConnected())
					{
						HandleIndividualConnection(i);
					}
				}
			}
		}
	}
}

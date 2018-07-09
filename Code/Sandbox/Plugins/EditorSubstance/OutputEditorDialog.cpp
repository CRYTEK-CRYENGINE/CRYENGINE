// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OutputEditorDialog.h"
#include "OutputEditor/EditorWidget.h"
#include "OutputEditor/GraphViewModel.h"
#include "OutputEditor/Nodes/SubstanceOutputNodeBase.h"
#include "IEditor.h"

#include <QVboxLayout>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <AssetSystem/Asset.h>
#include "EditorSubstanceManager.h"
#include "Renderers/Preview.h"
#include "EditorFramework/Inspector.h"
#include "EditorFramework/Events.h"
#include "EditorFramework/BroadcastManager.h"

namespace EditorSubstance
{


		CSubstanceOutputEditorDialog::CSubstanceOutputEditorDialog(ISubstancePreset* preset, QWidget* pParent)
			: CEditorDialog("Modify Outputs", pParent)
			, m_pBroadcastManager(new CBroadcastManager())
			, m_pPreset(preset)
		{
	

			QVBoxLayout* pMainLayout = new QVBoxLayout(this);
			pMainLayout->setContentsMargins(0, 0, 0, 0);
			m_editorWidget = new OutputEditor::CSubstanceOutputEditorWidget(preset->GetDefaultOutputs(), preset->GetOutputs(), true, this);
			pMainLayout->addWidget(m_editorWidget);
	
			
			SetTitle("Create Substance Graph Preset");
			QDialogButtonBox* pButtons = new QDialogButtonBox();
			pButtons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
			connect(pButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
			connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

			pMainLayout->addWidget(pButtons, Qt::AlignRight);

			setLayout(pMainLayout);		
			QObject::connect(m_editorWidget->GetGraphViewModel(), &OutputEditor::CGraphViewModel::SignalOutputsChanged, this, [=] { UpdatePreviews(); });
		}



		CSubstanceOutputEditorDialog::~CSubstanceOutputEditorDialog()
		{
			EditorSubstance::CManager::Instance()->PresetEditEnded(m_pPreset);
		}

		void CSubstanceOutputEditorDialog::LoadUpdatedOutputSettings(std::vector<SSubstanceOutput>& outputs)
		{
			m_editorWidget->LoadUpdatedOutputSettings(outputs);
		}

		void CSubstanceOutputEditorDialog::customEvent(QEvent *e)
		{
			if (e->type() == SandboxEvent::GetBroadcastManager)
			{
				static_cast<GetBroadcastManagerEvent*>(e)->SetManager(m_pBroadcastManager);
				e->accept();
			}
			else
			{
				CEditorDialog::customEvent(e);
			}
		}

		void CSubstanceOutputEditorDialog::OnPreviewUpdated(SubstanceAir::RenderResult* result, EditorSubstance::Renderers::SPreviewGeneratedOutputData* data)
		{
			if (data->presetInstanceID == m_pPreset->GetInstanceID())
			{
				auto node = m_editorWidget->GetGraphViewModel()->GetNodeItemById(data->outputName + OutputEditor::CSubstanceOutputNodeBase::GetIdSuffix(data->isVirtual ? OutputEditor::eInput : OutputEditor::eOutput));
				if (node)
				{
					const SubstanceTexture& texture = result->getTexture();
					std::shared_ptr<QImage> qImage = std::make_shared<QImage>((const uchar *)result->getTexture().buffer, result->getTexture().level0Width, result->getTexture().level0Height, QImage::Format_RGBA8888);
					static_cast<OutputEditor::CSubstanceOutputNodeBase*>(node)->SetPreviewImage(qImage);

				}
			}
		}

		void CSubstanceOutputEditorDialog::UpdatePreviews()
		{
			LoadUpdatedOutputSettings(m_pPreset->GetOutputs());
			CManager::Instance()->EnquePresetRender(m_pPreset, CManager::Instance()->GetPreviewRenderer());
			for (int i = 0; i < m_editorWidget->GetGraphViewModel()->GetNodeItemCount(); i++)
			{
				OutputEditor::CSubstanceOutputNodeBase* node = static_cast<OutputEditor::CSubstanceOutputNodeBase*>(m_editorWidget->GetGraphViewModel()->GetNodeItemByIndex(i));
				if (node->GetNodeType() == OutputEditor::eInput)
				{
					bool hasConnections = false;
					for (CryGraphEditor::CAbstractPinItem* pPin : node->GetPinItems())
					{
						const CryGraphEditor::ConnectionItemSet& connections = pPin->GetConnectionItems();
						if (connections.size() > 0)
						{
							hasConnections = true;
							break;
						}
						
					}
					if (!hasConnections)
					{
						node->ResetPreviewImage();
					}
			

				}
			}
		}

		void CSubstanceOutputEditorDialog::keyPressEvent(QKeyEvent * e)
		{
			QApplication::sendEvent(parent(), e);
			//QWidget::keyPressEvent(e);
		}

		void CSubstanceOutputEditorDialog::showEvent(QShowEvent* event)
		{
			static_cast<EditorSubstance::Renderers::CPreviewRenderer*>(CManager::Instance()->GetPreviewRenderer())->outputComputed.Connect(this, &CSubstanceOutputEditorDialog::OnPreviewUpdated);
			CManager::Instance()->EnquePresetRender(m_pPreset, CManager::Instance()->GetPreviewRenderer());
			CEditorDialog::showEvent(event);
		}

		void CSubstanceOutputEditorDialog::hideEvent(QHideEvent *e)
		{
			static_cast<EditorSubstance::Renderers::CPreviewRenderer*>(CManager::Instance()->GetPreviewRenderer())->outputComputed.DisconnectObject(this);
			CEditorDialog::hideEvent(e);
		}


}

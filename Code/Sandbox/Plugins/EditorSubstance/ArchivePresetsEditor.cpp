// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubstanceCommon.h"
#include "SandboxPlugin.h"
#include "ArchivePresetsEditor.h"

#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QString>

#include "FileDialogs/FileNameLineEdit.h"
#include "FilePathUtil.h"
#include "AssetSystem/Asset.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/Browser/AssetFoldersView.h"
#include "OutputsWidget.h"
#include "QAdvancedTreeView.h"
#include "QScrollableBox.h"
#include "Controls/QuestionDialog.h"
#include "OutputEditorDialog.h"
//#include "OutputEditor\GraphViewModel.h"
#include "AssetTypes\SubstanceArchive.h"
#include "QCollapsibleFrame.h"
#include "EditorSubstanceManager.h"



namespace EditorSubstance
{
	REGISTER_VIEWPANE_FACTORY_AND_MENU(CArchivePresetsEditor, "Substance Archive Graph Editor", "Substance", false, "Substance")

	CArchivePresetsEditor::CArchivePresetsEditor(QWidget* pParent/* = nullptr*/)
		: CAssetEditor("SubstanceDefinition")
		, m_pOutputsGraphEditor(nullptr)
	{

		m_pScrollBox = new QScrollableBox(this);

		m_pOutputsWidgetHolder = new QWidget(this);
		m_pOutputsWidgetHolder->setLayout(new QVBoxLayout());
		m_pButtons = new QDialogButtonBox();
		m_pButtons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		m_pModalGuard = new QFrame(this);
		m_pModalGuard->resize(size());
		m_pModalGuard->hide();


		auto* const pMainLayout = new QVBoxLayout();
		m_pScrollBox->addWidget(m_pOutputsWidgetHolder);

		SetContent(m_pScrollBox);

	}
	
	void CArchivePresetsEditor::OnEditOutputs(const string& graphName)
	{
		if (m_pOutputsGraphEditor)
		{
			m_pOutputsGraphEditor->raise();
			m_pOutputsGraphEditor->setFocus();
			m_pOutputsGraphEditor->activateWindow();
			return;
		}
		ISubstancePreset* preset = EditorSubstance::CManager::Instance()->GetPreviewPreset(m_substanceArchive, graphName);
		preset->GetOutputs().assign(m_outputs[graphName].begin(), m_outputs[graphName].end());
		m_pOutputsGraphEditor = new CSubstanceOutputEditorDialog(preset, this);
		QObject::connect(m_pOutputsGraphEditor, &CSubstanceOutputEditorDialog::accepted, [=] {
			CArchivePresetsEditor::OnOutputEditorAccepted(graphName);
		});
		connect(m_pOutputsGraphEditor, &CSubstanceOutputEditorDialog::rejected, this, &CArchivePresetsEditor::OnOutputEditorRejected);
		for (auto pair : m_outputsWidgets)
		{
			pair.second->setDisabled(true);
		}


		m_pButtons->setDisabled(true);
		m_pOutputsGraphEditor->Popup();
		m_pModalGuard->show();
		m_pModalGuard->installEventFilter(this);

	}

	void CArchivePresetsEditor::OnOutputEditorAccepted(const string& graphName)
	{

		m_pOutputsGraphEditor->LoadUpdatedOutputSettings(m_outputs[graphName]);
		m_outputsWidgets[graphName]->RefreshOutputs();
		OnOutputEditorRejected();
	}

	void CArchivePresetsEditor::OnOutputEditorRejected()
	{
		for (auto pair : m_outputsWidgets)
		{
			pair.second->setDisabled(false);
		}

		m_pOutputsGraphEditor->deleteLater();
		m_pOutputsGraphEditor = nullptr;
		m_pModalGuard->removeEventFilter(this);
		m_pModalGuard->hide();
	}

	bool CArchivePresetsEditor::OnOpenAsset(CAsset* pAsset)
	{
		QLayoutItem *wItem;
		while ((wItem = m_pOutputsWidgetHolder->layout()->takeAt(0)) != 0)
			delete wItem;

		m_outputsWidgets.clear();
		m_outputs.clear();
		m_resolutions.clear();
		m_substanceArchive = pAsset->GetFile(0);
		AssetTypes::CSubstanceArchiveType::SSubstanceArchiveCache cache = static_cast<const AssetTypes::CSubstanceArchiveType*>(pAsset->GetType())->GetArchiveCache(pAsset);
		for (string graphName : cache.GetGraphNames())
		{
			const AssetTypes::CSubstanceArchiveType::SSubstanceArchiveCache::Graph& graph = cache.GetGraph(graphName);
			std::vector<SSubstanceOutput>& grapOutputs = m_outputs[graphName];

			for (const SSubstanceOutput& output : graph.GetOutputsConfiguration())
			{
				grapOutputs.push_back(output);
				SSubstanceOutput& toWork = grapOutputs.back();
				toWork.UpdateState(graph.GetOutputNames());
			}
			QCollapsibleFrame* collapseFrame = new QCollapsibleFrame(graphName.c_str(), m_pOutputsWidgetHolder);
			OutputsWidget* w = new OutputsWidget(pAsset, graphName, m_outputs[graphName], collapseFrame);
			m_resolutions[graphName] = graph.GetResolution();
			w->SetResolution(graph.GetResolution());
			QObject::connect(w, &OutputsWidget::onResolutionChanged, [=](const Vec2i& res) {
				m_resolutions[graphName] = res;
			});

			collapseFrame->SetWidget(w);
			m_outputsWidgets[graphName] = w;
			m_pOutputsWidgetHolder->layout()->addWidget(collapseFrame);
			QObject::connect(w->GetEditOutputsButton(), &QPushButton::clicked, [=] {
				CArchivePresetsEditor::OnEditOutputs(graphName);
			});
		}
		return true;
	}

	bool CArchivePresetsEditor::OnSaveAsset(CEditableAsset& editAsset)
	{
		AssetTypes::CSubstanceArchiveType* assetType = static_cast<AssetTypes::CSubstanceArchiveType*>(CAssetManager::GetInstance()->FindAssetType("SubstanceDefinition"));
		for (auto& graphOutputs: m_outputs)
		{
			assetType->SetGraphOutputsConfiguration(&editAsset, graphOutputs.first, graphOutputs.second, m_resolutions[graphOutputs.first]);
		}
		editAsset.WriteToFile();
		return true;
	}

	void CArchivePresetsEditor::OnCloseAsset()
	{
	}

	bool CArchivePresetsEditor::eventFilter(QObject *obj, QEvent *event)
	{
		if (m_pOutputsGraphEditor)
		{

			event->accept();
			return true;
		}
		return CAssetEditor::eventFilter(obj, event);
	}

	void CArchivePresetsEditor::resizeEvent(QResizeEvent *e)
	{
		m_pModalGuard->resize(e->size());
	}

	bool CArchivePresetsEditor::event(QEvent *e)
	{
		if (e->type() == QEvent::WindowActivate) {
			if (m_pOutputsGraphEditor)
			{
				e->accept();
				return true;
			}
		}

		return CAssetEditor::event(e);
	}
}

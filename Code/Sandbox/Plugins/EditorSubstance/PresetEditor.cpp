// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PresetEditor.h"
#include "SandboxPlugin.h"
// EditorCommon
#include <AssetSystem/AssetManager.h>
#include <QtViewPane.h>
#include <QVBoxLayout>
#include <QSplitter>
#include <QLabel>

#include "Serialization/QPropertyTree/QPropertyTree.h"
#include <CrySerialization/IArchive.h>
#include "QScrollableBox.h"
#include "OutputEditorDialog.h"
#include "OutputsWidget.h"

#include "CryRenderer/IRenderer.h"
#include "CryRenderer/ITexture.h"
#include "EditorSubstanceManager.h"
#include "Renderers/Base.h"
#include "EditorFramework/PersonalizationManager.h"
#include "Controls/QMenuComboBox.h"



namespace Personalization
{
	static const char* Module = "EditorSubstance::PresetEditor";
	static const char* XRes = "ResolutionX";
	static const char* YRes = "ResolutionY";
	static const char* uniformRes = "ResolutionUniform";
}

namespace EditorSubstance
{
	REGISTER_VIEWPANE_FACTORY_AND_MENU(CSubstancePresetEditor, "Substance Instance Editor", "Substance", false, "Substance")

CSubstancePresetEditor::CSubstancePresetEditor(QWidget* pParent /*= nullptr*/)
	: CAssetEditor("SubstanceInstance")
	, m_pOutputsWidget(nullptr)
	, m_pOutputsGraphEditor(nullptr)
	, m_pPreset(nullptr)
{
	m_pScrollBox = new QScrollableBox();

	
	m_pSubstanceMenu = GetMenu("Substance Preset");
	QAction* const pAction = m_pSubstanceMenu->CreateAction("Reset Inputs");
	connect(pAction, &QAction::triggered, [=]()
	{
		if (m_pPreset)
		{
			m_pPreset->Reset();
			m_propertyTree->revert();
			PushPresetToRender();
		}
	});

	QAction* const pRevertAction = m_pSubstanceMenu->CreateAction("Revert Changes");
	connect(pRevertAction, &QAction::triggered, [=]()
	{
		if (m_pPreset)
		{
			m_pPreset->Reload();
			m_propertyTree->revert();
			m_pOutputsWidget->RefreshOutputs();
			PushPresetToRender();
		}
	});

	
	QHBoxLayout* resolutionLayout = new QHBoxLayout(this);
	m_pComboXRes = new QMenuComboBox(this);
	m_pComboYRes = new QMenuComboBox(this);
	m_pResUnified = new QToolButton(this);
	m_pResUnified->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum));
	m_pResUnified->setIcon(CryIcon("icons:General/Uniform.ico"));
	m_pResUnified->setIconSize(QSize(24, 24));
	m_pResUnified->setCheckable(true);
	bool resUnified = true;
	GetIEditor()->GetPersonalizationManager()->GetProperty(Personalization::Module, Personalization::uniformRes, resUnified);
	m_pResUnified->setChecked(resUnified);

	
	QLabel* text = new QLabel("Preview Resolution");
	resolutionLayout->addWidget(text);
	resolutionLayout->addWidget(m_pComboXRes);
	resolutionLayout->addWidget(m_pResUnified);
	resolutionLayout->addWidget(m_pComboYRes);

	for (int i = 2; i < 14; i++)
	{
		QVariant data(i);
		std::string text = std::to_string(int(pow(2, i)));
		m_pComboXRes->AddItem(text.c_str(), data);
		m_pComboYRes->AddItem(text.c_str(), data);
	}

	QString xRes("512");
	QString yRes("512");
	GetIEditor()->GetPersonalizationManager()->GetProperty(Personalization::Module, Personalization::XRes, xRes);
	GetIEditor()->GetPersonalizationManager()->GetProperty(Personalization::Module, Personalization::YRes, yRes);
	m_pComboXRes->SetText(xRes);
	m_pComboYRes->SetText(yRes);
	m_pResolutionWidget = new QWidget(m_pScrollBox);
	m_pResolutionWidget->setLayout(resolutionLayout);
	m_pScrollBox->addWidget(m_pResolutionWidget);


	QObject::connect(m_pResUnified, &QToolButton::clicked, this, &CSubstancePresetEditor::UniformResolutionClicked);
	m_pComboYRes->signalCurrentIndexChanged.Connect([this](int index)
	{
		ResolutionChanged(m_pComboYRes, index);
	});
	m_pComboXRes->signalCurrentIndexChanged.Connect([this](int index)
	{
		ResolutionChanged(m_pComboXRes, index);
	});

	m_propertyTree = new QPropertyTree(m_pScrollBox);
	m_pScrollBox->addWidget(m_propertyTree);
	m_propertyTree->setSizeToContent(true);
	SetContent(m_pScrollBox);
	QObject::connect(m_propertyTree, &QPropertyTree::signalContinuousChange, [=]
	{
		GetAssetBeingEdited()->SetModified(true);
		PushPresetToRender();
	}
	);
	QObject::connect(m_propertyTree, &QPropertyTree::signalSerialized, [=](Serialization::IArchive& ar)
	{
		if (ar.isOutput())
		{
			if (GetAssetBeingEdited())
			{
				GetAssetBeingEdited()->SetModified(true);
				PushPresetToRender();
			}
		}
	}
	);
	SetPreviewResolution();
	m_pResolutionWidget->hide();

}

void CSubstancePresetEditor::UniformResolutionClicked()
{
	if (m_pResUnified->isChecked())
	{
		m_pComboYRes->SetText(m_pComboXRes->GetText());
	}
	GetIEditor()->GetPersonalizationManager()->SetProperty(Personalization::Module, Personalization::uniformRes, m_pResUnified->isChecked());

	SetPreviewResolution();
	PushPresetToRender();
}

void CSubstancePresetEditor::ResolutionChanged(QMenuComboBox* sender, int index)
{
	if (m_pResUnified->isChecked())
	{
		if (sender == m_pComboXRes && m_pComboYRes->GetText() != sender->GetText())
		{
			m_pComboYRes->SetText(sender->GetText());
		}

		if (sender == m_pComboYRes && m_pComboXRes->GetText() != sender->GetText())
		{
			m_pComboXRes->SetText(sender->GetText());
		}
	}

	
	GetIEditor()->GetPersonalizationManager()->SetProperty(Personalization::Module, Personalization::XRes, m_pComboXRes->GetText());
	GetIEditor()->GetPersonalizationManager()->SetProperty(Personalization::Module, Personalization::YRes, m_pComboYRes->GetText());
	SetPreviewResolution();
	PushPresetToRender();
}


bool CSubstancePresetEditor::OnOpenAsset(CAsset* pAsset)
{
	if (!pAsset)
	{
		return false;
	}
	
	if (m_pOutputsWidget)
	{
		m_pScrollBox->removeWidget(m_pOutputsWidget);
		m_pOutputsWidget->deleteLater();
		m_pOutputsWidget = nullptr;
	}
	m_pResolutionWidget->show();
	m_pPreset = CManager::Instance()->GetSubstancePreset(pAsset);
	if (!m_pPreset)
	{
		return false;
	}
	m_pSerializer = m_pPreset->GetSerializer();
	m_propertyTree->attach(Serialization::SStruct(*m_pSerializer));

	m_pOutputsWidget = new OutputsWidget(pAsset, "", m_pPreset->GetOutputs(), m_pScrollBox, false);
	m_pScrollBox->addWidget(m_pOutputsWidget);
	m_pScrollBox->removeWidget(m_propertyTree);
	m_pScrollBox->addWidget(m_propertyTree);
	
	QObject::connect(m_pOutputsWidget->GetEditOutputsButton(), &QPushButton::clicked, [=] {
		CSubstancePresetEditor::OnEditOutputs();
	});
	QObject::connect(m_pOutputsWidget, &OutputsWidget::outputChanged, [=](SSubstanceOutput* output) {
		PushPresetToRender();
	});
	QObject::connect(m_pOutputsWidget, &OutputsWidget::outputStateChanged, [=](SSubstanceOutput* output) {
		PushPresetToRender();
	});
	SetPreviewResolution();

	return true;
}

bool CSubstancePresetEditor::OnSaveAsset(CEditableAsset& editAsset)
{
	m_pPreset->Save();
	std::vector<SAssetDependencyInfo> dependencies;
	dependencies.emplace_back(m_pPreset->GetSubstanceArchive(), 1);
	const std::vector<string> images = m_pPreset->GetInputImages();
	for( const string& image : images)
	{
		dependencies.emplace_back(image, 1);
	}
	editAsset.SetDependencies(dependencies);

	GetAssetBeingEdited()->SetModified(false);
	return true;
}

void CSubstancePresetEditor::OnCloseAsset()
{
	CManager::Instance()->PresetEditEnded(m_pPreset);
	m_pResolutionWidget->hide();
}

void CSubstancePresetEditor::PushPresetToRender()
{
	if (m_pPreset)
	{
		CManager::Instance()->EnquePresetRender(m_pPreset, CManager::Instance()->GetCompressedRenderer());
	}
}

void CSubstancePresetEditor::SetPreviewResolution()
{
	if (m_pPreset)
		m_pPreset->SetGraphResolution(m_pComboXRes->GetCurrentData().toInt(), m_pComboYRes->GetCurrentData().toInt());
}

void CSubstancePresetEditor::OnEditOutputs()
{
	if (m_pOutputsGraphEditor)
	{
		m_pOutputsGraphEditor->raise();
		m_pOutputsGraphEditor->setFocus();
		m_pOutputsGraphEditor->activateWindow();
		return;
	}
	ISubstancePreset* preset = EditorSubstance::CManager::Instance()->GetPreviewPreset(m_pPreset);
	m_pOutputsGraphEditor = new CSubstanceOutputEditorDialog(preset, this);
	connect(m_pOutputsGraphEditor, &CSubstanceOutputEditorDialog::accepted, this, &CSubstancePresetEditor::OnOutputEditorAccepted);
	connect(m_pOutputsGraphEditor, &CSubstanceOutputEditorDialog::rejected, this, &CSubstancePresetEditor::OnOutputEditorRejected);
	m_pOutputsWidget->setDisabled(true);
	m_pOutputsGraphEditor->Popup();
}

void CSubstancePresetEditor::OnOutputEditorAccepted()
{
	if (GetAssetBeingEdited())
	{
		GetAssetBeingEdited()->SetModified(true);
	}
	m_pOutputsGraphEditor->LoadUpdatedOutputSettings(m_pPreset->GetOutputs());
	
	m_pOutputsWidget->RefreshOutputs();
	PushPresetToRender();
	OnOutputEditorRejected();
}

void CSubstancePresetEditor::OnOutputEditorRejected()
{
	m_pOutputsWidget->setDisabled(false);
	m_pOutputsGraphEditor->deleteLater();
	m_pOutputsGraphEditor = nullptr;
}
}

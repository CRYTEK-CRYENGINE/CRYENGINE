// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PresetCreator.h"

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
#include "Controls/QuestionDialog.h"
#include "OutputEditorDialog.h"
#include "EditorSubstanceManager.h"

namespace EditorSubstance
{

	CPressetCreator::CPressetCreator(CAsset* asset, const string& graphName, std::vector<SSubstanceOutput>& outputs, const Vec2i& resolution, QWidget* parent /*= nullptr*/)
		: CEditorDialog("CPressetCreator", parent)
		, m_substanceArchive(asset->GetFile(0))
		, m_graphName(graphName)
		, m_outputs(outputs)
		, m_pOutputsGraphEditor(nullptr)
		, m_resolution(resolution)
	{
		SetTitle("Create Substance Graph Preset");
		m_pButtons = new QDialogButtonBox();
		m_pButtons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		connect(m_pButtons, &QDialogButtonBox::accepted, this, &CPressetCreator::OnAccept);
		connect(m_pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		m_pModalGuard = new QFrame();
		m_pModalGuard->resize(size());
		m_pModalGuard->hide();
		m_foldersView = new CAssetFoldersView(this);
		m_foldersView->SelectFolder(asset->GetFolder());
		m_foldersView->setSelectionMode(QAbstractItemView::SingleSelection);
		m_pPathEdit = new CFileNameLineEdit(this);
		m_pPathEdit->setText(QString(PathUtil::GetUniqueName(string(asset->GetName()) + "." + CAssetManager::GetInstance()->FindAssetType("SubstanceInstance")->GetFileExtension(), asset->GetFolder()).c_str()));
		QHBoxLayout* const pPathLayout = new QHBoxLayout();
		pPathLayout->addWidget(new QLabel(tr("Preset Name: ")));
		pPathLayout->addWidget(m_pPathEdit);

		auto* const pMainLayout = new QVBoxLayout();
		pMainLayout->addWidget(m_foldersView);
		pMainLayout->addLayout(pPathLayout);
		m_pOutputsWidget = new OutputsWidget(asset, graphName, m_outputs, this);
		m_pOutputsWidget->SetResolution(m_resolution);
		pMainLayout->addWidget(m_pOutputsWidget);

		pMainLayout->addWidget(m_pButtons, Qt::AlignRight);

		setLayout(pMainLayout);
		QObject::connect(m_pOutputsWidget->GetEditOutputsButton(), &QPushButton::clicked, [=] {
			CPressetCreator::OnEditOutputs();
		});

		QObject::connect(m_pOutputsWidget, &OutputsWidget::onResolutionChanged, [=](const Vec2i& res) {
			m_resolution = res;
		});
		
		CAssetManager* pAssetManager = GetIEditor()->GetAssetManager();
		pAssetManager->signalBeforeAssetsRemoved.Connect([this, asset](const std::vector<CAsset*>& assets)
		{
			if (std::find(assets.begin(), assets.end(), asset) != assets.end())
			{
				m_substanceArchive.clear();
				m_pOutputsWidget->setDisabled(true);
			}
		}, (uintptr_t)this);
	}

	CPressetCreator::~CPressetCreator()
	{
		GetIEditor()->GetAssetManager()->signalBeforeAssetsRemoved.DisconnectById((uintptr_t)this);
	}

	const string& CPressetCreator::GetTargetFileName() const
	{
		return m_finalFileName;
	}


	void CPressetCreator::OnAccept()
	{
		QString text = m_pPathEdit->text();
		QStringList folders = m_foldersView->GetSelectedFolders();
		if (text.isEmpty())
		{
			CQuestionDialog::SCritical("Error", "You must specify file name");
			return;
		}
		if (folders.count() > 0)
		{
			string fileExtension(CAssetManager::GetInstance()->FindAssetType("SubstanceInstance")->GetFileExtension());
			string targetFileName(PathUtil::Make(folders[0].toStdString().c_str(), (text + "." + fileExtension).toStdString().c_str()));
			ICryPak* const pPak = GetISystem()->GetIPak();
			if (pPak->IsFileExist(targetFileName))
			{
				CQuestionDialog::SCritical("Error", "File with the same name already exists.");
			}
			else {
				m_finalFileName = targetFileName;
				accept();
			}
		}
		else {
			CQuestionDialog::SCritical("Error", "You must select target folder");
		}

	}

	void CPressetCreator::OnEditOutputs()
	{
		if (m_substanceArchive.empty())
		{
			return;
		}

		if (m_pOutputsGraphEditor)
		{
			m_pOutputsGraphEditor->raise();
			m_pOutputsGraphEditor->setFocus();
			m_pOutputsGraphEditor->activateWindow();
			return;
		}
		ISubstancePreset* preset = EditorSubstance::CManager::Instance()->GetPreviewPreset(m_substanceArchive, m_graphName);
		preset->GetOutputs().assign(m_outputs.begin(), m_outputs.end());
		m_pOutputsGraphEditor = new CSubstanceOutputEditorDialog(preset, this);
		connect(m_pOutputsGraphEditor, &CSubstanceOutputEditorDialog::accepted, this, &CPressetCreator::OnOutputEditorAccepted);
		connect(m_pOutputsGraphEditor, &CSubstanceOutputEditorDialog::rejected, this, &CPressetCreator::OnOutputEditorRejected);
		m_pOutputsWidget->setDisabled(true);
		m_pButtons->setDisabled(true);
		m_pOutputsGraphEditor->Popup();
		m_pModalGuard->show();
		m_pModalGuard->installEventFilter(this);

	}

	void CPressetCreator::OnOutputEditorAccepted()
	{

		m_pOutputsGraphEditor->LoadUpdatedOutputSettings(m_outputs);
		m_pOutputsWidget->RefreshOutputs();
		OnOutputEditorRejected();
	}

	void CPressetCreator::OnOutputEditorRejected()
	{
		m_pOutputsWidget->setDisabled(false);
		m_pButtons->setDisabled(false);
		m_pOutputsGraphEditor->deleteLater();
		m_pOutputsGraphEditor = nullptr;
		m_pModalGuard->removeEventFilter(this);
		m_pModalGuard->hide();
	}

	void CPressetCreator::keyPressEvent(QKeyEvent *e)
	{
		if (e->matches(QKeySequence::Cancel)) {
			reject();
		}
		else
			if (!e->modifiers() || (e->modifiers() & Qt::KeypadModifier && e->key() == Qt::Key_Enter)) {
				switch (e->key()) {
				case Qt::Key_Enter:
				case Qt::Key_Return: {
					QList<QPushButton*> list = findChildren<QPushButton*>();
					for (int i = 0; i < list.size(); ++i) {
						QPushButton *pb = list.at(i);
						if (pb->isDefault() && pb->isVisible()) {
							if (pb->isEnabled())
								pb->click();
							return;
						}
					}
				}
									 break;
				default:
					QApplication::sendEvent(parent(), e);
					return;
				}
			}
			else {
				QApplication::sendEvent(parent(), e);
			}
	}

	bool CPressetCreator::eventFilter(QObject *obj, QEvent *event)
	{
		if (m_pOutputsGraphEditor)
		{
			OnEditOutputs();
			event->accept();
			return true;
		}
		return CEditorDialog::eventFilter(obj, event);
	}

	void CPressetCreator::resizeEvent(QResizeEvent *e)
	{
		m_pModalGuard->resize(e->size());
	}

	bool CPressetCreator::event(QEvent *e)
	{
		if (e->type() == QEvent::WindowActivate) {
			if (m_pOutputsGraphEditor)
			{
				OnEditOutputs();
				e->accept();
				return true;
			}
		}

		return CEditorDialog::event(e);
	}
}

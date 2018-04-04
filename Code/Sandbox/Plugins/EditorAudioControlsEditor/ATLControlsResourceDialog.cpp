// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ATLControlsResourceDialog.h"
#include "AudioControlsEditorPlugin.h"
#include "AudioAssetsManager.h"
#include "QAudioControlTreeWidget.h"
#include "QtUtil.h"

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QBoxLayout>
#include <QApplication>
#include <QHeaderView>
#include <QPushButton>
#include <ProxyModels/MountingProxyModel.h>

namespace ACE
{
string ATLControlsDialog::ms_controlName = "";

ATLControlsDialog::ATLControlsDialog(QWidget* pParent, EItemType eType)
	: CEditorDialog("ATLControlsDialog", pParent)
	, m_eType(eType)
{
	setWindowTitle("Choose...");
	setWindowModality(Qt::ApplicationModal);

	QBoxLayout* pLayout = new QBoxLayout(QBoxLayout::TopToBottom);
	setLayout(pLayout);

	QLineEdit* pTextFilterLineEdit = new QLineEdit(this);
	pTextFilterLineEdit->setAlignment(Qt::AlignLeading | Qt::AlignLeft | Qt::AlignVCenter);
	pTextFilterLineEdit->setPlaceholderText(QApplication::translate("AudioAssetsExplorer", "Search", 0));
	connect(pTextFilterLineEdit, &QLineEdit::textChanged, this, &ATLControlsDialog::SetTextFilter);
	pLayout->addWidget(pTextFilterLineEdit, 0);

	m_pControlTree = new QAudioControlsTreeView(this);
	m_pControlTree->header()->setVisible(false);
	m_pControlTree->setEnabled(true);
	m_pControlTree->setAutoScroll(true);
	m_pControlTree->setDragEnabled(false);
	m_pControlTree->setDragDropMode(QAbstractItemView::NoDragDrop);
	m_pControlTree->setDefaultDropAction(Qt::IgnoreAction);
	m_pControlTree->setAlternatingRowColors(false);
	m_pControlTree->setSelectionMode(QAbstractItemView::SingleSelection);
	m_pControlTree->setRootIsDecorated(true);
	m_pControlTree->setSortingEnabled(true);
	m_pControlTree->setAnimated(false);
	m_pControlTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
	pLayout->addWidget(m_pControlTree, 0);

	m_pAssetsManager = CAudioControlsEditorPlugin::GetAssetsManager();
	m_pProxyModel = new QAudioControlSortProxy(this);
	m_pAssetsModel = new CAudioAssetsExplorerModel(m_pAssetsManager);

	m_pMountedModel = new CMountingProxyModel(WrapMemberFunction(this, &ATLControlsDialog::CreateLibraryModelFromIndex));
	m_pMountedModel->SetHeaderDataCallbacks(1, &GetHeaderData);
	m_pMountedModel->SetSourceModel(m_pAssetsModel);

	m_pProxyModel->setSourceModel(m_pMountedModel);
	m_pControlTree->setModel(m_pProxyModel);

	m_pDialogButtons = new QDialogButtonBox(this);
	m_pDialogButtons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(m_pDialogButtons, &QDialogButtonBox::accepted, this, &ATLControlsDialog::accept);
	connect(m_pDialogButtons, &QDialogButtonBox::rejected, this, &ATLControlsDialog::reject);
	pLayout->addWidget(m_pDialogButtons, 0);

	connect(m_pControlTree->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ATLControlsDialog::UpdateSelectedControl);
	ApplyFilter();
	UpdateSelectedControl();
	m_pControlTree->setFocus();

	m_pControlTree->installEventFilter(this);
	m_pControlTree->viewport()->installEventFilter(this);

	connect(m_pControlTree->selectionModel(), &QItemSelectionModel::currentChanged, this, &ATLControlsDialog::StopTrigger);
	connect(m_pControlTree, &QAudioControlsTreeView::doubleClicked, this, &ATLControlsDialog::ItemDoubleClicked);

	pTextFilterLineEdit->setFocus(Qt::FocusReason::PopupFocusReason);

	// Expand the root that holds all the controls
	QModelIndex index = m_pProxyModel->index(0, 0);
	if (index.isValid())
	{
		m_pControlTree->setExpanded(index, true);
	}
}

ATLControlsDialog::~ATLControlsDialog()
{
	StopTrigger();
}

QAbstractItemModel* ATLControlsDialog::CreateLibraryModelFromIndex(const QModelIndex& sourceIndex)
{
	if (sourceIndex.model() != m_pAssetsModel)
	{
		return nullptr;
	}

	const int numLibraries = m_libraryModels.size();
	const int row = sourceIndex.row();
	if (row >= m_libraryModels.size())
	{

		m_libraryModels.resize(row + 1);
		for (uint i = numLibraries; i < row + 1; ++i)
		{
			m_libraryModels[i] = new CAudioLibraryModel(m_pAssetsManager, m_pAssetsManager->GetLibrary(i));
		}
	}

	return m_libraryModels[row];
}

const char* ATLControlsDialog::ChooseItem(const char* szCurrentValue)
{
	if (std::strcmp(szCurrentValue, "") != 0)
	{
		ms_controlName = szCurrentValue;
	}

	// Automatically select the current control
	if (m_pProxyModel && !ms_controlName.empty())
	{
		QModelIndex index = FindItem(ms_controlName);
		if (index.isValid())
		{
			m_pControlTree->setCurrentIndex(index);
		}
	}

	if (exec() == QDialog::Accepted)
	{
		return ms_controlName.c_str();
	}
	return szCurrentValue;
}

void ATLControlsDialog::UpdateSelectedControl()
{
	if (m_pAssetsManager)
	{
		QModelIndexList indexes = m_pControlTree->selectionModel()->selectedIndexes();
		if (!indexes.empty())
		{
			const QModelIndex& index = indexes[0];
			if (index.isValid())
			{
				IAudioAsset* pAsset = AudioModelUtils::GetAssetFromIndex(index);
				if (pAsset && pAsset->GetType() == m_eType)
				{
					ms_controlName = pAsset->GetName();
				}
			}
		}
	}

	QPushButton* pButton = m_pDialogButtons->button(QDialogButtonBox::Ok);
	if (pButton)
	{
		pButton->setEnabled(!ms_controlName.empty());
	}
}

void ATLControlsDialog::SetTextFilter(QString filter)
{
	if (m_sFilter != filter)
	{
		if (m_sFilter.isEmpty() && !filter.isEmpty())
		{
			m_pControlTree->SaveExpandedState();
			m_pControlTree->expandAll();
		}
		else if (!m_sFilter.isEmpty() && filter.isEmpty())
		{
			m_pControlTree->collapseAll();
			m_pControlTree->RestoreExpandedState();
		}
		m_sFilter = filter;
		ApplyFilter();
	}
}

void ATLControlsDialog::ApplyFilter()
{
	QModelIndex index = m_pProxyModel->index(0, 0);
	for (int i = 0; index.isValid(); ++i)
	{
		ApplyFilter(index);
		index = index.sibling(i, 0);
	}
}

bool ATLControlsDialog::ApplyFilter(const QModelIndex parent)
{
	if (parent.isValid())
	{
		bool bChildValid = false;
		QModelIndex child = parent.child(0, 0);
		for (int i = 1; child.isValid(); ++i)
		{
			if (ApplyFilter(child))
			{
				bChildValid = true;
			}
			child = parent.child(i, 0);
		}

		// If it has valid children or it is a valid control (not a folder), show it
		IAudioAsset* pAsset = AudioModelUtils::GetAssetFromIndex(parent);
		if (bChildValid || (pAsset && pAsset->GetType() == m_eType && IsValid(parent)))
		{
			m_pControlTree->setRowHidden(parent.row(), parent.parent(), false);
			return true;
		}
		else
		{
			m_pControlTree->setRowHidden(parent.row(), parent.parent(), true);

		}
	}
	return false;
}

bool ATLControlsDialog::IsValid(const QModelIndex index)
{
	const QString sName = index.data(Qt::DisplayRole).toString();
	if (m_sFilter.isEmpty() || sName.contains(m_sFilter, Qt::CaseInsensitive))
	{
		IAudioAsset* pAsset = AudioModelUtils::GetAssetFromIndex(index);
		if (pAsset && pAsset->GetType() == m_eType)
		{
			CAudioControl* pControl = static_cast<CAudioControl*>(pAsset);
			if (pControl)
			{
				const Scope scope = pControl->GetScope();
				if (scope == Utils::GetGlobalScope() || scope == m_scope)
				{
					return true;
				}
				return false;
			}
		}

	}
	return false;
}

QSize ATLControlsDialog::sizeHint() const
{
	return QSize(400, 900);
}

void ATLControlsDialog::SetScope(Scope scope)
{
	m_scope = scope;
	ApplyFilter();
}

QModelIndex ATLControlsDialog::FindItem(const string& sControlName)
{
	QModelIndexList indexes = m_pProxyModel->match(m_pProxyModel->index(0, 0, QModelIndex()), Qt::DisplayRole, QtUtil::ToQString(sControlName), -1, Qt::MatchRecursive);
	if (!indexes.empty())
	{
		IAudioAsset* pAsset = AudioModelUtils::GetAssetFromIndex(indexes[0]);
		if (pAsset)
		{
			CAudioControl* pControl = static_cast<CAudioControl*>(pAsset);
			if (pControl)
			{
				const Scope scope = pControl->GetScope();
				if (scope == Utils::GetGlobalScope() || scope == m_scope)
				{
					return indexes[0];
				}
			}
		}
	}
	return QModelIndex();
}
bool ATLControlsDialog::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (pEvent->type() == QEvent::KeyRelease)
	{
		QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(pEvent);
		if (pKeyEvent && pKeyEvent->key() == Qt::Key_Space)
		{
			QModelIndex index = m_pControlTree->currentIndex();
			if (index.isValid())
			{
				IAudioAsset* pAsset = AudioModelUtils::GetAssetFromIndex(index);
				if (pAsset)
				{
					CAudioControlsEditorPlugin::ExecuteTrigger(pAsset->GetName());
				}
			}
		}
	}
	return QWidget::eventFilter(pObject, pEvent);
}

void ATLControlsDialog::StopTrigger()
{
	CAudioControlsEditorPlugin::StopTriggerExecution();
}

void ATLControlsDialog::ItemDoubleClicked(const QModelIndex& modelIndex)
{
	IAudioAsset* pAsset = AudioModelUtils::GetAssetFromIndex(modelIndex);
	if (pAsset && pAsset->GetType() == m_eType)
	{
		ms_controlName = pAsset->GetName();
		accept();
	}
}
} // namespace ACE

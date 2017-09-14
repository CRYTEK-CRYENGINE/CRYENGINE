// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ATLControlsResourceDialog.h"
#include "AudioControlsEditorPlugin.h"
#include "AudioAssetsManager.h"
#include "QAudioControlTreeWidget.h"
#include "ResourceSelectorModel.h"
#include "QtUtil.h"

#include <QSearchBox.h>

#include <QDialogButtonBox>
#include <QBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QMenu>
#include <QModelIndex>
#include <ProxyModels/MountingProxyModel.h>

namespace ACE
{
string ATLControlsDialog::s_previousControlName = "";
EItemType ATLControlsDialog::s_previousControlType = eItemType_Invalid;

//////////////////////////////////////////////////////////////////////////
ATLControlsDialog::ATLControlsDialog(QWidget* pParent, EItemType eType)
	: CEditorDialog("ATLControlsDialog", pParent)
	, m_eType(eType)
{
	setWindowTitle("Choose...");
	setWindowModality(Qt::ApplicationModal);

	QBoxLayout* pLayout = new QBoxLayout(QBoxLayout::TopToBottom);
	setLayout(pLayout);

	QSearchBox* pSearchBox = new QSearchBox(this);
	pSearchBox->setToolTip(tr("Show only controls with this name"));
	connect(pSearchBox, &QSearchBox::textChanged, this, &ATLControlsDialog::SetTextFilter);
	pLayout->addWidget(pSearchBox, 0);

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
	m_pControlTree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pControlTree, &QAudioControlsTreeView::customContextMenuRequested, this, &ATLControlsDialog::ShowControlsContextMenu);
	pLayout->addWidget(m_pControlTree, 0);

	m_pAssetsManager = CAudioControlsEditorPlugin::GetAssetsManager();
	m_pProxyModel = new QAudioControlSortProxy(this);
	m_pAssetsModel = new CResourceControlModel(m_pAssetsManager);

	m_pMountedModel = new CMountingProxyModel(WrapMemberFunction(this, &ATLControlsDialog::CreateLibraryModelFromIndex));
	m_pMountedModel->SetHeaderDataCallbacks(1, &GetHeaderData);
	m_pMountedModel->SetSourceModel(m_pAssetsModel);

	m_pProxyModel->setSourceModel(m_pMountedModel);
	m_pControlTree->setModel(m_pProxyModel);

	m_pDialogButtons = new QDialogButtonBox(this);
	m_pDialogButtons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	m_pDialogButtons->button(QDialogButtonBox::Ok)->setEnabled(false);
	connect(m_pDialogButtons, &QDialogButtonBox::accepted, this, &ATLControlsDialog::accept);
	connect(m_pDialogButtons, &QDialogButtonBox::rejected, this, &ATLControlsDialog::reject);
	pLayout->addWidget(m_pDialogButtons, 0);

	if (s_previousControlType != eType)
	{
		s_previousControlName.clear();
		s_previousControlType = eType;
	}

	connect(m_pControlTree->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ATLControlsDialog::UpdateSelectedControl);
	ApplyFilter();
	UpdateSelectedControl();
	m_pControlTree->setFocus();

	m_pControlTree->installEventFilter(this);
	m_pControlTree->viewport()->installEventFilter(this);

	connect(m_pControlTree->selectionModel(), &QItemSelectionModel::currentChanged, this, &ATLControlsDialog::StopTrigger);
	connect(m_pControlTree, &QAudioControlsTreeView::doubleClicked, this, &ATLControlsDialog::ItemDoubleClicked);

	pSearchBox->setFocus(Qt::FocusReason::PopupFocusReason);
}

//////////////////////////////////////////////////////////////////////////
ATLControlsDialog::~ATLControlsDialog()
{
	StopTrigger();
}

//////////////////////////////////////////////////////////////////////////
QAbstractItemModel* ATLControlsDialog::CreateLibraryModelFromIndex(QModelIndex const& sourceIndex)
{
	if (sourceIndex.model() != m_pAssetsModel)
	{
		return nullptr;
	}

	int const numLibraries = m_libraryModels.size();
	int const row = sourceIndex.row();

	if (row >= m_libraryModels.size())
	{
		m_libraryModels.resize(row + 1);

		for (uint i = numLibraries; i < row + 1; ++i)
		{
			m_libraryModels[i] = new CResourceLibraryModel(m_pAssetsManager, m_pAssetsManager->GetLibrary(i));
		}
	}

	return m_libraryModels[row];
}

//////////////////////////////////////////////////////////////////////////
const char* ATLControlsDialog::ChooseItem(const char* szCurrentValue)
{
	if (std::strcmp(szCurrentValue, "") != 0)
	{
		s_previousControlName = szCurrentValue;
	}

	// Automatically select the previous control
	if (m_pProxyModel && !s_previousControlName.empty())
	{
		QModelIndex const& index = FindItem(s_previousControlName);

		if (index.isValid())
		{
			m_pControlTree->setCurrentIndex(index);
		}
	}

	if (exec() == QDialog::Accepted)
	{
		return s_previousControlName.c_str();
	}

	return szCurrentValue;
}

//////////////////////////////////////////////////////////////////////////
void ATLControlsDialog::UpdateSelectedControl()
{
	if (m_pAssetsManager)
	{
		QModelIndexList indexes = m_pControlTree->selectionModel()->selectedIndexes();

		if (!indexes.empty())
		{
			QModelIndex const& index = indexes[0];

			if (index.isValid())
			{
				IAudioAsset* pAsset = AudioModelUtils::GetAssetFromIndex(index);

				if (pAsset && pAsset->GetType() == m_eType)
				{
					s_previousControlName = pAsset->GetName();
				}

				m_bSelectionIsValid = (pAsset && (pAsset->GetType() != eItemType_Folder) && (pAsset && pAsset->GetType() != eItemType_Library));
				m_pDialogButtons->button(QDialogButtonBox::Ok)->setEnabled(m_bSelectionIsValid);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ATLControlsDialog::SetTextFilter(QString const& filter)
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

		QModelIndex const& currentIndex = m_pControlTree->currentIndex();

		if (currentIndex.isValid())
		{
			m_pControlTree->scrollTo(currentIndex);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ATLControlsDialog::ApplyFilter()
{
	QModelIndex index = m_pProxyModel->index(0, 0);

	for (int i = 0; index.isValid(); ++i)
	{
		ApplyFilter(index);
		index = index.sibling(i, 0);
	}
}

//////////////////////////////////////////////////////////////////////////
bool ATLControlsDialog::ApplyFilter(QModelIndex const& parent)
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

//////////////////////////////////////////////////////////////////////////
bool ATLControlsDialog::IsValid(QModelIndex const& index)
{
	QString const& sName = index.data(Qt::DisplayRole).toString();

	if (m_sFilter.isEmpty() || sName.contains(m_sFilter, Qt::CaseInsensitive))
	{
		IAudioAsset* pAsset = AudioModelUtils::GetAssetFromIndex(index);

		if (pAsset && pAsset->GetType() == m_eType)
		{
			CAudioControl* pControl = static_cast<CAudioControl*>(pAsset);

			if (pControl)
			{
				Scope const scope = pControl->GetScope();

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

//////////////////////////////////////////////////////////////////////////
QSize ATLControlsDialog::sizeHint() const
{
	return QSize(400, 900);
}

//////////////////////////////////////////////////////////////////////////
void ATLControlsDialog::SetScope(Scope const scope)
{
	m_scope = scope;
	ApplyFilter();
}

//////////////////////////////////////////////////////////////////////////
QModelIndex ATLControlsDialog::FindItem(string const& sControlName)
{
	QModelIndexList const indexes = m_pProxyModel->match(m_pProxyModel->index(0, 0, QModelIndex()), Qt::DisplayRole, QtUtil::ToQString(sControlName), -1, Qt::MatchRecursive);
	
	if (!indexes.empty())
	{
		IAudioAsset* pAsset = AudioModelUtils::GetAssetFromIndex(indexes[0]);

		if (pAsset)
		{
			CAudioControl* pControl = static_cast<CAudioControl*>(pAsset);

			if (pControl)
			{
				Scope const scope = pControl->GetScope();

				if (scope == Utils::GetGlobalScope() || scope == m_scope)
				{
					return indexes[0];
				}
			}
		}
	}

	return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////
bool ATLControlsDialog::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (pEvent->type() == QEvent::KeyRelease && m_bSelectionIsValid)
	{
		QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(pEvent);

		if (pKeyEvent && pKeyEvent->key() == Qt::Key_Space)
		{
			QModelIndex const& index = m_pControlTree->currentIndex();

			if (index.isValid())
			{
				IAudioAsset* pAsset = AudioModelUtils::GetAssetFromIndex(index);

				if (pAsset && (pAsset->GetType() == eItemType_Trigger))
				{
					CAudioControlsEditorPlugin::ExecuteTrigger(pAsset->GetName());
				}
			}
		}
	}

	return QWidget::eventFilter(pObject, pEvent);
}


//////////////////////////////////////////////////////////////////////////
void ATLControlsDialog::StopTrigger()
{
	CAudioControlsEditorPlugin::StopTriggerExecution();
}

//////////////////////////////////////////////////////////////////////////
void ATLControlsDialog::ItemDoubleClicked(QModelIndex const& modelIndex)
{
	if (m_bSelectionIsValid)
	{
		IAudioAsset* pAsset = AudioModelUtils::GetAssetFromIndex(modelIndex);

		if (pAsset && pAsset->GetType() == m_eType)
		{
			s_previousControlName = pAsset->GetName();
			accept();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ATLControlsDialog::ShowControlsContextMenu(QPoint const& pos)
{
	QModelIndex const& index = m_pControlTree->currentIndex();

	if (index.isValid())
	{
		IAudioAsset* pAsset = AudioModelUtils::GetAssetFromIndex(index);

		if (pAsset && (pAsset->GetType() == eItemType_Trigger))
		{
			QMenu contextMenu(tr("Context menu"), this);

			contextMenu.addAction(tr("Execute Trigger"), [&]()
			{
				CAudioControlsEditorPlugin::ExecuteTrigger(pAsset->GetName());
			});

			contextMenu.exec(QCursor::pos());
		}
	}
}
} // namespace ACE

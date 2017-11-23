// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ResourceSelectorDialog.h"

#include "ResourceSelectorModel.h"
#include "AudioControlsEditorPlugin.h"
#include "SystemAssetsManager.h"

#include <QtUtil.h>
#include <QAdvancedTreeView.h>
#include <QSearchBox.h>
#include <ProxyModels/MountingProxyModel.h>
#include <ProxyModels/DeepFilterProxyModel.h>

#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QMenu>
#include <QModelIndex>
#include <QKeyEvent>

namespace ACE
{
string CResourceSelectorDialog::s_previousControlName = "";
ESystemItemType CResourceSelectorDialog::s_previousControlType = ESystemItemType::Invalid;

//////////////////////////////////////////////////////////////////////////
CResourceSelectorDialog::CResourceSelectorDialog(QWidget* pParent, ESystemItemType const eType)
	: CEditorDialog("AudioControlsResourceSelectorDialog", pParent)
	, m_eType(eType)
	, m_pTreeView(new QAdvancedTreeView())
	, m_pDialogButtons(new QDialogButtonBox())
{
	setWindowTitle("Select Audio System Control");
	setWindowModality(Qt::ApplicationModal);

	m_pAssetsManager = CAudioControlsEditorPlugin::GetAssetsManager();
	m_pFilterProxyModel = new QDeepFilterProxyModel();
	m_pLibraryModel = new CResourceLibraryModel(m_pAssetsManager);

	m_pMountingProxyModel = new CMountingProxyModel(WrapMemberFunction(this, &CResourceSelectorDialog::CreateControlsModelFromIndex));
	m_pMountingProxyModel->SetHeaderDataCallbacks(1, &GetHeaderData);
	m_pMountingProxyModel->SetSourceModel(m_pLibraryModel);

	m_pFilterProxyModel->setSourceModel(m_pMountingProxyModel);

	QVBoxLayout* const pWindowLayout = new QVBoxLayout();
	setLayout(pWindowLayout);

	QSearchBox* const pSearchBox = new QSearchBox();
	pWindowLayout->addWidget(pSearchBox);
	
	m_pTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_pTreeView->setDragEnabled(false);
	m_pTreeView->setDragDropMode(QAbstractItemView::NoDragDrop);
	m_pTreeView->setDefaultDropAction(Qt::IgnoreAction);
	m_pTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setModel(m_pFilterProxyModel);
	m_pTreeView->sortByColumn(0, Qt::AscendingOrder);
	m_pTreeView->installEventFilter(this);
	pWindowLayout->addWidget(m_pTreeView);

	m_pDialogButtons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	m_pDialogButtons->button(QDialogButtonBox::Ok)->setEnabled(false);
	pWindowLayout->addWidget(m_pDialogButtons);

	QObject::connect(pSearchBox, &QSearchBox::textChanged, this, &CResourceSelectorDialog::SetTextFilter);
	QObject::connect(m_pTreeView, &QAdvancedTreeView::customContextMenuRequested, this, &CResourceSelectorDialog::OnContextMenu);
	QObject::connect(m_pTreeView, &QAdvancedTreeView::doubleClicked, this, &CResourceSelectorDialog::ItemDoubleClicked);
	QObject::connect(m_pTreeView->header(), &QHeaderView::sortIndicatorChanged, [&]() { m_pTreeView->scrollTo(m_pTreeView->currentIndex()); });
	QObject::connect(m_pTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CResourceSelectorDialog::UpdateSelectedControl);
	QObject::connect(m_pTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this, &CResourceSelectorDialog::StopTrigger);
	QObject::connect(m_pDialogButtons, &QDialogButtonBox::accepted, this, &CResourceSelectorDialog::accept);
	QObject::connect(m_pDialogButtons, &QDialogButtonBox::rejected, this, &CResourceSelectorDialog::reject);

	if (s_previousControlType != eType)
	{
		s_previousControlName.clear();
		s_previousControlType = eType;
	}

	ApplyFilter();
	UpdateSelectedControl();
}

//////////////////////////////////////////////////////////////////////////
CResourceSelectorDialog::~CResourceSelectorDialog()
{
	StopTrigger();

	delete m_pLibraryModel;

	for (auto const pControlsModel : m_controlsModels)
	{
		pControlsModel->DisconnectFromSystem();
		delete pControlsModel;
	}

	m_controlsModels.clear();
}

//////////////////////////////////////////////////////////////////////////
QAbstractItemModel* CResourceSelectorDialog::CreateControlsModelFromIndex(QModelIndex const& sourceIndex)
{
	if (sourceIndex.model() != m_pLibraryModel)
	{
		return nullptr;
	}

	size_t const numLibraries = m_controlsModels.size();
	size_t const row = static_cast<size_t>(sourceIndex.row());

	if (row >= numLibraries)
	{
		m_controlsModels.resize(row + 1);

		for (size_t i = numLibraries; i < row + 1; ++i)
		{
			m_controlsModels[i] = new CResourceControlsModel(m_pAssetsManager, m_pAssetsManager->GetLibrary(i));
		}
	}

	return m_controlsModels[row];
}

//////////////////////////////////////////////////////////////////////////
char const* CResourceSelectorDialog::ChooseItem(char const* szCurrentValue)
{
	if (std::strcmp(szCurrentValue, "") != 0)
	{
		s_previousControlName = szCurrentValue;
	}

	// Automatically select the previous control
	if ((m_pFilterProxyModel != nullptr) && !s_previousControlName.empty())
	{
		QModelIndex const& index = FindItem(s_previousControlName);

		if (index.isValid())
		{
			m_pTreeView->setCurrentIndex(index);
		}
	}

	if (exec() == QDialog::Accepted)
	{
		return s_previousControlName.c_str();
	}

	return szCurrentValue;
}

//////////////////////////////////////////////////////////////////////////
void CResourceSelectorDialog::UpdateSelectedControl()
{
	if (m_pAssetsManager != nullptr)
	{
		QModelIndex const& index = m_pTreeView->currentIndex();

		if (index.isValid())
		{
			CSystemAsset const* const pAsset = AudioModelUtils::GetAssetFromIndex(index);
			m_selectionIsValid = ((pAsset != nullptr) && (pAsset->GetType() == m_eType));

			if (m_selectionIsValid)
			{
				s_previousControlName = pAsset->GetName();
			}

			m_pDialogButtons->button(QDialogButtonBox::Ok)->setEnabled(m_selectionIsValid);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CResourceSelectorDialog::SetTextFilter(QString const& filter)
{
	if (m_sFilter != filter)
	{
		if (m_sFilter.isEmpty() && !filter.isEmpty())
		{
			m_pTreeView->BackupExpanded();
			m_pTreeView->expandAll();
		}
		else if (!m_sFilter.isEmpty() && filter.isEmpty())
		{
			m_pTreeView->collapseAll();
			m_pTreeView->RestoreExpanded();
		}

		m_sFilter = filter;
		ApplyFilter();

		QModelIndex const& currentIndex = m_pTreeView->currentIndex();

		if (currentIndex.isValid())
		{
			m_pTreeView->scrollTo(currentIndex);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CResourceSelectorDialog::ApplyFilter()
{
	QModelIndex index = m_pFilterProxyModel->index(0, 0);

	for (int i = 0; index.isValid(); ++i)
	{
		ApplyFilter(index);
		index = index.sibling(i, 0);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CResourceSelectorDialog::ApplyFilter(QModelIndex const& parent)
{
	if (parent.isValid())
	{
		bool isChildValid = false;
		QModelIndex child = parent.child(0, 0);

		for (int i = 1; child.isValid(); ++i)
		{
			if (ApplyFilter(child))
			{
				isChildValid = true;
			}

			child = parent.child(i, 0);
		}

		// If it has valid children or it is a valid control (not a folder), show it
		CSystemAsset const* const pAsset = AudioModelUtils::GetAssetFromIndex(parent);

		if (isChildValid || ((pAsset != nullptr) && pAsset->GetType() == m_eType && IsValid(parent)))
		{
			m_pTreeView->setRowHidden(parent.row(), parent.parent(), false);
			return true;
		}
		else
		{
			m_pTreeView->setRowHidden(parent.row(), parent.parent(), true);

		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CResourceSelectorDialog::IsValid(QModelIndex const& index)
{
	QString const& name = index.data(Qt::DisplayRole).toString();

	if (m_sFilter.isEmpty() || name.contains(m_sFilter, Qt::CaseInsensitive))
	{
		CSystemAsset* const pAsset = AudioModelUtils::GetAssetFromIndex(index);

		if ((pAsset != nullptr) && (pAsset->GetType() == m_eType))
		{
			CSystemControl const* const pControl = static_cast<CSystemControl*>(pAsset);

			if (pControl != nullptr)
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
QSize CResourceSelectorDialog::sizeHint() const
{
	return QSize(400, 900);
}

//////////////////////////////////////////////////////////////////////////
void CResourceSelectorDialog::SetScope(Scope const scope)
{
	m_scope = scope;
	ApplyFilter();
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CResourceSelectorDialog::FindItem(string const& sControlName)
{
	QModelIndexList const indexes = m_pFilterProxyModel->match(m_pFilterProxyModel->index(0, 0, QModelIndex()), Qt::DisplayRole, QtUtil::ToQString(sControlName), -1, Qt::MatchRecursive);
	
	if (!indexes.empty())
	{
		CSystemAsset* const pAsset = AudioModelUtils::GetAssetFromIndex(indexes[0]);

		if (pAsset != nullptr)
		{
			CSystemControl const* const pControl = static_cast<CSystemControl*>(pAsset);

			if (pControl != nullptr)
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
bool CResourceSelectorDialog::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if ((pEvent->type() == QEvent::KeyRelease) && m_selectionIsValid)
	{
		QKeyEvent const* const pKeyEvent = static_cast<QKeyEvent*>(pEvent);

		if ((pKeyEvent != nullptr) && (pKeyEvent->key() == Qt::Key_Space))
		{
			QModelIndex const& index = m_pTreeView->currentIndex();

			if (index.isValid())
			{
				CSystemAsset const* const pAsset = AudioModelUtils::GetAssetFromIndex(index);

				if ((pAsset != nullptr) && (pAsset->GetType() == ESystemItemType::Trigger))
				{
					CAudioControlsEditorPlugin::ExecuteTrigger(pAsset->GetName());
				}
			}
		}
	}

	return QWidget::eventFilter(pObject, pEvent);
}

//////////////////////////////////////////////////////////////////////////
void CResourceSelectorDialog::StopTrigger()
{
	CAudioControlsEditorPlugin::StopTriggerExecution();
}

//////////////////////////////////////////////////////////////////////////
void CResourceSelectorDialog::ItemDoubleClicked(QModelIndex const& modelIndex)
{
	if (m_selectionIsValid)
	{
		CSystemAsset const* const pAsset = AudioModelUtils::GetAssetFromIndex(modelIndex);

		if ((pAsset != nullptr) && (pAsset->GetType() == m_eType))
		{
			s_previousControlName = pAsset->GetName();
			accept();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CResourceSelectorDialog::OnContextMenu(QPoint const& pos)
{
	QModelIndex const& index = m_pTreeView->currentIndex();

	if (index.isValid())
	{
		CSystemAsset const* const pAsset = AudioModelUtils::GetAssetFromIndex(index);

		if ((pAsset != nullptr) && (pAsset->GetType() == ESystemItemType::Trigger))
		{
			QMenu* const pContextMenu = new QMenu();

			pContextMenu->addAction(tr("Execute Trigger"), [&]()
			{
				CAudioControlsEditorPlugin::ExecuteTrigger(pAsset->GetName());
			});

			pContextMenu->exec(QCursor::pos());
		}
	}
}
} // namespace ACE

// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ATLControlsResourceDialog.h"
#include "AudioControlsEditorPlugin.h"
#include "ATLControlsModel.h"
#include "QAudioControlTreeWidget.h"
#include "QtUtil.h"

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QBoxLayout>
#include <QApplication>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QPushButton>

namespace ACE
{
string ATLControlsDialog::ms_controlName = "";

ATLControlsDialog::ATLControlsDialog(QWidget* pParent, EACEControlType eType)
	: CEditorDialog("ATLControlsDialog")
	, m_eType(eType)
{
	setWindowTitle("Choose...");
	setWindowModality(Qt::ApplicationModal);

	QBoxLayout* pLayout = new QBoxLayout(QBoxLayout::TopToBottom);
	setLayout(pLayout);

	QLineEdit* pTextFilterLineEdit = new QLineEdit(this);
	pTextFilterLineEdit->setAlignment(Qt::AlignLeading | Qt::AlignLeft | Qt::AlignVCenter);
	pTextFilterLineEdit->setPlaceholderText(QApplication::translate("ATLControlsPanel", "Search", 0));
	connect(pTextFilterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(SetTextFilter(QString)));
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

	m_pATLModel = CAudioControlsEditorPlugin::GetATLModel();
	m_pTreeModel = CAudioControlsEditorPlugin::GetControlsTree();
	m_pProxyModel = new QAudioControlSortProxy(this);
	m_pProxyModel->setSourceModel(m_pTreeModel);
	m_pControlTree->setModel(m_pProxyModel);

	m_pDialogButtons = new QDialogButtonBox(this);
	m_pDialogButtons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(m_pDialogButtons, SIGNAL(accepted()), this, SLOT(accept()));
	connect(m_pDialogButtons, SIGNAL(rejected()), this, SLOT(reject()));
	pLayout->addWidget(m_pDialogButtons, 0);

	connect(m_pControlTree->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(UpdateSelectedControl()));
	ApplyFilter();
	UpdateSelectedControl();
	m_pControlTree->setFocus();

	m_pControlTree->installEventFilter(this);
	m_pControlTree->viewport()->installEventFilter(this);

	connect(m_pControlTree->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)), this, SLOT(StopTrigger()));
	connect(m_pControlTree, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(ItemDoubleClicked(const QModelIndex &)));

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

const char* ATLControlsDialog::ChooseItem(const char* szCurrentValue)
{
	if (std::strcmp(szCurrentValue, "") != 0)
	{
		ms_controlName = szCurrentValue;
	}

	// Automatically select the current control
	if (m_pTreeModel && m_pProxyModel && !ms_controlName.empty())
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
	if (m_pATLModel)
	{
		ControlList controls;
		QModelIndexList indexes = m_pControlTree->selectionModel()->selectedIndexes();
		if (!indexes.empty())
		{
			const QModelIndex& index = indexes[0];
			if (index.isValid() && index.data(eDataRole_Type) == eItemType_AudioControl)
			{
				const CID id = index.data(eDataRole_Id).toUInt();
				if (id != ACE_INVALID_ID)
				{
					CATLControl* pControl = m_pATLModel->GetControlByID(id);
					if (pControl && pControl->GetType() == m_eType)
					{
						ms_controlName = pControl->GetName();
					}
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
		if (bChildValid || ((parent.data(eDataRole_Type) == eItemType_AudioControl) && IsValid(parent)))
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
		if (index.data(eDataRole_Type) == eItemType_AudioControl)
		{
			const CATLControl* pControl = m_pATLModel->GetControlByID(index.data(eDataRole_Id).toUInt());
			if (pControl)
			{
				const Scope scope = pControl->GetScope();
				if (pControl->GetType() == m_eType && (scope == m_pATLModel->GetGlobalScope() || scope == m_scope))
				{
					return true;
				}
				return false;
			}
		}
		return true;
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
	if (m_pTreeModel && m_pATLModel)
	{
		QModelIndexList indexes = m_pTreeModel->match(m_pTreeModel->index(0, 0, QModelIndex()), Qt::DisplayRole, QtUtil::ToQString(sControlName), -1, Qt::MatchRecursive);
		if (!indexes.empty())
		{
			const int size = indexes.size();
			for (int i = 0; i < size; ++i)
			{
				QModelIndex index = indexes[i];
				if (index.isValid() && (index.data(eDataRole_Type) == eItemType_AudioControl))
				{
					const CID id = index.data(eDataRole_Id).toUInt();
					if (id != ACE_INVALID_ID)
					{
						CATLControl* pControl = m_pATLModel->GetControlByID(id);
						if (pControl && pControl->GetType() == m_eType)
						{
							const Scope scope = pControl->GetScope();
							if (scope == m_pATLModel->GetGlobalScope() || scope == m_scope)
							{
								return m_pProxyModel->mapFromSource(index);
							}
						}
					}
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
			if (index.isValid() && index.data(eDataRole_Type) == eItemType_AudioControl)
			{
				CAudioControlsEditorPlugin::ExecuteTrigger(QtUtil::ToString(index.data(Qt::DisplayRole).toString()));
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
	if (modelIndex.isValid())
	{
		if (modelIndex.data(eDataRole_Type) == eItemType_AudioControl)
		{
			const CID id = modelIndex.data(eDataRole_Id).toUInt();
			if (id != ACE_INVALID_ID)
			{
				const CATLControl* const pControl = m_pATLModel->GetControlByID(id);
				if (pControl && pControl->GetType() == m_eType)
				{
					ms_controlName = pControl->GetName();
					accept();
				}
			}
		}
	}
}
}

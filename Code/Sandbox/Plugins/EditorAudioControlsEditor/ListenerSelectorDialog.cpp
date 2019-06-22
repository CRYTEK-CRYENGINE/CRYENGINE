// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ListenerSelectorDialog.h"
#include "ListenerSelectorModel.h"
#include "TreeView.h"

#include <QtUtil.h>
#include <QSearchBox.h>
#include <ProxyModels/AttributeFilterProxyModel.h>

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QPushButton>

namespace ACE
{
string CListenerSelectorDialog::s_previousListenerName = "";

//////////////////////////////////////////////////////////////////////////
CListenerSelectorDialog::CListenerSelectorDialog(QWidget* const pParent)
	: CEditorDialog("AudioListenerSelectorDialog", pParent)
	, m_selectionIsValid(false)
	, m_pModel(new CListenerSelectorModel(this))
	, m_pAttributeFilterProxyModel(new QAttributeFilterProxyModel(QAttributeFilterProxyModel::BaseBehavior, this))
	, m_pTreeView(new CTreeView(this, QAdvancedTreeView::Behavior::None))
	, m_pSearchBox(new QSearchBox(this))
	, m_pDialogButtons(new QDialogButtonBox(this))
{
	setWindowTitle("Select Audio Listener");
	setModal(true);

	m_pAttributeFilterProxyModel->setSourceModel(m_pModel);

	m_pTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_pTreeView->setDragEnabled(false);
	m_pTreeView->setDragDropMode(QAbstractItemView::NoDragDrop);
	m_pTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->setContextMenuPolicy(Qt::NoContextMenu);
	m_pTreeView->setModel(m_pAttributeFilterProxyModel);
	m_pTreeView->sortByColumn(0, Qt::AscendingOrder);
	m_pTreeView->setItemsExpandable(false);
	m_pTreeView->setRootIsDecorated(false);
	m_pTreeView->header()->setContextMenuPolicy(Qt::PreventContextMenu);

	m_pSearchBox->SetModel(m_pAttributeFilterProxyModel);
	m_pSearchBox->SetAutoExpandOnSearch(m_pTreeView);
	m_pSearchBox->EnableContinuousSearch(true);

	m_pDialogButtons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	m_pDialogButtons->button(QDialogButtonBox::Ok)->setEnabled(false);

	auto const pWindowLayout = new QVBoxLayout(this);
	setLayout(pWindowLayout);
	pWindowLayout->addWidget(m_pSearchBox);
	pWindowLayout->addWidget(m_pTreeView);
	pWindowLayout->addWidget(m_pDialogButtons);

	QObject::connect(m_pTreeView, &CTreeView::doubleClicked, this, &CListenerSelectorDialog::OnItemDoubleClicked);
	QObject::connect(m_pTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CListenerSelectorDialog::OnUpdateSelectedListener);
	QObject::connect(m_pDialogButtons, &QDialogButtonBox::accepted, this, &CListenerSelectorDialog::accept);
	QObject::connect(m_pDialogButtons, &QDialogButtonBox::rejected, this, &CListenerSelectorDialog::reject);

	m_pSearchBox->signalOnSearch.Connect([&]()
		{
			m_pTreeView->scrollTo(m_pTreeView->currentIndex());
		}, reinterpret_cast<uintptr_t>(this));

	OnUpdateSelectedListener();
}

//////////////////////////////////////////////////////////////////////////
CListenerSelectorDialog::~CListenerSelectorDialog()
{
	m_pSearchBox->signalOnSearch.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pModel->deleteLater();
}

//////////////////////////////////////////////////////////////////////////
QSize CListenerSelectorDialog::sizeHint() const
{
	return QSize(400, 900);
}

//////////////////////////////////////////////////////////////////////////
CListenerSelectorDialog::SResourceSelectionDialogResult CListenerSelectorDialog::ChooseListener(char const* szCurrentValue)
{
	SResourceSelectionDialogResult result{ szCurrentValue, false };

	if (strcmp(szCurrentValue, "") != 0)
	{
		s_previousListenerName = szCurrentValue;
	}

	// Automatically select the previous listener
	if ((m_pAttributeFilterProxyModel != nullptr) && !s_previousListenerName.empty())
	{
		QModelIndex const& index = FindListenerIndex(s_previousListenerName);

		if (index.isValid())
		{
			m_pTreeView->setCurrentIndex(index);
		}
	}

	bool accepted = exec() == QDialog::Accepted;
	result.selectionAccepted = accepted;

	if (accepted)
	{
		result.selectedListener = s_previousListenerName.c_str();
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CListenerSelectorDialog::OnUpdateSelectedListener()
{
	QModelIndex const& index = m_pTreeView->currentIndex();

	if (index.isValid())
	{
		string const name = m_pModel->GetListenerNameFromIndex(index);
		m_selectionIsValid = !name.empty();

		if (m_selectionIsValid)
		{
			s_previousListenerName = name;
		}

		m_pDialogButtons->button(QDialogButtonBox::Ok)->setEnabled(m_selectionIsValid);
	}
}

//////////////////////////////////////////////////////////////////////////
QModelIndex CListenerSelectorDialog::FindListenerIndex(string const& listenerName)
{
	QModelIndex modelIndex = QModelIndex();

	auto const& matches = m_pAttributeFilterProxyModel->match(m_pAttributeFilterProxyModel->index(0, 0, QModelIndex()), Qt::DisplayRole, QtUtil::ToQString(listenerName), 1, Qt::MatchRecursive);

	if (!matches.empty())
	{
		modelIndex = matches[0];
	}

	return modelIndex;
}

//////////////////////////////////////////////////////////////////////////
void CListenerSelectorDialog::OnItemDoubleClicked(QModelIndex const& index)
{
	if (m_selectionIsValid)
	{
		string const name = m_pModel->GetListenerNameFromIndex(index);

		if (!name.empty())
		{
			s_previousListenerName = name;
			accept();
		}
	}
}
} // namespace ACE

// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QConnectionsWidget.h"
#include <IAudioSystemEditor.h>
#include <IAudioSystemItem.h>
#include "AudioAssets.h"
#include "AudioControlsEditorPlugin.h"
#include "IEditor.h"
#include "QtUtil.h"
#include "ImplementationManager.h"
#include "AudioSystemPanel.h"
#include "AudioSystemModel.h"
#include "AdvancedTreeView.h"
#include "QConnectionsModel.h"
#include "IUndoObject.h"
#include "Controls/QuestionDialog.h"

#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>
#include <Serialization/QPropertyTree/QPropertyTree.h>

#include <QDropEvent>
#include <QEvent>
#include <QMimeData>
#include <QMenu>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QSplitter>
#include <QSizePolicy>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
QConnectionsWidget::QConnectionsWidget(QWidget* pParent)
	: QWidget(pParent)
	, m_pControl(nullptr)
	, m_pConnectionModel(new QConnectionModel())
	, m_pConnectionsView(new CAdvancedTreeView())
{
	m_pConnectionsView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pConnectionsView->setDragEnabled(false);
	m_pConnectionsView->setAcceptDrops(true);
	m_pConnectionsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_pConnectionsView->setDragDropMode(QAbstractItemView::DropOnly);
	m_pConnectionsView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pConnectionsView->setModel(m_pConnectionModel);
	m_pConnectionsView->installEventFilter(this);
	m_pConnectionsView->header()->setMinimumSectionSize(50);
	m_pConnectionsView->setIndentation(0);
	
	connect(m_pConnectionsView, &CAdvancedTreeView::customContextMenuRequested, [&](QPoint const& pos)
	{
		int const selectionCount = m_pConnectionsView->selectionModel()->selectedRows().count();

		if (selectionCount > 0)
		{
			QMenu contextMenu(tr("Context menu"), this);

			char const* actionName = "Remove Connection";

			if (selectionCount > 1)
			{
				actionName = "Remove Connections";
			}

			QAction* pAction = contextMenu.addAction(tr(actionName));

			connect(pAction, &QAction::triggered, [&]()
			{
				RemoveSelectedConnection();
			});

			contextMenu.exec(QCursor::pos());
		}
	});

	m_pConnectionPropertiesFrame = new QFrame(this);
	m_pConnectionPropertiesFrame->setAutoFillBackground(false);
	m_pConnectionPropertiesFrame->setStyleSheet(QStringLiteral("#m_pConnectionPropertiesFrame { border: 1px solid #363636; }"));
	m_pConnectionPropertiesFrame->setFrameShape(QFrame::Box);
	m_pConnectionPropertiesFrame->setFrameShadow(QFrame::Plain);
	m_pConnectionPropertiesFrame->setHidden(true);
	m_pConnectionPropertiesFrame->setMinimumHeight(20);

	m_pConnectionProperties = new QPropertyTree();
	m_pConnectionProperties->setSizeToContent(false);

	QVBoxLayout* const pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(0, 0, 0, 0);
	pLayout->addWidget(m_pConnectionProperties);
	m_pConnectionPropertiesFrame->setLayout(pLayout);

	QSplitter* const pSplitter = new QSplitter(Qt::Vertical);
	pSplitter->addWidget(m_pConnectionsView);
	pSplitter->addWidget(m_pConnectionPropertiesFrame);
	pSplitter->setCollapsible(0, false);
	pSplitter->setCollapsible(1, false);

	QVBoxLayout* const pMainLayout = new QVBoxLayout();
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	pMainLayout->addWidget(pSplitter);
	setLayout(pMainLayout);

	// First hide all the columns except "Name" and "Path"
	int const count = m_pConnectionModel->columnCount(QModelIndex());

	for (int i = 2; i < count; ++i)
	{
		m_pConnectionsView->SetColumnVisible(i, false);
	}

	// Then hide the entire widget.
	setHidden(true);

	CAudioControlsEditorPlugin::GetAssetsManager()->signalConnectionRemoved.Connect([&](CAudioControl* pControl)
		{
			if (m_pControl == pControl)
			{
			  // clear the selection if a connection is removed
			  m_pConnectionsView->selectionModel()->clear();
			  RefreshConnectionProperties();
			}
		}, reinterpret_cast<uintptr_t>(this));

	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationAboutToChange.Connect([&]()
		{
			m_pConnectionsView->selectionModel()->clear();
			RefreshConnectionProperties();
		}, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
QConnectionsWidget::~QConnectionsWidget()
{
	CAudioControlsEditorPlugin::GetAssetsManager()->signalConnectionRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationAboutToChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
void QConnectionsWidget::Init()
{
	connect(m_pConnectionsView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &QConnectionsWidget::RefreshConnectionProperties);
}

//////////////////////////////////////////////////////////////////////////
bool QConnectionsWidget::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (pEvent->type() == QEvent::KeyPress)
	{
		QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(pEvent);

		if (pKeyEvent && pKeyEvent->key() == Qt::Key_Delete && pObject == m_pConnectionsView)
		{
			RemoveSelectedConnection();
			return true;
		}
	}
	return QWidget::eventFilter(pObject, pEvent);
}

//////////////////////////////////////////////////////////////////////////
void QConnectionsWidget::RemoveSelectedConnection()
{
	if (m_pControl)
	{
		CQuestionDialog messageBox;
		QModelIndexList const selectedIndices = m_pConnectionsView->selectionModel()->selectedRows();

		if (!selectedIndices.empty())
		{
			int const size = selectedIndices.length();
			QString text;

			if (size == 1)
			{
				text = R"(Are you sure you want to delete the connection between ")" + QtUtil::ToQString(m_pControl->GetName()) + R"(" and ")" + selectedIndices[0].data(Qt::DisplayRole).toString() + R"("?)";
			}
			else
			{
				text = "Are you sure you want to delete the " + QString::number(size) + " selected connections?";
			}

			messageBox.SetupQuestion("Audio Controls Editor", text);

			if (messageBox.Execute() == QDialogButtonBox::Yes)
			{
				CUndo undo("Disconnected Audio Control from Audio System");
				IAudioSystemEditor* pAudioSystemEditorImpl = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();

				if (pAudioSystemEditorImpl)
				{
					std::vector<IAudioSystemItem*> items;
					items.reserve(selectedIndices.size());

					for (QModelIndex index : selectedIndices)
					{
						CID id = index.data(QConnectionModel::eConnectionModelRoles_Id).toInt();
						items.push_back(pAudioSystemEditorImpl->GetControl(id));
					}

					for (IAudioSystemItem* pItem : items)
					{
						if (pItem)
						{
							m_pControl->RemoveConnection(pItem);
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void QConnectionsWidget::SetControl(CAudioControl* pControl)
{
	if (m_pControl != pControl)
	{
		m_pControl = pControl;
		Reload();
	}
}

//////////////////////////////////////////////////////////////////////////
void QConnectionsWidget::Reload()
{
	m_pConnectionModel->Init(m_pControl);
	m_pConnectionsView->selectionModel()->clear();
	m_pConnectionsView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	RefreshConnectionProperties();
}

//////////////////////////////////////////////////////////////////////////
void QConnectionsWidget::RefreshConnectionProperties()
{
	ConnectionPtr pConnection;
	if (m_pControl)
	{
		QModelIndexList const selectedIndices = m_pConnectionsView->selectionModel()->selectedIndexes();

		if (!selectedIndices.empty())
		{
			QModelIndex const index = selectedIndices[0];

			if (index.isValid())
			{
				CID const id = index.data(QConnectionModel::eConnectionModelRoles_Id).toInt();
				pConnection = m_pControl->GetConnection(id);
			}
		}
	}

	if (pConnection && pConnection->HasProperties())
	{
		m_pConnectionProperties->attach(Serialization::SStruct(*pConnection.get()));
		m_pConnectionPropertiesFrame->setHidden(false);
	}
	else
	{
		m_pConnectionProperties->detach();
		m_pConnectionPropertiesFrame->setHidden(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void QConnectionsWidget::BackupTreeViewStates()
{
	m_pConnectionsView->BackupSelection();
}

//////////////////////////////////////////////////////////////////////////
void QConnectionsWidget::RestoreTreeViewStates()
{
	m_pConnectionsView->RestoreSelection();
}
} // namespace ACE

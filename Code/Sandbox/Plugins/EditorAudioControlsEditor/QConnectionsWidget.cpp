// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QConnectionsWidget.h"
#include <IAudioSystemEditor.h>
#include <IAudioSystemItem.h>
#include "AudioControl.h"
#include "AudioControlsEditorPlugin.h"
#include "IEditor.h"
#include "QtUtil.h"
#include "ImplementationManager.h"
#include "AudioSystemPanel.h"
#include "AudioSystemModel.h"
#include "QConnectionsModel.h"

#include <QDropEvent>
#include <QEvent>
#include <QMimeData>
#include <QMessageBox>
#include <QMenu>
#include <QVBoxLayout>
#include <QTreeView>
#include <QHeaderView>

#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>
#include <QPropertyTree/QPropertyTree.h>
#include <QAdvancedTreeView.h>

namespace ACE
{

QConnectionsWidget::QConnectionsWidget(QWidget* pParent)
	: QWidget(pParent)
	, m_pControl(nullptr)
	, m_pConnectionModel(new QConnectionModel())
	, m_pConnectionsView(new QAdvancedTreeView())
{
	resize(326, 450);
	setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed));
	setMinimumSize(QSize(0, 450));
	setMaximumSize(QSize(16777215, 450));
	QVBoxLayout* pHorizontalLayout = new QVBoxLayout(this);
	pHorizontalLayout->setSpacing(0);
	pHorizontalLayout->setContentsMargins(0, 0, 0, 0);

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
	connect(m_pConnectionsView, &QTreeView::customContextMenuRequested, [&](const QPoint& pos)
		{
			QMenu contextMenu(tr("Context menu"), this);
			QAction* pAction = contextMenu.addAction(tr("Remove Connection"));
			connect(pAction, &QAction::triggered, [&]()
			{
				RemoveSelectedConnection();
			});
			contextMenu.exec(m_pConnectionsView->mapToGlobal(pos));
	  });

	m_pConnectionPropertiesFrame = new QFrame(this);
	m_pConnectionPropertiesFrame->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum));
	m_pConnectionPropertiesFrame->setAutoFillBackground(false);
	m_pConnectionPropertiesFrame->setStyleSheet(QStringLiteral("#m_pConnectionPropertiesFrame { border: 1px solid #363636; }"));
	m_pConnectionPropertiesFrame->setFrameShape(QFrame::Box);
	m_pConnectionPropertiesFrame->setFrameShadow(QFrame::Plain);
	m_pConnectionPropertiesFrame->setHidden(true);

	QVBoxLayout* pVerticalLayout = new QVBoxLayout(m_pConnectionPropertiesFrame);
	pVerticalLayout->setSpacing(0);
	pVerticalLayout->setContentsMargins(6, 6, 6, 6);
	m_pConnectionProperties = new QPropertyTree(m_pConnectionPropertiesFrame);
	m_pConnectionProperties->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum));
	m_pConnectionProperties->setSizeToContent(true);
	connect(m_pConnectionProperties, &QPropertyTree::signalChanged, [&]()
		{
			if (m_pControl)
			{
			  m_pControl->SignalControlModified();
			}
	  });

	pVerticalLayout->addWidget(m_pConnectionProperties);
	pHorizontalLayout->addWidget(m_pConnectionsView);
	pHorizontalLayout->addWidget(m_pConnectionPropertiesFrame);

	// Hide all the columns except "Name" and "Path"
	const int count = m_pConnectionModel->columnCount(QModelIndex());
	for (int i = 2; i < count; ++i)
	{
		m_pConnectionsView->SetColumnVisible(i, false);
	}
}

void QConnectionsWidget::Init()
{
	connect(m_pConnectionsView->selectionModel(), &QItemSelectionModel::selectionChanged, [&]()
		{
			ConnectionPtr pConnection;
			if (m_pControl)
			{
			  QModelIndexList selectedIndices = m_pConnectionsView->selectionModel()->selectedIndexes();
			  if (!selectedIndices.empty())
			  {
			    QModelIndex index = selectedIndices[0];
			    if (index.isValid())
			    {
			      const CID id = index.data(QConnectionModel::eConnectionModelRoles_Id).toInt();
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
	  });
}

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

void QConnectionsWidget::RemoveSelectedConnection()
{
	if (m_pControl)
	{
		QMessageBox messageBox;
		messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		messageBox.setDefaultButton(QMessageBox::Yes);
		messageBox.setWindowTitle("Audio Controls Editor");
		QModelIndexList selectedIndices = m_pConnectionsView->selectionModel()->selectedRows();

		if (!selectedIndices.empty())
		{
			const int size = selectedIndices.length();
			if (size == 1)
			{
				messageBox.setText("Are you sure you want to delete the connection between \"" + QtUtil::ToQString(m_pControl->GetName()) + "\" and \"" + selectedIndices[0].data(Qt::DisplayRole).toString() + "\"?");
			}
			else
			{
				messageBox.setText("Are you sure you want to delete the " + QString::number(size) + " selected connections?");
			}
			if (messageBox.exec() == QMessageBox::Yes)
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

void QConnectionsWidget::SetControl(CATLControl* pControl)
{
	if (m_pControl != pControl)
	{
		m_pControl = pControl;
		Reload();
	}
}

void QConnectionsWidget::Reload()
{
	m_pConnectionModel->Init(m_pControl);
	m_pConnectionsView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_pConnectionProperties->detach();
	m_pConnectionPropertiesFrame->setHidden(true);
}

}

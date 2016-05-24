// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ATLControlsPanel.h"
#include "AudioControl.h"
#include "ATLControlsModel.h"
#include "QAudioControlEditorIcons.h"
#include <IEditor.h>
#include <CrySystem/File/CryFile.h>
#include <CryString/CryPath.h>
#include <CryMath/Cry_Camera.h>
#include <CryAudio/IAudioSystem.h>
#include <IAudioSystemEditor.h>
#include <IAudioSystemItem.h>
#include "QtUtil.h"
#include <EditorStyleHelper.h>
#include "AudioControlsEditorPlugin.h"
#include "AudioSystemPanel.h"
#include "AudioSystemModel.h"
#include "QAudioControlTreeWidget.h"

#include <QWidgetAction>
#include <QPushButton>
#include <QPaintEvent>
#include <QPainter>
#include <QMessageBox>
#include <QMimeData>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QKeyEvent>
#include <QSortFilterProxyModel>
#include <QModelIndex>
#include <QVariant>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QVBoxLayout>

class QCheckableMenu final : public QMenu
{
public:
	QCheckableMenu(QWidget* pParent) : QMenu(pParent)
	{}

	virtual bool event(QEvent* event) override
	{
		switch (event->type())
		{
		case QEvent::KeyPress:
			{
				QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
				switch (keyEvent->key())
				{
				case Qt::Key_Space:
				case Qt::Key_Return:
				case Qt::Key_Enter:
					break;
				default:
					return QMenu::event(event);
				}
			}
		//Intentional fall through
		case QEvent::MouseButtonRelease:
			{
				auto action = activeAction();
				if (action)
				{
					action->trigger();
					return true;
				}
			}
		}
		return QMenu::event(event);
	}
};

namespace ACE
{
class QFilterButton : public QPushButton
{
public:
	QFilterButton(const QIcon& icon, const QString& text, QWidget* parent = 0) : QPushButton(icon, text, parent)
	{
		setStyleSheet("text-align: left;");
		setCheckable(true);
		setFlat(true);
	}
protected:
	void paintEvent(QPaintEvent* event)
	{
		QPushButton::paintEvent(event);
		if (isChecked())
		{
			QPainter painter(this);
			const int heightPadding = 4;
			const int widthPadding = 3;
			painter.setPen(QPen(GetStyleHelper()->highlightColor(), 2));
			painter.drawLine(QPoint(event->rect().width() - widthPadding, heightPadding),
			                 QPoint(event->rect().width() - widthPadding, event->rect().height() - heightPadding));
		}
	}
};

CATLControlsPanel::CATLControlsPanel(CATLControlsModel* pATLModel, QATLTreeModel* pATLControlsModel)
	: m_pATLModel(pATLModel)
	, m_pTreeModel(pATLControlsModel)
{
	resize(299, 674);
	setWindowTitle(tr("ATL Controls Panel"));
	setProperty("text", QVariant(tr("Reload")));

	QIcon icon;
	icon.addFile(QStringLiteral(":/Icons/Load_Icon.png"), QSize(), QIcon::Normal, QIcon::Off);
	setProperty("icon", QVariant(icon));
	QVBoxLayout* pMainLayout = new QVBoxLayout(this);
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	QHBoxLayout* pHorizontalLayout = new QHBoxLayout();
	m_pTextFilter = new QLineEdit(this);
	m_pTextFilter->setAlignment(Qt::AlignLeading | Qt::AlignLeft | Qt::AlignVCenter);
	pHorizontalLayout->addWidget(m_pTextFilter);

	QPushButton* pFiltersButton = new QPushButton(this);
	pFiltersButton->setEnabled(true);
	pFiltersButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	pFiltersButton->setMaximumSize(QSize(60, 16777215));
	pFiltersButton->setCheckable(false);
	pFiltersButton->setFlat(false);
	pFiltersButton->setToolTip(tr("Filter controls by type"));
	pFiltersButton->setText(tr("Filters"));
	pHorizontalLayout->addWidget(pFiltersButton);

	QCheckableMenu* pFilterMenu = new QCheckableMenu(this);

	m_pFilterActions[eACEControlType_Trigger] = new QAction(tr("Triggers"), this);
	connect(m_pFilterActions[eACEControlType_Trigger], &QAction::triggered, this, &CATLControlsPanel::ShowTriggers);
	m_pFilterActions[eACEControlType_Trigger]->setCheckable(true);
	m_pFilterActions[eACEControlType_Trigger]->setChecked(true);
	pFilterMenu->addAction(m_pFilterActions[eACEControlType_Trigger]);

	m_pFilterActions[eACEControlType_RTPC] = new QAction(tr("RTPCs"), this);
	connect(m_pFilterActions[eACEControlType_RTPC], &QAction::triggered, this, &CATLControlsPanel::ShowRTPCs);
	m_pFilterActions[eACEControlType_RTPC]->setCheckable(true);
	m_pFilterActions[eACEControlType_RTPC]->setChecked(true);
	pFilterMenu->addAction(m_pFilterActions[eACEControlType_RTPC]);

	m_pFilterActions[eACEControlType_Switch] = new QAction(tr("Switches"), this);
	connect(m_pFilterActions[eACEControlType_Switch], &QAction::triggered, this, &CATLControlsPanel::ShowSwitches);
	m_pFilterActions[eACEControlType_Switch]->setCheckable(true);
	m_pFilterActions[eACEControlType_Switch]->setChecked(true);
	pFilterMenu->addAction(m_pFilterActions[eACEControlType_Switch]);

	m_pFilterActions[eACEControlType_Environment] = new QAction(tr("Environments"), this);
	connect(m_pFilterActions[eACEControlType_Environment], &QAction::triggered, this, &CATLControlsPanel::ShowEnvironments);
	m_pFilterActions[eACEControlType_Environment]->setCheckable(true);
	m_pFilterActions[eACEControlType_Environment]->setChecked(true);
	pFilterMenu->addAction(m_pFilterActions[eACEControlType_Environment]);

	m_pFilterActions[eACEControlType_Preload] = new QAction(tr("Preloads"), this);
	connect(m_pFilterActions[eACEControlType_Preload], &QAction::triggered, this, &CATLControlsPanel::ShowPreloads);
	m_pFilterActions[eACEControlType_Preload]->setCheckable(true);
	m_pFilterActions[eACEControlType_Preload]->setChecked(true);
	pFilterMenu->addAction(m_pFilterActions[eACEControlType_Preload]);

	m_pFilterActions[eACEControlType_State] = nullptr; // we don't want to filter by state, so we don't include it

	pFiltersButton->setMenu(pFilterMenu);

	QPushButton* pAddButton = new QPushButton(this);
	pAddButton->setMaximumSize(QSize(60, 16777215));
	pHorizontalLayout->addWidget(pAddButton);

	pHorizontalLayout->setStretch(0, 1);
	pMainLayout->addLayout(pHorizontalLayout);

	m_pATLControlsTree = new QAudioControlsTreeView(this);
	m_pATLControlsTree->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
	m_pATLControlsTree->setDragEnabled(true);
	m_pATLControlsTree->setDragDropMode(QAbstractItemView::DragDrop);
	m_pATLControlsTree->setDefaultDropAction(Qt::MoveAction);
	m_pATLControlsTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pATLControlsTree->setSortingEnabled(true);
	m_pATLControlsTree->header()->setVisible(false);
	pMainLayout->addWidget(m_pATLControlsTree);

	m_pTextFilter->setToolTip(tr("Show only controls with this name"));
	m_pTextFilter->setPlaceholderText(tr("Search"));
	connect(m_pTextFilter, SIGNAL(textChanged(QString)), this, SLOT(SetFilterString(QString)));
	pAddButton->setToolTip(tr("Add new folder or control"));
	pAddButton->setText(tr("+ Add"));

	m_pATLControlsTree->installEventFilter(this);
	m_pATLControlsTree->viewport()->installEventFilter(this);

	// ************ Context Menu ************
	m_addItemMenu.addAction(GetControlTypeIcon(eACEControlType_Trigger), tr("Trigger"), this, SLOT(CreateTriggerControl()));
	m_addItemMenu.addAction(GetControlTypeIcon(eACEControlType_RTPC), tr("RTPC"), this, SLOT(CreateRTPCControl()));
	m_addItemMenu.addAction(GetControlTypeIcon(eACEControlType_Switch), tr("Switch"), this, SLOT(CreateSwitchControl()));
	m_addItemMenu.addAction(GetControlTypeIcon(eACEControlType_Environment), tr("Environment"), this, SLOT(CreateEnvironmentsControl()));
	m_addItemMenu.addAction(GetControlTypeIcon(eACEControlType_Preload), tr("Preload"), this, SLOT(CreatePreloadControl()));
	m_addItemMenu.addSeparator();
	m_addItemMenu.addAction(GetFolderIcon(), tr("Folder"), this, SLOT(CreateFolder()));
	pAddButton->setMenu(&m_addItemMenu);
	m_pATLControlsTree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pATLControlsTree, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(ShowControlsContextMenu(const QPoint &)));
	// *********************************

	m_pATLModel->AddListener(this);

	// Load data into tree control
	QAudioControlSortProxy* pProxyModel = new QAudioControlSortProxy(this);
	pProxyModel->setSourceModel(m_pTreeModel);
	m_pATLControlsTree->setModel(pProxyModel);
	m_pProxyModel = pProxyModel;

	connect(m_pATLControlsTree->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SIGNAL(SelectedControlChanged()));
	connect(m_pATLControlsTree->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)), this, SLOT(StopControlExecution()));
	connect(m_pTreeModel, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(ItemModified(QStandardItem*)));

	// Expand the root that holds all the controls
	QModelIndex index = m_pProxyModel->index(0, 0);
	if (index.isValid())
	{
		m_pATLControlsTree->setExpanded(index, true);
	}
}

void CATLControlsPanel::ApplyFilter()
{
	QModelIndex index = m_pProxyModel->index(0, 0);
	for (int i = 0; index.isValid(); ++i)
	{
		ApplyFilter(index);
		index = index.sibling(i, 0);
	}
}

void CATLControlsPanel::SetFilterString(const QString& filterText)
{
	if (m_sFilter != filterText)
	{
		if (m_sFilter.isEmpty() && !filterText.isEmpty())
		{
			m_pATLControlsTree->SaveExpandedState();
			m_pATLControlsTree->expandAll();
		}
		else if (!m_sFilter.isEmpty() && filterText.isEmpty())
		{
			m_pATLControlsTree->collapseAll();
			m_pATLControlsTree->RestoreExpandedState();
		}
		m_sFilter = filterText;
		ApplyFilter();
	}
}

bool CATLControlsPanel::ApplyFilter(const QModelIndex parent)
{
	if (parent.isValid())
	{
		bool bIsValid = false;
		bool bHasChildren = false;
		QModelIndex child = parent.child(0, 0);
		for (int i = 1; child.isValid(); ++i)
		{
			bHasChildren = true;
			if (ApplyFilter(child))
			{
				bIsValid = true;
			}
			child = parent.child(i, 0);
		}

		if (!bIsValid && IsValid(parent))
		{
			if (!bHasChildren || parent.data(eDataRole_Type) != eItemType_Folder)
			{
				// we want to hide empty folders, but show controls (ie. switches) even
				// if their children are hidden
				bIsValid = true;
			}
		}

		m_pATLControlsTree->setRowHidden(parent.row(), parent.parent(), !bIsValid);
		return bIsValid;
	}
	return false;
}

bool CATLControlsPanel::IsValid(const QModelIndex index)
{
	const QString sName = index.data(Qt::DisplayRole).toString();
	if (m_sFilter.isEmpty() || sName.contains(m_sFilter, Qt::CaseInsensitive))
	{
		if (index.data(eDataRole_Type) == eItemType_AudioControl)
		{
			const CATLControl* pControl = m_pATLModel->GetControlByID(index.data(eDataRole_Id).toUInt());
			if (pControl)
			{
				EACEControlType eType = pControl->GetType();
				if (eType == eACEControlType_State)
				{
					eType = eACEControlType_Switch;
				}
				if (m_visibleTypes[eType])
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

CATLControlsPanel::~CATLControlsPanel()
{
	StopControlExecution();
	m_pATLModel->RemoveListener(this);
}

bool CATLControlsPanel::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (pEvent->type() == QEvent::KeyRelease)
	{
		QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(pEvent);
		if (pKeyEvent)
		{
			if (pKeyEvent->key() == Qt::Key_Delete && !m_pATLControlsTree->IsEditing())
			{
				DeleteSelectedControl();
			}
			else if (pKeyEvent->key() == Qt::Key_Space)
			{
				ExecuteControl();
			}
		}
	}
	else if (pEvent->type() == QEvent::Drop)
	{
		QDropEvent* pDropEvent = static_cast<QDropEvent*>(pEvent);
		if (pDropEvent)
		{
			if (pDropEvent->source() != m_pATLControlsTree)
			{
				// External Drop
				HandleExternalDropEvent(pDropEvent);
				pDropEvent->accept();
			}
		}
	}
	return QWidget::eventFilter(pObject, pEvent);
}

ControlList CATLControlsPanel::GetSelectedControls()
{
	ControlList controls;
	QModelIndexList indexes = m_pATLControlsTree->selectionModel()->selectedIndexes();
	const int size = indexes.size();
	for (int i = 0; i < size; ++i)
	{
		if (indexes[i].isValid())
		{
			CID nID = indexes[i].data(eDataRole_Id).toUInt();
			if (nID != ACE_INVALID_ID)
			{
				controls.push_back(nID);
			}
		}
	}
	return controls;
}

void CATLControlsPanel::Reload()
{
	ResetFilters();

	// Expand the root that holds all the controls
	QModelIndex index = m_pProxyModel->index(0, 0);
	if (index.isValid())
	{
		m_pATLControlsTree->setExpanded(index, true);
	}
}

void CATLControlsPanel::ShowControlType(EACEControlType type, bool bShow, bool bExclusive)
{
	if (bExclusive)
	{
		for (int i = 0; i < eACEControlType_NumTypes; ++i)
		{
			EACEControlType controlType = (EACEControlType)i;
			m_visibleTypes[controlType] = !bShow;
			ControlTypeFiltered(controlType, !bShow);
			if (m_pFilterActions[controlType])
			{
				m_pFilterActions[controlType]->setChecked(!bShow);
			}
		}
	}
	m_visibleTypes[type] = bShow;
	if (type == eACEControlType_Switch)
	{
		m_visibleTypes[eACEControlType_State] = bShow;
	}
	ControlTypeFiltered(type, bShow);
	m_pFilterActions[type]->setChecked(bShow);
	ApplyFilter();
}

void CATLControlsPanel::ShowTriggers(bool bShow)
{
	ShowControlType(eACEControlType_Trigger, bShow, QGuiApplication::keyboardModifiers() & Qt::ControlModifier);
}

void CATLControlsPanel::ShowRTPCs(bool bShow)
{
	ShowControlType(eACEControlType_RTPC, bShow, QGuiApplication::keyboardModifiers() & Qt::ControlModifier);
}

void CATLControlsPanel::ShowEnvironments(bool bShow)
{
	ShowControlType(eACEControlType_Environment, bShow, QGuiApplication::keyboardModifiers() & Qt::ControlModifier);
}

void CATLControlsPanel::ShowSwitches(bool bShow)
{
	ShowControlType(eACEControlType_Switch, bShow, QGuiApplication::keyboardModifiers() & Qt::ControlModifier);
}

void CATLControlsPanel::ShowPreloads(bool bShow)
{
	ShowControlType(eACEControlType_Preload, bShow, QGuiApplication::keyboardModifiers() & Qt::ControlModifier);
}

void CATLControlsPanel::CreateRTPCControl()
{
	CATLControl* pControl = m_pTreeModel->CreateControl(eACEControlType_RTPC, "rtpc");
	if (pControl)
	{
		SelectItem(AddControl(pControl));
		m_pATLControlsTree->setFocus();
		m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
	}
}

void CATLControlsPanel::CreateSwitchControl()
{
	CATLControl* pControl = m_pTreeModel->CreateControl(eACEControlType_Switch, "switch");
	if (pControl)
	{
		SelectItem(AddControl(pControl));
		m_pATLControlsTree->setFocus();
		m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
	}
}

void CATLControlsPanel::CreateStateControl()
{
	QStandardItem* pSelectedItem = GetCurrentItem();
	if (pSelectedItem && m_pTreeModel->IsValidParent(pSelectedItem->index(), eACEControlType_State))
	{
		CATLControl* pControl = m_pTreeModel->CreateControl(eACEControlType_State, "state", GetControlFromItem(pSelectedItem));
		if (pControl)
		{
			SelectItem(AddControl(pControl));
			m_pATLControlsTree->setFocus();
			m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
		}
	}
}

void CATLControlsPanel::CreateTriggerControl()
{
	CATLControl* pControl = m_pTreeModel->CreateControl(eACEControlType_Trigger, "trigger");
	if (pControl)
	{
		SelectItem(AddControl(pControl));
		m_pATLControlsTree->setFocus();
		m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
	}
}

void CATLControlsPanel::CreateEnvironmentsControl()
{
	CATLControl* pControl = m_pTreeModel->CreateControl(eACEControlType_Environment, "environment");
	if (pControl)
	{
		SelectItem(AddControl(pControl));
		m_pATLControlsTree->setFocus();
		m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
	}
}

void CATLControlsPanel::CreatePreloadControl()
{
	CATLControl* pControl = m_pTreeModel->CreateControl(eACEControlType_Preload, "preload");
	if (pControl)
	{
		SelectItem(AddControl(pControl));
		m_pATLControlsTree->setFocus();
		m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
	}
}

QStandardItem* CATLControlsPanel::CreateFolder()
{
	QModelIndex parent = m_pATLControlsTree->currentIndex();
	while (parent.isValid() && (parent.data(eDataRole_Type) != eItemType_Folder))
	{
		parent = parent.parent();
	}

	QStandardItem* pParentItem = nullptr;
	if (parent.isValid())
	{
		pParentItem = m_pTreeModel->itemFromIndex(m_pProxyModel->mapToSource(parent));
		if (parent.isValid() && m_pATLControlsTree->isRowHidden(parent.row(), parent.parent()))
		{
			ResetFilters();
		}
	}

	QStandardItem* pFolder = m_pTreeModel->CreateFolder(pParentItem, "new_folder");
	SelectItem(pFolder);
	m_pATLControlsTree->setFocus();
	m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
	return pFolder;
}

QStandardItem* CATLControlsPanel::AddControl(CATLControl* pControl)
{
	if (pControl)
	{
		// Find a suitable parent for the new control starting from the one currently selected
		QStandardItem* pParent = GetCurrentItem();
		if (pParent)
		{
			while (pParent && !m_pTreeModel->IsValidParent(pParent->index(), pControl->GetType()))
			{
				pParent = pParent->parent();
			}
		}

		if (!pParent)
		{
			assert(pControl->GetType() != eACEControlType_State);
			pParent = CreateFolder();
		}

		return m_pTreeModel->AddControl(pControl, pParent);
	}
	return nullptr;
}

void CATLControlsPanel::ShowControlsContextMenu(const QPoint& pos)
{
	QMenu contextMenu(tr("Context menu"), this);
	QMenu addMenu(tr("Add"));

	QModelIndex index = m_pATLControlsTree->currentIndex();

	const bool bIsAudioAsset = index.isValid() && index.data(eDataRole_Type) != eItemType_Invalid;

	if (bIsAudioAsset)
	{
		CATLControl* pControl = GetControlFromIndex(index);
		if (pControl)
		{
			EACEControlType eControlType = pControl->GetType();
			switch (pControl->GetType())
			{
			case eACEControlType_Trigger:
				contextMenu.addAction(tr("Execute Trigger"), this, SLOT(ExecuteControl()));
				contextMenu.addSeparator();
				break;
			case eACEControlType_Switch:
			case eACEControlType_State:
				addMenu.addAction(GetControlTypeIcon(eACEControlType_State), tr("State"), this, SLOT(CreateStateControl()));
				addMenu.addSeparator();
				break;
			}
		}

		addMenu.addAction(GetControlTypeIcon(eACEControlType_Trigger), tr("Trigger"), this, SLOT(CreateTriggerControl()));
		addMenu.addAction(GetControlTypeIcon(eACEControlType_RTPC), tr("RTPC"), this, SLOT(CreateRTPCControl()));
		addMenu.addAction(GetControlTypeIcon(eACEControlType_Switch), tr("Switch"), this, SLOT(CreateSwitchControl()));
		addMenu.addAction(GetControlTypeIcon(eACEControlType_Environment), tr("Environment"), this, SLOT(CreateEnvironmentsControl()));
		addMenu.addAction(GetControlTypeIcon(eACEControlType_Preload), tr("Preload"), this, SLOT(CreatePreloadControl()));
		addMenu.addSeparator();
	}

	addMenu.addAction(GetFolderIcon(), tr("Folder"), this, SLOT(CreateFolder()));
	contextMenu.addMenu(&addMenu);

	if (bIsAudioAsset)
	{
		QAction* pAction = new QAction(tr("Rename"), this);
		connect(pAction, &QAction::triggered, [&]()
			{
				m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
		  });
		contextMenu.addAction(pAction);

		pAction = new QAction(tr("Delete"), this);
		connect(pAction, SIGNAL(triggered()), this, SLOT(DeleteSelectedControl()));
		contextMenu.addAction(pAction);
	}

	contextMenu.addSeparator();
	QAction* pAction = new QAction(tr("Expand All"), this);
	connect(pAction, SIGNAL(triggered()), m_pATLControlsTree, SLOT(expandAll()));
	contextMenu.addAction(pAction);
	pAction = new QAction(tr("Collapse All"), this);
	connect(pAction, SIGNAL(triggered()), m_pATLControlsTree, SLOT(collapseAll()));
	contextMenu.addAction(pAction);
	contextMenu.exec(m_pATLControlsTree->mapToGlobal(pos));
}

void CATLControlsPanel::DeleteSelectedControl()
{
	QMessageBox messageBox;
	QModelIndexList indexList = m_pATLControlsTree->selectionModel()->selectedIndexes();
	const int size = indexList.length();
	if (size > 0)
	{
		if (size == 1)
		{
			QModelIndex index = m_pProxyModel->mapToSource(indexList[0]);
			QStandardItem* pItem = m_pTreeModel->itemFromIndex(index);
			if (pItem)
			{
				messageBox.setText("Are you sure you want to delete \"" + pItem->text() + "\"?.");
			}
		}
		else
		{
			messageBox.setText("Are you sure you want to delete the selected controls and folders?.");
		}
		messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		messageBox.setDefaultButton(QMessageBox::Yes);
		messageBox.setWindowTitle("Audio Controls Editor");
		if (messageBox.exec() == QMessageBox::Yes)
		{
			CUndo undo("Audio Control Removed");

			QModelIndexList sourceIndexList;
			for (int i = 0; i < size; ++i)
			{
				sourceIndexList.push_back(m_pProxyModel->mapToSource(indexList[i]));
			}
			m_pTreeModel->RemoveItems(sourceIndexList);
		}
	}
}

void CATLControlsPanel::OnControlAdded(CATLControl* pControl)
{
	// Remove filters if the control added is hidden
	EACEControlType controlType = pControl->GetType();
	if (!m_visibleTypes[controlType])
	{
		m_visibleTypes[controlType] = true;
		if (m_pFilterActions[controlType])
		{
			m_pFilterActions[controlType]->setChecked(true);
		}
		ControlTypeFiltered(controlType, true);
	}
	m_pTextFilter->setText("");
	ApplyFilter();
}

void CATLControlsPanel::ExecuteControl()
{
	const CATLControl* const pControl = GetControlFromIndex(m_pATLControlsTree->currentIndex());
	if (pControl)
	{
		CAudioControlsEditorPlugin::ExecuteTrigger(pControl->GetName());
	}
}

void CATLControlsPanel::StopControlExecution()
{
	CAudioControlsEditorPlugin::StopTriggerExecution();
}

CATLControl* CATLControlsPanel::GetControlFromItem(QStandardItem* pItem)
{
	if (pItem && (pItem->data(eDataRole_Type) == eItemType_AudioControl))
	{
		return m_pATLModel->GetControlByID(pItem->data(eDataRole_Id).toUInt());
	}
	return nullptr;
}

CATLControl* CATLControlsPanel::GetControlFromIndex(QModelIndex index)
{
	if (index.isValid() && (index.data(eDataRole_Type) == eItemType_AudioControl))
	{
		return m_pATLModel->GetControlByID(index.data(eDataRole_Id).toUInt());
	}
	return nullptr;
}

void CATLControlsPanel::SelectItem(QStandardItem* pItem)
{
	// Expand and scroll to the new item
	if (pItem)
	{
		QModelIndex index = m_pTreeModel->indexFromItem(pItem);
		if (index.isValid())
		{
			m_pATLControlsTree->selectionModel()->setCurrentIndex(m_pProxyModel->mapFromSource(index), QItemSelectionModel::ClearAndSelect);
		}
	}
}

void CATLControlsPanel::HandleExternalDropEvent(QDropEvent* pDropEvent)
{
	ACE::IAudioSystemEditor* pAudioSystemEditorImpl = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();
	if (pAudioSystemEditorImpl)
	{
		const QMimeData* pData = pDropEvent->mimeData();
		const QString format = QAudioSystemModel::ms_szMimeType;
		if (pData->hasFormat(format))
		{
			QByteArray encoded = pData->data(format);
			QDataStream stream(&encoded, QIODevice::ReadOnly);
			while (!stream.atEnd())
			{
				CID id;
				stream >> id;
				if (id != ACE_INVALID_ID)
				{
					// Dropped item mime data
					IAudioSystemItem* pAudioSystemControl = pAudioSystemEditorImpl->GetControl(id);
					if (pAudioSystemControl)
					{
						const EACEControlType controlType = pAudioSystemEditorImpl->ImplTypeToATLType(pAudioSystemControl->GetType());

						// Find a suitable parent for the new control
						QModelIndex index = m_pProxyModel->mapToSource(m_pATLControlsTree->indexAt(pDropEvent->pos()));
						while (index.isValid() && !m_pTreeModel->IsValidParent(index, controlType))
						{
							index = index.parent();
						}

						if (index.isValid())
						{
							QStandardItem* pTargetItem = m_pTreeModel->itemFromIndex(index);
							if (pTargetItem)
							{
								// Create the new control and connect it to the one dragged in externally
								string controlName = pAudioSystemControl->GetName();
								if (controlType == eACEControlType_Preload)
								{
									PathUtil::RemoveExtension(controlName);
								}

								CATLControl* pTargetControl = m_pTreeModel->CreateControl(controlType, controlName, GetControlFromItem(pTargetItem));
								if (pTargetControl)
								{
									ConnectionPtr pAudioConnection = pAudioSystemEditorImpl->CreateConnectionToControl(pTargetControl->GetType(), pAudioSystemControl);
									if (pAudioConnection)
									{
										pTargetControl->AddConnection(pAudioConnection);
									}

									pTargetItem = m_pTreeModel->AddControl(pTargetControl, pTargetItem);
									SelectItem(pTargetItem);
								}
							}
						}
					}
				}
			}
		}
	}
}

QStandardItem* CATLControlsPanel::GetCurrentItem()
{
	return m_pTreeModel->itemFromIndex(m_pProxyModel->mapToSource(m_pATLControlsTree->currentIndex()));
}

void CATLControlsPanel::ResetFilters()
{
	// Remove filters
	for (int i = 0; i < eACEControlType_NumTypes; ++i)
	{
		EACEControlType controlType = (EACEControlType)i;
		m_visibleTypes[controlType] = true;
		if (m_pFilterActions[controlType])
		{
			m_pFilterActions[controlType]->setChecked(true);
		}
	}
	m_pTextFilter->setText("");
	ApplyFilter();
}

void CATLControlsPanel::ItemModified(QStandardItem* pItem)
{
	if (pItem)
	{
		string sName = QtUtil::ToString(pItem->text());
		if (pItem->data(eDataRole_Type) == eItemType_AudioControl)
		{
			CATLControl* pControl = m_pATLModel->GetControlByID(pItem->data(eDataRole_Id).toUInt());
			if (pControl)
			{
				if (QtUtil::ToString(pItem->text()) != pControl->GetName())
				{
					pControl->SetName(sName);
					m_pTreeModel->blockSignals(true);
					pItem->setText(QtUtil::ToQString(pControl->GetName()));
					m_pTreeModel->blockSignals(false);
				}
			}

		}
		m_pTreeModel->SetItemAsDirty(pItem);
	}
}
}

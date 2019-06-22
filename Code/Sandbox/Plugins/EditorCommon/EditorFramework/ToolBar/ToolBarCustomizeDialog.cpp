// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "ToolBarCustomizeDialog.h"

#include "Commands/CommandManager.h"
#include "Commands/CommandModel.h"
#include "Commands/CustomCommand.h"
#include "Commands/QCommandAction.h"
#include "Commands/CVarListDockable.h"
#include "Controls/QEditableComboBox.h"
#include "Controls/QResourceBrowserDialog.h"
#include "CryIcon.h"
#include "DragDrop.h"
#include "EditorFramework/Editor.h"
#include "PathUtils.h"
#include "ProxyModels/DeepFilterProxyModel.h"
#include "QAdvancedTreeView.h"
#include "QSearchBox.h"
#include "QtUtil.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolBar>
#include <QLabel>
#include <QToolButton>
#include <QAction>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QDrag>
#include <QDropEvent>
#include <QPainter>
#include <QJsonDocument>
#include <QToolButton>
#include <QHeaderView>

const char* CToolBarCustomizeDialog::QDropContainer::s_toolBarItemMimeType = QT_TR_NOOP("ToolBarItem");

//////////////////////////////////////////////////////////////////////////

class CToolBarCustomizeDialog::QCustomToolBar : public QToolBar
{
public:
	QCustomToolBar(const QString& title)
		: QToolBar(title)
		, m_pSelectedWidget(nullptr)
		, m_bShowDropTarget(false)
		, m_WasToolButtonCheckable(false)
	{

	}

	void SetShowDropTarget(bool show)
	{
		m_bShowDropTarget = show;
		update();
	}

	void SetDragStartPosition(const QPoint& dragPos)
	{
		m_DragStartPosition = dragPos;
	}

	void DrawDropTarget(const QPoint& globalPos)
	{
		m_DropTargetPos = mapFromGlobal(globalPos);
		QAction* pAction = ClosestActionTo(globalPos);
		QWidget* pChild = widgetForAction(pAction);
		if (pChild)
		{
			QSize size = pChild->size();
			QPoint childPos = pChild->mapFromGlobal(globalPos);
			bool firstHalf = size.width() / 2 > childPos.x();

			childPos = mapFromGlobal(pChild->mapToGlobal(QPoint(0, 0)));
			if (firstHalf)
				m_DropTargetPos.setX(childPos.x());
			else
				m_DropTargetPos.setX(childPos.x() + pChild->size().width() + layout()->spacing());
		}

		SetShowDropTarget(true);
	}

	void SelectWidget(QToolButton* pWidget)
	{
		if (m_pSelectedWidget)
		{
			m_pSelectedWidget->setCheckable(m_WasToolButtonCheckable);
			m_pSelectedWidget->setChecked(false);
		}

		m_pSelectedWidget = pWidget;

		// Use checked state to simulate selection during toolbar customization
		m_WasToolButtonCheckable = m_pSelectedWidget->isCheckable();
		m_pSelectedWidget->setCheckable(true);
		m_pSelectedWidget->setChecked(true);
		update();
	}

	QAction* ClosestActionTo(const QPoint& globalPos)
	{
		QPoint localPos = mapFromGlobal(globalPos);
		QAction* pAction = actionAt(localPos);

		if (!pAction)
		{
			int currentX = 0;

			for (QAction* pChildAction : actions())
			{
				QWidget* pWidget = widgetForAction(pChildAction);
				int widgetX = pWidget->pos().x();
				if (localPos.x() > widgetX && localPos.x() < widgetX + pWidget->size().width())
				{
					return pChildAction;
				}
				else if (localPos.x() > widgetX && currentX < widgetX)
				{
					currentX = pWidget->pos().x();
					pAction = pChildAction;
				}
			}
		}

		return pAction;
	}

protected:
	virtual void paintEvent(QPaintEvent* pEvent) override
	{
		QToolBar::paintEvent(pEvent);

		QPainter painter(this);
		painter.save();

		if (m_bShowDropTarget)
		{
			QPixmap pixmap(":toolbar-drop-target.png");
			int spacing = (pixmap.width() + layout()->spacing()) / 2;
			QPoint drawPos = QPoint(m_DropTargetPos.x() - spacing, 0);
			QRect drawRect(drawPos, QSize(pixmap.width(), size().height()));

			painter.drawTiledPixmap(drawRect, pixmap);
		}

		painter.restore();
	}

protected:
	QToolButton* m_pSelectedWidget;
	QPoint       m_DragStartPosition;
	QPoint       m_DropTargetPos;
	bool         m_bShowDropTarget;
	bool         m_WasToolButtonCheckable;
};

CToolBarCustomizeDialog::QDropContainer::QDropContainer(CToolBarCustomizeDialog* pParent)
	: QWidget(pParent)
	, m_pToolBarCreator(pParent)
	, m_pPreviewLayout(nullptr)
	, m_pCurrentToolBar(nullptr)
	, m_bDragStarted(false)
{
	setAcceptDrops(true);
	m_pPreviewLayout = new QHBoxLayout();
	m_pPreviewLayout->setMargin(0);
	setLayout(m_pPreviewLayout);
}

CToolBarCustomizeDialog::QDropContainer::~QDropContainer()
{
	if (m_pCurrentToolBar)
	{
		delete m_pCurrentToolBar;
		m_pCurrentToolBar = nullptr;
	}
}

void CToolBarCustomizeDialog::QDropContainer::UpdateToolBar()
{
	GetIEditor()->GetToolBarService()->SaveToolBar(m_pCurrentToolBarDesc);
	BuildPreview();

	signalToolBarModified(m_pCurrentToolBarDesc);
}

void CToolBarCustomizeDialog::QDropContainer::AddItem(const QVariant& itemVariant, int idx /* = -1*/)
{
	m_pCurrentToolBarDesc->InsertItem(itemVariant, idx);
	UpdateToolBar();
}

void CToolBarCustomizeDialog::QDropContainer::AddCommand(const CCommand* pCommand, int idx /* = -1*/)
{
	if (!m_pCurrentToolBarDesc)
		return;

	m_pCurrentToolBarDesc->InsertCommand(pCommand, idx);
	UpdateToolBar();
}

void CToolBarCustomizeDialog::QDropContainer::AddSeparator(int sourceIdx /* = -1*/, int targetIdx /* = -1*/)
{
	if (sourceIdx > -1)
		m_pCurrentToolBarDesc->MoveItem(sourceIdx, targetIdx);
	else
		m_pCurrentToolBarDesc->InsertSeparator(targetIdx);

	UpdateToolBar();
}

void CToolBarCustomizeDialog::QDropContainer::AddCVar(const QString& cvarName, int idx /* = -1*/)
{
	if (cvarName.isEmpty())
	{
		m_pCurrentToolBarDesc->InsertCVar(cvarName, idx);
	}

	UpdateToolBar();
}

void CToolBarCustomizeDialog::QDropContainer::RemoveCommand(const CCommand* pCommand)
{
	m_pCurrentToolBarDesc->RemoveItem(m_pCurrentToolBarDesc->IndexOfCommand(pCommand));
	UpdateToolBar();
}

void CToolBarCustomizeDialog::QDropContainer::RemoveItem(std::shared_ptr<CToolBarService::QItemDesc> pItem)
{
	m_pCurrentToolBarDesc->RemoveItem(pItem);
	UpdateToolBar();
}

void CToolBarCustomizeDialog::QDropContainer::RemoveItemAt(int idx)
{
	m_pCurrentToolBarDesc->RemoveItem(idx);
	UpdateToolBar();
}

int CToolBarCustomizeDialog::QDropContainer::GetIndexFromMouseCoord(const QPoint& globalPos)
{
	QList<QAction*> actions = m_pCurrentToolBar->actions();
	QAction* pAction = m_pCurrentToolBar->ClosestActionTo(globalPos);

	if (!pAction)
		return actions.size();

	QWidget* pWidget = m_pCurrentToolBar->widgetForAction(pAction);
	if (!pWidget)
		return actions.size();

	QSize size = pWidget->size();
	QPoint childPos = pWidget->mapFromGlobal(globalPos);
	bool firstHalf = size.width() / 2 > childPos.x();

	if (firstHalf)
		return actions.indexOf(pAction);

	for (int i = 0; i < actions.size(); ++i)
	{
		if (actions[i] == pAction)
		{
			if (++i == actions.size())
				return actions.size();
			else
				return i;
		}
	}

	return actions.size();
}

void CToolBarCustomizeDialog::QDropContainer::SetCurrentToolBarDesc(std::shared_ptr<CToolBarService::QToolBarDesc>& toolBarDesc)
{
	m_pCurrentToolBarDesc = toolBarDesc;
	BuildPreview();
}

void CToolBarCustomizeDialog::QDropContainer::BuildPreview()
{
	std::shared_ptr<CToolBarService::QItemDesc> pSelectedItem = m_pSelectedItem;

	if (m_pCurrentToolBar)
	{
		selectedItemChanged(nullptr);
		m_pSelectedItem = nullptr;
		m_pPreviewLayout->removeWidget(m_pCurrentToolBar);
		m_pCurrentToolBar->deleteLater();
		m_pCurrentToolBar = nullptr;
	}

	QString toolBarText = m_pToolBarCreator->GetCurrentToolBarText();
	if (toolBarText.isEmpty())
		return;

	m_pCurrentToolBar = CreateToolBar(m_pCurrentToolBarDesc->GetName(), m_pCurrentToolBarDesc);
	m_pPreviewLayout->addWidget(m_pCurrentToolBar);

	if (pSelectedItem && m_pCurrentToolBarDesc && m_pCurrentToolBarDesc->GetItems().contains(pSelectedItem))
	{
		m_pSelectedItem = pSelectedItem;
		selectedItemChanged(pSelectedItem);
	}
}

void CToolBarCustomizeDialog::QDropContainer::ShowContextMenu(const QPoint& position)
{
	QAction* pAction = m_pCurrentToolBar->actionAt(position);

	QMenu* pMenu = new QMenu();
	QAction* pAddSeparator = pMenu->addAction("Add Separator");
	QAction* pAddCVar = pMenu->addAction("Add CVar");
	QAction* pRemove = pMenu->addAction("Remove");
	pRemove->setEnabled(pAction != nullptr);

	pMenu->popup(m_pCurrentToolBar->mapToGlobal(position));

	connect(pAddSeparator, &QAction::triggered, [this, position]()
	{
		AddSeparator(-1, GetIndexFromMouseCoord(m_pCurrentToolBar->mapToGlobal(position)));
	});

	connect(pAddCVar, &QAction::triggered, [this, position]()
	{
		AddCVar("", GetIndexFromMouseCoord(m_pCurrentToolBar->mapToGlobal(position)));
	});

	connect(pRemove, &QAction::triggered, [this, pAction]()
	{
		RemoveItemAt(m_pCurrentToolBar->actions().indexOf(pAction));
	});
}

void CToolBarCustomizeDialog::QDropContainer::SetSelectedActionIcon(const char* szIconPath)
{
	if (!m_pSelectedItem)
	{
		return;
	}

	if (m_pSelectedItem->GetType() == CToolBarService::QItemDesc::Command)
	{
		std::static_pointer_cast<CToolBarService::QCommandDesc>(m_pSelectedItem)->SetIcon(szIconPath);
	}
	else if (m_pSelectedItem->GetType() == CToolBarService::QItemDesc::CVar)
	{
		std::static_pointer_cast<CToolBarService::QCVarDesc>(m_pSelectedItem)->SetIcon(szIconPath);
	}
	UpdateToolBar();
}

CToolBarCustomizeDialog::QCustomToolBar* CToolBarCustomizeDialog::QDropContainer::CreateToolBar(const QString& title, const std::shared_ptr<CToolBarService::QToolBarDesc>& pToolBarDesc)
{
	if (title.isEmpty())
		return nullptr;

	QCustomToolBar* pToolBar = new QCustomToolBar(title);
	pToolBar->setObjectName(title + "ToolBar");
	pToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(pToolBar, &QToolBar::customContextMenuRequested, this, &CToolBarCustomizeDialog::QDropContainer::ShowContextMenu);

	GetIEditor()->GetToolBarService()->CreateToolBar(pToolBarDesc, pToolBar);
	QList<QAction*> toolBarActions = pToolBar->actions();

	for (QAction* pAction : toolBarActions)
	{
		QWidget* pWidget = pToolBar->widgetForAction(pAction);
		pWidget->installEventFilter(this); // install event filter so we can catch the mouse pressed events
	}

	return pToolBar;
}

bool CToolBarCustomizeDialog::QDropContainer::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (pEvent->type() == QEvent::MouseButtonPress)
	{
		QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(pEvent);
		if (pMouseEvent->button() == Qt::LeftButton)
		{
			QToolButton* pToolButton = qobject_cast<QToolButton*>(pObject);
			if (!pToolButton)
			{
				m_pSelectedItem = nullptr;
				selectedItemChanged(nullptr);
				return false;
			}

			m_pCurrentToolBar->SelectWidget(pToolButton);
			m_DragStartPosition = pMouseEvent->globalPos();
			m_bDragStarted = true;
			QList<QAction*> actions = m_pCurrentToolBar->actions();
			QAction* pAction = m_pCurrentToolBar->ClosestActionTo(m_DragStartPosition);
			m_pSelectedItem = m_pCurrentToolBarDesc->GetItemDescAt(actions.indexOf(pAction));
			selectedItemChanged(m_pSelectedItem);

			return true;
		}
	}

	return QObject::eventFilter(pObject, pEvent);
}

void CToolBarCustomizeDialog::QDropContainer::mouseMoveEvent(QMouseEvent* pEvent)
{
	if (!m_pCurrentToolBar && !m_pSelectedItem || !m_bDragStarted || !(pEvent->buttons() & Qt::LeftButton))
		return;

	if ((pEvent->pos() - m_DragStartPosition).manhattanLength() < QApplication::startDragDistance())
		return;

	QDrag* pDrag = new QDrag(this);
	connect(pDrag, &QDrag::destroyed, [this]()
	{
		m_pCurrentToolBar->SetShowDropTarget(false);
	});
	CDragDropData* pDragDropData = new CDragDropData();

	QJsonDocument doc = QJsonDocument::fromVariant(m_pSelectedItem->ToVariant());
	pDragDropData->SetCustomData(GetToolBarItemMimeType(), doc.toBinaryData());

	pDrag->setMimeData(pDragDropData);
	m_pCurrentToolBar->SetDragStartPosition(m_DragStartPosition);
	pDrag->exec(Qt::CopyAction | Qt::MoveAction);
	m_pCurrentToolBar->DrawDropTarget(pEvent->globalPos());
}

void CToolBarCustomizeDialog::QDropContainer::dragEnterEvent(QDragEnterEvent* pEvent)
{
	if (!m_pCurrentToolBar)
	{
		return;
	}

	const CDragDropData* pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());

	if (pDragDropData->HasCustomData(GetToolBarItemMimeType()) || pDragDropData->HasCustomData(CCommandModel::GetCommandMimeType()))
	{
		pEvent->acceptProposedAction();
		m_pCurrentToolBar->DrawDropTarget(mapToGlobal(pEvent->pos()));
	}
}

void CToolBarCustomizeDialog::QDropContainer::dragMoveEvent(QDragMoveEvent* pEvent)
{
	const CDragDropData* pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());

	if (pDragDropData->HasCustomData(GetToolBarItemMimeType()) || pDragDropData->HasCustomData(CCommandModel::GetCommandMimeType()))
	{
		pEvent->acceptProposedAction();
		m_pCurrentToolBar->DrawDropTarget(mapToGlobal(pEvent->pos()));
	}
}

void CToolBarCustomizeDialog::QDropContainer::dragLeaveEvent(QDragLeaveEvent* pEvent)
{
	m_pCurrentToolBar->SetShowDropTarget(false);
}

void CToolBarCustomizeDialog::QDropContainer::paintEvent(QPaintEvent*)
{
	QStyleOption styleOption;
	styleOption.init(this);
	QPainter painter(this);
	style()->drawPrimitive(QStyle::PE_Widget, &styleOption, &painter, this);
}

void CToolBarCustomizeDialog::QDropContainer::dropEvent(QDropEvent* pEvent)
{
	m_pCurrentToolBar->SetShowDropTarget(false);

	const CDragDropData* pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());

	if (pDragDropData->HasCustomData(GetToolBarItemMimeType()))
	{
		QJsonDocument doc = QJsonDocument::fromBinaryData(pDragDropData->GetCustomData(GetToolBarItemMimeType()));
		QVariant itemVariant = doc.toVariant();

		int oldIdx = m_pCurrentToolBarDesc->IndexOfItem(m_pSelectedItem);
		int newIdx = GetIndexFromMouseCoord(mapToGlobal(pEvent->pos()));
		if (oldIdx < 0 || oldIdx == newIdx)
			return;

		AddItem(itemVariant, newIdx);

		if (oldIdx > newIdx)
			++oldIdx;

		RemoveItemAt(oldIdx);
	}
	if (pDragDropData->HasCustomData(CCommandModel::GetCommandMimeType()))
	{
		QJsonDocument doc = QJsonDocument::fromBinaryData(pDragDropData->GetCustomData(CCommandModel::GetCommandMimeType()));
		QVariantMap itemMap = doc.toVariant().toMap();

		const string command = QtUtil::ToString(itemMap["command"].toString());
		CCommand* pCommand = GetIEditor()->GetICommandManager()->GetCommand(command.c_str());
		int oldIdx = m_pCurrentToolBarDesc->IndexOfCommand(pCommand);
		int newIdx = GetIndexFromMouseCoord(mapToGlobal(pEvent->pos()));
		AddItem(itemMap, newIdx);

		if (oldIdx >= 0)
		{
			if (oldIdx > newIdx)
				++oldIdx;
			RemoveItemAt(oldIdx);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CToolBarCustomizeDialog::CToolBarCustomizeDialog(QWidget* pParent, const char* szEditorName)
	: CEditorDialog("ToolBar Creator", pParent)
	, m_pSelectedItem(nullptr)
	, m_pEditor(nullptr)
	, m_editorName(szEditorName)
{
	setAttribute(Qt::WA_DeleteOnClose);

	m_pToolbarSelect = new QEditableComboBox();
	connect(m_pToolbarSelect, &QEditableComboBox::ItemRenamed, this, &CToolBarCustomizeDialog::RenameToolBar);

	std::set<string> toolBarNames = GetIEditor()->GetToolBarService()->GetToolBarNames(m_editorName.c_str());

	for (const string& toolBarName : toolBarNames)
	{
		m_pToolbarSelect->AddItem(toolBarName.c_str());
	}

	QVBoxLayout* pLayout = new QVBoxLayout();
	pLayout->setMargin(0);
	pLayout->setSpacing(0);

	QHBoxLayout* pInnerLayout = new QHBoxLayout();
	pInnerLayout->setAlignment(Qt::AlignVCenter);
	pInnerLayout->setMargin(0);
	pInnerLayout->setSpacing(0);

	QToolButton* pAddToolBar = new QToolButton();
	pAddToolBar->setIcon(CryIcon("icons:General/Plus.ico"));
	pAddToolBar->setToolTip("Create new Toolbar");
	connect(pAddToolBar, &QToolButton::clicked, this, &CToolBarCustomizeDialog::OnAddToolBar);

	QToolButton* pRemoveToolBar = new QToolButton();
	pRemoveToolBar->setIcon(CryIcon("icons:General/Folder_Remove.ico"));
	pRemoveToolBar->setToolTip("Remove Toolbar");
	connect(pRemoveToolBar, &QToolButton::clicked, this, &CToolBarCustomizeDialog::OnRemoveToolBar);

	QToolButton* pRenameToolBar = new QToolButton();
	pRenameToolBar->setIcon(CryIcon("icons:General/editable_true.ico"));
	pRenameToolBar->setToolTip("Rename Toolbar");
	connect(pRenameToolBar, &QToolButton::clicked, [this]()
	{
		// Don't rename unless there's a toolbar selected (not empty)
		if (!m_pToolbarSelect->GetCurrentText().isEmpty())
			m_pToolbarSelect->OnBeginEditing();
	});

	pInnerLayout->addWidget(m_pToolbarSelect);
	pInnerLayout->addWidget(pAddToolBar);
	pInnerLayout->addWidget(pRenameToolBar);
	pInnerLayout->addWidget(pRemoveToolBar);

	m_pTreeView = new QAdvancedTreeView();
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pTreeView, &QTreeView::customContextMenuRequested, this, &CToolBarCustomizeDialog::OnContextMenu);

	m_pItemModel = new CCommandModel();
	m_pItemModel->EnableDragAndDropSupport(true);

	m_pProxyModel = new QDeepFilterProxyModel();
	m_pProxyModel->setSourceModel(m_pItemModel);

	QWidget* pSearchBoxContainer = new QWidget();
	QHBoxLayout* pSearchBoxLayout = new QHBoxLayout();
	pSearchBoxLayout->setAlignment(Qt::AlignTop);
	m_pSearchBox = new QSearchBox();
	m_pSearchBox->EnableContinuousSearch(true);
	m_pSearchBox->SetModel(m_pProxyModel);
	pSearchBoxLayout->setSpacing(0);
	pSearchBoxLayout->setMargin(0);
	pSearchBoxLayout->addWidget(m_pSearchBox);
	pSearchBoxContainer->setObjectName("SearchBoxContainer");
	pSearchBoxContainer->setLayout(pSearchBoxLayout);

	m_pTreeView->setModel(m_pProxyModel);
	m_pTreeView->setDragEnabled(true);
	m_pTreeView->setSortingEnabled(true);
	m_pTreeView->sortByColumn(0, Qt::AscendingOrder);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->setAllColumnsShowFocus(true);
	m_pTreeView->header()->setStretchLastSection(true);
	m_pTreeView->header()->setDefaultSectionSize(300);

	m_pSearchBox->SetAutoExpandOnSearch(m_pTreeView);

	m_pDropContainer = new QDropContainer(this);
	m_pDropContainer->setObjectName("DropContainer");
	m_pDropContainer->signalToolBarModified.Connect(this, &CToolBarCustomizeDialog::OnToolBarModified);
	connect(m_pToolbarSelect, &QEditableComboBox::OnCurrentIndexChanged, this, &CToolBarCustomizeDialog::CurrentItemChanged);

	QHBoxLayout* pNameLayout = new QHBoxLayout();
	pNameLayout->setMargin(0);
	pNameLayout->setSpacing(0);
	QLabel* pNameLabel = new QLabel("Name");
	pNameLayout->addWidget(pNameLabel);
	m_pNameInput = new QLineEdit();
	m_pNameInput->setEnabled(false);
	pNameLayout->addWidget(m_pNameInput);

	connect(m_pNameInput, &QLineEdit::editingFinished, [this]()
	{
		SetCommandName(m_pNameInput->text().toStdString().c_str());
	});

	m_commandWidgets.push_back(pNameLabel);
	m_commandWidgets.push_back(m_pNameInput);

	QHBoxLayout* pCommandLayout = new QHBoxLayout();
	QLabel* pCommandLabel = new QLabel("Command");
	pCommandLayout->addWidget(pCommandLabel);
	pCommandLayout->setMargin(0);
	pCommandLayout->setSpacing(0);
	m_pCommandInput = new QLineEdit();
	m_pCommandInput->setEnabled(false);
	pCommandLayout->addWidget(m_pCommandInput);

	m_commandWidgets.push_back(pCommandLabel);
	m_commandWidgets.push_back(m_pCommandInput);

	m_pCVarInput = new QLineEdit();
	m_pCVarInput->setEnabled(false);
	m_pCVarInput->setVisible(false);
	m_pCVarBrowserButton = new QToolButton();
	m_pCVarBrowserButton->setIcon(CryIcon("icons:General/Folder.ico"));
	m_pCVarBrowserButton->setEnabled(false);
	m_pCVarBrowserButton->setVisible(false);
	m_pCVarBrowserButton->setObjectName("open-cvar");
	connect(m_pCVarBrowserButton, &QToolButton::clicked, [this]()
	{
		CCVarBrowserDialog* pDialog = new CCVarBrowserDialog();
		pDialog->show();

		connect(pDialog, &QDialog::accepted, this, [this, pDialog]()
		{
			CVarsSelected(pDialog->GetSelectedCVars());
		});
	});

	connect(m_pCVarInput, &QLineEdit::editingFinished, [this]()
	{
		SetCVarName(m_pCVarInput->text().toStdString().c_str());
	});

	QHBoxLayout* pCVarLayout = new QHBoxLayout();
	pCVarLayout->setMargin(0);
	pCVarLayout->setSpacing(0);
	QLabel* pCVarLabel = new QLabel("CVar");
	pCVarLabel->setVisible(false);
	pCVarLayout->addWidget(pCVarLabel);
	pCVarLayout->addWidget(m_pCVarInput);
	pCVarLayout->addWidget(m_pCVarBrowserButton);

	m_cvarWidgets.push_back(pCVarLabel);
	m_cvarWidgets.push_back(m_pCVarInput);
	m_cvarWidgets.push_back(m_pCVarBrowserButton);

	QHBoxLayout* pCVarValueLayout = new QHBoxLayout();
	pCVarValueLayout->setMargin(0);
	pCVarValueLayout->setSpacing(0);
	QLabel* pCVarValueLabel = new QLabel("Value");
	pCVarValueLabel->setVisible(false);
	m_pCVarValueInput = new QLineEdit();
	m_pCVarValueInput->setEnabled(false);
	m_pCVarValueInput->setVisible(false);
	pCVarValueLayout->addWidget(pCVarValueLabel);
	pCVarValueLayout->addWidget(m_pCVarValueInput);

	connect(m_pCVarValueInput, &QLineEdit::editingFinished, [this]()
	{
		SetCVarValue(m_pCVarValueInput->text().toStdString().c_str());
	});

	m_cvarWidgets.push_back(pCVarValueLabel);
	m_cvarWidgets.push_back(m_pCVarValueInput);

	m_pIconInput = new QLineEdit();
	m_pIconInput->setEnabled(false);
	m_pIconBrowserButton = new QToolButton();
	m_pIconBrowserButton->setIcon(CryIcon("icons:General/Folder.ico"));
	m_pIconBrowserButton->setObjectName("open-icon");
	m_pIconBrowserButton->setEnabled(false);
	connect(m_pIconBrowserButton, &QToolButton::clicked, [this]()
	{
		QResourceBrowserDialog* pDialog = new QResourceBrowserDialog();
		pDialog->show();

		connect(pDialog, &QDialog::accepted, this, [this, pDialog]()
		{
			IconSelected(pDialog->GetSelectedResource().toStdString().c_str());
		});
	});

	connect(m_pIconInput, &QLineEdit::editingFinished, [this]()
	{
		SetIconPath(m_pIconInput->text().toStdString().c_str());
	});

	QHBoxLayout* pIconLayout = new QHBoxLayout();
	pIconLayout->setMargin(0);
	pIconLayout->setSpacing(0);
	QLabel* pIconLabel = new QLabel("Icon");
	pIconLayout->addWidget(pIconLabel);
	pIconLayout->addWidget(m_pIconInput);
	pIconLayout->addWidget(m_pIconBrowserButton);

	m_commandWidgets.push_back(pIconLabel);
	m_commandWidgets.push_back(m_pIconInput);
	m_commandWidgets.push_back(m_pIconBrowserButton);
	m_cvarWidgets.push_back(pIconLabel);
	m_cvarWidgets.push_back(m_pIconInput);
	m_cvarWidgets.push_back(m_pIconBrowserButton);

	// Create a container widget for styling purposes
	QWidget* pInnerContainer = new QWidget();
	pInnerContainer->setLayout(pInnerLayout);
	pInnerContainer->setObjectName("ToolbarComboBoxContainer");
	// Create a container widget for styling purposes
	QVBoxLayout* pCommandDetailsLayout = new QVBoxLayout();
	pCommandDetailsLayout->setSpacing(0);
	pCommandDetailsLayout->setMargin(0);
	pCommandDetailsLayout->addLayout(pNameLayout);
	pCommandDetailsLayout->addLayout(pCommandLayout);
	pCommandDetailsLayout->addLayout(pCVarLayout);
	pCommandDetailsLayout->addLayout(pCVarValueLayout);
	pCommandDetailsLayout->addLayout(pIconLayout);

	QWidget* pCommandDetails = new QWidget();
	pCommandDetails->setLayout(pCommandDetailsLayout);
	pCommandDetails->setObjectName("ToolbarPropertiesContainer");

	pLayout->addWidget(pSearchBoxContainer);
	pLayout->addWidget(m_pTreeView);
	pLayout->addWidget(pInnerContainer);
	pLayout->addWidget(m_pDropContainer);
	pLayout->addWidget(pCommandDetails);

	m_pDropContainer->selectedItemChanged.Connect(this, &CToolBarCustomizeDialog::OnSelectedItemChanged);

	// Force creation of current toolbar editing state
	CurrentItemChanged();

	setAcceptDrops(true);
	setLayout(pLayout);
}

CToolBarCustomizeDialog::CToolBarCustomizeDialog(CEditor* pEditor)
	: CToolBarCustomizeDialog(pEditor, pEditor->GetEditorName())
{
	m_pEditor = pEditor;

	m_pItemModel->deleteLater();
	m_pItemModel = new CCommandModel(m_pEditor->GetCommands());
	m_pItemModel->EnableDragAndDropSupport(true);

	m_pProxyModel = new QDeepFilterProxyModel();
	m_pProxyModel->setSourceModel(m_pItemModel);

	m_pSearchBox->SetModel(m_pProxyModel);
	m_pTreeView->setModel(m_pProxyModel);
}

void CToolBarCustomizeDialog::IconSelected(const char* szIconPath)
{
	m_pIconInput->setText(szIconPath);
	SetIconPath(szIconPath);
}

void CToolBarCustomizeDialog::CVarsSelected(const QStringList& selectedCVars)
{
	if (selectedCVars.size() != 1)
		return;

	const string selectedCVar = QtUtil::ToString(selectedCVars[0]);
	m_pCVarInput->setText(selectedCVar.c_str());
	SetCVarName(selectedCVar.c_str());
}

void CToolBarCustomizeDialog::SetCVarName(const char* szCVarName)
{
	ICVar* pCVar = gEnv->pConsole->GetCVar(szCVarName);
	if (!pCVar)
	{
		m_pCVarInput->clear();
		m_pCVarValueInput->setEnabled(false);
		return;
	}

	std::static_pointer_cast<CToolBarService::QCVarDesc>(m_pSelectedItem)->SetCVar(szCVarName);
	m_pCVarValueInput->setEnabled(true);
	SetCVarValueRegExp();
}

void CToolBarCustomizeDialog::SetCVarValueRegExp()
{
	const string inputText = QtUtil::ToString(m_pCVarInput->text());
	ICVar* pCVar = gEnv->pConsole->GetCVar(inputText.c_str());
	if (!pCVar)
		return;

	switch (pCVar->GetType())
	{
	case ECVarType::Int:
		m_pCVarValueInput->setValidator(new QIntValidator());
		break;
	case ECVarType::Float:
		m_pCVarValueInput->setValidator(new QDoubleValidator());
		break;
	case ECVarType::Int64:
		CRY_ASSERT_MESSAGE(false, "CToolBarCustomizeDialog::SetCVarValueRegExp int64 cvar not implemented");
	// fall through
	default:
		m_pCVarValueInput->setValidator(nullptr);
	}
}

void CToolBarCustomizeDialog::SetCVarValue(const char* cvarValue)
{
	const string inputText = QtUtil::ToString(m_pCVarInput->text());
	ICVar* pCVar = gEnv->pConsole->GetCVar(inputText.c_str());
	if (!pCVar)
	{
		m_pCVarValueInput->clear();
		return;
	}

	QString valueStr(cvarValue);
	switch (pCVar->GetType())
	{
	case ECVarType::Int:
		std::static_pointer_cast<CToolBarService::QCVarDesc>(m_pSelectedItem)->SetCVarValue(valueStr.toInt());
		break;
	case ECVarType::Float:
		std::static_pointer_cast<CToolBarService::QCVarDesc>(m_pSelectedItem)->SetCVarValue(valueStr.toDouble());
		break;
	case ECVarType::Int64:
		CRY_ASSERT_MESSAGE(false, "CToolBarCustomizeDialog::SetCVarValue int64 cvar not implemented");
	// fall through
	default:
		std::static_pointer_cast<CToolBarService::QCVarDesc>(m_pSelectedItem)->SetCVarValue(cvarValue);
	}
}

void CToolBarCustomizeDialog::SetCommandName(const char* commandName)
{
	std::shared_ptr<CToolBarService::QCommandDesc> pCommandDesc = std::static_pointer_cast<CToolBarService::QCommandDesc>(m_pSelectedItem);

	if (!pCommandDesc->IsCustom())
		return;

	CCommand* pCommand = GetIEditor()->GetICommandManager()->GetCommand(pCommandDesc->GetCommand().toStdString().c_str());
	CCustomCommand* pCustomCommand = static_cast<CCustomCommand*>(pCommand);
	pCustomCommand->SetName(commandName);
	pCommandDesc->SetName(commandName);
}

void CToolBarCustomizeDialog::SetIconPath(const char* szIconPath)
{
	m_pDropContainer->SetSelectedActionIcon(szIconPath);
}

void CToolBarCustomizeDialog::OnSelectedItemChanged(std::shared_ptr<CToolBarService::QItemDesc> selectedItem)
{
	m_pSelectedItem = selectedItem;
	if (!m_pSelectedItem)
	{
		m_pNameInput->clear();
		m_pCommandInput->clear();
		m_pIconInput->clear();

		m_pNameInput->setEnabled(false);
		m_pCommandInput->setEnabled(false);

		m_pIconInput->blockSignals(true);
		m_pIconInput->setEnabled(false);
		m_pIconInput->blockSignals(false);
		m_pIconBrowserButton->setEnabled(false);

		m_pCVarInput->blockSignals(true);
		m_pCVarInput->setEnabled(false);
		m_pCVarInput->blockSignals(false);

		m_pCVarValueInput->blockSignals(true);
		m_pCVarValueInput->setEnabled(false);
		m_pCVarValueInput->blockSignals(false);

		m_pCVarBrowserButton->setEnabled(false);
		return;
	}

	if (m_pSelectedItem->GetType() == CToolBarService::QItemDesc::Command)
	{
		ShowCommandWidgets();
	}
	else if (m_pSelectedItem->GetType() == CToolBarService::QItemDesc::CVar)
	{
		ShowCVarWidgets();
	}

}

void CToolBarCustomizeDialog::ShowCommandWidgets()
{
	std::shared_ptr<CToolBarService::QCommandDesc> pCommandDesc = std::static_pointer_cast<CToolBarService::QCommandDesc>(m_pSelectedItem);

	for (QWidget* pWidget : m_cvarWidgets)
		pWidget->setVisible(false);

	for (QWidget* pWidget : m_commandWidgets)
		pWidget->setVisible(true);

	if (!pCommandDesc->IsCustom())
	{
		m_pNameInput->setEnabled(false);
		m_pCommandInput->setEnabled(false);
	}
	else
	{
		m_pNameInput->setEnabled(true);
		m_pCommandInput->setEnabled(true);
	}

	m_pIconInput->setEnabled(true);
	m_pIconBrowserButton->setEnabled(true);

	m_pNameInput->setText(pCommandDesc->GetName());
	m_pCommandInput->setText(pCommandDesc->GetCommand());
	m_pIconInput->setText(pCommandDesc->GetIcon());
}

void CToolBarCustomizeDialog::ShowCVarWidgets()
{
	std::shared_ptr<CToolBarService::QCVarDesc> pCVarDesc = std::static_pointer_cast<CToolBarService::QCVarDesc>(m_pSelectedItem);

	for (QWidget* pWidget : m_commandWidgets)
	{
		pWidget->blockSignals(true);
		pWidget->setVisible(false);
	}

	for (QWidget* pWidget : m_cvarWidgets)
	{
		pWidget->setVisible(true);
		pWidget->blockSignals(false);
	}

	m_pIconInput->setEnabled(true);
	m_pIconInput->setText(pCVarDesc->GetIcon());
	m_pIconBrowserButton->setEnabled(true);
	m_pCVarValueInput->setEnabled(false);
	m_pCVarInput->setEnabled(true);
	m_pCVarInput->setText(pCVarDesc->GetName());
	m_pCVarBrowserButton->setEnabled(true);

	const string cvarName = QtUtil::ToString(m_pCVarInput->text());
	if (cvarName.empty())
		return;

	ICVar* pCVar = gEnv->pConsole->GetCVar(cvarName.c_str());

	if (pCVar)
	{
		m_pCVarValueInput->setEnabled(true);
		m_pCVarValueInput->setText(pCVarDesc->GetValue().toString());
		SetCVarValueRegExp();
	}
}

void CToolBarCustomizeDialog::CurrentItemChanged()
{
	CToolBarService* pToolBarService = GetIEditor()->GetToolBarService();
	std::shared_ptr<CToolBarService::QToolBarDesc> pToolBarDesc;

	string currentToolBarName = QtUtil::ToString(m_pToolbarSelect->GetCurrentText());
	if (currentToolBarName.empty())
	{
		m_pDropContainer->SetCurrentToolBarDesc(pToolBarDesc);
		return;
	}

	if (m_pEditor)
	{
		pToolBarDesc = pToolBarService->GetToolBarDesc(m_pEditor, currentToolBarName.c_str());
	}
	else
	{
		// Legacy way of getting a toolbar descriptor. This will be deprecated as soon as the mainframe is unable to host
		// toolbars
		pToolBarDesc = pToolBarService->GetToolBarDesc(PathUtil::Make(m_editorName.c_str(),
		                                                              currentToolBarName.c_str(),
		                                                              ".json"));
	}

	m_pDropContainer->SetCurrentToolBarDesc(pToolBarDesc);
}

void CToolBarCustomizeDialog::OnAddToolBar()
{
	CToolBarService* pToolBarService = GetIEditor()->GetToolBarService();
	std::set<string> toolBarNames = pToolBarService->GetToolBarNames(m_editorName.c_str());

	string name = "New ToolBar";
	int count = 0;

	while (toolBarNames.find(name.c_str()) != toolBarNames.cend())
	{
		name.Format("New ToolBar %d", ++count);
	}

	std::shared_ptr<CToolBarService::QToolBarDesc> pToolBarDesc;
	if (m_pEditor)
	{
		pToolBarDesc = pToolBarService->CreateToolBarDesc(m_pEditor, name.c_str());
	}
	else
	{
		// Legacy way of getting a toolbar descriptor. This will be deprecated as soon as the mainframe is unable to host
		// toolbars
		pToolBarDesc = pToolBarService->CreateToolBarDesc(m_editorName.c_str(), name.c_str());
	}

	pToolBarService->SaveToolBar(pToolBarDesc);
	signalToolBarAdded(pToolBarService->CreateEditorToolBar(pToolBarDesc, m_pEditor));

	m_pToolbarSelect->AddItem(name.c_str());
	m_pToolbarSelect->SetCurrentItem(name.c_str());
	m_pToolbarSelect->OnBeginEditing();
}

void CToolBarCustomizeDialog::OnRemoveToolBar()
{
	m_pToolbarSelect->OnEditingCancelled();

	std::shared_ptr<CToolBarService::QToolBarDesc> pCurrentToolBarDesc = m_pDropContainer->GetCurrentToolBarDesc();
	// Just return if there is no current toolbar being edited
	if (!pCurrentToolBarDesc)
		return;

	GetIEditor()->GetToolBarService()->RemoveToolBar(pCurrentToolBarDesc);

	string toolBarObjectName = QtUtil::ToString(pCurrentToolBarDesc->GetObjectName());
	signalToolBarRemoved(toolBarObjectName.c_str());

	m_pToolbarSelect->RemoveCurrentItem();
}

void CToolBarCustomizeDialog::OnToolBarModified(std::shared_ptr<CToolBarService::QToolBarDesc> pToolBarDesc)
{
	signalToolBarModified(GetIEditor()->GetToolBarService()->CreateEditorToolBar(pToolBarDesc, m_pEditor));
}

void CToolBarCustomizeDialog::RenameToolBar(const QString& before, const QString& after)
{
	// Don't allow empty names or names with all spaces as a name
	if (before.trimmed().isEmpty() || after.trimmed().isEmpty())
		return;

	CToolBarService* pToolBarService = GetIEditor()->GetToolBarService();
	std::shared_ptr<CToolBarService::QToolBarDesc> pToolBarDesc;
	string toolBarName = QtUtil::ToString(before);

	if (m_pEditor)
	{
		pToolBarDesc = pToolBarService->GetToolBarDesc(m_pEditor, toolBarName.c_str());
	}
	else
	{
		// Legacy way of getting a toolbar descriptor. This will be deprecated as soon as the mainframe is unable to host
		// toolbars
		pToolBarDesc = pToolBarService->GetToolBarDesc(PathUtil::Make(m_editorName.c_str(),
		                                                              toolBarName.c_str(),
		                                                              ".json"));
	}

	// attempt to save tool bar with new name
	pToolBarDesc->SetName(after);
	if (pToolBarService->SaveToolBar(pToolBarDesc))
	{
		pToolBarDesc->SetName(before);
		pToolBarService->RemoveToolBar(pToolBarDesc);
		const string oldObjectName = QtUtil::ToString(pToolBarDesc->GetObjectName());

		pToolBarDesc->SetName(after);
		signalToolBarRenamed(oldObjectName.c_str(), pToolBarService->CreateEditorToolBar(pToolBarDesc, m_pEditor));
		m_pDropContainer->SetCurrentToolBarDesc(pToolBarDesc);
	}
}

QString CToolBarCustomizeDialog::GetCurrentToolBarText()
{
	return m_pToolbarSelect->GetCurrentText();
}

void CToolBarCustomizeDialog::dragEnterEvent(QDragEnterEvent* pEvent)
{
	const CDragDropData* pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());

	if (pDragDropData->HasCustomData(QDropContainer::GetToolBarItemMimeType()))
	{
		pEvent->acceptProposedAction();
	}
}

void CToolBarCustomizeDialog::dropEvent(QDropEvent* pEvent)
{
	const CDragDropData* pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());

	if (pDragDropData->HasCustomData(QDropContainer::GetToolBarItemMimeType()))
	{
		m_pDropContainer->RemoveItem(m_pSelectedItem);
	}
}

void CToolBarCustomizeDialog::OnContextMenu(const QPoint& position) const
{
	QModelIndex index = m_pTreeView->indexAt(position);

	if (!index.isValid())
		return;

	CCommand* pCommand = nullptr;
	QVariant commandVar = index.model()->data(index, (int)CCommandModel::Roles::CommandPointerRole);
	if (commandVar.isValid())
		pCommand = commandVar.value<CCommand*>();

	if (!pCommand)
		return;

	QMenu* menu = new QMenu();
	QAction* pAction = menu->addAction("Add");
	pAction->setEnabled(!m_pToolbarSelect->GetCurrentText().isEmpty());

	connect(pAction, &QAction::triggered, [this, pCommand]()
	{
		m_pDropContainer->AddCommand(pCommand);
	});

	menu->popup(m_pTreeView->mapToGlobal(position));
}


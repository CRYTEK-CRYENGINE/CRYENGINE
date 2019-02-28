// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "QToolBarCreator.h"

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

#include "IEditorImpl.h"
#include "QT/QtMainFrame.h"
#include "Controls/QEditableComboBox.h"
#include "Controls/QResourceBrowserDialog.h"
#include "QtUtil.h"
#include "Commands/QCommandAction.h"
#include "Commands/CustomCommand.h"
#include "Commands/CVarListDockable.h"

#include "QAdvancedTreeView.h"
#include "ProxyModels/DeepFilterProxyModel.h"
#include "Commands/CommandModel.h"

#include "QMainToolBarManager.h"
#include "QSearchBox.h"
#include <CryIcon.h>

static const char* s_dragType = "CEToolBarItem";
static const char* s_dragListType = "CECommandListItem";

//////////////////////////////////////////////////////////////////////////

class QToolBarCreator::DropCommandModel : public CommandModel
{
	friend CommandModelFactory;
public:
	//QAbstractItemModel implementation begin
	virtual Qt::DropActions supportedDropActions() const override;
	virtual Qt::ItemFlags   flags(const QModelIndex& index) const override;
	virtual QMimeData*      mimeData(const QModelIndexList& indexes) const override;
	//QAbstractItemModel implementation end

protected:
	DropCommandModel();
};

QToolBarCreator::DropCommandModel::DropCommandModel()
{

}

Qt::DropActions QToolBarCreator::DropCommandModel::supportedDropActions() const
{
	return Qt::CopyAction;
}

Qt::ItemFlags QToolBarCreator::DropCommandModel::flags(const QModelIndex& index) const
{
	Qt::ItemFlags flags = CommandModel::flags(index);

	CCommand* pCommand = nullptr;
	QVariant commandVar = index.model()->data(index, (int)CommandModel::Roles::CommandPointerRole);
	if (commandVar.isValid())
	{
		pCommand = commandVar.value<CCommand*>();
	}

	if (index.isValid() && pCommand)
		return flags | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
	else
		return flags;
}

QMimeData* QToolBarCreator::DropCommandModel::mimeData(const QModelIndexList& indexes) const
{
	QMimeData* pMimeData = new QMimeData();

	CCommand* pCommand = nullptr;

	for (const QModelIndex& index : indexes)
	{
		if (index.isValid())
		{
			QVariant commandVar = index.model()->data(index, (int)CommandModel::Roles::CommandPointerRole);
			if (commandVar.isValid())
			{
				pCommand = commandVar.value<CCommand*>();
			}
		}
	}

	if (pCommand)
	{
		QMainToolBarManager::QCommandDesc commandDesc(pCommand);
		QJsonDocument doc = QJsonDocument::fromVariant(commandDesc.ToVariant());
		pMimeData->setData(s_dragListType, doc.toBinaryData());
	}

	return pMimeData;
}

//////////////////////////////////////////////////////////////////////////

class QToolBarCreator::QCustomToolBar : public QToolBar
{
public:
	QCustomToolBar(const QString& title)
		: QToolBar(title)
		, m_pSelectedWidget(nullptr)
		, m_bShowDropTarget(false)
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

	void SelectWidget(QWidget* pWidget)
	{
		m_pSelectedWidget = pWidget;
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

		if (m_pSelectedWidget)
		{
			painter.setRenderHint(QPainter::Antialiasing);
			painter.setPen(QPen(QColor(166, 166, 166, 255)));

			QSize widgetSize = m_pSelectedWidget->size();
			widgetSize.setHeight(widgetSize.height() - 1);
			painter.drawRoundedRect(QRect(m_pSelectedWidget->pos(), widgetSize), 2, 2);
		}

		if (m_bShowDropTarget)
		{
			QAction* pAction = actionAt(m_DropTargetPos);
			QWidget* pWidget = widgetForAction(pAction);

			QPixmap pixmap(":toolbar-drop-target.png");
			int spacing = (pixmap.width() + layout()->spacing()) / 2;
			QPoint drawPos = QPoint(m_DropTargetPos.x() - spacing, 0);
			QRect drawRect(drawPos, QSize(pixmap.width(), size().height()));

			painter.drawTiledPixmap(drawRect, pixmap);
		}

		painter.restore();
	}

protected:
	QWidget* m_pSelectedWidget;
	QPoint   m_DragStartPosition;
	QPoint   m_DropTargetPos;
	bool     m_bShowDropTarget;
};

QToolBarCreator::QDropContainer::QDropContainer(QToolBarCreator* pParent)
	: QWidget(pParent)
	, m_pToolBarCreator(pParent)
	, m_pPreviewLayout(nullptr)
	, m_pCurrentToolBar(nullptr)
	, m_bDragStarted(false)
{
	setAcceptDrops(true);
	m_pPreviewLayout = new QHBoxLayout();
	m_pPreviewLayout->setMargin(0);
	BuildPreview();
	setLayout(m_pPreviewLayout);
}

QToolBarCreator::QDropContainer::~QDropContainer()
{
	if (m_pCurrentToolBar)
	{
		delete m_pCurrentToolBar;
		m_pCurrentToolBar = nullptr;
	}
}

void QToolBarCreator::QDropContainer::AddItem(const QVariant& itemVariant, int idx /* = -1*/)
{
	m_pCurrentToolBarDesc->InsertItem(itemVariant, idx);

	CEditorMainFrame::GetInstance()->GetToolBarManager()->UpdateToolBar(m_pCurrentToolBarDesc);
	BuildPreview();
}

void QToolBarCreator::QDropContainer::AddCommand(const CCommand* pCommand, int idx /* = -1*/)
{
	m_pCurrentToolBarDesc->InsertCommand(pCommand, idx);

	CEditorMainFrame::GetInstance()->GetToolBarManager()->UpdateToolBar(m_pCurrentToolBarDesc);
	BuildPreview();
}

void QToolBarCreator::QDropContainer::AddSeparator(int sourceIdx /* = -1*/, int targetIdx /* = -1*/)
{
	if (sourceIdx > -1)
		m_pCurrentToolBarDesc->MoveItem(sourceIdx, targetIdx);
	else
		m_pCurrentToolBarDesc->InsertSeparator(targetIdx);

	CEditorMainFrame::GetInstance()->GetToolBarManager()->UpdateToolBar(m_pCurrentToolBarDesc);
	BuildPreview();
}

void QToolBarCreator::QDropContainer::AddCVar(const QString& cvarName, int idx /* = -1*/)
{
	if (cvarName.isEmpty())
	{
		m_pCurrentToolBarDesc->InsertCVar(cvarName, idx);
	}

	CEditorMainFrame::GetInstance()->GetToolBarManager()->UpdateToolBar(m_pCurrentToolBarDesc);
	BuildPreview();
}

void QToolBarCreator::QDropContainer::RemoveCommand(const CCommand* pCommand)
{
	m_pCurrentToolBarDesc->RemoveItem(m_pCurrentToolBarDesc->IndexOfCommand(pCommand));

	CEditorMainFrame::GetInstance()->GetToolBarManager()->UpdateToolBar(m_pCurrentToolBarDesc);
	BuildPreview();
}

void QToolBarCreator::QDropContainer::RemoveItem(std::shared_ptr<QMainToolBarManager::QItemDesc> pItem)
{
	m_pCurrentToolBarDesc->RemoveItem(pItem);

	CEditorMainFrame::GetInstance()->GetToolBarManager()->UpdateToolBar(m_pCurrentToolBarDesc);
	BuildPreview();
}

void QToolBarCreator::QDropContainer::RemoveItemAt(int idx)
{
	m_pCurrentToolBarDesc->RemoveItem(idx);

	CEditorMainFrame::GetInstance()->GetToolBarManager()->UpdateToolBar(m_pCurrentToolBarDesc);
	BuildPreview();
}

int QToolBarCreator::QDropContainer::GetIndexFromMouseCoord(const QPoint& globalPos)
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

void QToolBarCreator::QDropContainer::BuildPreview()
{
	if (m_pCurrentToolBar)
	{
		selectedItemChanged(nullptr);
		m_pPreviewLayout->removeWidget(m_pCurrentToolBar);
		m_pCurrentToolBar->deleteLater();
		m_pCurrentToolBar = nullptr;
	}

	QMap<QString, std::shared_ptr<QMainToolBarManager::QToolBarDesc>> toolBars = CEditorMainFrame::GetInstance()->GetToolBarManager()->GetToolBars();
	QString toolBarText = m_pToolBarCreator->GetCurrentToolBarText();
	if (toolBarText.isEmpty())
		return;

	m_pCurrentToolBarDesc = toolBars[m_pToolBarCreator->GetCurrentToolBarText()];
	m_pCurrentToolBar = CreateToolBar(m_pCurrentToolBarDesc->GetName(), m_pCurrentToolBarDesc);
	m_pPreviewLayout->addWidget(m_pCurrentToolBar);
}

void QToolBarCreator::QDropContainer::ShowToolBarContextMenu(const QPoint& position)
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

void QToolBarCreator::QDropContainer::SetSelectedActionIcon(const char* szPath)
{
	int idx = m_pCurrentToolBarDesc->IndexOfItem(m_pSelectedItem);
	if (idx < 0)
		return;

	QList<QAction*> toolBarActions = m_pCurrentToolBar->actions();
	if (idx >= toolBarActions.size())
		return;

	toolBarActions[idx]->setIcon(CryIcon(szPath));
}

QToolBarCreator::QCustomToolBar* QToolBarCreator::QDropContainer::CreateToolBar(const QString& title, const std::shared_ptr<QMainToolBarManager::QToolBarDesc> toolBarDesc)
{
	if (title.isEmpty())
		return nullptr;

	QCustomToolBar* pToolBar = new QCustomToolBar(title);
	pToolBar->setObjectName(title + "ToolBar");
	pToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(pToolBar, &QToolBar::customContextMenuRequested, this, &QToolBarCreator::QDropContainer::ShowToolBarContextMenu);

	CEditorMainFrame* pMainFrame = CEditorMainFrame::GetInstance();
	pMainFrame->GetToolBarManager()->CreateToolBar(toolBarDesc, pToolBar);
	QList<QAction*> toolBarActions = pToolBar->actions();

	for (QAction* pAction : toolBarActions)
	{
		QWidget* pWidget = pToolBar->widgetForAction(pAction);
		pWidget->installEventFilter(this); // install event filter so we can catch the mouse pressed events
	}

	return pToolBar;
}

bool QToolBarCreator::QDropContainer::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (pEvent->type() == QEvent::MouseButtonPress)
	{
		QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(pEvent);
		if (pMouseEvent->button() == Qt::LeftButton)
		{
			QWidget* pWidget = qobject_cast<QWidget*>(pObject);
			if (!pWidget)
			{
				m_pSelectedItem = nullptr;
				selectedItemChanged(nullptr);
				return false;
			}

			m_pCurrentToolBar->SelectWidget(pWidget);
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

void QToolBarCreator::QDropContainer::mouseMoveEvent(QMouseEvent* pEvent)
{
	if (!m_pSelectedItem || !m_bDragStarted || !(pEvent->buttons() & Qt::LeftButton))
		return;

	if ((pEvent->pos() - m_DragStartPosition).manhattanLength() < QApplication::startDragDistance())
		return;

	QDrag* pDrag = new QDrag(this);
	connect(pDrag, &QDrag::destroyed, [this]()
	{
		m_pCurrentToolBar->SetShowDropTarget(false);
	});
	QMimeData* pMimeData = new QMimeData();

	QJsonDocument doc = QJsonDocument::fromVariant(m_pSelectedItem->ToVariant());
	pMimeData->setData(s_dragType, doc.toBinaryData());

	pDrag->setMimeData(pMimeData);
	m_pCurrentToolBar->SetDragStartPosition(m_DragStartPosition);
	Qt::DropAction dropAction = pDrag->exec(Qt::CopyAction | Qt::MoveAction);
	m_pCurrentToolBar->DrawDropTarget(pEvent->globalPos());
}

void QToolBarCreator::QDropContainer::dragEnterEvent(QDragEnterEvent* pEvent)
{
	const QMimeData* pMimeData = pEvent->mimeData();
	if (pMimeData->hasFormat(s_dragType) || pMimeData->hasFormat(s_dragListType))
	{
		pEvent->acceptProposedAction();
		m_pCurrentToolBar->DrawDropTarget(mapToGlobal(pEvent->pos()));
	}
}

void QToolBarCreator::QDropContainer::dragMoveEvent(QDragMoveEvent* pEvent)
{
	const QMimeData* pMimeData = pEvent->mimeData();
	if (pMimeData->hasFormat(s_dragType))
	{
		pEvent->acceptProposedAction();
		m_pCurrentToolBar->DrawDropTarget(mapToGlobal(pEvent->pos()));
	}
}

void QToolBarCreator::QDropContainer::dragLeaveEvent(QDragLeaveEvent* pEvent)
{
	m_pCurrentToolBar->SetShowDropTarget(false);
}

void QToolBarCreator::QDropContainer::dropEvent(QDropEvent* pEvent)
{
	m_pCurrentToolBar->SetShowDropTarget(false);

	if (pEvent->mimeData()->hasFormat(s_dragType))
	{
		QJsonDocument doc = QJsonDocument::fromBinaryData(pEvent->mimeData()->data(s_dragType));
		QVariant itemVariant = doc.toVariant();

		int oldIdx = m_pCurrentToolBarDesc->IndexOfItem(m_pSelectedItem);
		int newIdx = GetIndexFromMouseCoord(mapToGlobal(pEvent->pos()));
		if (oldIdx == newIdx)
			return;

		AddItem(itemVariant, newIdx);

		if (oldIdx >= 0)
		{
			if (oldIdx > newIdx)
				++oldIdx;
			RemoveItemAt(oldIdx);
		}
	}
	if (pEvent->mimeData()->hasFormat(s_dragListType))
	{
		QJsonDocument doc = QJsonDocument::fromBinaryData(pEvent->mimeData()->data(s_dragListType));
		QVariantMap itemMap = doc.toVariant().toMap();

		QString command = itemMap["command"].toString();
		CCommand* pCommand = GetIEditorImpl()->GetCommandManager()->GetCommand(command.toLocal8Bit());
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

QToolBarCreator::QToolBarCreator()
	: CEditorDialog("ToolBar Creator")
	, m_pSelectedItem(nullptr)
{
	m_pToolbarSelect = new QEditableComboBox();
	connect(m_pToolbarSelect, &QEditableComboBox::ItemRenamed, this, &QToolBarCreator::RenameToolBar);

	QMap<QString, std::shared_ptr<QMainToolBarManager::QToolBarDesc>> toolBars = CEditorMainFrame::GetInstance()->GetToolBarManager()->GetToolBars();

	QStringList itemNames;
	for (QMap<QString, std::shared_ptr<QMainToolBarManager::QToolBarDesc>>::const_iterator ite = toolBars.begin(); ite != toolBars.end(); ++ite)
	{
		const std::shared_ptr<QMainToolBarManager::QToolBarDesc> toolBarDesc = ite.value();
		if (toolBarDesc->GetName().isEmpty())
			continue;

		itemNames.push_back(toolBarDesc->GetName());
	}

	m_pToolbarSelect->AddItems(itemNames);

	QVBoxLayout* pLayout = new QVBoxLayout();
	QHBoxLayout* pInnerLayout = new QHBoxLayout();

	QToolButton* pAddToolBar = new QToolButton();
	pAddToolBar->setIcon(CryIcon("icons:General/Element_Add.ico"));
	pAddToolBar->setToolTip("Create new Toolbar");
	connect(pAddToolBar, &QToolButton::clicked, this, &QToolBarCreator::OnAddToolBar);

	QToolButton* pRemoveToolBar = new QToolButton();
	pRemoveToolBar->setIcon(CryIcon("icons:General/Element_Remove.ico"));
	pRemoveToolBar->setToolTip("Remove Toolbar");
	connect(pRemoveToolBar, &QToolButton::clicked, this, &QToolBarCreator::OnRemoveToolBar);

	QToolButton* pRenameToolBar = new QToolButton();
	pRenameToolBar->setIcon(CryIcon("icons:General/Edit.ico"));
	pRenameToolBar->setToolTip("Rename Toolbar");
	connect(pRenameToolBar, &QToolButton::clicked, [this]()
	{
		// Don't rename unless there's a toolbar selected (not empty)
		if (!m_pToolbarSelect->GetCurrentText().isEmpty())
			m_pToolbarSelect->OnBeginEditing();
	});

	pInnerLayout->addWidget(new QLabel("Toolbar"));
	pInnerLayout->addWidget(m_pToolbarSelect);
	pInnerLayout->addWidget(pRenameToolBar);
	pInnerLayout->addWidget(pAddToolBar);
	pInnerLayout->addWidget(pRemoveToolBar);

	m_pTreeView = new QAdvancedTreeView();
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pTreeView, &QTreeView::customContextMenuRequested, this, &QToolBarCreator::OnContextMenu);

	m_pItemModel = CommandModelFactory::Create<DropCommandModel>();

	m_pProxyModel = new QDeepFilterProxyModel();
	m_pProxyModel->setSourceModel(m_pItemModel);

	QSearchBox* pSearchBox = new QSearchBox();
	pSearchBox->EnableContinuousSearch(true);
	pSearchBox->SetModel(m_pProxyModel);

	m_pTreeView->setModel(m_pProxyModel);
	m_pTreeView->setDragEnabled(true);
	m_pTreeView->setSortingEnabled(true);
	m_pTreeView->sortByColumn(0, Qt::AscendingOrder);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->setAllColumnsShowFocus(true);
	m_pTreeView->header()->setStretchLastSection(true);
	m_pTreeView->header()->setDefaultSectionSize(300);

	pSearchBox->SetAutoExpandOnSearch(m_pTreeView);

	m_pDropContainer = new QDropContainer(this);
	m_pDropContainer->setObjectName("DropContainer");
	connect(m_pToolbarSelect, &QEditableComboBox::OnCurrentIndexChanged, this, &QToolBarCreator::CurrentItemChanged);

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

	pLayout->addWidget(pSearchBox);
	pLayout->addWidget(m_pTreeView);
	pLayout->addLayout(pInnerLayout);
	pLayout->addWidget(m_pDropContainer);
	pLayout->addLayout(pNameLayout);
	pLayout->addLayout(pCommandLayout);
	pLayout->addLayout(pCVarLayout);
	pLayout->addLayout(pCVarValueLayout);
	pLayout->addLayout(pIconLayout);

	m_pDropContainer->selectedItemChanged.Connect(this, &QToolBarCreator::OnSelectedItemChanged);

	setAcceptDrops(true);
	setLayout(pLayout);
}

void QToolBarCreator::IconSelected(const char* szIconPath)
{
	m_pIconInput->setText(szIconPath);
	SetIconPath(szIconPath);
}

void QToolBarCreator::CVarsSelected(const QStringList& selectedCVars)
{
	if (selectedCVars.size() != 1)
		return;

	const QString& selectedCVar = selectedCVars[0];
	m_pCVarInput->setText(selectedCVar);
	SetCVarName(selectedCVar.toLocal8Bit());
}

void QToolBarCreator::SetCVarName(const char* szCVarName)
{
	ICVar* pCVar = gEnv->pConsole->GetCVar(szCVarName);
	if (!pCVar)
	{
		m_pCVarInput->clear();
		m_pCVarValueInput->setEnabled(false);
		return;
	}

	std::static_pointer_cast<QMainToolBarManager::QCVarDesc>(m_pSelectedItem)->SetCVar(szCVarName);
	m_pCVarValueInput->setEnabled(true);
	SetCVarValueRegExp();
}

void QToolBarCreator::SetCVarValueRegExp()
{
	ICVar* pCVar = gEnv->pConsole->GetCVar(m_pCVarInput->text().toLocal8Bit());
	if (!pCVar)
		return;

	switch (pCVar->GetType())
	{
	case CVAR_INT:
		m_pCVarValueInput->setValidator(new QIntValidator());
		break;

	case CVAR_FLOAT:
		m_pCVarValueInput->setValidator(new QDoubleValidator());
		break;

	default:
		m_pCVarValueInput->setValidator(nullptr);
	}
}

void QToolBarCreator::SetCVarValue(const char* cvarValue)
{
	ICVar* pCVar = gEnv->pConsole->GetCVar(m_pCVarInput->text().toLocal8Bit());
	if (!pCVar)
	{
		m_pCVarValueInput->clear();
		return;
	}

	QString valueStr(cvarValue);
	switch (pCVar->GetType())
	{
	case CVAR_INT:
		std::static_pointer_cast<QMainToolBarManager::QCVarDesc>(m_pSelectedItem)->SetCVarValue(valueStr.toInt());
		break;
	case CVAR_FLOAT:
		std::static_pointer_cast<QMainToolBarManager::QCVarDesc>(m_pSelectedItem)->SetCVarValue(valueStr.toDouble());
		break;

	default:
		std::static_pointer_cast<QMainToolBarManager::QCVarDesc>(m_pSelectedItem)->SetCVarValue(cvarValue);
	}
}

void QToolBarCreator::SetCommandName(const char* commandName)
{
	std::shared_ptr<QMainToolBarManager::QCommandDesc> pCommandDesc = std::static_pointer_cast<QMainToolBarManager::QCommandDesc>(m_pSelectedItem);

	if (!pCommandDesc->IsCustom())
		return;

	CCommand* pCommand = GetIEditorImpl()->GetCommandManager()->GetCommand(pCommandDesc->GetCommand().toStdString().c_str());
	CCustomCommand* pCustomCommand = static_cast<CCustomCommand*>(pCommand);
	pCustomCommand->SetName(commandName);
	pCommandDesc->SetName(commandName);
}

void QToolBarCreator::SetIconPath(const char* szIconPath)
{
	if (m_pSelectedItem->GetType() == QMainToolBarManager::QItemDesc::Command)
	{
		std::static_pointer_cast<QMainToolBarManager::QCommandDesc>(m_pSelectedItem)->SetIcon(szIconPath);
	}
	else if (m_pSelectedItem->GetType() == QMainToolBarManager::QItemDesc::CVar)
	{
		m_pDropContainer->SetSelectedActionIcon(szIconPath);
		std::static_pointer_cast<QMainToolBarManager::QCVarDesc>(m_pSelectedItem)->SetIcon(szIconPath);
	}
}

void QToolBarCreator::OnSelectedItemChanged(std::shared_ptr<QMainToolBarManager::QItemDesc> selectedItem)
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
		m_pCVarBrowserButton->setEnabled(false);
		return;
	}

	if (m_pSelectedItem->GetType() == QMainToolBarManager::QItemDesc::Command)
	{
		ShowCommandWidgets();
	}
	else if (m_pSelectedItem->GetType() == QMainToolBarManager::QItemDesc::CVar)
	{
		ShowCVarWidgets();
	}

}

void QToolBarCreator::ShowCommandWidgets()
{
	std::shared_ptr<QMainToolBarManager::QCommandDesc> pCommandDesc = std::static_pointer_cast<QMainToolBarManager::QCommandDesc>(m_pSelectedItem);

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

void QToolBarCreator::ShowCVarWidgets()
{
	std::shared_ptr<QMainToolBarManager::QCVarDesc> pCVarDesc = std::static_pointer_cast<QMainToolBarManager::QCVarDesc>(m_pSelectedItem);

	for (QWidget* pWidget : m_commandWidgets)
		pWidget->setVisible(false);

	for (QWidget* pWidget : m_cvarWidgets)
		pWidget->setVisible(true);

	m_pIconInput->setEnabled(true);
	m_pIconInput->setText(pCVarDesc->GetIcon());
	m_pIconBrowserButton->setEnabled(true);
	m_pCVarValueInput->setEnabled(false);
	m_pCVarInput->setEnabled(true);
	m_pCVarInput->setText(pCVarDesc->GetName());
	m_pCVarBrowserButton->setEnabled(true);

	QString cvarName = m_pCVarInput->text();
	if (cvarName.isEmpty())
		return;

	ICVar* pCVar = gEnv->pConsole->GetCVar(cvarName.toLocal8Bit());

	if (pCVar)
	{
		m_pCVarValueInput->setEnabled(true);
		m_pCVarValueInput->setText(pCVarDesc->GetValue().toString());
		SetCVarValueRegExp();
	}
}

void QToolBarCreator::CurrentItemChanged(int index)
{
	m_pDropContainer->BuildPreview();
}

void QToolBarCreator::OnAddToolBar()
{
	QMainToolBarManager* pToolBarManager = CEditorMainFrame::GetInstance()->GetToolBarManager();
	QMap<QString, std::shared_ptr<QMainToolBarManager::QToolBarDesc>> toolBars = pToolBarManager->GetToolBars();

	std::shared_ptr<QMainToolBarManager::QToolBarDesc> newToolBar = std::make_shared<QMainToolBarManager::QToolBarDesc>();
	QString name = "New ToolBar";
	int count = 0;

	while (toolBars.contains(name))
	{
		name = "New ToolBar" + QString::number(++count);
	}

	newToolBar->SetName(name);
	pToolBarManager->AddToolBar(name, newToolBar);

	m_pToolbarSelect->AddItem(name);
	m_pToolbarSelect->SetCurrentItem(name);
	m_pToolbarSelect->OnBeginEditing();
}

void QToolBarCreator::OnRemoveToolBar()
{
	m_pToolbarSelect->OnEditingCancelled();

	QString currentText = m_pToolbarSelect->GetCurrentText();

	CEditorMainFrame::GetInstance()->GetToolBarManager()->RemoveToolBar(currentText);
	m_pToolbarSelect->RemoveCurrentItem();
}

void QToolBarCreator::RenameToolBar(const QString& before, const QString& after)
{
	QMainToolBarManager* pToolBarManager = CEditorMainFrame::GetInstance()->GetToolBarManager();

	const QMap<QString, std::shared_ptr<QMainToolBarManager::QToolBarDesc>> toolbars = pToolBarManager->GetToolBars();

	if (toolbars.contains(before))
	{
		std::shared_ptr<QMainToolBarManager::QToolBarDesc> toolbar = toolbars[before];
		pToolBarManager->RemoveToolBar(before);
		toolbar->SetName(after);
		pToolBarManager->AddToolBar(after, toolbar);
	}
}

QString QToolBarCreator::GetCurrentToolBarText()
{
	return m_pToolbarSelect->GetCurrentText();
}

void QToolBarCreator::dragEnterEvent(QDragEnterEvent* pEvent)
{
	if (pEvent->mimeData()->hasFormat(s_dragType))
	{
		pEvent->acceptProposedAction();
	}
}

void QToolBarCreator::dropEvent(QDropEvent* pEvent)
{
	if (pEvent->mimeData()->hasFormat(s_dragType))
	{
		m_pDropContainer->RemoveItem(m_pSelectedItem);
	}
}

void QToolBarCreator::OnContextMenu(const QPoint& position) const
{
	CUiCommand* command = nullptr;
	QModelIndex index = m_pTreeView->indexAt(position);

	if (!index.isValid())
		return;

	CCommand* pCommand = nullptr;
	QVariant commandVar = index.model()->data(index, (int)CommandModel::Roles::CommandPointerRole);
	if (commandVar.isValid())
		pCommand = commandVar.value<CCommand*>();

	if (!pCommand)
		return;

	QMenu* menu = new QMenu();
	QAction* pAction = menu->addAction("Add");

	connect(pAction, &QAction::triggered, [this, pCommand]()
	{
		m_pDropContainer->AddCommand(pCommand);
	});

	menu->popup(m_pTreeView->mapToGlobal(position));
}


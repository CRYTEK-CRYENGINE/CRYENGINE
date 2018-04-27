// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObjectStructureWidget.h"

#include "ScriptBrowserUtils.h"
#include "PropertiesWidget.h"
#include "NameEditorWidget.h"

#include "StateItem.h"

#include "ObjectModel.h"

#include <QtUtil.h>
#include <QFilteringPanel.h>
#include <QAdvancedPropertyTree.h>
#include <ProxyModels/AttributeFilterProxyModel.h>
#include <Controls/QPopupWidget.h>
#include <Controls/DictionaryWidget.h>
#include <EditorFramework/BroadcastManager.h>
#include <EditorFramework/Inspector.h>
#include <ICommandManager.h>

#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QAdvancedTreeView.h>
#include <QLabel>
#include <QString>
#include <QHelpEvent>
#include <QSizePolicy>
#include <QToolButton>
#include <QToolBar>
#include <QFont>
#include <QItemSelection>
#include <QMenu>
#include <QVariantMap>
#include <QCollapsibleFrame.h>

namespace CrySchematycEditor {

class CObjectStructureModel : public QAbstractItemModel
{
public:
	enum EColumn
	{
		Column_Name,

		Column_Count
	};

	enum ERole : int32
	{
		Role_Display = Qt::DisplayRole,
		Role_ToolTip = Qt::ToolTipRole,
		Role_Icon    = Qt::DecorationRole,
		Role_Pointer = Qt::UserRole
	};

public:
	CObjectStructureModel(CAbstractObjectStructureModel& model)
		: m_model(model)
	{
		m_model.SignalObjectStructureItemAdded.Connect(this, &CObjectStructureModel::OnItemAdded);
		m_model.SignalObjectStructureItemRemoved.Connect(this, &CObjectStructureModel::OnItemRemoved);
		m_model.SignalObjectStructureItemInvalidated.Connect(this, &CObjectStructureModel::OnItemInvalidated);
	}

	~CObjectStructureModel()
	{
		m_model.SignalObjectStructureItemAdded.DisconnectObject(this);
		m_model.SignalObjectStructureItemRemoved.DisconnectObject(this);
		m_model.SignalObjectStructureItemInvalidated.DisconnectObject(this);
	}

	// QAbstractItemModel
	virtual int rowCount(const QModelIndex& parent) const override
	{
		if (!parent.isValid())
		{
			return m_model.GetGraphItemCount() + m_model.GetStateMachineItemCount();
		}
		else
		{
			const CAbstractObjectStructureModelItem* pItem = static_cast<const CAbstractObjectStructureModelItem*>(parent.internalPointer());
			if (pItem)
			{
				return pItem->GetNumChildItems();
			}
		}
		return 0;
	}

	virtual int columnCount(const QModelIndex& parent) const override
	{
		return Column_Count;
	}

	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
	{
		if (orientation == Qt::Horizontal && role == Role_Display)
		{
			switch (section)
			{
			case Column_Name:
				return QString("Name");
			default:
				break;
			}
		}
		return QVariant();
	}

	virtual QVariant data(const QModelIndex& index, int role) const override
	{
		if (index.isValid())
		{
			const CAbstractObjectStructureModelItem* pItem = static_cast<const CAbstractObjectStructureModelItem*>(index.internalPointer());
			if (pItem)
			{
				switch (role)
				{
				case Role_Display:
					{
						if (index.column() == Column_Name)
						{
							if (!pItem->GetName().isEmpty())
								return pItem->GetName();
						}
					}
					break;
				case Role_ToolTip:
					{
						const QString text = pItem->GetToolTip();
						if (!text.isEmpty())
							return text;
					}
					break;
				case Role_Pointer:
					{
						return reinterpret_cast<quintptr>(pItem);
					}
				case Role_Icon:
					{
						if (index.column() == Column_Name)
						{
							const CryIcon* pIcon = pItem->GetIcon();
							if (pIcon && !pIcon->isNull())
								return *pItem->GetIcon();
						}
					}
					break;
				default:
					break;
				}
			}
		}
		return QVariant();
	}

	virtual Qt::ItemFlags flags(const QModelIndex& index) const override
	{
		Qt::ItemFlags flags = QAbstractItemModel::flags(index);

		if (index.isValid() && index.column() == Column_Name)
		{
			CAbstractObjectStructureModelItem* pItem = static_cast<CAbstractObjectStructureModelItem*>(index.internalPointer());
			if (pItem && pItem->AllowsRenaming())
			{
				flags |= Qt::ItemIsEditable;
			}
			else
			{
				flags &= ~Qt::ItemIsEditable;
			}
		}
		return flags;
	}

	virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override
	{
		if (!parent.isValid())
		{
			CAbstractObjectStructureModelItem* pItem = m_model.GetChildItemByIndex(row);
			return QAbstractItemModel::createIndex(row, column, reinterpret_cast<quintptr>(pItem));
		}
		else
		{
			CAbstractObjectStructureModelItem* pItem = static_cast<CAbstractObjectStructureModelItem*>(parent.internalPointer());
			if (pItem)
			{
				return QAbstractItemModel::createIndex(row, column, reinterpret_cast<quintptr>(pItem->GetChildItemByIndex(row)));
			}
		}

		return QModelIndex();
	}

	virtual QModelIndex parent(const QModelIndex& index) const override
	{
		if (index.isValid())
		{
			const CAbstractObjectStructureModelItem* pItem = static_cast<const CAbstractObjectStructureModelItem*>(index.internalPointer());
			if (pItem)
			{
				const CAbstractObjectStructureModelItem* pParentItem = pItem->GetParentItem();
				if (pParentItem)
				{
					const CAbstractObjectStructureModelItem* pParentParentItem = pParentItem->GetParentItem();
					if (pParentParentItem)
					{
						for (int32 i = 0; i < pParentParentItem->GetNumChildItems(); ++i)
						{
							if (pParentParentItem->GetChildItemByIndex(i) == pParentItem)
							{
								return QAbstractItemModel::createIndex(i, 0, reinterpret_cast<quintptr>(pParentItem));
							}
						}
					}
					else
					{
						for (int32 i = 0; i < m_model.GetNumItems(); ++i)
						{
							if (m_model.GetChildItemByIndex(i) == pParentItem)
							{
								return QAbstractItemModel::createIndex(i, 0, reinterpret_cast<quintptr>(pParentItem));
							}
						}
					}
				}
			}

		}
		return QModelIndex();
	}

	virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override
	{
		if (role == Qt::EditRole)
		{
			if (index.isValid())
			{
				switch (index.column())
				{
				case Column_Name:
					{
						CAbstractObjectStructureModelItem* pItem = reinterpret_cast<CAbstractObjectStructureModelItem*>(index.internalPointer());
						if (pItem)
						{
							const QString newName = value.value<QString>();
							if (pItem->GetName() != newName)
								pItem->SetName(newName);
						}
					}
					break;
				default:
					return false;
				}
			}

			return true;
		}

		return false;
	}
	// ~QAbstractItemModel

	QModelIndex GetIndexForItem(const CAbstractObjectStructureModelItem& item, int column = 0) const
	{
		//const uint32 row = item.GetIndex();

		/*std::function<QModelIndex(const CAbstractObjectStructureModelItem&, int)> GetIndexRecursive;
		   GetIndexRecursive = [&GetIndexRecursive, this](const CAbstractObjectStructureModelItem& item, int column) -> QModelIndex
		   {
		   const CAbstractObjectStructureModelItem* pParentItem = item.GetParentItem();
		   if (pParentItem)
		   {
		    const uint32 idx = item.GetIndex();
		    QModelIndex parent = GetIndexRecursive(*pParentItem, column);

		    return index(idx, column, parent);
		   }

		   const uint32 idx = item.GetIndex();
		   return index(idx, column, QModelIndex());
		   };*/

		const CAbstractObjectStructureModelItem* pParentItem = item.GetParentItem();
		if (pParentItem)
		{
			const uint32 idx = item.GetIndex();
			QModelIndex parent = GetIndexForItem(*pParentItem, column);

			return index(idx, column, parent);
		}

		const uint32 idx = item.GetIndex();
		return index(idx, column, QModelIndex());

		//return GetIndexRecursive(item, column);
	}

	void OnItemAdded(CAbstractObjectStructureModelItem& item)
	{
		const uint32 index = item.GetIndex();

		QModelIndex parent;
		if (CAbstractObjectStructureModelItem* pParentItem = item.GetParentItem())
		{
			parent = GetIndexForItem(*pParentItem);
		}
		QAbstractItemModel::beginInsertRows(parent, index, index);
		QAbstractItemModel::endInsertRows();

		// Note: We need to call dataChanged here otherwise it's not possible to edit the item directly after inserting it.
		const QModelIndex rowLeftCol = GetIndexForItem(item, 0);
		const QModelIndex rowRightCol = GetIndexForItem(item, Column_Count - 1);

		QVector<int> roles;
		roles.push_back(Qt::EditRole);
		QAbstractItemModel::dataChanged(rowLeftCol, rowRightCol);
	}

	void OnItemRemoved(CAbstractObjectStructureModelItem& item)
	{
		const uint32 index = item.GetIndex();

		QModelIndex parent;
		if (CAbstractObjectStructureModelItem* pParentItem = item.GetParentItem())
		{
			parent = GetIndexForItem(*pParentItem);
		}
		QAbstractItemModel::beginRemoveRows(parent, index, index);
		QAbstractItemModel::endRemoveRows();
	}

	void OnItemInvalidated(CAbstractObjectStructureModelItem& item)
	{
		const uint32 index = item.GetIndex();

		QModelIndex parent;
		if (CAbstractObjectStructureModelItem* pParentItem = item.GetParentItem())
		{
			parent = GetIndexForItem(*pParentItem);
		}

		const QModelIndex rowLeftCol = GetIndexForItem(item, 0);
		const QModelIndex rowRightCol = GetIndexForItem(item, Column_Count - 1);

		QVector<int> roles;
		roles.push_back(Qt::EditRole);
		QAbstractItemModel::dataChanged(rowLeftCol, rowRightCol);
	}

private:
	CAbstractObjectStructureModel& m_model;
};

class CObjectItemDelegate : public QStyledItemDelegate
{
	typedef CNameEditor<CAbstractObjectStructureModelItem> EditorWidget;
public:
	CObjectItemDelegate(QObject* pParent = nullptr)
		: QStyledItemDelegate(pParent)
	{
		m_pLabel = new QLabel();
		m_pLabel->setWordWrap(true);
		m_pToolTip = new QPopupWidget("StructureItemTooltip", m_pLabel);
		m_pToolTip->setAttribute(Qt::WA_ShowWithoutActivating);
	}

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		if (index.column() == CObjectStructureModel::Column_Name)
		{
			CAbstractObjectStructureModelItem* pItem = reinterpret_cast<CAbstractObjectStructureModelItem*>(index.data(CObjectStructureModel::Role_Pointer).value<quintptr>());
			if (pItem)
			{
				EditorWidget* pEditor = new EditorWidget(*pItem, parent);
				return pEditor;
			}
		}

		return QStyledItemDelegate::createEditor(parent, option, index);
	}

	virtual bool helpEvent(QHelpEvent* pEvent, QAbstractItemView* pView, const QStyleOptionViewItem& option, const QModelIndex& index) override
	{
		QString description = index.data(CObjectStructureModel::Role_ToolTip).value<QString>();
		if (!description.isEmpty())
		{
			if (m_currentDisplayedIndex != index)
			{
				m_pLabel->setText(description);

				if (m_pToolTip->isHidden())
					m_pToolTip->ShowAt(pEvent->globalPos());
				else
					m_pToolTip->move(pEvent->globalPos());
			}
			else if (m_pToolTip->isHidden())
				m_pToolTip->ShowAt(pEvent->globalPos());

			m_currentDisplayedIndex = index;

			return true;
		}

		m_pToolTip->hide();
		return false;
	}

private:
	QModelIndex   m_currentDisplayedIndex;
	QPopupWidget* m_pToolTip;
	QLabel*       m_pLabel;
};

CGraphsWidget::CGraphsWidget(QWidget* pParent)
	: QWidget(pParent)
	, m_pModel(nullptr)
	, m_pFilter(nullptr)
	, m_pComponentsList(nullptr)
	, m_pFilterProxy(nullptr)
	, m_pDataModel(nullptr)
{
	setContentsMargins(0, 0, 0, 0);

	QVBoxLayout* pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(0, 0, 0, 0);

	QLabel* pTitle = new QLabel("Functions & States");
	QFont font = pTitle->font();
	font.setBold(true);
	pTitle->setFont(font);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	m_pAddButton = new QToolButton();
	m_pAddButton->setText("+");

	QToolBar* pToolBar = new QToolBar();
	pToolBar->addWidget(pTitle);
	pToolBar->addWidget(pSpacer);
	pToolBar->addWidget(m_pAddButton);

	m_pComponentsList = new QAdvancedTreeView();
	m_pComponentsList->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pComponentsList->setSelectionMode(QAbstractItemView::SingleSelection);
	m_pComponentsList->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pComponentsList->setItemDelegate(new CObjectItemDelegate());
	m_pComponentsList->setHeaderHidden(true);
	m_pComponentsList->setMouseTracking(true);
	m_pComponentsList->setItemsExpandable(true);
	m_pComponentsList->setRootIsDecorated(true);
	m_pComponentsList->setTreePosition(static_cast<int>(CObjectStructureModel::Column_Name));
	m_pComponentsList->setEditTriggers(QTreeView::DoubleClicked | QTreeView::EditKeyPressed);
	m_pComponentsList->sortByColumn(static_cast<int>(CObjectStructureModel::Column_Name), Qt::AscendingOrder);

	m_pFilter = new QFilteringPanel("Schematyc Function and States", nullptr);
	m_pFilter->SetContent(m_pComponentsList);
	m_pFilter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	pLayout->addWidget(pToolBar);
	pLayout->addWidget(m_pFilter);
	pLayout->addWidget(m_pComponentsList);

	QObject::connect(m_pComponentsList, &QTreeView::clicked, this, &CGraphsWidget::OnClicked);
	QObject::connect(m_pComponentsList, &QTreeView::doubleClicked, this, &CGraphsWidget::OnDoubleClicked);
	QObject::connect(m_pComponentsList, &QTreeView::customContextMenuRequested, this, &CGraphsWidget::OnContextMenu);

	QObject::connect(m_pAddButton, &QToolButton::clicked, this, &CGraphsWidget::OnAddPressed);

	setLayout(pLayout);

	m_pContextMenuContent = new CDictionaryWidget();
	//QObject::connect(m_pContextMenuContent, &CDictionaryWidget::OnEntryClicked, this, &CObjectStructureWidget::OnAddComponent);

	m_pContextMenu = new QPopupWidget("Add Component", m_pContextMenuContent, QSize(250, 400), true);
}

CGraphsWidget::~CGraphsWidget()
{
	if (m_pFilterProxy)
	{
		// TODO: This is safe but not sure if required!
		m_pFilterProxy->setSourceModel(nullptr);
		// ~TODO
		m_pFilterProxy->deleteLater();
	}

	if (m_pDataModel)
	{
		m_pDataModel->deleteLater();
	}

	delete m_pContextMenu;
}

void CGraphsWidget::SetModel(CAbstractObjectStructureModel* pModel)
{
	if (m_pModel != pModel)
	{
		m_pModel = pModel;
		if (pModel)
		{
			m_pDataModel = new CObjectStructureModel(*pModel);
			m_pFilterProxy = new QAttributeFilterProxyModel(QAttributeFilterProxyModel::AcceptIfChildMatches);
			m_pFilterProxy->setSourceModel(m_pDataModel);
			m_pFilterProxy->setFilterKeyColumn(static_cast<int>(CObjectStructureModel::Column_Name));

			m_pFilter->SetModel(m_pFilterProxy);
			m_pComponentsList->setModel(m_pFilterProxy);
			QObject::connect(m_pComponentsList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CGraphsWidget::OnSelectionChanged);
		}
		else
		{
			CObjectStructureModel* pComponentsModel = static_cast<CObjectStructureModel*>(m_pFilterProxy->sourceModel());
			delete pComponentsModel;

			m_pFilterProxy->setSourceModel(nullptr);
			delete m_pFilterProxy;
			m_pFilterProxy = nullptr;
			m_pDataModel = nullptr;

			m_pFilter->SetModel(nullptr);
			m_pComponentsList->setModel(nullptr);
		}
	}
}

void CGraphsWidget::OnClicked(const QModelIndex& index)
{
	if (index.isValid())
	{
		CAbstractObjectStructureModelItem* pItem = reinterpret_cast<CAbstractObjectStructureModelItem*>(index.data(CObjectStructureModel::Role_Pointer).value<quintptr>());
		if (pItem)
		{
			SignalEntryClicked(*pItem);
		}
	}
}

void CGraphsWidget::OnDoubleClicked(const QModelIndex& index)
{
	if (index.isValid())
	{
		CAbstractObjectStructureModelItem* pItem = reinterpret_cast<CAbstractObjectStructureModelItem*>(index.data(CObjectStructureModel::Role_Pointer).value<quintptr>());
		if (pItem)
		{
			SignalEntryDoubleClicked(*pItem);
		}
	}
}

void CGraphsWidget::OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	QModelIndexList selectedIndexes = selected.indexes();
	if (selectedIndexes.count())
	{
		int32 row = selectedIndexes.at(0).row();
		for (QModelIndex index : selectedIndexes)
		{
			if (row == index.row())
				continue;

			row = -1;
			break;
		}

		if (row != -1)
		{
			QModelIndex index = selectedIndexes.at(0);
			CAbstractObjectStructureModelItem* pItem = reinterpret_cast<CAbstractObjectStructureModelItem*>(index.data(CObjectStructureModel::Role_Pointer).value<quintptr>());

			if (CBroadcastManager* pBroadcastManager = CBroadcastManager::Get(this))
			{
				CPropertiesWidget* pPropertiesWidget = nullptr /*new CPropertiesWidget(*pItem)*/;
				PopulateInspectorEvent popEvent([pPropertiesWidget](CInspector& inspector)
				{
					QCollapsibleFrame* pInspectorWidget = new QCollapsibleFrame("Properties");
					pInspectorWidget->SetWidget(pPropertiesWidget);
					inspector.AddWidget(pInspectorWidget);
				});

				pBroadcastManager->Broadcast(popEvent);

				if (pItem->GetType() == eObjectItemType_State)
				{
					CStateItem* pStateItem = static_cast<CStateItem*>(pItem);

					/*QVariantMap params;
					   params.insert("Model", reinterpret_cast<quintptr>(static_cast<CAbstractVariablesModel*>(pStateItem)));

					   // TODO: Remove hardcoded event name!
					   CustomEditorEvent editorEvent("PopulateVariablesWidget", params);
					   // ~TODO
					   pBroadcastManager->Broadcast(editorEvent);*/
				}
			}

			SignalEntrySelected(*pItem);
		}
	}
	else
	{
		QModelIndexList deselectedIndexes = deselected.indexes();
		if (deselectedIndexes.count())
		{
			if (CBroadcastManager* pBroadcastManager = CBroadcastManager::Get(this))
			{
				for (QModelIndex index : selectedIndexes)
				{
					CAbstractObjectStructureModelItem* pItem = reinterpret_cast<CAbstractObjectStructureModelItem*>(index.data(CObjectStructureModel::Role_Pointer).value<quintptr>());
					if (pItem->GetType() == eObjectItemType_State)
					{
						// TODO: Remove hardcoded event name!
						CustomEditorEvent editorEvent("ClearVariablesWidget");
						// ~TODO
						pBroadcastManager->Broadcast(editorEvent);
					}
				}
			}
		}
	}
}

void CGraphsWidget::OnContextMenu(const QPoint& point)
{
	const QPoint menuPos = m_pComponentsList->viewport()->mapToGlobal(point);

	const QModelIndex index = m_pComponentsList->indexAt(point);
	if (index.isValid())
	{
		CAbstractObjectStructureModelItem* pItem = reinterpret_cast<CAbstractObjectStructureModelItem*>(index.data(CObjectStructureModel::Role_Pointer).value<quintptr>());
		if (pItem)
		{
			ICommandManager* pCommandManager = GetIEditor()->GetICommandManager();

			QMenu menu;

			menu.addAction(pCommandManager->GetAction("general.delete"));

			{
				QAction* pAction = menu.addAction(QObject::tr("Rename"));
				QObject::connect(pAction, &QAction::triggered, this, [this, index]()
					{
						const QModelIndex editIndex = m_pFilterProxy->mapFromSource(m_pComponentsList->model()->index(index.row(), CComponentsDictionary::Column_Name, index.parent()));
						m_pComponentsList->edit(editIndex);
				  });
			}

			const EObjectStructureItemType itemType = static_cast<EObjectStructureItemType>(pItem->GetType());
			switch (itemType)
			{
			case eObjectItemType_Graph:
				break;
			case eObjectItemType_StateMachine:
				{
					{
						QAction* pAction = menu.addAction(QObject::tr("Add State"));
						QObject::connect(pAction, &QAction::triggered, this, [this, pItem]()
							{
								CStateMachineItem* pStateMachineItem = static_cast<CStateMachineItem*>(pItem);
								CStateItem* pCreatedItem = pStateMachineItem->CreateState();
								if (pCreatedItem)
								{
								  const QModelIndex index = m_pFilterProxy->mapFromSource(m_pDataModel->GetIndexForItem(*pCreatedItem));
								  m_pComponentsList->setCurrentIndex(index);
								  m_pComponentsList->edit(index);
								}
						  });
					}
				}
				break;
			case eObjectItemType_State:
				{
					PopulateContextMenuForItem(menu, *static_cast<CStateItem*>(pItem));
				}
				break;
			default:
				break;
			}

			menu.addSeparator();

			menu.exec(menuPos);
		}
	}
	else
	{
		// TODO: Show dictionary menu.
		//if (m_pModel)
		//m_pContextMenuContent->SetDictionary(m_pModel->());
		// ~TODO

		m_pContextMenu->ShowAt(menuPos);
	}
}

void CGraphsWidget::OnAddPressed()
{
	const QPoint menuPos = m_pAddButton->mapToGlobal(m_pAddButton->mapFromParent(m_pAddButton->pos())) + QPoint(0, m_pAddButton->width());

	//if (m_pModel)
	//m_pContextMenuContent->SetDictionary(m_pModel->GetAvailableComponentsDictionary());

	m_pContextMenu->ShowAt(menuPos, QPopupWidget::TopRight);
}

bool CGraphsWidget::OnDeleteEvent()
{
	// TODO: Undo action!
	const QModelIndex index = m_pComponentsList->selectionModel()->currentIndex();
	if (index.isValid())
	{
		CAbstractObjectStructureModelItem* pItem = reinterpret_cast<CAbstractObjectStructureModelItem*>(index.data(CObjectStructureModel::Role_Pointer).value<quintptr>());
		if (pItem)
		{
			CAbstractObjectStructureModelItem* pParent = pItem->GetParentItem();
			if (pParent)
				pParent->RemoveChild(*pItem);
			else
			{
				m_pModel->RemoveItem(*pItem);
			}
		}
	}

	return true;
}

void CGraphsWidget::customEvent(QEvent* pEvent)
{
	if (pEvent->type() != SandboxEvent::Command)
	{
		QWidget::customEvent(pEvent);
		return;
	}

	CommandEvent* pCommandEvent = static_cast<CommandEvent*>(pEvent);
	const string& command = pCommandEvent->GetCommand();

	if (command == "general.delete")
	{
		pEvent->setAccepted(OnDeleteEvent());
	}
	else
		QWidget::customEvent(pEvent);
}

void CGraphsWidget::EditItem(CAbstractObjectStructureModelItem& item) const
{
	const QModelIndex index = m_pDataModel->GetIndexForItem(item);
	m_pComponentsList->setCurrentIndex(m_pFilterProxy->mapFromSource(index));
	m_pComponentsList->edit(index);
}

void CGraphsWidget::PopulateContextMenuForItem(QMenu& menu, CStateItem& stateItem) const
{
	menu.addSeparator();

	{
		QAction* pAction = menu.addAction(QObject::tr("Add State"));
		QObject::connect(pAction, &QAction::triggered, [this, &stateItem]()
			{
				CStateItem* pCreatedItem = stateItem.CreateState();
				if (pCreatedItem)
				{
				  EditItem(*pCreatedItem);
				}
		  });
	}
	{
		QAction* pAction = menu.addAction(QObject::tr("Add Function"));
		QObject::connect(pAction, &QAction::triggered, [this, &stateItem]()
			{
				CGraphItem* pCreatedItem = stateItem.CreateGraph(CGraphItem::eGraphType_Function);
				if (pCreatedItem)
				{
				  EditItem(*pCreatedItem);
				}
		  });
	}
	{
		QAction* pAction = menu.addAction(QObject::tr("Add Signal"));
		QObject::connect(pAction, &QAction::triggered, [this, &stateItem]()
			{
				CRY_ASSERT_MESSAGE(false, "Not yet implemented!");
		  });
	}
	{
		QAction* pAction = menu.addAction(QObject::tr("Add Signals Receiver"));
		QObject::connect(pAction, &QAction::triggered, [this, &stateItem]()
			{
				CGraphItem* pCreatedItem = stateItem.CreateGraph(CGraphItem::eGraphType_SignalReceiver);
				if (pCreatedItem)
				{
				  EditItem(*pCreatedItem);
				}
		  });
	}
}

}


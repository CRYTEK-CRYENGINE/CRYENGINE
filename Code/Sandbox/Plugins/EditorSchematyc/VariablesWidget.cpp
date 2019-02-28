// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VariablesWidget.h"

#include "VariablesModel.h"
#include "PropertiesWidget.h"
#include "NameEditorWidget.h"

#include <QtUtil.h>
#include <QSearchBox.h>
#include <QAdvancedPropertyTree.h>
#include <QCollapsibleFrame.h>
#include <ProxyModels/DeepFilterProxyModel.h>
#include <Controls/QPopupWidget.h>
#include <Controls/DictionaryWidget.h>
#include <ICommandManager.h>
#include <EditorFramework/BroadcastManager.h>
#include <EditorFramework/Events.h>
#include <EditorFramework/Inspector.h>

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
#include <QLineEdit>

namespace CrySchematycEditor {

class CVariablesModel : public QAbstractItemModel
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
		Role_Pointer = Qt::UserRole
	};

public:
	CVariablesModel(CAbstractVariablesModel& model)
		: m_model(model)
	{
		m_model.SignalVariableAdded.Connect(this, &CVariablesModel::OnVariableAdded);
		m_model.SignalVariableRemoved.Connect(this, &CVariablesModel::OnVariableRemoved);
		m_model.SignalVariableInvalidated.Connect(this, &CVariablesModel::OnVariableInvalidated);
	}

	~CVariablesModel()
	{
		m_model.SignalVariableAdded.DisconnectObject(this);
		m_model.SignalVariableRemoved.DisconnectObject(this);
		m_model.SignalVariableInvalidated.DisconnectObject(this);
	}

	// QAbstractItemModel
	virtual int rowCount(const QModelIndex& parent) const override
	{
		if (!parent.isValid())
		{
			return m_model.GetNumVariables();
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
			const CAbstractVariablesModelItem* pItem = static_cast<const CAbstractVariablesModelItem*>(index.internalPointer());
			if (pItem)
			{
				switch (role)
				{
				case Role_Display:
					{
						const int32 col = index.column();
						if (col == Column_Name)
							return pItem->GetName();
					}
					break;
				case Role_Pointer:
					{
						return QVariant::fromValue(reinterpret_cast<quintptr>(pItem));
					}
				default:
					break;
				}
			}
		}
		return QVariant();
	}

	virtual Qt::ItemFlags flags(const QModelIndex& index) const override
	{
		if (index.isValid() && index.column() == Column_Name)
		{
			return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
		}
		return QAbstractItemModel::flags(index);
	}

	virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override
	{
		if (!parent.isValid())
		{
			CAbstractVariablesModelItem* pItem = static_cast<CAbstractVariablesModelItem*>(m_model.GetVariableItemByIndex(row));
			return QAbstractItemModel::createIndex(row, column, reinterpret_cast<quintptr>(pItem));
		}

		return QModelIndex();
	}

	virtual QModelIndex parent(const QModelIndex& index) const override
	{
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
						CAbstractVariablesModelItem* pItem = reinterpret_cast<CAbstractVariablesModelItem*>(index.internalPointer());
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

	QModelIndex GetIndexForItem(const CAbstractVariablesModelItem& item, int column = 0) const
	{
		const uint32 row = item.GetIndex();
		return index(row, column);
	}

	void OnVariableAdded(CAbstractVariablesModelItem& item)
	{
		const uint32 index = item.GetIndex();
		QAbstractItemModel::beginInsertRows(QModelIndex(), index, index);
		QAbstractItemModel::endInsertRows();

		// Note: We need to call dataChanged here otherwise it's not possible to edit the item directly after inserting it.
		const QModelIndex rowLeftCol = QAbstractItemModel::createIndex(index, 0, reinterpret_cast<quintptr>(&item));
		const QModelIndex rowRightCol = QAbstractItemModel::createIndex(index, Column_Count - 1, reinterpret_cast<quintptr>(&item));

		QVector<int> roles;
		roles.push_back(Qt::EditRole);
		QAbstractItemModel::dataChanged(rowLeftCol, rowRightCol);
	}

	void OnVariableRemoved(CAbstractVariablesModelItem& item)
	{
		const uint32 index = item.GetIndex();
		QAbstractItemModel::beginRemoveRows(QModelIndex(), index, index);
		QAbstractItemModel::endRemoveRows();
	}

	void OnVariableInvalidated(CAbstractVariablesModelItem& item)
	{
		const uint32 index = item.GetIndex();
		const QModelIndex rowLeftCol = QAbstractItemModel::createIndex(index, 0, reinterpret_cast<quintptr>(&item));
		const QModelIndex rowRightCol = QAbstractItemModel::createIndex(index, Column_Count - 1, reinterpret_cast<quintptr>(&item));

		QVector<int> roles;
		roles.push_back(Qt::EditRole);
		QAbstractItemModel::dataChanged(rowLeftCol, rowRightCol);
	}

private:
	CAbstractVariablesModel& m_model;
};

class CVariableItemDelegate : public QStyledItemDelegate
{
	typedef CNameEditor<CAbstractVariablesModelItem> EditorWidget;

public:
	CVariableItemDelegate(QObject* pParent = nullptr)
		: QStyledItemDelegate(pParent)
	{

	}

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		if (index.column() == CVariablesModel::Column_Name)
		{
			CAbstractVariablesModelItem* pItem = reinterpret_cast<CAbstractVariablesModelItem*>(index.data(CVariablesModel::Role_Pointer).value<quintptr>());
			if (pItem)
			{
				EditorWidget* pEditor = new EditorWidget(*pItem, parent);
				return pEditor;
			}
		}

		return QStyledItemDelegate::createEditor(parent, option, index);
	}
};

CVariablesWidget::CVariablesWidget(QString label, QWidget* pParent)
	: QWidget(pParent)
	, m_pModel(nullptr)
	, m_pFilter(nullptr)
	, m_pVariablesList(nullptr)
	, m_pFilterProxy(nullptr)
	, m_pDataModel(nullptr)
{
	setContentsMargins(0, 0, 0, 0);
	setContextMenuPolicy(Qt::CustomContextMenu);

	QVBoxLayout* pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(0, 0, 0, 0);

	QLabel* pTitle = new QLabel(label);
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

	m_pVariablesList = new QAdvancedTreeView();
	m_pVariablesList->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pVariablesList->setSelectionMode(QAbstractItemView::SingleSelection);
	m_pVariablesList->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pVariablesList->setItemDelegate(new CVariableItemDelegate());
	m_pVariablesList->setHeaderHidden(true);
	m_pVariablesList->setMouseTracking(true);
	m_pVariablesList->setItemsExpandable(false);
	m_pVariablesList->setRootIsDecorated(false);
	m_pVariablesList->setTreePosition(static_cast<int>(CVariablesModel::Column_Name));
	m_pVariablesList->setEditTriggers(QTreeView::DoubleClicked | QTreeView::EditKeyPressed | QTreeView::SelectedClicked);
	m_pVariablesList->sortByColumn(static_cast<int>(CVariablesModel::Column_Name), Qt::AscendingOrder);

	m_pFilter = new QSearchBox(nullptr);
	m_pFilter->EnableContinuousSearch(true);
	m_pFilter->setMaximumSize(16777215, 20);
	m_pFilter->setPlaceholderText("Search");

	pLayout->addWidget(pToolBar);
	pLayout->addWidget(m_pFilter);
	pLayout->addWidget(m_pVariablesList);

	QObject::connect(m_pVariablesList, &QTreeView::clicked, this, &CVariablesWidget::OnClicked);
	QObject::connect(m_pVariablesList, &QTreeView::doubleClicked, this, &CVariablesWidget::OnDoubleClicked);
	QObject::connect(m_pVariablesList, &QTreeView::customContextMenuRequested, this, &CVariablesWidget::OnContextMenu);

	QObject::connect(m_pAddButton, &QToolButton::clicked, this, &CVariablesWidget::OnAddPressed);

	setLayout(pLayout);

	m_pContextMenuContent = new CDictionaryWidget();
	//QObject::connect(m_pContextMenuContent, &CDictionaryWidget::OnEntryClicked, this, &CObjectStructureWidget::OnAddComponent);

	m_pContextMenu = new QPopupWidget("Add Component", m_pContextMenuContent, QSize(250, 400), true);
}

CVariablesWidget::~CVariablesWidget()
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

void CVariablesWidget::SetModel(CAbstractVariablesModel* pModel)
{
	if (m_pModel != pModel)
	{
		m_pModel = pModel;
		if (pModel)
		{
			m_pDataModel = new CVariablesModel(*pModel);
			m_pFilterProxy = new QDeepFilterProxyModel();
			m_pFilterProxy->setSourceModel(m_pDataModel);
			m_pFilterProxy->setFilterKeyColumn(static_cast<int>(CVariablesModel::Column_Name));

			m_pFilter->SetModel(m_pFilterProxy);
			m_pVariablesList->setModel(m_pFilterProxy);
			QObject::connect(m_pVariablesList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CVariablesWidget::OnSelectionChanged);
		}
		else
		{
			CVariablesModel* pComponentsModel = static_cast<CVariablesModel*>(m_pFilterProxy->sourceModel());
			delete pComponentsModel;

			m_pFilterProxy->setSourceModel(nullptr);
			delete m_pFilterProxy;
			m_pFilterProxy = nullptr;
			m_pDataModel = nullptr;

			m_pFilter->SetModel((QDeepFilterProxyModel*)nullptr);
			m_pVariablesList->setModel(nullptr);
		}
	}
}

void CVariablesWidget::ConnectToBroadcast(CBroadcastManager* pBroadcastManager)
{
	m_broadcastManager = pBroadcastManager;
	m_broadcastManager->Connect(BroadcastEvent::CustomEditorEvent, this, &CVariablesWidget::OnEditorEvent);
}

void CVariablesWidget::OnClicked(const QModelIndex& index)
{
	if (index.isValid())
	{
		CAbstractVariablesModelItem* pItem = reinterpret_cast<CAbstractVariablesModelItem*>(index.data(CVariablesModel::Role_Pointer).value<quintptr>());
		if (pItem)
		{
			SignalEntryClicked(*pItem);
		}
	}
}

void CVariablesWidget::OnDoubleClicked(const QModelIndex& index)
{
	if (index.isValid())
	{
		CAbstractVariablesModelItem* pItem = reinterpret_cast<CAbstractVariablesModelItem*>(index.data(CVariablesModel::Role_Pointer).value<quintptr>());
		if (pItem)
		{
			SignalEntryDoubleClicked(*pItem);
		}
	}
}

void CVariablesWidget::OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	QModelIndexList selections = selected.indexes();
	if (selections.count())
	{
		int32 row = selections.at(0).row();
		for (QModelIndex index : selections)
		{
			if (row == index.row())
				continue;

			row = -1;
			break;
		}

		if (row != -1)
		{
			QModelIndex index = selections.at(0);
			CAbstractVariablesModelItem* pItem = reinterpret_cast<CAbstractVariablesModelItem*>(index.data(CVariablesModel::Role_Pointer).value<quintptr>());

			if (CBroadcastManager* pBroadcastManager = CBroadcastManager::Get(this))
			{
				CrySchematycEditor::CPropertiesWidget* pPropertiesWidget = nullptr /*new CrySchematycEditor::CPropertiesWidget(*pItem)*/;
				PopulateInspectorEvent popEvent([pPropertiesWidget](CInspector& inspector)
				{
					QCollapsibleFrame* pInspectorWidget = new QCollapsibleFrame("Properties");
					pInspectorWidget->SetWidget(pPropertiesWidget);
					inspector.AddWidget(pInspectorWidget);
				});
				pBroadcastManager->Broadcast(popEvent);
			}

			SignalEntrySelected(*pItem);
		}
	}
}

void CVariablesWidget::OnContextMenu(const QPoint& point)
{
	const QPoint menuPos = mapToGlobal(point);

	QMenu menu(this);
	{
		QAction* pAction = menu.addAction(QObject::tr("Add Variable"));
		QObject::connect(pAction, &QAction::triggered, this, &CVariablesWidget::OnAddPressed);
	}

	const QModelIndex index = m_pVariablesList->indexAt(point);
	if (index.isValid())
	{
		CAbstractVariablesModelItem* pItem = reinterpret_cast<CAbstractVariablesModelItem*>(index.data(CVariablesModel::Role_Pointer).value<quintptr>());
		if (pItem)
		{
			ICommandManager* pCommandManager = GetIEditor()->GetICommandManager();

			menu.addSeparator();

			{
				QAction* pAction = menu.addAction(QObject::tr("Rename"));
				QObject::connect(pAction, &QAction::triggered, this, [this, index]()
					{
						m_pVariablesList->edit(index);
				  });
			}

			menu.addAction(pCommandManager->GetAction("general.delete"));
		}
	}

	menu.exec(menuPos);
}

void CVariablesWidget::OnAddPressed()
{
	if (m_pModel)
	{
		// TODO: Undo action!
		CAbstractVariablesModelItem* pVariableItem = m_pModel->CreateVariable();
		if (pVariableItem)
		{
			const QModelIndex index = m_pFilterProxy->mapFromSource(m_pDataModel->GetIndexForItem(*pVariableItem));
			m_pVariablesList->setCurrentIndex(index);
			m_pVariablesList->edit(index);
		}
	}
}

bool CVariablesWidget::OnDeleteEvent()
{
	if (m_pModel)
	{
		// TODO: Undo action!
		const QModelIndex index = m_pVariablesList->selectionModel()->currentIndex();
		if (index.isValid())
		{
			CAbstractVariablesModelItem* pItem = reinterpret_cast<CAbstractVariablesModelItem*>(index.data(CVariablesModel::Role_Pointer).value<quintptr>());
			if (pItem)
			{
				m_pModel->RemoveVariable(*pItem);
			}
		}
	}
	return true;
}

void CVariablesWidget::OnEditorEvent(BroadcastEvent& event)
{
	if (event.type() == BroadcastEvent::CustomEditorEvent)
	{
		CustomEditorEvent& editorEvent = static_cast<CustomEditorEvent&>(event);
		const QString& action = editorEvent.GetAction();
		if (action == "PopulateVariablesWidget")
		{
			const QVariantMap& params = editorEvent.GetParams();
			auto result = params.find("Model");
			if (result != params.end())
			{
				CAbstractVariablesModel* pVariablesModel = reinterpret_cast<CAbstractVariablesModel*>(result->value<quintptr>());
				if (pVariablesModel)
				{
					SetModel(pVariablesModel);
				}
			}
		}
		else if (action == "ClearVariablesWidget")
		{
			SetModel(nullptr);
		}
	}
}

void CVariablesWidget::Disconnect()
{
	SetModel(nullptr);

	if (m_broadcastManager)
	{
		m_broadcastManager->DisconnectObject(this);
	}
}

void CVariablesWidget::customEvent(QEvent* pEvent)
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

}


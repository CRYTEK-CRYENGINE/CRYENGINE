// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ComponentsWidget.h"

#include "ComponentsModel.h"
#include "ScriptBrowserUtils.h"
#include "PropertiesWidget.h"

#include "ObjectModel.h"
#include "MainWindow.h"

#include <QtUtil.h>
#include <QFilteringPanel.h>
#include <QAdvancedPropertyTree.h>
#include <QCollapsibleFrame.h>
#include <ProxyModels/AttributeFilterProxyModel.h>
#include <Controls/QPopupWidget.h>
#include <Controls/DictionaryWidget.h>
#include <ICommandManager.h>
#include <EditorFramework/BroadcastManager.h>
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

namespace CrySchematycEditor {

class CComponentsModel : public QAbstractItemModel
{
public:
	enum EColumn
	{
		Column_Type,
		Column_Name,

		ColumnCount
	};

	enum ERole : int32
	{
		Role_Display = Qt::DisplayRole,
		Role_ToolTip = Qt::ToolTipRole,
		Role_Icon    = Qt::DecorationRole,
		Role_Pointer = Qt::UserRole
	};

public:
	CComponentsModel(CAbstractComponentsModel& viewModel)
		: m_model(viewModel)
	{
		m_model.SignalAddedComponentItem.Connect(this, &CComponentsModel::OnAddedComponentItem);
		m_model.SignalRemovedComponentItem.Connect(this, &CComponentsModel::OnRemovedComponentItem);
	}

	~CComponentsModel()
	{
		m_model.SignalAddedComponentItem.DisconnectObject(this);
		m_model.SignalRemovedComponentItem.DisconnectObject(this);
	}

	// QAbstractItemModel
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override
	{
		if (!parent.isValid())
			return m_model.GetComponentItemCount();
		else
			return 0;
	}

	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override
	{
		return ColumnCount;
	}

	virtual QVariant data(const QModelIndex& index, int role) const override
	{
		if (index.isValid())
		{
			const CComponentItem* pItem = static_cast<const CComponentItem*>(index.internalPointer());
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
						if (!pItem->GetDescription().isEmpty())
							return pItem->GetDescription();
					}
					break;
				case Role_Icon:
					{
						if (index.column() == Column_Type)
						{
							if (!pItem->GetIcon().isNull())
								return pItem->GetIcon();
						}
					}
					break;
				case Role_Pointer:
					{
						return reinterpret_cast<quintptr>(pItem);
					}
					break;
				default:
					break;
				}
			}
		}
		return QVariant();
	}

	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
	{
		if (orientation == Qt::Horizontal && role == Role_Display)
		{
			switch (section)
			{
			case Column_Name:
				return QObject::tr("Name");
			case Column_Type:
				return QObject::tr("Type");
			default:
				break;
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
			return QAbstractItemModel::createIndex(row, column, reinterpret_cast<quintptr>(m_model.GetComponentItemByIndex(row)));
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
						CComponentItem* pItem = reinterpret_cast<CComponentItem*>(index.internalPointer());
						if (pItem)
						{
							const QString newName = value.value<QString>();
							pItem->SetName(newName);
						}
					}
					break;
				default:
					return false;
				}
			}

			QVector<int> roles;
			roles.push_back(role);
			dataChanged(index, index, roles);

			return true;
		}

		return false;
	}

	virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override
	{
		if (row >= 0 && row + count <= m_model.GetComponentItemCount())
		{
			beginRemoveRows(QModelIndex(), row, count);
			for (int i = 0; i < count; ++i)
			{
				if (CComponentItem* pComponentItem = m_model.GetComponentItemByIndex(row))
				{
					m_model.RemoveComponent(*pComponentItem);
				}
			}
			endRemoveRows();

			return true;
		}

		return false;
	}
	// ~QAbstractItemModel

	void OnAddedComponentItem(CComponentItem& item)
	{
		// TODO: Use proper index here.
		const int32 index = 0;
		QAbstractItemModel::beginInsertRows(QModelIndex(), index, index);
		QAbstractItemModel::endInsertRows();
	}

	void OnRemovedComponentItem(CComponentItem& item)
	{
		// TODO: Use proper index here.
		const int32 index = 0;
		QAbstractItemModel::beginRemoveRows(QModelIndex(), index, index);
		QAbstractItemModel::endRemoveRows();
	}

private:
	CAbstractComponentsModel& m_model;
};

class CComponentDelegate : public QStyledItemDelegate
{
public:
	CComponentDelegate(QObject* pParent = nullptr)
		: QStyledItemDelegate(pParent)
	{
		m_pLabel = new QLabel();
		m_pLabel->setWordWrap(true);
		m_pToolTip = new QPopupWidget("NodeSearchMenuToolTip", m_pLabel);
		m_pToolTip->setAttribute(Qt::WA_ShowWithoutActivating);
	}

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		// TODO: Use a custom editor for renaming so we can validate input.
		return QStyledItemDelegate::createEditor(parent, option, index);
	}

	virtual bool helpEvent(QHelpEvent* pEvent, QAbstractItemView* pView, const QStyleOptionViewItem& option, const QModelIndex& index) override
	{
		QString description = index.data(CComponentsModel::Role_ToolTip).value<QString>();
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

CComponentsWidget::CComponentsWidget(CMainWindow& editor, QWidget* pParent)
	: QWidget(pParent)
	, m_pEditor(&editor)
	, m_pModel(nullptr)
	, m_pFilter(nullptr)
	, m_pComponentsList(nullptr)
	, m_pFilterProxy(nullptr)
{
	setContentsMargins(0, 0, 0, 0);

	QVBoxLayout* pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(0, 0, 0, 0);

	QLabel* pTitle = new QLabel("Components");
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
	//m_pComponentsList->setHeaderHidden(true);
	m_pComponentsList->setMouseTracking(true);
	m_pComponentsList->setItemDelegate(new CComponentDelegate());
	m_pComponentsList->setItemsExpandable(false);
	m_pComponentsList->setRootIsDecorated(false);
	m_pComponentsList->setEditTriggers(QTreeView::DoubleClicked | QTreeView::EditKeyPressed | QTreeView::SelectedClicked);
	m_pComponentsList->sortByColumn(static_cast<int>(CComponentsModel::Column_Name), Qt::AscendingOrder);

	m_pFilter = new QFilteringPanel("Schematyc Components", nullptr);
	m_pFilter->SetContent(m_pComponentsList);
	m_pFilter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	pLayout->addWidget(pToolBar);
	pLayout->addWidget(m_pFilter);
	pLayout->addWidget(m_pComponentsList);

	QObject::connect(m_pComponentsList, &QTreeView::clicked, this, &CComponentsWidget::OnClicked);
	QObject::connect(m_pComponentsList, &QTreeView::doubleClicked, this, &CComponentsWidget::OnDoubleClicked);
	QObject::connect(m_pComponentsList, &QTreeView::customContextMenuRequested, this, &CComponentsWidget::OnContextMenu);

	QObject::connect(m_pAddButton, &QToolButton::clicked, this, &CComponentsWidget::OnAddButton);

	setLayout(pLayout);

	m_pContextMenuContent = new CDictionaryWidget();
	QObject::connect(m_pContextMenuContent, &CDictionaryWidget::OnEntryClicked, this, &CComponentsWidget::OnAddComponent);

	m_pContextMenu = new QPopupWidget("Add Component", m_pContextMenuContent, QSize(250, 400), true);

	QObject::connect(m_pEditor, &CMainWindow::SignalOpenedModel, this, &CComponentsWidget::SetModel);
	QObject::connect(m_pEditor, &CMainWindow::SignalReleasingModel, [this]
		{
			SetModel(nullptr);
	  });
	QObject::connect(m_pEditor, &QWidget::destroyed, [this](QObject*)
		{
			m_pEditor = nullptr;
	  });
}

CComponentsWidget::~CComponentsWidget()
{
	if (m_pFilterProxy)
	{
		m_pFilterProxy->sourceModel()->deleteLater();
		m_pFilterProxy->deleteLater();
	}

	if (m_pEditor)
	{
		QObject::disconnect(m_pEditor);
	}
}

void CComponentsWidget::SetModel(CAbstractComponentsModel* pModel)
{
	if (m_pModel != pModel)
	{
		m_pModel = pModel;
		if (pModel)
		{
			CComponentsModel* pComponentsModel = new CComponentsModel(*pModel);
			m_pFilterProxy = new QAttributeFilterProxyModel(QAttributeFilterProxyModel::AcceptIfChildMatches);
			m_pFilterProxy->setSourceModel(pComponentsModel);
			m_pFilterProxy->setFilterKeyColumn(static_cast<int>(CComponentsModel::Column_Name));

			m_pFilter->SetModel(m_pFilterProxy);
			m_pComponentsList->setModel(m_pFilterProxy);
			QObject::connect(m_pComponentsList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CComponentsWidget::OnSelectionChanged);
		}
		else
		{
			if (m_pFilterProxy)
			{
				CComponentsModel* pComponentsModel = static_cast<CComponentsModel*>(m_pFilterProxy->sourceModel());
				delete pComponentsModel;
				pComponentsModel = nullptr;

				m_pFilterProxy->setSourceModel(nullptr);
				delete m_pFilterProxy;
				m_pFilterProxy = nullptr;
			}

			m_pFilter->SetModel(nullptr);
			m_pComponentsList->setModel(nullptr);
		}
	}
}

void CComponentsWidget::OnClicked(const QModelIndex& index)
{
	if (index.isValid())
	{
		CComponentItem* pItem = reinterpret_cast<CComponentItem*>(index.data(CComponentsModel::Role_Pointer).value<quintptr>());
		if (pItem)
		{
			SignalComponentClicked(*pItem);
		}
	}
}

void CComponentsWidget::OnDoubleClicked(const QModelIndex& index)
{
	if (index.isValid())
	{
		CComponentItem* pItem = reinterpret_cast<CComponentItem*>(index.data(CComponentsModel::Role_Pointer).value<quintptr>());
		if (pItem)
		{
			SignalComponentDoubleClicked(*pItem);
		}
	}
}

void CComponentsWidget::OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
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
			CComponentItem* pItem = reinterpret_cast<CComponentItem*>(index.data(CComponentsModel::Role_Pointer).value<quintptr>());

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
			}
		}
	}
}
void CComponentsWidget::OnContextMenu(const QPoint& point)
{
	const QPoint menuPos = mapToGlobal(mapFromParent(point));

	const QModelIndex index = m_pComponentsList->indexAt(point);
	if (index.isValid())
	{
		CComponentItem* pItem = reinterpret_cast<CComponentItem*>(index.data(CComponentsModel::Role_Pointer).value<quintptr>());
		if (pItem)
		{
			ICommandManager* pCommandManager = GetIEditor()->GetICommandManager();

			QMenu menu;

			{
				QAction* pActivateAction = menu.addAction(QObject::tr("Rename"));
				QObject::connect(pActivateAction, &QAction::triggered, this, [this, index](bool isChecked)
					{
						const QModelIndex editIndex = m_pComponentsList->model()->index(index.row(), CComponentsDictionary::Column_Name, index.parent());
						m_pComponentsList->edit(editIndex);
				  });
			}

			menu.addSeparator();

			menu.addAction(pCommandManager->GetAction("general.copy"));
			menu.addAction(pCommandManager->GetAction("general.delete"));

			menu.exec(menuPos);
		}
	}
	else
	{
		if (m_pModel)
			m_pContextMenuContent->AddDictionary(*m_pModel->GetAvailableComponentsDictionary());

		m_pContextMenu->ShowAt(menuPos);
	}
}

void CComponentsWidget::OnAddButton(bool checked)
{
	const QPoint menuPos = m_pAddButton->mapToGlobal(m_pAddButton->mapFromParent(m_pAddButton->pos()));

	if (m_pModel)
		m_pContextMenuContent->AddDictionary(*m_pModel->GetAvailableComponentsDictionary());

	m_pContextMenu->ShowAt(menuPos, QPopupWidget::TopRight);
}

void CComponentsWidget::OnAddComponent(CAbstractDictionaryEntry& entry)
{
	if (m_pModel)
	{
		// TODO: Undo action!
		CComponentDictionaryEntry* pComponentEntry = static_cast<CComponentDictionaryEntry*>(&entry);

		Schematyc::CStackString uniqueName = QtUtil::ToString(pComponentEntry->GetName()).c_str();
		Schematyc::ScriptBrowserUtils::MakeScriptElementNameUnique(uniqueName, m_pModel->GetScriptElement());
		m_pModel->CreateComponent(pComponentEntry->GetTypeGUID(), uniqueName.c_str());
	}
}

bool CComponentsWidget::OnDeleteEvent()
{
	// TODO: Undo action!
	const QModelIndex index = m_pComponentsList->selectionModel()->currentIndex();
	if (index.isValid())
	{
		CComponentItem* pItem = reinterpret_cast<CComponentItem*>(index.data(CComponentsModel::Role_Pointer).value<quintptr>());
		if (pItem)
		{
			m_pModel->RemoveComponent(*pItem);
		}
	}

	return true;
}

bool CComponentsWidget::OnCopyEvent()
{
	// TODO: Implementation missing.
	return false;
}

bool CComponentsWidget::OnPasteEvent()
{
	// TODO: Implementation missing.
	return false;
}

void CComponentsWidget::customEvent(QEvent* pEvent)
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
	else if (command == "general.copy")
	{
		pEvent->setAccepted(OnCopyEvent());
	}
	else if (command == "general.paste")
	{
		pEvent->setAccepted(OnPasteEvent());
	}
	else
		QWidget::customEvent(pEvent);
}

}


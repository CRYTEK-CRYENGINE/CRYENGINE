// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "DictionaryWidget.h"

#include "QSearchBox.h"
#include "QPopupWidget.h"
#include "QAdvancedTreeView.h"
#include "ProxyModels/AttributeFilterProxyModel.h"
#include "ProxyModels/ItemModelAttribute.h"
#include "ProxyModels/MergingProxyModel.h"
#include "EditorStyleHelper.h"
#include "QFilteringPanel.h"

#include <QStyledItemDelegate>
#include <QAbstractItemModel>

#include <QHelpEvent>
#include <QEventLoop>

const CItemModelAttribute* CAbstractDictionary::GetFilterAttribute() const
{
	static CItemModelAttribute filterAttribute(GetName(), eAttributeType_String);
	return &filterAttribute;
}

const CItemModelAttribute* CAbstractDictionary::GetColumnAttribute(int32 index) const
{
	// TODO: This is just a hack until all users of the abstract dictionary implement this function
	static CItemModelAttribute fallback("", eAttributeType_String);
	QString& columnName = const_cast<QString&>(fallback.GetName());
	columnName = GetColumnName(index);
	return &fallback;
	// ~TODO
}

class CDictionaryModel : public QAbstractItemModel
{
public:
	enum ERole : int32
	{
		DisplayRole   = Qt::DisplayRole,
		ToolTipRole   = Qt::ToolTipRole,
		IconRole      = Qt::DecorationRole,
		ColorRole     = Qt::TextColorRole,
		PointerRole   = Qt::UserRole + 0,
		MergingRole   = Qt::UserRole + 1,
		FilteringRole = Qt::UserRole + 2,

	};

public:
	CDictionaryModel(CAbstractDictionary& dictionary)
		: m_dictionary(dictionary)
	{
	}

	~CDictionaryModel()
	{
	}

	CAbstractDictionary* GetDictionary() const
	{
		return &m_dictionary;
	}

	void Initialize()
	{
		m_dictionary.signalDictionaryClear.Connect(this, &CDictionaryModel::Clear);
	}

	void ShutDown()
	{
		m_dictionary.signalDictionaryClear.DisconnectObject(this);
	}

	void Clear()
	{
		beginRemoveRows(QModelIndex(), 0, m_dictionary.GetNumEntries() - 1);
		m_dictionary.ClearEntries();
		endRemoveRows();
	}

	// QAbstractItemModel
	virtual int rowCount(const QModelIndex& parent) const override
	{
		if (!parent.isValid())
		{
			return m_dictionary.GetNumEntries();
		}
		else
		{
			const CAbstractDictionaryEntry* pEntry = static_cast<const CAbstractDictionaryEntry*>(parent.internalPointer());
			if (pEntry)
			{
				return pEntry->GetNumChildEntries();
			}
		}
		return 0;
	}

	virtual int CDictionaryModel::columnCount(const QModelIndex& parent) const override
	{
		return m_dictionary.GetNumColumns();
	}

	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
	{
		return GetHeaderDataFromDictionary(m_dictionary, section, orientation, role);
	}

	virtual QVariant CDictionaryModel::data(const QModelIndex& index, int role) const override
	{
		if (index.isValid())
		{
			const CAbstractDictionaryEntry* pEntry = static_cast<const CAbstractDictionaryEntry*>(index.internalPointer());
			if (pEntry)
			{
				switch (role)
				{
				case CDictionaryModel::DisplayRole:
				case CDictionaryModel::MergingRole:
				case CDictionaryModel::FilteringRole:
					{
						return QVariant::fromValue(pEntry->GetColumnValue(index.column()));
					}
					break;
				case CDictionaryModel::ToolTipRole:
					{
						const QString text = pEntry->GetToolTip();
						if (!text.isEmpty())
						   return QVariant::fromValue(text);
					}
					break;
				case CDictionaryModel::IconRole:
					{
						if (const QIcon* pIcon = pEntry->GetColumnIcon(index.column()))
							return QVariant::fromValue(*pIcon);
					}
					break;
				case CDictionaryModel::PointerRole:
					{
						return QVariant::fromValue(reinterpret_cast<quintptr>(pEntry));
					}
					break;
				case CDictionaryModel::ColorRole:
				{
					if (pEntry->IsEnabled())
					{
						return GetStyleHelper()->textColor();
					}
					else
					{
						return GetStyleHelper()->disabledTextColor();
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

	virtual QModelIndex CDictionaryModel::index(int row, int column, const QModelIndex& parent) const override
	{
		if (!parent.isValid())
		{
			return QAbstractItemModel::createIndex(row, column, reinterpret_cast<quintptr>(m_dictionary.GetEntry(row)));
		}

		const CAbstractDictionaryEntry* pParentEntry = static_cast<const CAbstractDictionaryEntry*>(parent.internalPointer());
		if (pParentEntry)
		{
			return QAbstractItemModel::createIndex(row, column, reinterpret_cast<quintptr>(pParentEntry->GetChildEntry(row)));
		}

		return QModelIndex();
	}

	virtual QModelIndex CDictionaryModel::parent(const QModelIndex& index) const override
	{
		if (index.isValid())
		{
			const CAbstractDictionaryEntry* pEntry = static_cast<const CAbstractDictionaryEntry*>(index.internalPointer());
			if (pEntry)
			{
				const CAbstractDictionaryEntry* pParentEntry = pEntry->GetParentEntry();
				if (pParentEntry)
				{
					const CAbstractDictionaryEntry* pParentParentEntry = pParentEntry->GetParentEntry();
					if (pParentParentEntry)
					{
						for (int32 i = 0; i < pParentParentEntry->GetNumChildEntries(); ++i)
						{
							if (pParentParentEntry->GetChildEntry(i) == pParentEntry)
							{
								return QAbstractItemModel::createIndex(i, 0, reinterpret_cast<quintptr>(pParentEntry));
							}
						}
					}
					else
					{
						for (int32 i = 0; i < m_dictionary.GetNumEntries(); ++i)
						{
							if (m_dictionary.GetEntry(i) == pParentEntry)
							{
								return QAbstractItemModel::createIndex(i, 0, reinterpret_cast<quintptr>(pParentEntry));
							}
						}
					}
				}
			}

		}
		return QModelIndex();
	}
	// ~QAbstractItemModel

public:
	static QVariant GetHeaderDataFromDictionary(const CAbstractDictionary& dictionary, int section, Qt::Orientation orientation, int role)
	{
		if (orientation != Qt::Horizontal)
		{
			return QVariant();
		}

		const CItemModelAttribute* pAttribute = dictionary.GetColumnAttribute(section);
		if (pAttribute)
		{
			if (role == DisplayRole || role == ToolTipRole || role == MergingRole)
			{
				return pAttribute->GetName();
			}
			else if (role == Attributes::s_getAttributeRole)
			{
				return QVariant::fromValue(const_cast<CItemModelAttribute*>(pAttribute));
			}
			else if (role == Attributes::s_attributeMenuPathRole)
			{
				return "";
			}
		}

		return QVariant();
	}

private:
	CAbstractDictionary& m_dictionary;
};

class CDictionaryFilterProxyModel : public QAttributeFilterProxyModel//public QDeepFilterProxyModel
{
	using QAttributeFilterProxyModel::QAttributeFilterProxyModel;

	// Ensures folders and entries are always grouped together in the sorting order.
	bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
	{
		CAbstractDictionaryEntry* pLeft = reinterpret_cast<CAbstractDictionaryEntry*>(left.data(CDictionaryModel::PointerRole).value<quintptr>());
		CAbstractDictionaryEntry* pRight = reinterpret_cast<CAbstractDictionaryEntry*>(right.data(CDictionaryModel::PointerRole).value<quintptr>());
		if (pLeft && pRight && pLeft->GetType() != pRight->GetType())
		{
			return pLeft->GetType() == CAbstractDictionaryEntry::Type_Folder;
		}
		else
			return QDeepFilterProxyModel::lessThan(left, right);
	}
};

class CDictionaryDelegate : public QStyledItemDelegate
{
public:
	CDictionaryDelegate(QObject* pParent = nullptr)
		: QStyledItemDelegate(pParent)
	{
		m_pLabel = new QLabel();
		m_pLabel->setWordWrap(true);
		m_pToolTip = new QPopupWidget("DictionaryToolTip ", m_pLabel);
		m_pToolTip->setAttribute(Qt::WA_ShowWithoutActivating);
	}

	virtual bool helpEvent(QHelpEvent* pEvent, QAbstractItemView* pView, const QStyleOptionViewItem& option, const QModelIndex& index) override
	{
		QString description = index.data(CDictionaryModel::ToolTipRole).value<QString>();
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

void CAbstractDictionary::Clear()
{
	signalDictionaryClear();
}

CDictionaryWidget::CDictionaryWidget(QWidget* pParent, QFilteringPanel* pFilteringPanel)
	: m_pFilterProxy(nullptr)
	, m_pFilteringPanel(pFilteringPanel)
	, m_pMergingModel(new CMergingProxyModel(this))
	, m_pTreeView(new QAdvancedTreeView(static_cast<QAdvancedTreeView::Behavior>(QAdvancedTreeView::PreserveExpandedAfterReset | QAdvancedTreeView::PreserveSelectionAfterReset | QAdvancedTreeView::UseItemModelAttribute)))
{
	m_pFilter = pFilteringPanel ? pFilteringPanel->GetSearchBox() : new QSearchBox();

	m_pFilter->EnableContinuousSearch(true);
	m_pFilter->setPlaceholderText("Search");
	m_pFilter->signalOnFiltered.Connect(this, &CDictionaryWidget::OnFiltered);

	m_pTreeView->setSelectionMode(QAbstractItemView::NoSelection);
	m_pTreeView->setHeaderHidden(true);
	m_pTreeView->setItemsExpandable(true);
	m_pTreeView->setRootIsDecorated(true);
	m_pTreeView->setMouseTracking(true);
	m_pTreeView->setItemDelegate(new CDictionaryDelegate());

	QObject::connect(m_pTreeView, &QTreeView::clicked, this, &CDictionaryWidget::OnClicked);
	QObject::connect(m_pTreeView, &QTreeView::doubleClicked, this, &CDictionaryWidget::OnDoubleClicked);

	QVBoxLayout* pLayout = new QVBoxLayout();

	if (m_pFilteringPanel)
		pLayout->addWidget(m_pFilteringPanel);
	else
		pLayout->addWidget(m_pFilter);

	if (m_pFilteringPanel)
	{
		m_pFilteringPanel->SetContent(m_pTreeView);
		m_pFilteringPanel->GetSearchBox()->SetAutoExpandOnSearch(m_pTreeView);
	}
	else
	{
		pLayout->addWidget(m_pTreeView);
	}

	setLayout(pLayout);
}

CDictionaryWidget::~CDictionaryWidget()
{
	m_pFilter->deleteLater();
	m_pTreeView->deleteLater();
	m_pMergingModel->deleteLater();

	if (m_pFilterProxy)
		m_pFilterProxy->deleteLater();

	for (auto it : m_modelMap)
		it->deleteLater();
}

CAbstractDictionary* CDictionaryWidget::GetDictionary(const QString& name) const
{
	CAbstractDictionary* pDictionary = nullptr;

	if (m_modelMap.contains(name))
		pDictionary = static_cast<CDictionaryModel*>(m_modelMap.value(name))->GetDictionary();

	return pDictionary;
}

void CDictionaryWidget::AddDictionary(CAbstractDictionary& newDictionary)
{
	RemoveDictionary(newDictionary.GetName());

	CDictionaryModel* pNewModel = new CDictionaryModel(newDictionary);
	pNewModel->Initialize();

	m_modelMap.insert(newDictionary.GetName(), pNewModel);

	if (m_pFilterProxy)
		m_pFilterProxy->deleteLater();

	std::vector<CItemModelAttribute*> columns;
	GatherItemModelAttributes(columns);

	m_pMergingModel->SetHeaderDataCallbacks(columns.size(), std::bind(&CDictionaryWidget::GeneralHeaderDataCallback, this, columns, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), CDictionaryModel::MergingRole);
	m_pMergingModel->MountAppend(pNewModel);

	m_pFilterProxy = new CDictionaryFilterProxyModel(static_cast<CDictionaryFilterProxyModel::BehaviorFlags>(CDictionaryFilterProxyModel::AcceptIfChildMatches | CDictionaryFilterProxyModel::AcceptIfParentMatches), this, CDictionaryModel::FilteringRole);
	m_pFilterProxy->setSourceModel(m_pMergingModel);

	if (m_pFilteringPanel)
		m_pFilteringPanel->SetModel(m_pFilterProxy);

	m_pFilter->SetModel(m_pFilterProxy);
	m_pTreeView->setModel(m_pFilterProxy);	

	const int32 filterColumn = newDictionary.GetDefaultFilterColumn();
	if (filterColumn >= 0)
		m_pFilterProxy->setFilterKeyColumn(filterColumn);

	const int32 sortColumn = newDictionary.GetDefaultSortColumn();
	if (sortColumn >= 0)
	{
		m_pTreeView->setSortingEnabled(true);
		m_pTreeView->sortByColumn(sortColumn, Qt::SortOrder::AscendingOrder);
	}
	else
	{
		m_pTreeView->setSortingEnabled(false);
	}
}

void CDictionaryWidget::RemoveDictionary(const QString& name)
{
	CAbstractDictionary* pDictionary = GetDictionary(name);
	if (pDictionary)
	{
		CDictionaryModel* pModel = static_cast<CDictionaryModel*>(m_modelMap[name]);

		m_pMergingModel->Unmount(pModel);

		m_modelMap.erase(m_modelMap.find(name));

		pModel->ShutDown();
		pModel->deleteLater();
	}
}

void CDictionaryWidget::RemoveAllDictionaries()
{
	for (auto it = m_modelMap.begin(); it != m_modelMap.end(); it++)
	{
		CDictionaryModel* pModel = static_cast<CDictionaryModel*>(it.value());
		m_pMergingModel->Unmount(pModel);

		m_modelMap.erase(it);

		pModel->ShutDown();
		pModel->deleteLater();
	}
}

void CDictionaryWidget::ShowHeader(bool flag)
{
	m_pTreeView->setHeaderHidden(!flag);
}

void CDictionaryWidget::SetFilterText(const QString& filterText)
{
	m_pFilter->setText(filterText);
}

void CDictionaryWidget::OnClicked(const QModelIndex& index)
{
	if (index.isValid())
	{
		CAbstractDictionaryEntry* pEntry = reinterpret_cast<CAbstractDictionaryEntry*>(index.data(CDictionaryModel::PointerRole).value<quintptr>());
		if (pEntry)
		{
			OnEntryClicked(*pEntry);
			if (pEntry->GetType() == CAbstractDictionaryEntry::Type_Folder)
			{
				if (m_pTreeView->isExpanded(index))
					m_pTreeView->collapse(index);
				else
					m_pTreeView->expand(index);
			}
		}
	}
}

void CDictionaryWidget::OnDoubleClicked(const QModelIndex& index)
{
	if (index.isValid())
	{
		CAbstractDictionaryEntry* pEntry = reinterpret_cast<CAbstractDictionaryEntry*>(index.data(CDictionaryModel::PointerRole).value<quintptr>());
		if (pEntry)
		{
			OnEntryDoubleClicked(*pEntry);
		}
	}
}

void CDictionaryWidget::OnFiltered()
{
	const QString text = m_pFilter->text();
	if (!text.isEmpty())
	{
		m_pTreeView->expandAll();
	}
	else
	{
		m_pTreeView->collapseAll();
	}
}

void CDictionaryWidget::GatherItemModelAttributes(std::vector<CItemModelAttribute*>& columns)
{
	for (auto it : m_modelMap)
	{
		CDictionaryModel*    model = static_cast<CDictionaryModel*>(it);
		CAbstractDictionary* dictionary = model->GetDictionary();

		size_t count = dictionary->GetNumColumns();
		for (size_t i = 0; i < count; i++)
		{
			auto columnAttribute = const_cast<CItemModelAttribute*>(dictionary->GetColumnAttribute(i));
			if (std::find(columns.begin(), columns.end(), columnAttribute) == columns.end())
			{
				columns.push_back(columnAttribute);
			}
		}
	}
}

QVariant CDictionaryWidget::GeneralHeaderDataCallback(std::vector<CItemModelAttribute*>& columns, int section, Qt::Orientation orientation, int role)
{
	CRY_ASSERT(section < columns.size());

	if (orientation != Qt::Horizontal)
	{
		return QVariant();
	}

	const CItemModelAttribute* pAttribute = columns[section];
	if (pAttribute)
	{
		if (role == CDictionaryModel::DisplayRole || role == CDictionaryModel::ToolTipRole || role == CDictionaryModel::MergingRole)
		{
			return pAttribute->GetName();
		}
		else if (role == CDictionaryModel::FilteringRole)//&& pAttribute->GetVisibility() == CItemModelAttribute::AlwaysHidden)
		{
			return QVariant::fromValue(const_cast<CItemModelAttribute*>(pAttribute));
		}
		else if (role == Attributes::s_getAttributeRole)
		{
			return QVariant::fromValue(const_cast<CItemModelAttribute*>(pAttribute));
		}
		else if (role == Attributes::s_attributeMenuPathRole)
		{
			return "";
		}
	}

	return QVariant();
}

void CDictionaryWidget::showEvent(QShowEvent* pEvent)
{
	m_pFilter->setText("");
	QWidget::showEvent(pEvent);
	m_pFilter->setFocus(Qt::FocusReason::PopupFocusReason);
}

void CDictionaryWidget::hideEvent(QHideEvent* pEvent)
{
	OnHide();
	QWidget::hideEvent(pEvent);
}

CModalPopupDictionary::CModalPopupDictionary(QString name, CAbstractDictionary& dictionary)
	: m_pResult(nullptr)
	, m_pEventLoop(nullptr)
	, m_dictName(dictionary.GetName())
{
	m_pEventLoop = new QEventLoop();

	m_pDictionaryWidget = new CDictionaryWidget();
	m_pDictionaryWidget->AddDictionary(dictionary);

	m_pPopupWidget = new QPopupWidget(name, m_pDictionaryWidget);

	QObject::connect(m_pDictionaryWidget, &CDictionaryWidget::OnEntryClicked, this, &CModalPopupDictionary::OnEntryClicked);
	QObject::connect(m_pDictionaryWidget, &CDictionaryWidget::OnEntryDoubleClicked, this, &CModalPopupDictionary::OnEntryClicked);

	QObject::connect(m_pPopupWidget, &QPopupWidget::SignalHide, this, &CModalPopupDictionary::OnAborted);
}

CModalPopupDictionary::~CModalPopupDictionary()
{
	m_pDictionaryWidget->RemoveDictionary(m_dictName);
	m_pPopupWidget->deleteLater();
	delete m_pEventLoop;
}

void CModalPopupDictionary::ExecAt(const QPoint& globalPos, QPopupWidget::Origin origin /* = Unknown*/)
{
	m_pPopupWidget->ShowAt(globalPos, origin);
	m_pEventLoop->exec();
}

void CModalPopupDictionary::OnEntryClicked(CAbstractDictionaryEntry& entry)
{
	if (entry.GetType() == CAbstractDictionaryEntry::Type_Entry && entry.IsEnabled())
	{
		m_pResult = &entry;
		m_pPopupWidget->hide();
		if (m_pEventLoop)
			m_pEventLoop->exit();
	}
}

void CModalPopupDictionary::OnAborted()
{
	if (m_pEventLoop)
		m_pEventLoop->exit();
}


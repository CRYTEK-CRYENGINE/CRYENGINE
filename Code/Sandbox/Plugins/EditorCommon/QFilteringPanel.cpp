// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QFilteringPanel.h"

#include "EditorFramework/PersonalizationManager.h"
#include "Controls/QMenuComboBox.h"
#include "Controls/QPopupWidget.h"
#include "Menu/MenuWidgetBuilders.h"
#include "ProxyModels/FavoritesHelper.h"
#include "QAdvancedTreeView.h"
#include "QSearchBox.h"
#include "QtUtil.h"
#include "QControls.h"

#include <IEditor.h>
#include <CryIcon.h>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

class QFilteringPanel::CSavedFiltersModel : public QAbstractListModel
{
public:
	CSavedFiltersModel(QFilteringPanel* parent)
		: m_filterPanel(parent)
	{
		Update();
	}

	void Update()
	{
		beginResetModel();
		m_filtersName.clear();
		auto savedFiltersVariant = GetIEditor()->GetPersonalizationManager()->GetProjectProperty("QFilteringPanel", m_filterPanel->m_uniqueName);
		if (savedFiltersVariant.isValid() && savedFiltersVariant.type() == QVariant::Map)
		{
			QVariantMap map = savedFiltersVariant.toMap();
			m_filtersName = map.keys();
		}
		endResetModel();
	}

private:
	virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override { return m_filtersName.count(); }
	Qt::ItemFlags flags(const QModelIndex &index) const override { return Qt::ItemIsEditable | QAbstractListModel::flags(index); }

	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
	{
		if (section == 1 && orientation == Qt::Horizontal && role == Qt::DisplayRole)
			return "Name";
		else
			return QVariant();
	}

	virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
	{
		if (index.isValid())
		{
			switch (role)
			{
			case Qt::DisplayRole:
			case Qt::EditRole:
				return m_filtersName[index.row()];
			default:
				break;
			}
		}

		return QVariant();
	}

	virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole)
	{
		if(index.isValid())
		{
			const QString newName = value.toString();
			if (!newName.isEmpty() && !m_filtersName.contains(newName))
			{
				const int row = index.row();
				m_filterPanel->RenameFilter(m_filtersName[row].toStdString().c_str(), newName.toStdString().c_str());
				m_filtersName[row] = newName;
				return true;
			}
		}

		return false;
	}

	QStringList m_filtersName;
	QFilteringPanel* m_filterPanel;
};


class QFilteringPanel::CSavedFiltersWidget : public QWidget
{
public:
	CSavedFiltersWidget(QFilteringPanel* parent);
	~CSavedFiltersWidget() {}

	void Update() { m_model->Update(); }

private:
	void OnSaveCurrentFilter();
	void OnLoadSelectedFilter();
	void OnDeleteSelectedFilter();

	CSavedFiltersModel* m_model;
	QFilteringPanel* m_filterPanel;
	QAdvancedTreeView* m_view;
};

QFilteringPanel::CSavedFiltersWidget::CSavedFiltersWidget(QFilteringPanel* parent)
	: m_filterPanel(parent)
{
	//Models
	m_model = new CSavedFiltersModel(parent);

	auto filterModel = new QSortFilterProxyModel();
	filterModel->setSourceModel(m_model);
	filterModel->sort(0);

	QSearchBox* searchBox = new QSearchBox();
	searchBox->SetModel(filterModel);

	m_view = new QAdvancedTreeView();
	m_view->setModel(filterModel);
	m_view->setHeaderHidden(true);
	m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_view->setSelectionMode(QAbstractItemView::SingleSelection);
	m_view->header()->setStretchLastSection(true);

	auto saveButton = new QToolButton();
	saveButton->setIcon(CryIcon("icons:General/Save_as.ico"));
	saveButton->setText(tr("Save Filter"));
	saveButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	connect(saveButton, &QToolButton::clicked, [this]() { OnSaveCurrentFilter(); });

	auto loadButton = new QToolButton();
	loadButton->setIcon(CryIcon("icons:General/Save_To_Disc.ico"));
	loadButton->setText(tr("Load Filter"));
	loadButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	connect(loadButton, &QToolButton::clicked, [this]() { OnLoadSelectedFilter(); });

	auto deleteButton = new QToolButton();
	deleteButton->setIcon(CryIcon("icons:General/Close.ico"));
	deleteButton->setText(tr("Delete Filter"));
	deleteButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	connect(deleteButton, &QToolButton::clicked, [this]() { OnDeleteSelectedFilter(); });

	auto buttonsLayout = new QHBoxLayout();
	buttonsLayout->setMargin(0);
	buttonsLayout->addWidget(saveButton);
	buttonsLayout->addWidget(loadButton);
	buttonsLayout->addWidget(deleteButton);

	QVBoxLayout* layout = new QVBoxLayout();
	layout->addWidget(searchBox);
	layout->addWidget(m_view);
	layout->addLayout(buttonsLayout);
	setLayout(layout);
}

void QFilteringPanel::CSavedFiltersWidget::OnSaveCurrentFilter()
{
	//Create unique name
	QString newFilterName("New Filter");

	auto savedFiltersVariant = GetIEditor()->GetPersonalizationManager()->GetProjectProperty("QFilteringPanel", m_filterPanel->m_uniqueName);
	if (savedFiltersVariant.isValid() && savedFiltersVariant.type() == QVariant::Map)
	{
		QVariantMap map = savedFiltersVariant.toMap();
		auto keys = map.keys();
		auto i = 1;
		while (keys.contains(newFilterName))
		{
			newFilterName = QString("New Filter %1").arg(i);
			i++;
		}
	}

	m_filterPanel->SaveFilter(newFilterName.toStdString().c_str());
	Update();

	//select and edit new filter
	auto model = m_view->model();//must use the model from the view (the proxy)
	QModelIndexList list = model->match(model->index(0, 0, QModelIndex()), Qt::DisplayRole, newFilterName, 1, Qt::MatchExactly);
	if (!list.isEmpty())
	{
		m_view->scrollTo(list.first());
		m_view->selectionModel()->setCurrentIndex(list.first(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		m_view->edit(list.first());
	}
}

void QFilteringPanel::CSavedFiltersWidget::OnLoadSelectedFilter()
{
	QModelIndex selected = m_view->selectionModel()->currentIndex();
	if (!selected.isValid())
		return;

	QString filterName = selected.data(Qt::DisplayRole).toString();
	m_filterPanel->LoadFilter(filterName);
}

void QFilteringPanel::CSavedFiltersWidget::OnDeleteSelectedFilter()
{
	QModelIndex selected = m_view->selectionModel()->currentIndex();
	if (!selected.isValid())
		return;

	QString filterName = selected.data(Qt::DisplayRole).toString();
	m_filterPanel->DeleteFilter(filterName.toStdString().c_str());

	Update();
}

//////////////////////////////////////////////////////////////////////////

class QFilteringPanel::CFilterWidget : public QWidget, public IStateSerializable
{
public:
	CFilterWidget(QFilteringPanel* parent);
	~CFilterWidget();

	void        Remove();
	void        OnSelectAttribute(int index);
	void        OnSelectOperator(Attributes::IAttributeFilterOperator* op);
	void        UpdateAttributesComboBox();

	bool        HasSameState(const QVariantMap& state) const;

	void        SetState(const QVariantMap& state);
	QVariantMap GetState() const;

	void		Enable(bool enabled);

	//Using QMenuComboBox for consistency with the multi-select combo box necessary for enum types
	QMenuComboBox*           m_pAttributesComboBox;
	QMenuComboBox*           m_pOperatorsComboBox;
	QContainer*              m_pEditWidgetContainer;
	QToolButton*			 m_pInvertButton;
	QCheckBox*				 m_pEnabledCheckBox;
	AttributeFilterSharedPtr m_pFilter;
	QFilteringPanel*         m_pParent;
};

QFilteringPanel::CFilterWidget::CFilterWidget(QFilteringPanel* parent)
	: m_pAttributesComboBox(new QMenuComboBox(this))
	, m_pOperatorsComboBox(new QMenuComboBox(this))
	, m_pEditWidgetContainer(new QContainer(this))
	, m_pEnabledCheckBox(new QCheckBox(this))
	, m_pInvertButton(new QToolButton(this))
	, m_pFilter(new CAttributeFilter(nullptr))
	, m_pParent(parent)
{
	CRY_ASSERT(parent->GetAttributes().size() > 0);

	m_pParent->OnAddFilter(this);

	m_pEnabledCheckBox->setChecked(true);
	m_pEnabledCheckBox->setToolTip(tr("Enable this filter"));
	connect(m_pEnabledCheckBox, &QCheckBox::toggled, this, &CFilterWidget::Enable);

	UpdateAttributesComboBox();

	m_pOperatorsComboBox->signalCurrentIndexChanged.Connect([&](int index)
	{
		OnSelectOperator(reinterpret_cast<Attributes::IAttributeFilterOperator*>(m_pOperatorsComboBox->GetData(index).value<intptr_t>()));
	});

	QToolButton* pRemoveButton = new QToolButton();
	pRemoveButton->setIcon(CryIcon("icons:General/Element_Remove.ico"));
	pRemoveButton->setToolTip(tr("Remove this filter"));
	connect(pRemoveButton, &QToolButton::clicked, this, [=]()
	{
		Remove();
	});

	m_pInvertButton->setIcon(CryIcon("icons:General/Invert.ico"));
	m_pInvertButton->setCheckable(true);
	m_pInvertButton->setChecked(false);
	m_pInvertButton->setToolTip(tr("Invert this filter"));
	connect(m_pInvertButton, &QToolButton::clicked, [this](bool checked)
	{
		bool newVal = !m_pFilter->IsInverted();
		m_pInvertButton->setChecked(newVal);
		m_pFilter->SetInverted(newVal);
	});

	QHBoxLayout* pLayout = new QHBoxLayout(this);
	pLayout->setMargin(0);
	
	pLayout->addWidget(m_pEnabledCheckBox, Qt::AlignLeft);
	pLayout->addWidget(m_pAttributesComboBox, Qt::AlignLeft);
	pLayout->addWidget(m_pOperatorsComboBox, Qt::AlignLeft);
	pLayout->addWidget(m_pEditWidgetContainer, Qt::AlignLeft);
	pLayout->addWidget(m_pInvertButton, Qt::AlignRight);
	pLayout->addWidget(pRemoveButton, Qt::AlignRight);
	setLayout(pLayout);

	m_pEnabledCheckBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_pInvertButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	pRemoveButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_pAttributesComboBox->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
	m_pOperatorsComboBox->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
	m_pEditWidgetContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QFilteringPanel::CFilterWidget::~CFilterWidget()
{
	if (m_pParent)
	{
		m_pParent->OnRemoveFilter(this);
	}
}

void QFilteringPanel::CFilterWidget::Enable(bool enabled)
{
	m_pAttributesComboBox->setEnabled(enabled);
	m_pEditWidgetContainer->setEnabled(enabled);
	m_pInvertButton->setEnabled(enabled);
	m_pFilter->SetEnabled(enabled);
}

void QFilteringPanel::CFilterWidget::UpdateAttributesComboBox()
{
	auto selectedAttribute = m_pFilter->GetAttribute();

	m_pAttributesComboBox->Clear();

	auto attributes = m_pParent->GetAttributes();
	for (CItemModelAttribute* attr : attributes)
	{
		m_pAttributesComboBox->AddItem(attr->GetName(), reinterpret_cast<intptr_t>(attr));
	}

	m_pAttributesComboBox->signalCurrentIndexChanged.Connect(this, &CFilterWidget::OnSelectAttribute);
	m_pAttributesComboBox->setVisible(attributes.size() > 1);

	if (selectedAttribute)
	{
		for (int i = 0; i < attributes.size(); i++)
		{
			if (attributes[i] == selectedAttribute)
				m_pAttributesComboBox->SetChecked(i, true);
		}
	}
	else
	{
		OnSelectAttribute(0);
	}
}

void QFilteringPanel::CFilterWidget::OnSelectAttribute(int index)
{
	CItemModelAttribute* attribute = reinterpret_cast<CItemModelAttribute*>(m_pAttributesComboBox->GetData(index).value<intptr_t>());
	if (attribute != m_pFilter->GetAttribute())
	{
		m_pFilter->SetAttribute(attribute);
		m_pFilter->SetFilterValue(attribute->GetDefaultFilterValue());

		const IAttributeType* pType = attribute->GetType();

		std::vector<Attributes::IAttributeFilterOperator*> operators = pType->GetOperators();

		CRY_ASSERT(operators.size() > 0);

		if (operators.size() == 1)
		{
			m_pOperatorsComboBox->setVisible(false);
		}
		else
		{
			m_pOperatorsComboBox->Clear();
			for (Attributes::IAttributeFilterOperator* pOperator : operators)
			{
				m_pOperatorsComboBox->AddItem(pOperator->GetName(), reinterpret_cast<intptr_t>(pOperator));
			}
			m_pOperatorsComboBox->setVisible(true);
		}

		if (attribute->GetType() == &Attributes::s_booleanAttributeType)
		{
			m_pInvertButton->setChecked(false);
			m_pInvertButton->setVisible(false);
		}
		else
		{
			m_pInvertButton->setVisible(true);
		}

		m_pFilter->SetOperator(nullptr);
		OnSelectOperator(operators[0]);
	}
}

void QFilteringPanel::CFilterWidget::OnSelectOperator(Attributes::IAttributeFilterOperator* op)
{
	if (op != m_pFilter->GetOperator())
	{
		m_pFilter->SetOperator(op);

		QWidget* pEditWidget = op->CreateEditWidget(m_pFilter, m_pParent->GetAttributeEnumEntries(m_pFilter->GetAttribute()->GetName()));
		pEditWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		m_pEditWidgetContainer->SetChild(pEditWidget);
	}
}

void QFilteringPanel::CFilterWidget::Remove()
{
	deleteLater();
}

bool QFilteringPanel::CFilterWidget::HasSameState(const QVariantMap& state) const
{
	QVariant enabled = state.value("enabled");
	QVariant attributeName = state.value("attributeName");
	QVariant operatorName = state.value("operator");
	QVariant filterValue = state.value("filterValue");
	QVariant inverted = state.value("inverted");

	if (!attributeName.isValid() || m_pFilter->GetAttribute()->GetName() != attributeName.toString())
		return false;

	if (!operatorName.isValid() || m_pFilter->GetOperator()->GetName() != operatorName.toString())
		return false;

	if (!filterValue.isValid() || m_pFilter->GetFilterValue() != filterValue)
		return false;

	if (!enabled.isValid() || m_pFilter->IsEnabled() != enabled.toBool())
		return false;

	if (!inverted.isValid() || m_pFilter->IsInverted() != inverted.toBool())
		return false;

	return true;
}

void QFilteringPanel::CFilterWidget::SetState(const QVariantMap& state)
{
	QVariant enabled = state.value("enabled");
	QVariant attributeName = state.value("attributeName");
	QVariant operatorName = state.value("operator");
	QVariant filterValue = state.value("filterValue");
	QVariant inverted = state.value("inverted");

	if (attributeName.isValid())
		m_pAttributesComboBox->SetChecked(attributeName.toString());

	if (operatorName.isValid())
		m_pOperatorsComboBox->SetChecked(operatorName.toString());

	if (filterValue.isValid())
	{
		m_pFilter->SetFilterValue(filterValue);

		QWidget* widget = m_pEditWidgetContainer->GetChild();
		m_pFilter->GetOperator()->UpdateWidget(widget, filterValue);
	}

	if (enabled.isValid())
		m_pEnabledCheckBox->setChecked(enabled.toBool());

	if (inverted.isValid())
		m_pInvertButton->setChecked(inverted.toBool());
}

QVariantMap QFilteringPanel::CFilterWidget::GetState() const
{
	QVariantMap state;

	if (m_pFilter->GetAttribute())
	{
		state.insert("enabled", m_pFilter->IsEnabled());
		state.insert("inverted", m_pFilter->IsInverted());
		state.insert("attributeName", m_pFilter->GetAttribute()->GetName());
		state.insert("operator", m_pFilter->GetOperator()->GetName());
		state.insert("filterValue", m_pFilter->GetFilterValue());
	}

	return state;
}

//////////////////////////////////////////////////////////////////////////

QFilteringPanel::QFilteringPanel(const char* uniqueName, QAttributeFilterProxyModel* pModel, QWidget* pParent)
	: QWidget(pParent)
	, m_pModel(nullptr)
	, m_timerMsec(0)
	, m_timer(nullptr)
	, m_uniqueName(uniqueName)
	, m_savedFiltersWidget(nullptr)
{
	m_pSearchBox = new QSearchBox(this);
	m_pSearchBox->SetModel(pModel);

	m_addFilterButton = new QToolButton();
	m_addFilterButton->setIcon(CryIcon("icons:General/Element_Add.ico"));
	m_addFilterButton->setText(tr("Add Criterion"));
	m_addFilterButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	connect(m_addFilterButton, &QToolButton::clicked, [this](){ AddFilter(); });

	m_saveLoadButton = new QToolButton();
	m_saveLoadButton->setIcon(CryIcon("icons:General/Save_as.ico"));
	m_saveLoadButton->setText(tr("Save/Load Filter"));
	m_saveLoadButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	connect(m_saveLoadButton, &QToolButton::clicked, [this]() { ShowSavedFilters(); });

	QToolButton* pClearCriteria = new QToolButton();
	pClearCriteria->setIcon(CryIcon("icons:General/Element_Clear.ico"));
	pClearCriteria->setText(tr("Clear Criteria"));
	pClearCriteria->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	connect(pClearCriteria, &QToolButton::clicked, this, &QFilteringPanel::Clear);

	m_favoritesButton = new QToolButton();
	m_favoritesButton->setIcon(FavoritesHelper::GetFavoriteIcon(false));
	m_favoritesButton->setCheckable(true);
	m_favoritesButton->setToolTip(tr("Show favorites"));
	connect(m_favoritesButton, &QToolButton::toggled, [=](bool checked)
	{
		m_favoritesFilter->SetEnabled(checked);
		m_favoritesButton->setIcon(FavoritesHelper::GetFavoriteIcon(checked));
		OnFilterChanged();
	});

	m_optionsButton = new QToolButton();
	m_optionsButton->setIcon(CryIcon("icons:General/Filter.ico"));
	m_optionsButton->setCheckable(true);
	connect(m_optionsButton, &QToolButton::clicked, [=]()
	{
		SetExpanded(!IsExpanded());
		UpdateOptionsIcon();
	});

	//Quick filters on right click on filter button
	{
		QMenu* const pMenu = new QMenu(this);
		m_optionsButton->setMenu(pMenu);
		m_optionsButton->setContextMenuPolicy(Qt::CustomContextMenu);
		m_optionsButton->setObjectName("Filter");
		connect(m_optionsButton, &QToolButton::customContextMenuRequested, [this]() 
		{
			CAbstractMenu abstractMenu;
			FillMenu(&abstractMenu);
			abstractMenu.Build(MenuWidgetBuilders::CMenuBuilder(m_optionsButton->menu()));
			m_optionsButton->showMenu();
		});
	}
	
	m_optionsLayout = new QGridLayout();
	m_optionsLayout->setContentsMargins(0, 0, 0, 0);
	m_optionsLayout->setMargin(0);
	m_optionsLayout->setSpacing(0);
	m_optionsLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	m_optionsLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
	m_optionsLayout->addWidget(m_addFilterButton, 0, 0, Qt::AlignLeft);
	m_optionsLayout->addWidget(pClearCriteria, 0, 1, Qt::AlignLeft);
	m_optionsLayout->addWidget(m_saveLoadButton, 0, 2, Qt::AlignLeft);

	m_pFiltersLayout = new QVBoxLayout();
	m_pFiltersLayout->setContentsMargins(0, 0, 0, 0);
	m_pFiltersLayout->setSpacing(0);
	m_pFiltersLayout->setMargin(0);
	m_pFiltersLayout->addLayout(m_optionsLayout);
	m_pFiltersLayout->addStretch();

	m_pFiltersWidget = QtUtil::MakeScrollable(m_pFiltersLayout);
	m_pFiltersWidget->setHidden(true);

	QWidget* pTopBarContainer = new QWidget();
	pTopBarContainer->setObjectName("SearchBoxContainer");

	QHBoxLayout* pTopBarLayout = new QHBoxLayout();
	pTopBarLayout->setSpacing(0);
	pTopBarLayout->setMargin(0);
	pTopBarLayout->setAlignment(Qt::AlignTop);
	pTopBarLayout->addWidget(m_favoritesButton);
	pTopBarLayout->addWidget(m_pSearchBox);
	pTopBarLayout->addWidget(m_optionsButton);

	pTopBarContainer->setLayout(pTopBarLayout);

	m_splitter = new QSplitter();
	m_splitter->setOrientation(Qt::Vertical);
	m_splitter->setChildrenCollapsible(false);
	m_splitter->addWidget(m_pFiltersWidget);

	QVBoxLayout* vbox = new QVBoxLayout();
	vbox->setSpacing(0);
	vbox->setMargin(0);
	vbox->addWidget(pTopBarContainer);
	vbox->addWidget(m_splitter);
	setLayout(vbox);

	SetModel(pModel);

	EnableDelayedSearch(true);
}

QFilteringPanel::~QFilteringPanel()
{
	// this destructor is called before the destructors of the child filter widgets are processed because
	// the filter widgets are children of the filtering panel's layout and the layout is deleted after this
	// destructor is called. the filter widget destructor tries to access the parent filtering panel if it
	// is non-nullptr, so set the parent filtering panel to nullptr to avoid the invalid access.
	for (auto pFilterWidget : m_filters)
	{
		pFilterWidget->m_pParent = nullptr;
	}
}

void QFilteringPanel::OnAddFilter(CFilterWidget* filter)
{
	CRY_ASSERT(filter);

	if (m_pModel)
	{
		m_pModel->AddFilter(filter->m_pFilter);
	}

	filter->m_pFilter->signalChanged.Connect(this, &QFilteringPanel::OnFilterChanged);
	m_filters.push_back(filter);

	UpdateOptionsIcon();
}

void QFilteringPanel::OnRemoveFilter(CFilterWidget* filter)
{
	CRY_ASSERT(filter);

	if (m_pModel)
	{
		m_pModel->RemoveFilter(filter->m_pFilter);
	}

	filter->m_pFilter->signalChanged.DisconnectObject(this);

	auto it = std::find(m_filters.begin(), m_filters.end(), filter);
	CRY_ASSERT(it != m_filters.end());
	m_filters.erase(it);

	UpdateOptionsIcon();
}

bool QFilteringPanel::HasActiveFilters() const
{
	if (m_favoritesFilter && m_favoritesFilter->IsEnabled())
		return true;

	for (CFilterWidget* w : m_filters)
	{
		if (w->m_pFilter->IsEnabled())
			return true;
	}
	return false;
}

void QFilteringPanel::CreateMenu(CAbstractMenu* pParentMenu)
{
	const QString filterMenuName(tr("Apply Filter"));

	// Make sure a menu with this name doesn't already exist, if so clear it
	string menuName = QtUtil::ToString(filterMenuName);
	CAbstractMenu* pFilterMenu = pParentMenu->FindMenu(menuName);

	if (!pFilterMenu)
	{
		pFilterMenu = pParentMenu->CreateMenu(menuName);
	}

	pFilterMenu->signalAboutToShow.Connect(this, &QFilteringPanel::FillMenu);
}

void QFilteringPanel::FillMenu(CAbstractMenu* pMenu)
{
	if (!pMenu)
	{
		return;
	}

	pMenu->Clear();

	const QVector<QString> filters = GetSavedFilters();
	for (QString filterName : filters)
	{
		QAction* const pAction = pMenu->CreateAction(filterName);
		pAction->setCheckable(true);
		pAction->setChecked(IsFilterSet(filterName));
		connect(pAction, &QAction::triggered, [this, filterName](bool checked)
		{
			if (checked)
			{
				LoadFilter(filterName);
			}
			else
			{
				Clear();
			}
		});
	}

	// If there are filters then add a section for clearing the current criteria
	if (filters.empty())
	{
		QAction* const pAction = pMenu->CreateAction(tr("No Saved Filters"));
		pAction->setEnabled(false);
	}

	int section = pMenu->GetNextEmptySection();

	QAction* pClearCriteria = pMenu->CreateAction("Clear Criteria", section);
	pClearCriteria->setIcon(CryIcon("icons:General/Element_Clear.ico"));
	connect(pClearCriteria, &QAction::triggered, this, &QFilteringPanel::Clear);
}

bool QFilteringPanel::IsFilterSet(const QString& filterName) const
{
	auto savedFiltersVariant = GetIEditor()->GetPersonalizationManager()->GetProjectProperty("QFilteringPanel", m_uniqueName);
	if (!savedFiltersVariant.isValid() || savedFiltersVariant.type() != QVariant::Map)
		return false;

	QVariantMap map = savedFiltersVariant.toMap();
	QVariant state = map[filterName];
	if (!state.isValid() || state.type() != QVariant::List)
		return false;

	QVariantList filtersList = state.value<QVariantList>();

	if (m_filters.size() != filtersList.size())
		return false;

	int idx = 0;
	for (QVariant& var : filtersList)
	{
		if (var.isValid() && var.type() == QVariant::Map)
		{
			if (!m_filters[idx++]->HasSameState(var.value<QVariantMap>()))
				return false;
		}
		else
		{
			return false;
		}
	}

	return true;
}

QVector<QString> QFilteringPanel::GetSavedFilters() const
{
	auto savedFiltersVariant = GetIEditor()->GetPersonalizationManager()->GetProjectProperty("QFilteringPanel", m_uniqueName);
	if (savedFiltersVariant.isValid() && savedFiltersVariant.type() == QVariant::Map)
	{
		QVariantMap map = savedFiltersVariant.toMap();
		return map.keys().toVector();
	}

	return QVector<QString>();
}

void QFilteringPanel::UpdateOptionsIcon()
{
	if (HasActiveFilters())
	{
		m_optionsButton->setChecked(true);
		m_optionsButton->setToolTip(tr("Options\n(Filters active)"));
	}
	else
	{
		m_optionsButton->setChecked(false);
		m_optionsButton->setToolTip(tr("Options\n(No filters active)"));
	}
}

void QFilteringPanel::ShowFavoriteFilter(bool show, bool isFiltering)
{
	if (show && !m_favoritesFilter)
	{
		m_favoritesFilter = std::make_shared<CAttributeFilter>(&Attributes::s_favoriteAttribute);
		m_favoritesFilter->SetOperator(Attributes::s_booleanAttributeType.GetDefaultOperator());
		m_favoritesFilter->SetFilterValue(Qt::Checked);//value is not changed, we just enable or disable the filter
		m_favoritesFilter->SetEnabled(m_favoritesButton->isChecked());
		m_pModel->AddFilter(m_favoritesFilter);
	}

	m_favoritesButton->setVisible(show);
	m_favoritesButton->setChecked(isFiltering);
}

void QFilteringPanel::ShowSavedFilters()
{
	if (!m_savedFiltersWidget)
	{
		m_savedFiltersWidget = new CSavedFiltersWidget(this);
	}

	m_savedFiltersWidget->Update();
	QPopupWidget* popup = new QPopupWidget("QFilteringPanelPopup", m_savedFiltersWidget);
	popup->ShowAt(m_saveLoadButton->mapToGlobal(m_saveLoadButton->rect().bottomLeft()));
}

void QFilteringPanel::EnableDelayedSearch(bool bEnable, float timerMsec /*= 100*/)
{
	m_pSearchBox->EnableContinuousSearch(bEnable, timerMsec);

	if (bEnable)
	{
		if (!m_timer)
		{
			m_timer = new QTimer(this);
			m_timer->setSingleShot(true);
			connect(m_timer, &QTimer::timeout, this, &QFilteringPanel::OnSearch);
		}

		m_timerMsec = timerMsec;
	}
	else if (m_timer)
	{
		delete m_timer;
		m_timer = nullptr;
	}
}

void QFilteringPanel::SetModel(QAttributeFilterProxyModel* pModel)
{
	if (m_pModel != pModel)
	{
		if (m_pModel)
		{
			m_pModel->signalAttributesChanged.DisconnectObject(this);

			for (auto& filter : m_filters)
				m_pModel->RemoveFilter(filter->m_pFilter);
		}

		m_pModel = pModel;

		m_pSearchBox->SetModel(m_pModel);
		setEnabled(m_pModel != nullptr);
		OnModelUpdated();

		if (m_pModel)
		{
			//can not use sourceModelChanged signal as attributes are computed after the source model has changed
			m_pModel->signalAttributesChanged.Connect(this, &QFilteringPanel::OnModelUpdated);

			for (auto& filter : m_filters)
				m_pModel->AddFilter(filter->m_pFilter);
		}
	}
}

void QFilteringPanel::SetContent(QWidget* widget)
{
	//TODO : could handle changing the content by using QContainer, but not necessary at the moment
	CRY_ASSERT(m_splitter->count() == 1);
	m_splitter->addWidget(widget);
}

void QFilteringPanel::SetState(const QVariantMap& state)
{
	Clear();

	const QVariant filtersListVar = state.value("filters");
	SetFilterState(filtersListVar);

	const QVariant expandedVar = state.value("expanded");
	if (expandedVar.isValid())
	{
		SetExpanded(expandedVar.toBool());
	}

	const QVariant favVar = state.value("favoriteFilter");
	const bool bFilteringFavorites = favVar.isValid() ? favVar.toBool() : false;
	const bool bVisible = m_favoritesButton->isVisibleTo(m_favoritesButton->parentWidget());
	ShowFavoriteFilter(bVisible, bFilteringFavorites);
}

QVariantMap QFilteringPanel::GetState() const
{
	QVariantMap state;

	//Getting filter state should be reused for save and load filters
	if (!m_filters.empty())
	{
		state.insert("filters", GetFilterState());
	}

	state.insert("expanded", IsExpanded());

	if(m_favoritesButton->isVisible())
		state.insert("favoriteFilter", m_favoritesButton->isChecked());

	return state;
}

void QFilteringPanel::OnModelUpdated()
{
	if (m_pModel)
	{
		m_attributes.clear();

		ShowFavoriteFilter(false, false);

		auto it = m_pModel->m_attributes.begin();
		m_attributes.reserve(m_pModel->m_attributes.size());
		for (; it != m_pModel->m_attributes.end(); ++it)
		{
			auto attr = it->first;
			if (attr->IsFilterable())
				m_attributes.push_back(attr);
			else if (attr == &Attributes::s_favoriteAttribute)
				ShowFavoriteFilter(true, false);
		}

		std::sort(m_attributes.begin(), m_attributes.end(),
		          [&](CItemModelAttribute* a, CItemModelAttribute* b)
		{
			return m_pModel->m_attributes[a] < m_pModel->m_attributes[b];
		});
	}
	else
	{
		m_attributes.clear();
		ShowFavoriteFilter(false, false);
	}

	m_addFilterButton->setEnabled(!m_attributes.empty());

	if (IsExpanded() && !m_pModel)
		SetExpanded(false);

	for (auto it = m_filters.begin(); it != m_filters.end();)
	{
		auto filter = *it;
		if (std::find(m_attributes.begin(), m_attributes.end(), filter->m_pFilter->GetAttribute()) == m_attributes.end())
		{
			filter->Remove();
		}
		else
		{
			filter->UpdateAttributesComboBox();
		}
		++it;
	}

	UpdateOptionsIcon();
}

bool QFilteringPanel::IsExpanded() const
{
	return m_pFiltersWidget->isVisible();
}

void QFilteringPanel::SetExpanded(bool expanded)
{
	if (expanded != IsExpanded())
	{
		m_pFiltersWidget->setVisible(expanded);
	}
}

void QFilteringPanel::Clear()
{
	for (auto& filter : m_filters)
	{
		filter->Remove();
	}
}

QFilteringPanel::CFilterWidget* QFilteringPanel::AddFilter()
{
	auto pFilterWidget = new CFilterWidget(this);
	// the last item in the layout is the stretch to get all filter widgets to the top
	m_pFiltersLayout->insertWidget(m_pFiltersLayout->count() - 1, pFilterWidget);

	return pFilterWidget;
}

void QFilteringPanel::AddFilter(const QString& attributeName, const QString& operatorName, const QString& filterValue)
{
	QVariantMap state;
	state.insert("enabled", true);
	state.insert("inverted", false);
	state.insert("attributeName", attributeName);
	state.insert("operator", operatorName);
	state.insert("filterValue", filterValue);

	auto filter = AddFilter();
	filter->SetState(state);
}

void QFilteringPanel::OverrideAttributeEnumEntries(const QString& attributeName, const QStringList& values)
{
	m_attributeValues[attributeName] = values;
}

void QFilteringPanel::SaveFilter(const char* filterName)
{
	auto savedFiltersVariant = GetIEditor()->GetPersonalizationManager()->GetProjectProperty("QFilteringPanel", m_uniqueName);

	QVariantMap map;
	if (savedFiltersVariant.isValid() && savedFiltersVariant.type() == QVariant::Map)
	{
		map = savedFiltersVariant.toMap();
	}

	map[filterName] = GetFilterState();
	GetIEditor()->GetPersonalizationManager()->SetProjectProperty("QFilteringPanel", m_uniqueName, map);
}

void QFilteringPanel::LoadFilter(const QString& filterName)
{
	Clear();
	auto savedFiltersVariant = GetIEditor()->GetPersonalizationManager()->GetProjectProperty("QFilteringPanel", m_uniqueName);
	if (savedFiltersVariant.isValid() && savedFiltersVariant.type() == QVariant::Map)
	{
		QVariantMap map = savedFiltersVariant.toMap();
		SetFilterState(map[filterName]);
	}
}

void QFilteringPanel::RenameFilter(const char* filterName, const char* filterNewName)
{
	auto savedFiltersVariant = GetIEditor()->GetPersonalizationManager()->GetProjectProperty("QFilteringPanel", m_uniqueName);

	if (savedFiltersVariant.isValid() && savedFiltersVariant.type() == QVariant::Map)
	{
		QVariantMap map = savedFiltersVariant.toMap();
		QVariant filterState = map[filterName];
		map.remove(filterName);
		map[filterNewName] = filterState;
		GetIEditor()->GetPersonalizationManager()->SetProjectProperty("QFilteringPanel", m_uniqueName, map);
	}
}

void QFilteringPanel::DeleteFilter(const char* filterName)
{
	auto savedFiltersVariant = GetIEditor()->GetPersonalizationManager()->GetProjectProperty("QFilteringPanel", m_uniqueName);

	if (savedFiltersVariant.isValid() && savedFiltersVariant.type() == QVariant::Map)
	{
		QVariantMap map = savedFiltersVariant.toMap();
		map.remove(filterName);
		GetIEditor()->GetPersonalizationManager()->SetProjectProperty("QFilteringPanel", m_uniqueName, map);
	}
}

QVariant QFilteringPanel::GetFilterState() const
{
	QVariantList filterState;
	for (auto& filter : m_filters)
	{
		filterState.push_back(filter->GetState());
	}
	return filterState;
}

void QFilteringPanel::SetFilterState(const QVariant& state)
{
	if (m_attributes.empty())
	{
		return;
	}
	if (state.isValid() && state.type() == QVariant::List)
	{
		QVariantList filtersList = state.value<QVariantList>();
		for (QVariant& var : filtersList)
		{
			if (var.isValid() && var.type() == QVariant::Map)
			{
				auto filter = AddFilter();
				filter->SetState(var.value<QVariantMap>());
			}
		}
	}
}

const QStringList* QFilteringPanel::GetAttributeEnumEntries(const QString& attributeName)
{
	auto it = m_attributeValues.find(attributeName);
	if (it == m_attributeValues.end())
	{
		return nullptr;
	}
	return &it->second;
}

void QFilteringPanel::OnFilterChanged()
{
	if (m_timer)
	{
		m_timer->start(m_timerMsec);
	}
	else
	{
		OnSearch();
	}

	UpdateOptionsIcon();
}

void QFilteringPanel::OnSearch()
{
	if (m_timer)
		m_timer->stop();

	m_pModel->InvalidateFilter();
	signalOnFiltered();
}

void QFilteringPanel::paintEvent(QPaintEvent* pEvent)
{
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
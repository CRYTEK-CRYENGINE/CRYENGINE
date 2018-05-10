// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Commands/CVarListDockable.h"

#include <CrySystem/ISystem.h>
#include "CryIcon.h"
#include "QAdvancedItemDelegate.h"
#include "Qt/QtMainFrame.h"
#include "FileDialogs/SystemFileDialog.h"
#include "ProxyModels/DeepFilterProxyModel.h"
#include "QAdvancedTreeView.h"
#include "QSearchBox.h"
#include <QtUtil.h>
#include <QDate>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLayout>
#include <QLineEdit>
#include <QMenuBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QDir>
#include <QApplication>
#include <QClipboard>

REGISTER_HIDDEN_VIEWPANE_FACTORY(CCVarListDockable, "Console Variables", "", true);

namespace Private_CVarListDockable
{
void PyImportCVarList()
{
	CSystemFileDialog::RunParams runParams;
	runParams.title = CEditorMainFrame::tr("Import CVar List");
	runParams.initialDir = QDir(QtUtil::GetAppDataFolder()).absolutePath();
	runParams.extensionFilters << CExtensionFilter(CEditorMainFrame::tr("JSON Files"), "json");

	const QString path = CSystemFileDialog::RunImportFile(runParams, CEditorMainFrame::GetInstance());
	if (!path.isEmpty())
	{
		CCVarListDockable::LoadCVarListFromFile(path.toStdString().c_str());
	}
}

void PyExportCVarList()
{
	CSystemFileDialog::RunParams runParams;
	runParams.title = CEditorMainFrame::tr("Export CVar List");
	runParams.initialDir = QDir(QtUtil::GetAppDataFolder()).absolutePath();
	runParams.extensionFilters << CExtensionFilter(CEditorMainFrame::tr("JSON Files"), "json");

	const QString path = CSystemFileDialog::RunExportFile(runParams, CEditorMainFrame::GetInstance());
	if (!path.isEmpty())
	{
		CCVarListDockable::SaveCVarListToFile(path.toStdString().c_str());
	}
}
}

const char* CCVarModel::s_szColumnNames[] = { QT_TR_NOOP(""), QT_TR_NOOP("Variable"), QT_TR_NOOP("Value"), QT_TR_NOOP("Description"), QT_TR_NOOP("Type") };
enum ECVarColumn
{
	eCVarListColumn_Favorite,
	eCVarListColumn_Name,
	eCVarListColumn_Value,
	eCVarListColumn_Description,
	eCVarListColumn_Type,
};

class CCVarBrowser::CCVarListDelegate : public QAdvancedItemDelegate
{
public:
	CCVarListDelegate(QAbstractItemView* const pView)
		: QAdvancedItemDelegate(pView)
	{
	}

	virtual QWidget* createEditor(QWidget* pParent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		if (index.column() == eCVarListColumn_Value)
		{
			const CCVarModel::DataTypes type = static_cast<CCVarModel::DataTypes>(index.model()->data(index, CCVarModel::DataTypeRole).toInt());
			switch (type)
			{
			case CCVarModel::Int:
				{
					const auto pSpinBox = new QSpinBox(pParent);
					pSpinBox->setMaximum(std::numeric_limits<int>::max());
					pSpinBox->setMinimum(std::numeric_limits<int>::lowest());

					return pSpinBox;
				}

			case CCVarModel::Float:
				{
					const auto pSpinBox = new QDoubleSpinBox(pParent);
					pSpinBox->setMaximum(std::numeric_limits<float>::max());
					pSpinBox->setMinimum(std::numeric_limits<float>::lowest());
					pSpinBox->setSingleStep(0.1);
					pSpinBox->setDecimals(5);

					return pSpinBox;
				}

			case CCVarModel::String:
				return new QLineEdit(pParent);

			default:
				break;
			}
		}

		return QStyledItemDelegate::createEditor(pParent, option, index);
	}

	virtual void setEditorData(QWidget* pEditor, const QModelIndex& index) const override
	{
		if (index.column() == eCVarListColumn_Value)
		{
			const QVariant value = index.model()->data(index);
			const CCVarModel::DataTypes type = static_cast<CCVarModel::DataTypes>(index.model()->data(index, CCVarModel::DataTypeRole).toInt());
			switch (type)
			{
			case CCVarModel::Int:
				{
					const auto pSpinBox = static_cast<QSpinBox*>(pEditor);
					pSpinBox->setValue(value.toInt());
					return;
				}

			case CCVarModel::Float:
				{
					const auto pSpinBox = static_cast<QDoubleSpinBox*>(pEditor);
					pSpinBox->setValue(value.toDouble());
					return;
				}

			case CCVarModel::String:
				{
					const auto pLineEdit = static_cast<QLineEdit*>(pEditor);
					pLineEdit->setText(value.toString());
					return;
				}

			default:
				break;
			}
		}

		return QStyledItemDelegate::setEditorData(pEditor, index);
	}

	virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		// TODO: Cache the size somehow. Not done currently as the size is different depending on the edit control.
		auto size = QStyledItemDelegate::sizeHint(option, index);
		const auto pEditorWidget = std::unique_ptr<QWidget>(createEditor(nullptr, option, index));
		if (pEditorWidget)
		{
			if (pEditorWidget->sizeHint().height() > size.height())
			{
				size.setHeight(pEditorWidget->sizeHint().height());
			}
		}

		return size;
	}
};

class CFavoriteFilterProxy : public QDeepFilterProxyModel
{
public:
	CFavoriteFilterProxy(BehaviorFlags behavior = AcceptIfChildMatches, bool bFavoritesOnly = false, QObject* pParent = nullptr)
		: QDeepFilterProxyModel(behavior, pParent)
		, m_bIsFiltering(bFavoritesOnly)
	{
	}

	bool IsFilteringByFavorites() const
	{
		return m_bIsFiltering;
	}

	void SetFilterByFavorites(bool bFavoritesOnly)
	{
		m_bIsFiltering = bFavoritesOnly;
		GetIEditorImpl()->GetPersonalizationManager()->SetProperty("ConsoleVariables", "FavoritesOnly", m_bIsFiltering);
		this->invalidate();
	}

protected:
	virtual bool rowMatchesFilter(int sourceRow, const QModelIndex& sourceParent) const override
	{
		bool bMatchesFilter = true;
		if (IsFilteringByFavorites())
		{
			auto index = sourceModel()->index(sourceRow, eCVarListColumn_Favorite, sourceParent);
			bMatchesFilter = index.data(Qt::CheckStateRole).toBool();
		}
		return bMatchesFilter && QDeepFilterProxyModel::rowMatchesFilter(sourceRow, sourceParent);
	}

private:
	bool m_bIsFiltering;
};

CCVarModel::CCVarModel()
	: m_favHelper("CVarFavorites", eCVarListColumn_Favorite)
{
	const auto pConsole = gEnv->pConsole;
	std::vector<const char*> commands;
	commands.resize(pConsole->GetNumVars());
	const size_t cmdCount = pConsole->GetSortedVars(&commands[0], commands.size(),nullptr,1);

	m_cvars.reserve(commands.size());
	for (auto szCommand : commands)
	{
		if (szCommand && *szCommand)
			m_cvars.push_back(szCommand);
	}
	std::sort(m_cvars.begin(),m_cvars.end());

	gEnv->pConsole->AddConsoleVarSink(this);
}

CCVarModel::~CCVarModel()
{
	gEnv->pConsole->RemoveConsoleVarSink(this);
}

ICVar* CCVarModel::GetCVar(const QModelIndex& idx) const
{
	CRY_ASSERT(idx.row() < m_cvars.size());
	const char *cvarName = m_cvars[idx.row()].c_str();
	return gEnv->pConsole->GetCVar(cvarName);
}

QVariant CCVarModel::data(const QModelIndex& index, int role /*= Qt::DisplayRole*/) const
{
	CRY_ASSERT(index.row() < m_cvars.size());
	ICVar *pCVar = GetCVar(index);
	if (!pCVar)
		return QVariant();

	if (role == Qt::DisplayRole || role == Qt::ToolTipRole || role == Roles::SortRole)
	{
		switch (index.column())
		{
		case eCVarListColumn_Name:
			return pCVar->GetName();

		case eCVarListColumn_Value:
			switch (pCVar->GetType())
			{
			case CVAR_INT:
				return pCVar->GetIVal();

			case CVAR_FLOAT:
				return pCVar->GetFVal();

			case CVAR_STRING:
				return pCVar->GetString();

			default:
				return QVariant();
			}

		case eCVarListColumn_Description:
			{
				const char* szHelpText = pCVar->GetHelp();
				if (role == Qt::ToolTipRole)
					return pCVar->GetHelp();

				QString helpText = szHelpText;
				const int linebreak = helpText.indexOf('\n');
				if (linebreak > 0)
				{
					helpText.resize(linebreak);
					helpText.append(" [...]");
				}

				return helpText;
			}

		case eCVarListColumn_Type:
			{
				switch (pCVar->GetType())
				{
				case CVAR_INT:
					return "Integer";

				case CVAR_FLOAT:
					return "Floating point";

				case CVAR_STRING:
					return "String";

				default:
					return "UNKNOWN";
				}
			}

		default:
			break;
		}

		if (role == Roles::SortRole)
		{
			if (index.column() == eCVarListColumn_Favorite)
			{
				return m_favHelper.IsFavorite(index);
			}
		}
	}
	else if (role == Roles::DataTypeRole)
	{
		switch (pCVar->GetType())
		{
		case CVAR_INT:
			return DataTypes::Int;

		case CVAR_FLOAT:
			return DataTypes::Float;

		case CVAR_STRING:
			return DataTypes::String;

		default:
			return -1;
		}
	}
	else if (role == FavoritesHelper::s_FavoritesIdRole)
	{
		return pCVar->GetName();
	}

	return m_favHelper.data(index, role);
}

bool CCVarModel::hasChildren(const QModelIndex& parent /*= QModelIndex()*/) const
{
	return !parent.isValid();
}

QVariant CCVarModel::headerData(int section, Qt::Orientation orientation, int role /*= Qt::DisplayRole*/) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		return tr(s_szColumnNames[section]);
	}
	else
	{
		return QVariant();
	}
}

Qt::ItemFlags CCVarModel::flags(const QModelIndex& index) const
{
	auto itemFlags = QAbstractItemModel::flags(index);
	if (index.isValid())
	{
		if (index.column() == eCVarListColumn_Value)
			itemFlags |= Qt::ItemIsEditable;
		else if (index.column() == eCVarListColumn_Favorite)
			itemFlags |= m_favHelper.flags(index);
	}
	return itemFlags;
}

QModelIndex CCVarModel::index(int row, int column, const QModelIndex& parent /*= QModelIndex()*/) const
{
	return createIndex(row, column, nullptr);
}

QModelIndex CCVarModel::parent(const QModelIndex& index) const
{
	return QModelIndex();
}

bool CCVarModel::setData(const QModelIndex& index, const QVariant& value, int role /*= Qt::EditRole*/)
{
	if (index.isValid())
	{
		if (role == Qt::EditRole)
		{
			CRY_ASSERT(index.row() <= m_cvars.size());
			if (index.column() == eCVarListColumn_Value)
			{
				ICVar *pCVar = GetCVar(index);
				switch (pCVar->GetType())
				{
				case CVAR_INT:
				{
					const auto valueInt = value.toInt();
					pCVar->Set(valueInt);
					return true;
				}

				case CVAR_FLOAT:
				{
					const auto valueFloat = value.toFloat();
					pCVar->Set(valueFloat);
					return true;
				}

				case CVAR_STRING:
				{
					const auto valueString = value.toString().toStdString();
					pCVar->Set(valueString.c_str());
					return true;
				}

				default:
					return false;
				}
			}
		}
		else return m_favHelper.setData(index, value, role);
	}

	return false;
}

int CCVarModel::rowCount(const QModelIndex& parent /*= QModelIndex()*/) const
{
	return !parent.isValid() ? m_cvars.size() : 0;
}

void CCVarModel::OnCVarChanged(ICVar* const pCVar)
{
	string cvarname = pCVar->GetName();
	const auto it = std::lower_bound(m_cvars.cbegin(), m_cvars.cend(), cvarname); // Find by name in a sorted vector.
	if (it != m_cvars.cend())
	{
		const auto index = std::distance(m_cvars.cbegin(), it);
		const auto modelIndex = this->index(index, 0);
		const auto modelIndex2 = this->index(index+1, 0);

		QVector<int> roles;
		roles.push_back(Qt::DisplayRole);
		dataChanged(modelIndex, modelIndex2, roles);
	}
}

bool CCVarModel::OnBeforeVarChange(ICVar* pVar, const char* sNewValue)
{
	return true;
}

void CCVarModel::OnAfterVarChange(ICVar* pVar)
{
	OnCVarChanged(pVar);
}
//////////////////////////////////////////////////////////////////////////

CCVarBrowser::CCVarBrowser(QWidget* pParent /* = nullptr*/)
	: m_pModel(new CCVarModel())
{
	const bool bFavoritesOnly = GetIEditorImpl()->GetPersonalizationManager()->GetProperty("ConsoleVariables", "FavoritesOnly").toBool();

	const CFavoriteFilterProxy::BehaviorFlags behavior = CFavoriteFilterProxy::AcceptIfChildMatches | CFavoriteFilterProxy::AcceptIfParentMatches;
	m_pFilterProxy = std::unique_ptr<CFavoriteFilterProxy>(new CFavoriteFilterProxy(behavior, bFavoritesOnly));
	m_pFilterProxy->setSourceModel(m_pModel.get());
	m_pFilterProxy->setSortRole(static_cast<int>(CCVarModel::Roles::SortRole));
	m_pFilterProxy->setFilterKeyColumn(eCVarListColumn_Name);

	const auto pSearchBox = new QSearchBox(this);
	pSearchBox->SetModel(m_pFilterProxy.get());
	pSearchBox->EnableContinuousSearch(true);

	const auto pFavoriteToggle = new QToolButton(this);
	pFavoriteToggle->setIcon(FavoritesHelper::GetFavoriteIcon(bFavoritesOnly));

	const auto treeBehavior = static_cast<QAdvancedTreeView::Behavior>(QAdvancedTreeView::PreserveExpandedAfterReset | QAdvancedTreeView::PreserveSelectionAfterReset);
	m_pTreeView = new QAdvancedTreeView(treeBehavior, this);
	m_pTreeView->setModel(m_pFilterProxy.get());
	m_pTreeView->setSortingEnabled(true);
	m_pTreeView->sortByColumn(eCVarListColumn_Name, Qt::AscendingOrder);
	m_pTreeView->setAllColumnsShowFocus(true);
	m_pTreeView->header()->setStretchLastSection(true);
	m_pTreeView->header()->setDefaultSectionSize(300);
	m_pTreeView->setEditTriggers(QTreeView::DoubleClicked | QTreeView::EditKeyPressed | QTreeView::SelectedClicked | QTreeView::CurrentChanged);
	m_pTreeView->setColumnWidth(eCVarListColumn_Favorite, 20);
	m_pTreeView->setColumnWidth(eCVarListColumn_Value, 75);
	m_pTreeView->setColumnWidth(eCVarListColumn_Type, 75);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->expandAll();
	m_pTreeView->setRootIsDecorated(false);

	pSearchBox->SetAutoExpandOnSearch(m_pTreeView);

	auto pDelegate = new CCVarListDelegate(m_pTreeView);
	pDelegate->SetColumnBehavior(eCVarListColumn_Favorite, QAdvancedItemDelegate::BehaviorFlags(QAdvancedItemDelegate::OverrideCheckIcon));
	m_pTreeView->setItemDelegate(pDelegate);

	const auto pHLayout = new QHBoxLayout();
	pHLayout->setContentsMargins(0, 0, 0, 0);
	pHLayout->addWidget(pSearchBox);
	pHLayout->addWidget(pFavoriteToggle);

	const auto pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(0, 0, 0, 0);
	pLayout->addLayout(pHLayout);
	pLayout->addWidget(m_pTreeView);
	setLayout(pLayout);

	connect(m_pTreeView, &QAbstractItemView::clicked, [=](const QModelIndex& index)
	{
		QModelIndex mappedIndex = m_pFilterProxy->mapToSource(index);
		if (!index.isValid())
			return;

		ICVar *pCVar = m_pModel->GetCVar(mappedIndex);
		if (pCVar)
		{
			signalOnClick(pCVar->GetName());
		}
	});

	connect(m_pTreeView, &QAbstractItemView::doubleClicked, [=](const QModelIndex& index)
	{
		QModelIndex mappedIndex = m_pFilterProxy->mapToSource(index);
		if (!index.isValid())
			return;

		ICVar *pCVar = 	m_pModel->GetCVar(mappedIndex);
		if (pCVar)
		{
			signalOnDoubleClick(pCVar->GetName());
		}
	});

	connect(pFavoriteToggle, &QToolButton::clicked, [=](bool bChecked = false)
	{
		bool filtering = m_pFilterProxy->IsFilteringByFavorites();
		m_pFilterProxy->SetFilterByFavorites(!filtering);
		pFavoriteToggle->setIcon(FavoritesHelper::GetFavoriteIcon(!filtering));
	});
}

QStringList CCVarBrowser::GetSelectedCVars() const
{
	QModelIndexList indexes = m_pTreeView->selectionModel()->selectedIndexes();

	QStringList selectedCVars;
	for (QModelIndex index : indexes)
	{
		if (index.column() != 0)
			continue;

		QModelIndex mappedIndex = m_pFilterProxy->mapToSource(index);
		if (!index.isValid())
			continue;

		ICVar* pCVar = m_pModel->GetCVar(mappedIndex);
		if (pCVar)
		{
			selectedCVars << pCVar->GetName();
		}
	}
	return selectedCVars;
}

//////////////////////////////////////////////////////////////////////////

CCVarBrowserDialog::CCVarBrowserDialog()
	: CEditorDialog("CVar Browser")
{
	CCVarBrowser* pCVarBrowser = new CCVarBrowser(this);
	pCVarBrowser->signalOnDoubleClick.Connect([this](const char* cvarName)
	{
		CVarsSelected(QStringList(QString(cvarName)));
	});

	QPushButton* pOkButton = new QPushButton(this);
	pOkButton->setText(tr("Ok"));
	pOkButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
	pOkButton->setDefault(true);

	connect(pOkButton, &QPushButton::clicked, [this, pCVarBrowser]()
	{
		const QStringList& selectedResources = pCVarBrowser->GetSelectedCVars();
		if (selectedResources.isEmpty())
		{
		  reject();
		  return;
		}

		CVarsSelected(selectedResources);
	});

	QPushButton* pCancelButton = new QPushButton(this);
	pCancelButton->setText(tr("Cancel"));
	pCancelButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
	connect(pCancelButton, &QPushButton::clicked, [this]()
	{
		reject();
	});

	QHBoxLayout* pActionLayout = new QHBoxLayout();
	pActionLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
	pActionLayout->addWidget(pOkButton, 0, Qt::AlignRight);
	pActionLayout->addWidget(pCancelButton, 0, Qt::AlignRight);

	QVBoxLayout* pMainLayout = new QVBoxLayout();
	pMainLayout->addWidget(pCVarBrowser);
	pMainLayout->addLayout(pActionLayout);
	setLayout(pMainLayout);
}

void CCVarBrowserDialog::CVarsSelected(const QStringList& cvars)
{
	m_selectedCVars = cvars;
	accept();
}

//////////////////////////////////////////////////////////////////////////

CCVarListDockable::CCVarListDockable(QWidget* const pParent)
	: CDockableEditor(pParent)
{
	const auto pMenuBar = new QMenuBar();
	const auto pFileMenu = pMenuBar->addMenu("&File");
	const auto pImportAction = pFileMenu->addAction("&Import...");
	const auto pExportAction = pFileMenu->addAction("&Export...");

	QObject::connect(pImportAction, &QAction::triggered, &Private_CVarListDockable::PyImportCVarList);
	QObject::connect(pExportAction, &QAction::triggered, &Private_CVarListDockable::PyExportCVarList);

	const auto pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(1, 1, 1, 1);
	m_pCVarBrowser = new CCVarBrowser(this);
	pLayout->setMenuBar(pMenuBar);
	pLayout->addWidget(m_pCVarBrowser);
	SetContent(pLayout);
}

CCVarListDockable::~CCVarListDockable()
{
}

bool CCVarListDockable::OnCopy()
{
	auto selectedCVars = m_pCVarBrowser->GetSelectedCVars();
	if (selectedCVars.isEmpty())
		return false;

	QString clipBoardData;
	for (auto i = 0; i < selectedCVars.size() - 1; ++i)
	{
		clipBoardData += selectedCVars[i] + " ";
	}
	clipBoardData += selectedCVars[selectedCVars.size() - 1];

	QApplication::clipboard()->setText(clipBoardData);
	return true;
}

QVariant CCVarListDockable::GetState()
{
	const auto pConsole = gEnv->pConsole;
	std::vector<const char*> cvars;
	cvars.resize(pConsole->GetNumVars());
	const size_t numCVars = pConsole->GetSortedVars(&cvars[0], cvars.size(), nullptr, 1);
	cvars.resize(numCVars); // We might have less CVars than we were expecting (invisible ones, etc.)

	QVariantMap map;
	for (auto szCVar : cvars)
	{
		CRY_ASSERT(szCVar);
		ICVar* pCVar = pConsole->GetCVar(szCVar);
		if (pCVar)
		{
			switch (pCVar->GetType())
			{
			case CVAR_INT:
				map.insert(pCVar->GetName(), pCVar->GetIVal());
				break;

			case CVAR_FLOAT:
				map.insert(pCVar->GetName(), pCVar->GetFVal());
				break;

			case CVAR_STRING:
				map.insert(pCVar->GetName(), pCVar->GetString());
				break;

			default:
				CRY_ASSERT(false);
				break;
			}
		}
	}

	return map;
}

void CCVarListDockable::SetState(const QVariant& state)
{
	if (!state.isValid() || state.type() != QVariant::Map)
	{
		return;
	}

	QVariantMap map = state.value<QVariantMap>();
	for (auto it = map.cbegin(); it != map.cend(); ++it)
	{
		const auto cvar = it.key().toStdString();
		ICVar* pCVar = gEnv->pConsole->GetCVar(cvar.c_str());
		if (pCVar)
		{
			switch (it.value().type())
			{
			case QVariant::Int:
				pCVar->Set(it.value().toInt());
				break;

			case QVariant::Double:
				pCVar->Set(it.value().toFloat());
				break;

			case QVariant::String:
				pCVar->Set(it.value().toString().toStdString().c_str());
				break;

			default:
				break;
			}
		}
	}
}

void CCVarListDockable::LoadCVarListFromFile(const char* szFilename)
{
	QFile file(szFilename);
	if (!file.open(QIODevice::ReadOnly))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to open path: %s", szFilename);
		return;
	}

	QJsonDocument doc(QJsonDocument::fromJson(file.readAll()));
	QVariant cvars = doc.toVariant();

	if (cvars.isValid())
	{
		SetState(cvars);
	}
	//TODO : error message
}

void CCVarListDockable::SaveCVarListToFile(const char* szFilename)
{
	QFile file(szFilename);
	if (!file.open(QIODevice::WriteOnly))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to open path: %s", szFilename);
		return;
	}

	QJsonDocument doc(QJsonDocument::fromVariant(GetState()));
	file.write(doc.toJson());
}


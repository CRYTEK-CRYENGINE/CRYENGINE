// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "Commands/CommandListDockable.h"

#include "ProxyModels/DeepFilterProxyModel.h"
#include "QAdvancedTreeView.h"
#include "QSearchBox.h"
#include <QHeaderView>
#include <QLayout>

REGISTER_HIDDEN_VIEWPANE_FACTORY(CCommandListDockable, "Console Commands", "", true);

const char* CCommandListModel::s_ColumnNames[] = { QT_TR_NOOP("Command"), QT_TR_NOOP("Description") };
enum ECommandListColumn
{
	eCommandListColumn_Name,
	eCommandListColumn_Description,
};

enum ECommandListDepth
{
	eCommandListDepth_Module,
	eCommandListDepth_Command,
	eCommandListDepth_Argument,
};

ECommandListDepth GetIndexDepth(const QModelIndex& index)
{
	size_t depth = 0;
	for (auto it = index; it.isValid(); ++depth)
	{
		it = it.parent();
	}
	return static_cast<ECommandListDepth>(depth - 1);
}

CCommandListModel::CCommandListModel()
{
}

CCommandListModel::~CCommandListModel()
{
}

void CCommandListModel::Initialize()
{
	Rebuild();
}

QVariant CCommandListModel::data(const QModelIndex& index, int role) const
{
	// Do not display icons
	if (role == Qt::DecorationRole)
	{
		return QVariant();
	}

	const auto depth = GetIndexDepth(index);
	switch (depth)
	{
	case eCommandListDepth_Module:
		{
			// Base class doesn't know about our SortRole
			if (role == CCommandListModel::SortRole)
			{
				role = Qt::DisplayRole;
			}
			break;
		}

	case eCommandListDepth_Command:
		{
			const QModelIndex parent = index.parent();
			if (role == Qt::DisplayRole || role == CCommandListModel::SortRole)
			{
				switch (index.column())
				{
				case eCommandListColumn_Name:
					return CommandModel::data(this->index(index.row(), 1, parent), Qt::DisplayRole);

				case eCommandListColumn_Description:
					return CommandModel::data(this->index(index.row(), 0, parent), static_cast<int>(CommandModel::Roles::CommandDescriptionRole));

				default:
					break;
				}
			}

			break;
		}

	case eCommandListDepth_Argument:
		{
			const QModelIndex parent = index.parent();
			const QModelIndex grandParent = parent.parent();
			const auto& parameters = m_modules[grandParent.row()].m_commands[parent.row()]->GetParameters();
			if (role == Qt::DisplayRole)
			{
				switch (index.column())
				{
				case eCommandListColumn_Name:
					return parameters[index.row()].GetName().c_str();

				case eCommandListColumn_Description:
					return parameters[index.row()].GetDescription().c_str();

				default:
					return QVariant();
				}
			}
			else if (role == CCommandListModel::SortRole)
			{
				return parameters[index.row()].GetIndex();
			}
			else if (role == static_cast<int>(CommandModel::Roles::SearchRole))
			{
				return QVariant();
			}
		}

	default:
		break;
	}

	return CommandModel::data(index, role);
}

Qt::ItemFlags CCommandListModel::flags(const QModelIndex& index) const
{
	return QAbstractItemModel::flags(index);
}

bool CCommandListModel::hasChildren(const QModelIndex& parent) const
{
	const auto depth = GetIndexDepth(parent);
	if (depth == eCommandListDepth_Command)
	{
		const auto grandParent = parent.parent();
		return !m_modules[grandParent.row()].m_commands[parent.row()]->GetParameters().empty();
	}
	else if (depth == eCommandListDepth_Argument)
	{
		return false;
	}

	return CommandModel::hasChildren(parent);
}

QVariant CCommandListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		return tr(s_ColumnNames[section]);
	}

	return QVariant();
}

QModelIndex CCommandListModel::index(int row, int column, const QModelIndex& parent) const
{
	if (GetIndexDepth(parent) == eCommandListDepth_Command)
	{
		const auto grandParent = parent.parent();
		const auto& parameters = m_modules[grandParent.row()].m_commands[parent.row()]->GetParameters();
		return createIndex(row, column, reinterpret_cast<quintptr>(&parameters[row]));
	}

	return CommandModel::index(row, column, parent);
}

QModelIndex CCommandListModel::parent(const QModelIndex& index) const
{
	const auto it = std::lower_bound(m_params.cbegin(), m_params.cend(), index.internalPointer());
	if (it != m_params.cend() && *it == index.internalPointer())
	{
		const auto pParam = reinterpret_cast<CCommandArgument*>(index.internalPointer());
		return GetIndex(static_cast<CUiCommand*>(pParam->GetCommand()));
	}

	return CommandModel::parent(index);
}

int CCommandListModel::rowCount(const QModelIndex& parent) const
{
	const auto depth = GetIndexDepth(parent);
	if (depth == eCommandListDepth_Command)
	{
		const auto grandParent = parent.parent();
		return m_modules[grandParent.row()].m_commands[parent.row()]->GetParameters().size();
	}
	else if (depth == eCommandListDepth_Argument)
	{
		return 0;
	}

	return CommandModel::rowCount(parent);
}

void CCommandListModel::Rebuild()
{
	beginResetModel();

	m_modules.clear();
	m_params.clear();

	std::vector<CCommand*> commands;
	GetIEditorImpl()->GetCommandManager()->GetCommandList(commands);

	for (const auto& command : commands)
	{
		auto& module = FindOrCreateModule(command->GetModule().c_str());
		module.m_commands.push_back(command);

		const auto& params = command->GetParameters();
		for (const auto& param : params)
		{
			m_params.push_back(&param);
		}
	}
	std::sort(m_params.begin(), m_params.end());

	endResetModel();
}

class CCommandListSortProxyModel : public QDeepFilterProxyModel
{
public:
	CCommandListSortProxyModel(const BehaviorFlags behavior = AcceptIfChildMatches, QObject* pParent = nullptr)
		: QDeepFilterProxyModel(behavior, pParent)
	{
	}

	virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
	{
		CRY_ASSERT(GetIndexDepth(left) == GetIndexDepth(right));

		if (GetIndexDepth(left) == eCommandListDepth_Argument)
		{
			const auto leftIndex = sourceModel()->data(left, sortRole()).toInt();
			const auto rightIndex = sourceModel()->data(right, sortRole()).toInt();

			// We always want to display arguments in same same order
			return sortOrder() == Qt::AscendingOrder ? leftIndex<rightIndex : leftIndex> rightIndex;
		}

		return QDeepFilterProxyModel::lessThan(left, right);
	}
};

//////////////////////////////////////////////////////////////////////////

CCommandListDockable::CCommandListDockable(QWidget* const pParent)
	: CDockableWidget(pParent)
{
	m_pModel = std::unique_ptr<CommandModel>(CommandModelFactory::Create<CCommandListModel>());

	const QDeepFilterProxyModel::BehaviorFlags behavior = QDeepFilterProxyModel::AcceptIfChildMatches | QDeepFilterProxyModel::AcceptIfParentMatches;
	const auto pFilterProxy = new CCommandListSortProxyModel(behavior);
	pFilterProxy->setSourceModel(m_pModel.get());
	pFilterProxy->setFilterKeyColumn(eCommandListColumn_Name);
	pFilterProxy->setFilterRole(static_cast<int>(CommandModel::Roles::SearchRole));
	pFilterProxy->setSortRole(CCommandListModel::SortRole);

	const auto pSearchBox = new QSearchBox();
	pSearchBox->SetModel(pFilterProxy);
	pSearchBox->EnableContinuousSearch(true);

	m_pTreeView = new QAdvancedTreeView();
	m_pTreeView->setModel(pFilterProxy);
	m_pTreeView->setSortingEnabled(true);
	m_pTreeView->sortByColumn(eCommandListColumn_Name, Qt::AscendingOrder);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->setAllColumnsShowFocus(true);
	m_pTreeView->header()->setStretchLastSection(true);
	m_pTreeView->header()->setDefaultSectionSize(300);
	m_pTreeView->expandToDepth(eCommandListDepth_Module);

	pSearchBox->SetAutoExpandOnSearch(m_pTreeView);

	const auto pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(1, 1, 1, 1);
	pLayout->addWidget(pSearchBox);
	pLayout->addWidget(m_pTreeView);
	setLayout(pLayout);
}

CCommandListDockable::~CCommandListDockable()
{
}


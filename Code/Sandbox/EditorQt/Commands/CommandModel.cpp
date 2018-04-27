// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CommandModel.h"
#include "QCommandAction.h"
#include "CustomCommand.h"
#include "QtUtil.h"

const char* CommandModel::s_ColumnNames[3] = { QT_TR_NOOP("Action"), QT_TR_NOOP("Command"), QT_TR_NOOP("Shortcut") };

CommandModel::CommandModel()
{

}

CommandModel::~CommandModel()
{
	GetIEditorImpl()->GetCommandManager()->signalChanged.DisconnectObject(this);
}

void CommandModel::Initialize()
{
	GetIEditorImpl()->GetCommandManager()->signalChanged.Connect(this, &CommandModel::Rebuild);
	Rebuild();
}

void CommandModel::Rebuild()
{
	beginResetModel();

	m_modules.clear();

	std::vector<CCommand*> cmds;
	GetIEditorImpl()->GetCommandManager()->GetCommandList(cmds);

	for (CCommand* cmd : cmds)
	{
		if (cmd->CanBeUICommand())
		{
			CUiCommand* uiCmd = static_cast<CUiCommand*>(cmd);

			CommandModule& module = FindOrCreateModule(cmd->GetModule().c_str());
			module.m_commands.push_back(uiCmd);
		}
	}

	endResetModel();
}

QVariant CommandModel::data(const QModelIndex& index, int role /* = Qt::DisplayRole */) const
{
	if (!index.isValid())
	{
		return QVariant();
	}

	switch (role)
	{
	case Qt::DecorationRole:
		{
			// display icon only for the first column
			if (index.column())
				return QVariant();

			QModelIndex parent = index.parent();

			if (!parent.isValid())
				return QVariant();

			const auto pCommand = static_cast<CUiCommand*>(m_modules[parent.row()].m_commands[index.row()]);
			CUiCommand::UiInfo* pInfo = pCommand->GetUiInfo();
			if (!pInfo)
				return QVariant();

			QCommandAction* pAction = static_cast<QCommandAction*>(pInfo);
			return pAction->QAction::icon();
		}
		break;
	case Qt::DisplayRole:
	case Qt::EditRole:
		{
			QModelIndex parent = index.parent();
			if (parent.isValid())
			{
				//command item
				switch (index.column())
				{
				case 0:
					{
						string name = m_modules[parent.row()].m_commands[index.row()]->GetName();
						return QString(m_modules[parent.row()].m_desc->FormatCommandForUI(name));
					}

				case 1:
					{
						return m_modules[parent.row()].m_commands[index.row()]->GetCommandString().c_str();
					}

				default:
					return QVariant();
				}
			}
			else
			{
				//top level item
				switch (index.column())
				{
				case 0:
					return m_modules[index.row()].m_desc->GetUiName().c_str();

				case 1:
					return m_modules[index.row()].m_desc->GetDescription().c_str();

				default:
					return QVariant();
				}
			}
		}
	case Roles::CommandDescriptionRole:
		{
			QModelIndex parent = index.parent();
			if (parent.isValid())
			{
				return QtUtil::ToQString(m_modules[parent.row()].m_commands[index.row()]->GetDescription());
			}
			return QVariant();
		}
	case Qt::ToolTipRole:
		{
			QModelIndex parent = index.parent();
			if (parent.isValid())
			{
				if (index.column() == 0)
				{
					return QtUtil::ToQString(m_modules[parent.row()].m_commands[index.row()]->GetDescription());
				}
				else
				{
					return QVariant();
				}
			}
			return QVariant();
		}
		break;
	case Roles::SearchRole:
		{
			QModelIndex parent = index.parent();
			if (parent.isValid())
			{
				return m_modules[parent.row()].m_commands[index.row()]->GetCommandString().c_str();
			}
			else
			{
				return m_modules[index.row()].m_name;
			}
		}
		break;
	case Roles::CommandPointerRole:
		{
			return QVariant::fromValue((CCommand*)GetCommand(index));
		}
		break;
	default:
		break;
	}

	return QVariant();
}

bool CommandModel::hasChildren(const QModelIndex& parent /* = QModelIndex() */) const
{
	if (!parent.isValid())
	{
		return true;
	}
	else
	{
		return !parent.parent().isValid();
	}
}

QVariant CommandModel::headerData(int section, Qt::Orientation orientation, int role /* = Qt::DisplayRole */) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		return tr(s_ColumnNames[section]);
	}
	else
	{
		return QVariant();
	}
}

Qt::ItemFlags CommandModel::flags(const QModelIndex& index) const
{
	if (index.parent().isValid())
	{
		const auto pCommand = GetCommand(index);
		if (pCommand->IsCustomCommand() || index.column() == 2)
		{
			return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
		}
	}

	return QAbstractItemModel::flags(index);
}

QModelIndex CommandModel::index(int row, int column, const QModelIndex& parent /*= QModelIndex()*/) const
{
	if (parent.isValid())
	{
		if (parent.row() >= m_modules.size() || row >= m_modules[parent.row()].m_commands.size())
		{
			return QModelIndex();
		}
		else
		{
			return createIndex(row, column, reinterpret_cast<quintptr>(m_modules[parent.row()].m_commands[row]));
		}
	}
	else
	{
		if (row >= m_modules.size())
		{
			return QModelIndex();
		}
		else
		{
			return createIndex(row, column, reinterpret_cast<quintptr>(m_modules[row].m_desc));
		}
	}
}

QModelIndex CommandModel::parent(const QModelIndex& index) const
{
	{
		const CCommandModuleDescription* desc = reinterpret_cast<const CCommandModuleDescription*>(index.internalPointer());

		for (int i = 0; i < m_modules.size(); ++i)
		{
			if (desc == m_modules[i].m_desc)
			{
				return QModelIndex();
			}
		}
	}

	{
		CUiCommand* cmd = reinterpret_cast<CUiCommand*>(index.internalPointer());
		QString moduleName(cmd->GetModule().c_str());

		for (int i = 0; i < m_modules.size(); ++i)
		{
			if (m_modules[i].m_name == moduleName)
			{
				return CommandModel::index(i, 0);
			}
		}
	}

	return QModelIndex();
}

bool CommandModel::setData(const QModelIndex& index, const QVariant& value, int role /*= Qt::EditRole*/)
{
	//no need to notify of the change here because there can only be one view of this model
	if (role == Qt::EditRole)
	{
		QString str = value.toString();

		QModelIndex parent = index.parent();
		if (parent.isValid())
		{
			switch (index.column())
			{
			case 0: //name
				{
					const auto pCommand = GetCommand(index);
					if (pCommand->IsCustomCommand())
					{
						const auto pCustomCommand = static_cast<CCustomCommand*>(pCommand);
						pCustomCommand->SetName(str.toStdString().c_str());
					}
				}
				break;
			case 1: //command
				{
					const auto pCommand = GetCommand(index);
					if (pCommand->IsCustomCommand())
					{
						const auto pCustomCommand = static_cast<CCustomCommand*>(pCommand);
						pCustomCommand->SetCommandString(str.toStdString().c_str());
					}
				}
				break;
			case 2: //shortcut
				{
					return false;
				}
				break;
			default:
				return false;
			}
			// emit event to notify of change
			QVector<int> roles;
			roles.push_back(role);
			dataChanged(index, index, roles);
			return true;
		}
	}
	return false;
}

int CommandModel::rowCount(const QModelIndex& parent /* = QModelIndex() */) const
{
	if (parent.isValid())
	{
		return m_modules[parent.row()].m_commands.size();
	}
	else
	{
		return m_modules.size();
	}
}

QModelIndex CommandModel::GetIndex(CCommand* command, uint column /*= 0*/) const
{
	QString moduleName(command->GetModule().c_str());

	for (int i = 0; i < m_modules.size(); ++i)
	{
		if (m_modules[i].m_name == moduleName)
		{
			for (int j = 0; j < m_modules[i].m_commands.size(); ++j)
			{
				if (m_modules[i].m_commands[j] == command)
				{
					return CommandModel::index(j, column, CommandModel::index(i, column));
				}
			}
		}
	}

	return QModelIndex();
}

CCommand* CommandModel::GetCommand(const QModelIndex& index) const
{
	if (index.isValid())
	{
		QModelIndex parent = index.parent();
		if (parent.isValid())
		{
			return m_modules[parent.row()].m_commands[index.row()];
		}
	}

	return nullptr;
}

QCommandAction* CommandModel::GetAction(uint moduleIndex, uint commandIndex) const
{
	const auto pCommand = static_cast<CUiCommand*>(m_modules[moduleIndex].m_commands[commandIndex]);
	return static_cast<QCommandAction*>(pCommand->GetUiInfo());
}

CommandModel::CommandModule& CommandModel::FindOrCreateModule(const QString& moduleName)
{
	CommandModule module;
	module.m_name = moduleName;
	module.m_desc = GetIEditorImpl()->GetCommandManager()->GetCommandModuleDescription(moduleName.toStdString().c_str());

	auto it = std::lower_bound(m_modules.begin(), m_modules.end(), module, [](const CommandModule& a, const CommandModule& b) { return a.m_name < b.m_name; });
	if (it != m_modules.end() && it->m_name == moduleName)
	{
		return *it;
	}
	else
	{
		auto inserted = m_modules.insert(it, module);
		return *inserted;
	}
}


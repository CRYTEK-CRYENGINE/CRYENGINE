// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>

#include "KeybindEditor.h"
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAbstractItemModel>
#include <QPushButton>
#include <QHeaderView>
#include <QAction>
#include <QKeySequence>
#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QKeyEvent>
#include <QSortFilterProxyModel>
#include <QJsonDocument>
#include <QKeySequenceEdit>
#include <QMenu>
#include <QToolButton>
#include <QDir>

#include "Command.h"
#include "Commands/CommandModel.h"
#include "Commands/CommandManager.h"
#include "Commands/CustomCommand.h"
#include "QCommandAction.h"
#include "QSearchBox.h"
#include "QAdvancedTreeView.h"
#include "ProxyModels/DeepFilterProxyModel.h"
#include "Util/BoostPythonHelpers.h"
#include "Qt/QtUtil.h"
#include "QT/QtMainFrame.h"
#include "FileDialogs/SystemFileDialog.h"
#include "QtViewPane.h"
#include <QtUtil.h>
#include <CryIcon.h>
#include <EditorStyleHelper.h>
#include <CrySystem/IProjectManager.h>

REGISTER_HIDDEN_VIEWPANE_FACTORY(CKeybindEditor, "Keyboard Shortcuts", "Settings", true)

namespace Private_KeybindEditor
{
QString KeySequenceListToString(const QList<QKeySequence>& list)
{
	QString string(QKeySequence::listToString(list, QKeySequence::NativeText));

#if CRY_PLATFORM_WINDOWS
	string.replace("Meta", "Win");
#endif
	return string;
}

QList<QKeySequence> StringToKeySequenceList(const QString& string)
{
#if CRY_PLATFORM_WINDOWS
	QString formatted(string);
	formatted.replace("Win", "Meta");
	return QKeySequence::listFromString(formatted, QKeySequence::NativeText);
#else
	return QKeySequence::listFromString(string, QKeySequence::NativeText);
#endif

}

QString KeySequenceToString(const QKeySequence& sequence)
{
	QString string(sequence.toString(QKeySequence::NativeText));

#if CRY_PLATFORM_WINDOWS
	string.replace("Meta", "Win");
#endif
	return string;
}

QKeySequence StringToKeySequence(const QString& string)
{
#if CRY_PLATFORM_WINDOWS
	QString formatted(string);
	formatted.replace("Win", "Meta");
	return QKeySequence(formatted, QKeySequence::NativeText);
#else
	return QKeySequence(string, QKeySequence::NativeText);
#endif
}

void PySetKeybind(const char* command, const char* shortcut)
{
	CEditorCommandManager* comMgr = GetIEditorImpl()->GetCommandManager();
	QCommandAction* action = comMgr->GetCommandAction(command);
	if (action)
	{
		action->setShortcuts(Private_KeybindEditor::StringToKeySequenceList(shortcut));
		comMgr->signalChanged();  //TODO : better way to notify changes
	}
}

void PyResetAllKeybinds()
{
	GetIEditorImpl()->GetCommandManager()->ResetAllShortcuts();
}

void PyResetKeybind(const char* command)
{
	GetIEditorImpl()->GetCommandManager()->ResetShortcut(command);
}

void PyAddCustomCommand(const char* uiName = nullptr, const char* command = nullptr, const char* shortcut = nullptr)
{
	CCustomCommand* customCommand = new CCustomCommand(uiName, command);
	QCommandAction* action = new QCommandAction(*customCommand);
	action->setShortcuts(CKeyboardShortcut(shortcut).ToKeySequence());
	customCommand->SetUiInfo(action);
	customCommand->Register();
}

void PyRemoveCustomCommand(const char* command)
{
	CCommand* cmd = GetIEditorImpl()->GetCommandManager()->GetCommand(command);
	if (cmd && cmd->IsCustomCommand())
	{
		CCustomCommand* pCustomCommand = static_cast<CCustomCommand*>(cmd);
		pCustomCommand->Unregister();
	}
}

void PyImportKeybinds()
{
	CSystemFileDialog::RunParams runParams;
	runParams.title = CEditorMainFrame::tr("Import Keybinds");
	runParams.initialDir = QDir(QtUtil::GetAppDataFolder()).absolutePath();
	runParams.extensionFilters << CExtensionFilter(CEditorMainFrame::tr("JSON Files"), "json");

	const QString path = CSystemFileDialog::RunImportFile(runParams, CEditorMainFrame::GetInstance());
	if (!path.isEmpty())
	{
		CKeybindEditor::LoadKeybindsFromFile(path.toStdString().c_str());
	}
}

void PyExportKeybinds()
{
	CSystemFileDialog::RunParams runParams;
	runParams.title = CEditorMainFrame::tr("Export Keybinds");
	runParams.initialDir = QDir(QtUtil::GetAppDataFolder()).absolutePath();
	runParams.extensionFilters << CExtensionFilter(CEditorMainFrame::tr("JSON Files"), "json");

	const QString path = CSystemFileDialog::RunExportFile(runParams, CEditorMainFrame::GetInstance());
	if (!path.isEmpty())
	{
		CKeybindEditor::SaveKeybindsToFile(path.toStdString().c_str());
	}
}
}

DECLARE_PYTHON_MODULE(keybind);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_KeybindEditor::PySetKeybind, keybind, set,
                                     "Assign shortcut to a command",
                                     "keybind.set(str command, str shortcut)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_KeybindEditor::PyResetAllKeybinds, keybind, reset_all,
                                     "Reset All to Default",
                                     "keybind.reset_all()");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_KeybindEditor::PyResetKeybind, keybind, reset,
                                     "Reset keybind to default for a command",
                                     "keybind.reset(str commandFullName)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_KeybindEditor::PyAddCustomCommand, keybind, add_custom_command,
                                     "Creates a new custom command binds it to the shortcut",
                                     "keybind.add_custom_command(str uiName, std command, str shortcut)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_KeybindEditor::PyRemoveCustomCommand, keybind, remove_custom_command,
                                     "Creates a new custom command binds it to the shortcut",
                                     "keybind.remove_custom_command(str command)");

class CKeybindEditor::KeybindModel : public CommandModel
{
	friend CommandModelFactory;

public:
	virtual ~KeybindModel();

	virtual void Initialize();

	//QAbstractItemModel implementation begin
	virtual QVariant data(const QModelIndex& index, int role /* = Qt::DisplayRole */) const override;
	virtual bool     setData(const QModelIndex& index, const QVariant& value, int role /*= Qt::EditRole*/) override;
	//QAbstractItemModel implementation end

protected:
	KeybindModel();

	typedef std::map<QKeySequence, std::vector<CUiCommand*>> ConflictsMap;

	virtual void Rebuild() override;

	void         UpdateConflicts(CUiCommand* command, QList<QKeySequence> oldShortcuts);
	bool         HasConflicts(CUiCommand* command, ConflictsMap* conflictsOut = nullptr) const;

protected:
	ConflictsMap m_conflicts;
};

//////////////////////////////////////////////////////////////////////////

CKeybindEditor::KeybindModel::KeybindModel()
{
	GetIEditorImpl()->GetCommandManager()->signalChanged.Connect(this, &KeybindModel::Rebuild);
	Rebuild();
}

CKeybindEditor::KeybindModel::~KeybindModel()
{
	GetIEditorImpl()->GetCommandManager()->signalChanged.DisconnectObject(this);
}

void CKeybindEditor::KeybindModel::Initialize()
{
	GetIEditorImpl()->GetCommandManager()->signalChanged.Connect(this, &KeybindModel::Rebuild);
	Rebuild();
}

void CKeybindEditor::KeybindModel::Rebuild()
{
	CommandModel::Rebuild();

	m_conflicts.clear();

	std::vector<CCommand*> cmds;
	GetIEditorImpl()->GetCommandManager()->GetCommandList(cmds);

	for (CCommand* cmd : cmds)
	{
		if (cmd->CanBeUICommand())
		{
			CUiCommand* uiCmd = static_cast<CUiCommand*>(cmd);
			QCommandAction* action = static_cast<QCommandAction*>(uiCmd->GetUiInfo());
			for (const QKeySequence& seq : action->shortcuts())
			{
				m_conflicts[seq].push_back(uiCmd);
			}
		}
	}
}

void CKeybindEditor::KeybindModel::UpdateConflicts(CUiCommand* command, QList<QKeySequence> oldShortcuts)
{
	QList<QKeySequence> shortcuts = static_cast<QCommandAction*>(command->GetUiInfo())->shortcuts();

	bool bChanged = false;
	QSet<CCommand*> changedCommands;

	for (const QKeySequence& seq : oldShortcuts)
	{
		if (!shortcuts.contains(seq))
		{
			std::vector<CUiCommand*>& conflictingCommands = m_conflicts[seq];

			auto it = std::find(conflictingCommands.begin(), conflictingCommands.end(), command);
			if (it != conflictingCommands.end())
			{
				conflictingCommands.erase(it);

				changedCommands.insert(command);

				if (conflictingCommands.size() == 1)
					changedCommands.insert(conflictingCommands[0]);
			}
		}
		else
		{
			shortcuts.removeAll(seq);
		}
	}

	for (const QKeySequence& seq : shortcuts)
	{
		std::vector<CUiCommand*>& conflictingCommands = m_conflicts[seq];

		if (!conflictingCommands.empty())
		{
			changedCommands.insert(command);
		}

		if (conflictingCommands.size() == 1)
		{
			changedCommands.insert(conflictingCommands[0]);
		}

		conflictingCommands.push_back(command);
	}

	if (!changedCommands.isEmpty())
	{
		for (CCommand* changedCmd : changedCommands)
		{
			QModelIndex index = GetIndex(changedCmd);

			dataChanged(createIndex(index.row(), 0, index.internalId()), createIndex(index.row(), s_ColumnCount - 1, index.internalId()));
		}
	}

}

bool CKeybindEditor::KeybindModel::HasConflicts(CUiCommand* command, ConflictsMap* conflictsOut /*= nullptr*/) const
{
	QCommandAction* action = static_cast<QCommandAction*>(command->GetUiInfo());

	if (conflictsOut)
		conflictsOut->clear();

	for (const QKeySequence& seq : action->shortcuts())
	{
		if (m_conflicts.at(seq).size() > 1)
		{
			if (conflictsOut)
			{
				(*conflictsOut)[seq] = m_conflicts.at(seq);
			}
			else
			{
				return true;
			}
		}
	}

	if (conflictsOut)
	{
		return !conflictsOut->empty();
	}
	else
	{
		return false;
	}
}

QVariant CKeybindEditor::KeybindModel::data(const QModelIndex& index, int role /* = Qt::DisplayRole */) const
{
	QVariant variant = CommandModel::data(index, role);
	if (variant.isValid())
		return variant;

	if (!index.isValid())
	{
		return QVariant();
	}

	switch (role)
	{
	case Qt::DisplayRole:
	case Qt::EditRole:
		{
			QModelIndex parent = index.parent();
			if (parent.isValid())
			{
				//command item
				switch (index.column())
				{
				case 2:
					{
						QCommandAction* action = GetAction(parent.row(), index.row());
						return Private_KeybindEditor::KeySequenceListToString(action->shortcuts());
					}
					break;
				default:
					break;
				}
			}

			return QVariant();
		}
		break;
	case Qt::ToolTipRole:
		{
			QModelIndex parent = index.parent();
			if (parent.isValid())
			{
				ConflictsMap conflicts;

				CUiCommand* thisCommand = static_cast<CUiCommand*>(m_modules[parent.row()].m_commands[index.row()]);

				if (HasConflicts(thisCommand, &conflicts))
				{
					QString tooltip(tr("Conflict with the following command(s): \n"));

					for (auto it = conflicts.begin(); it != conflicts.end(); ++it)
					{
						for (CUiCommand* cmd : it->second)
						{
							if (cmd != thisCommand)
							{
								QVariant displayData = data(GetIndex(cmd, 0), Qt::DisplayRole);
								tooltip += QString("%1 (%2)\n").arg(displayData.toString()).arg(cmd->GetCommandString().c_str());
							}
						}
					}

					return tooltip;
				}
			}
			else
			{
				return QVariant();
			}
		}
		break;
	case Qt::ForegroundRole:
		{
			QModelIndex parent = index.parent();
			if (parent.isValid())
			{
				const auto pCommand = static_cast<CUiCommand*>(m_modules[parent.row()].m_commands[index.row()]);
				if (HasConflicts(pCommand, nullptr))
				{
					return GetStyleHelper()->errorColor();
				}
			}
			else
			{
				return QVariant();
			}
		}
		break;
	default:
		break;
	}

	return QVariant();
}

bool CKeybindEditor::KeybindModel::setData(const QModelIndex& index, const QVariant& value, int role /*= Qt::EditRole*/)
{
	if (CommandModel::setData(index, value, role))
		return true;

	//no need to notify of the change here because there can only be one view of this model
	if (role == Qt::EditRole)
	{
		QString str = value.toString();

		QModelIndex parent = index.parent();
		if (parent.isValid())
		{
			switch (index.column())
			{
			case 2: //shortcut
				{
					QCommandAction* action = GetAction(parent.row(), index.row());
					QList<QKeySequence> oldShortcuts = action->shortcuts();
					action->setShortcuts(Private_KeybindEditor::StringToKeySequenceList(str));
					const auto pCommand = static_cast<CUiCommand*>(m_modules[parent.row()].m_commands[index.row()]);
					UpdateConflicts(pCommand, oldShortcuts);
					GetIEditorImpl()->GetCommandManager()->signalChanged();
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

//////////////////////////////////////////////////////////////////////////

class CKeybindEditor::KeybindItemDelegate : public QStyledItemDelegate
{
	//Notes:
	//We are not using QKeySequenceEdit intentionally
	//ESC, Enter/Return and modifier keys on their own can not be bound at this point
	//Windows: Alt+F4 will close the parent QWindow. This can be alleviated by installing an event filter but binding a shortcut to Alt+F4 makes little sense
	class KeybindLineEdit : public QWidget
	{
	public:
		KeybindLineEdit(QWidget* parent)
			: QWidget(parent)
			, m_lineEdit(new QLineEdit(this))
		{
			m_lineEdit->setPlaceholderText(tr("Press shortcut"));
			m_lineEdit->setFrame(true);
			m_lineEdit->installEventFilter(this);

			QToolButton* plusButton = new QToolButton;
			plusButton->setText(tr("Add Shortcut"));
			plusButton->setToolTip(tr("Add Shortcut"));
			plusButton->setIcon(CryIcon(QPixmap(":icons/General/Plus.ico")));
			connect(plusButton, &QToolButton::clicked, this, &KeybindLineEdit::OnPlus);

			QToolButton* minusButton = new QToolButton;
			minusButton->setText(tr("Remove Shortcut"));
			minusButton->setToolTip(tr("Remove Shortcut"));
			minusButton->setIcon(CryIcon(QPixmap(":icons/General/Minus.ico")));
			connect(minusButton, &QToolButton::clicked, this, &KeybindLineEdit::OnMinus);

			QToolButton* clearButton = new QToolButton;
			clearButton->setText(tr("Clear Shortcuts"));
			clearButton->setToolTip(tr("Clear Shortcuts"));
			clearButton->setIcon(CryIcon(QPixmap(":icons/General/Close.ico")));
			connect(clearButton, &QToolButton::clicked, this, &KeybindLineEdit::OnClear);

			QHBoxLayout* layout = new QHBoxLayout;
			layout->setContentsMargins(0, 0, 0, 0);
			layout->setSpacing(0);
			layout->addWidget(m_lineEdit);
			layout->addWidget(plusButton);
			layout->addWidget(minusButton);
			layout->addWidget(clearButton);

			setLayout(layout);
			setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
		}

		bool event(QEvent* event) override
		{
			//Shortcuts will not be triggered from within this control, they will set the keybind instead.
			if (event->type() == QEvent::Shortcut || event->type() == QEvent::ShortcutOverride)
			{
				QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
				keyPressEvent(keyEvent);
				keyEvent->accept();
				return true;
			}

			return QWidget::event(event);
		}

		bool eventFilter(QObject* object, QEvent* event)
		{
			switch (event->type())
			{
			case QEvent::Shortcut:
			case QEvent::ShortcutOverride:
			case QEvent::KeyPress:
			{
				QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
				keyPressEvent(keyEvent);
				keyEvent->accept();
				return true;
			}
			case QEvent::FocusOut:
			{
				// Destroy the in-place editing widget when focus is lost so we don't have a non-focused
				// line edit in the Keybinds view
				deleteLater();
			}
			default:
				return false;
			}
		}

		void keyPressEvent(QKeyEvent* event) override
		{
			if (event->isAutoRepeat())
			{
				return;
			}

			QList<QKeySequence> shortcuts = Private_KeybindEditor::StringToKeySequenceList(m_lineEdit->text());

			switch (event->key())
			{
			case 0:
			case Qt::Key_unknown:
			case Qt::Key_Meta:
			case Qt::Key_Control:
			case Qt::Key_Alt:
			case Qt::Key_AltGr:
			case Qt::Key_Shift:
				break;

			default:
				QKeySequence seq = QKeySequence(event->key() | event->modifiers());

				if (shortcuts.indexOf(seq) == -1)
				{
					shortcuts.pop_back();
					shortcuts.push_back(seq);

					m_lineEdit->setText(Private_KeybindEditor::KeySequenceListToString(shortcuts));
				}
				break;
			}
		}

		void OnPlus()
		{
			QList<QKeySequence> shortcuts = Private_KeybindEditor::StringToKeySequenceList(m_lineEdit->text());

			if (shortcuts.isEmpty() || !shortcuts.last().isEmpty())
				shortcuts.push_back(QKeySequence());

			m_lineEdit->setText(Private_KeybindEditor::KeySequenceListToString(shortcuts));
			m_lineEdit->setFocus(Qt::OtherFocusReason);
		}

		void OnMinus()
		{
			QList<QKeySequence> shortcuts = Private_KeybindEditor::StringToKeySequenceList(m_lineEdit->text());

			if (!shortcuts.isEmpty())
				shortcuts.pop_back();

			m_lineEdit->setText(Private_KeybindEditor::KeySequenceListToString(shortcuts));
			m_lineEdit->setFocus(Qt::OtherFocusReason);
		}

		void OnClear()
		{
			m_lineEdit->setText(QString());
			m_lineEdit->setFocus(Qt::OtherFocusReason);
		}

		QLineEdit* m_lineEdit;
	};

public:
	KeybindItemDelegate(QObject* parent = 0)
		: QStyledItemDelegate(parent)
	{}

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		return new KeybindLineEdit(parent);
	}

	void setEditorData(QWidget* editor, const QModelIndex& index) const override
	{
		auto keybindLineEdit = static_cast<KeybindLineEdit*>(editor);
		auto text = index.data(Qt::DisplayRole).toString();
		keybindLineEdit->m_lineEdit->setText(text);
	}

	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
	{
		auto keybindLineEdit = static_cast<KeybindLineEdit*>(editor);
		model->setData(index, keybindLineEdit->m_lineEdit->text(), Qt::EditRole);
	}

	void paint(QPainter* pPainter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		QStyleOptionViewItem opt = option;
		opt.state &= ~QStyle::State_Open;
		QStyledItemDelegate::paint(pPainter, opt, index);
	}
};

//////////////////////////////////////////////////////////////////////////

CKeybindEditor::CKeybindEditor(QWidget* parent)
	: CDockableWidget(parent)
	, m_treeView(new QAdvancedTreeView())
{
	m_model = CommandModelFactory::Create<KeybindModel>();
	QDeepFilterProxyModel::BehaviorFlags behavior = QDeepFilterProxyModel::AcceptIfChildMatches | QDeepFilterProxyModel::AcceptIfParentMatches;
	QDeepFilterProxyModel* proxy = new QDeepFilterProxyModel(behavior);
	proxy->setSourceModel(m_model);
	proxy->setFilterKeyColumn(1);
	proxy->setFilterRole((int)CommandModel::Roles::SearchRole);

	QSearchBox* searchBox = new QSearchBox();
	searchBox->SetModel(proxy);
	searchBox->EnableContinuousSearch(true);
	searchBox->SetAutoExpandOnSearch(m_treeView);

	m_treeView->setModel(proxy);
	m_treeView->setSortingEnabled(true);
	m_treeView->sortByColumn(0, Qt::AscendingOrder);
	// does not take ownership -> give parent
	m_treeView->setItemDelegateForColumn(2, new KeybindItemDelegate(this));
	m_treeView->setUniformRowHeights(true);
	m_treeView->setAllColumnsShowFocus(true);
	m_treeView->header()->setStretchLastSection(true);
	m_treeView->header()->setDefaultSectionSize(300);
	m_treeView->setEditTriggers(QTreeView::DoubleClicked | QTreeView::EditKeyPressed | QTreeView::SelectedClicked);
	m_treeView->expandToDepth(0); // Only expand the first depth so we don't get blue icons
	m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);

	QObject::connect(m_treeView, &QTreeView::customContextMenuRequested, this, &CKeybindEditor::OnContextMenu);
	QObject::connect(m_model, &QAbstractItemModel::dataChanged, this, &CKeybindEditor::OnKeybindsChanged);

	//buttons
	QPushButton* buttonAddCustom = new QPushButton("Add Custom");
	QPushButton* buttonReset = new QPushButton("Reset All");
	QPushButton* buttonImport = new QPushButton("Import...");
	QPushButton* buttonExport = new QPushButton("Export...");

	QObject::connect(buttonAddCustom, &QPushButton::clicked, this, &CKeybindEditor::OnAddCustomCommand);
	QObject::connect(buttonReset, &QPushButton::clicked, &Private_KeybindEditor::PyResetAllKeybinds);
	QObject::connect(buttonImport, &QPushButton::clicked, &Private_KeybindEditor::PyImportKeybinds);
	QObject::connect(buttonExport, &QPushButton::clicked, &Private_KeybindEditor::PyExportKeybinds);

	QHBoxLayout* buttons = new QHBoxLayout();
	buttons->setContentsMargins(1, 1, 1, 1);
	buttons->addWidget(buttonAddCustom);
	buttons->addWidget(buttonReset);
	buttons->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
	buttons->addWidget(buttonImport);
	buttons->addWidget(buttonExport);

	QVBoxLayout* layout = new QVBoxLayout();
	layout->setContentsMargins(1, 1, 1, 1);
	setLayout(layout);
	layout->addWidget(searchBox);
	layout->addWidget(m_treeView);
	layout->addLayout(buttons);
}

CKeybindEditor::~CKeybindEditor()
{
	// m_model is created via CommandModelFactory::Create and has no parent
	// so it has to be deleted manually
	delete m_model;
}

void CKeybindEditor::OnContextMenu(const QPoint& pos) const
{
	CEditorCommandManager* comMan = GetIEditorImpl()->GetCommandManager();
	QMenu* menu = new QMenu();

	CCommand* command = nullptr;
	QModelIndex index = m_treeView->indexAt(pos);
	if (index.isValid())
	{
		QVariant commandVar = index.model()->data(index, (int)CommandModel::Roles::CommandPointerRole);
		if (commandVar.isValid())
		{
			command = commandVar.value<CCommand*>();
		}
	}

	if (command && !command->IsCustomCommand())
	{
		QAction* action = menu->addAction(tr("Reset to Default Value"));
		QObject::connect(action, &QAction::triggered, [=](){ Private_KeybindEditor::PyResetKeybind(command->GetCommandString()); });
	}

	menu->addAction(comMan->GetCommandAction("keybind.reset_all"));
	menu->addSeparator();

	QAction* action = menu->addAction(tr("Add Custom Command"));
	QObject::connect(action, &QAction::triggered, this, &CKeybindEditor::OnAddCustomCommand);

	if (command && command->IsCustomCommand())
	{
		QAction* action = menu->addAction(tr("Remove Custom Command"));
		QObject::connect(action, &QAction::triggered, [=](){ OnRemoveCustomCommand(static_cast<CCustomCommand*>(command)); });
	}

	menu->popup(m_treeView->viewport()->mapToGlobal(pos));
}

void CKeybindEditor::customEvent(QEvent* event)
{
	if (event->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(event);

		const string& command = commandEvent->GetCommand();
		if (command == "general.delete")
		{
			CEditorCommandManager* comMan = GetIEditorImpl()->GetCommandManager();

			CCommand* command = nullptr;
			QModelIndex index = m_treeView->selectionModel()->currentIndex();
			if (index.isValid())
			{
				QVariant commandVar = index.model()->data(index, (int)CommandModel::Roles::CommandPointerRole);
				if (commandVar.isValid())
				{
					command = commandVar.value<CCommand*>();
					if (command->IsCustomCommand())
					{
						OnRemoveCustomCommand(static_cast<CCustomCommand*>(command));
					}
				}
			}

			commandEvent->setAccepted(true);
		}
	}
}

void CKeybindEditor::OnAddCustomCommand() const
{
	CCustomCommand* customCommand = new CCustomCommand(QT_TR_NOOP("New Custom Command"), "");
	customCommand->Register();

	QAbstractItemModel* model = m_treeView->model();
	QModelIndexList list = model->match(model->index(0, 0, QModelIndex()), (int)CommandModel::Roles::CommandPointerRole, QVariant::fromValue((CCommand*)customCommand), 1, Qt::MatchRecursive | Qt::MatchExactly);
	if (!list.isEmpty())
	{
		m_treeView->expand(list.first().parent());
		m_treeView->scrollTo(list.first());
		m_treeView->selectionModel()->setCurrentIndex(list.first(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		m_treeView->edit(list.first());
	}

	SaveUserKeybinds();
}

void CKeybindEditor::OnRemoveCustomCommand(CCustomCommand* command) const
{
	command->Unregister();

	SaveUserKeybinds();
}

QVariant CKeybindEditor::GetState()
{
	std::vector<CCommand*> cmds;
	GetIEditorImpl()->GetCommandManager()->GetCommandList(cmds);

	QVariantMap map;

	for (CCommand* cmd : cmds)
	{
		if (cmd->CanBeUICommand())
		{
			CUiCommand* uiCmd = static_cast<CUiCommand*>(cmd);
			QCommandAction* action = static_cast<QCommandAction*>(uiCmd->GetUiInfo());
			QList<QKeySequence> shortcuts = action->shortcuts();
			QStringList shortcutsToWrite;
			QVariantMap commandData;

			if (shortcuts != action->GetDefaultShortcuts())
			{
				for (const QKeySequence& shortcut : shortcuts)
				{
					shortcutsToWrite.push_back(shortcut.toString(QKeySequence::PortableText));
				}

				commandData.insert("shortcuts", shortcutsToWrite);
			}

			if (cmd->IsCustomCommand())
			{
				commandData.insert("custom", cmd->IsCustomCommand());
				commandData.insert("uiName", cmd->GetName().c_str());
			}

			if (!commandData.isEmpty())
			{
				QString command = uiCmd->GetCommandString().c_str();
				map.insert(command, commandData);
			}
		}
	}

	return map;
}

void CKeybindEditor::SetState(const QVariant& state)
{
	//TODO : if a command is not yet registered when this is loaded, settings are lost !
	if (!state.isValid() || state.type() != QVariant::Map)
		return;

	CEditorCommandManager* comMgr = GetIEditorImpl()->GetCommandManager();
	comMgr->RemoveAllCustomCommands();

	QVariantMap varMap = state.value<QVariantMap>();

	for (auto it = varMap.begin(); it != varMap.end(); ++it)
	{
		QVariantMap commandData = it.value().toMap();
		QList<QKeySequence> shortcutsAsKeySeq;

		QVariant shortcutsVar = commandData.value("shortcuts");
		if (shortcutsVar.isValid())
		{
			QStringList shortcutsAsString = shortcutsVar.toStringList();

			for (QString& shortcutStr : shortcutsAsString)
			{
				shortcutsAsKeySeq.push_back(QKeySequence(shortcutStr, QKeySequence::PortableText));
			}
		}

		string commandStr = it.key().toStdString().c_str();

		QVariant customVar = commandData.value("custom");
		if (customVar.isValid() && customVar.toBool())
		{
			CCustomCommand* customCommand = new CCustomCommand(commandData.value("uiName").toString().toStdString().c_str(), commandStr);
			customCommand->Register();

			if (shortcutsVar.isValid())
				static_cast<QCommandAction*>(customCommand->GetUiInfo())->setShortcuts(shortcutsAsKeySeq);
		}
		else
		{
			QCommandAction* action = comMgr->GetCommandAction(commandStr);
			if (action)
			{
				if (shortcutsVar.isValid())//read valid shortcuts even if shortcutsAsKeySeq is empty
					action->setShortcuts(shortcutsAsKeySeq);
			}
		}
	}

	//Notify change
	comMgr->signalChanged();
}

void CKeybindEditor::SaveKeybindsToFile(const char* filename)
{
	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to open path: %s", filename);
		return;
	}

	QJsonDocument doc(QJsonDocument::fromVariant(GetState()));
	file.write(doc.toJson());
}

void CKeybindEditor::LoadKeybindsFromFile(const char* filename)
{
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_COMMENT, "Failed to open path: %s", filename);
		return;
	}

	QJsonDocument doc(QJsonDocument::fromJson(file.readAll()));
	QVariant layout = doc.toVariant();

	if (layout.isValid())
	{
		SetState(layout);
		GetIEditorImpl()->GetCommandManager()->signalChanged();
	}
	//TODO : error message
}

void CKeybindEditor::OnKeybindsChanged()
{
	SaveUserKeybinds();
}

void CKeybindEditor::SaveUserKeybinds()
{
	QString userKeybinds = QtUtil::GetAppDataFolder();
	QDir(userKeybinds).mkpath(userKeybinds);
	userKeybinds += "/Keybinds.json";
	SaveKeybindsToFile(userKeybinds.toStdString().c_str());
}

void CKeybindEditor::LoadUserKeybinds()
{
	QString userKeybinds = QtUtil::GetAppDataFolder();
	userKeybinds += "/Keybinds.json";
	QFile file(userKeybinds);

	// If there's no user keybinds, try and load project keybinds
	if (file.exists())
		return LoadKeybindsFromFile(userKeybinds.toStdString().c_str());

	QString projKeybinds(GetIEditorImpl()->GetProjectManager()->GetCurrentProjectDirectoryAbsolute());
	projKeybinds = projKeybinds + "/Editor/Keybinds.json";

	if (QFile(projKeybinds).exists())
		return LoadKeybindsFromFile(projKeybinds.toUtf8());

	LoadKeybindsFromFile(PathUtil::Make(PathUtil::GetEnginePath(), "%EDITOR%/Keybinds.json").c_str());
}


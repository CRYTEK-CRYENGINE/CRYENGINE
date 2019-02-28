// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ConsoleWindow.h"
#include "ConsolePlugin.h"
#include <QTimer>
#include <QKeyEvent>
#include <QScrollBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QtUtil.h>
#include <QClipboard>
#include <QMenu>
#include "FileDialogs/SystemFileDialog.h"
#include <QDir>
#include <EditorStyleHelper.h>
#include <CryIcon.h>
#include <QToolButton>
#include <QTime>
#include <Preferences/GeneralPreferences.h>
#include <Menu/AbstractMenu.h>

//timer interval, in ms, for forcing the popup window in place
//once this is no longer necessary, set this to 0 to save some cycles
#define TIMER_INTERVAL 100

//flags used for sticky tracking
#define FLAG_STICKY     1
#define FLAG_IGNOREMOVE 2

CConsoleWindow::CConsoleWindow(QWidget* pParent /*= nullptr*/)
	: m_popup(this)
	, m_sticky(FLAG_STICKY)
{
	//set up UI
	SetupUI();

	//attach event handlers
	connect(m_pInput, &QLineEdit::textEdited, this, &CConsoleWindow::HandleInput);
	connect(m_pInput, &QTabLineEdit::tabPressed, this, &CConsoleWindow::HandleTab);
	connect(m_pInput, &QLineEdit::returnPressed, this, &CConsoleWindow::HandleEnter);
	connect(&m_popup, &CAutoCompletePopup::selectionChanged, this, &CConsoleWindow::HandleAutoCompleteSelection);
	connect(&m_searchBox, &QSearchBox::textChanged, this, &CConsoleWindow::SearchBox);
	connect(&m_searchBox, &QSearchBox::returnPressed, this, &CConsoleWindow::OnFindNext);

	m_pInput->installEventFilter(this);
	
	layout()->setContentsMargins(1, 1, 1, 1);

	gEnv->pLog->AddCallback(this);

	m_lastConsoleLineNumber = 0;

	m_searchWidget->hide();

	m_searchBackwards = false;

	InitMenu();
}

void CConsoleWindow::InitMenu()
{
	const CEditor::MenuItems items[] = {
		CEditor::MenuItems::FileMenu,
		CEditor::MenuItems::EditMenu,
		CEditor::MenuItems::Save
	};
	AddToMenu(&items[0], CRY_ARRAY_COUNT(items));

	auto pEditMenu = GetMenu(MenuItems::EditMenu);
	pEditMenu->AddAction(GetAction("general.consoleClearLog"), -1, -1);

	const CEditor::MenuItems items2[] = {
		CEditor::MenuItems::Find,
		CEditor::MenuItems::FindNext,
		CEditor::MenuItems::FindPrevious
	};
	AddToMenu(&items2[0], CRY_ARRAY_COUNT(items2));

	const CEditor::MenuItems itemsHelp[] = {
		CEditor::MenuItems::HelpMenu,
	};

	AddToMenu(&itemsHelp[0], CRY_ARRAY_COUNT(itemsHelp));

	auto menu = GetMenu(MenuItems::HelpMenu);

	QString name(QStringLiteral("Console Commands"));
	QString cmd = QString(QStringLiteral("general.open_pane '%1'")).arg(name);
	QAction* pAction = GetIEditor()->GetICommandManager()->GetAction(cmd.toUtf8().constData(), name.toUtf8().constData());
	pAction->setIcon(CryIcon(QStringLiteral("icons:ObjectTypes/Console_Commands.ico")));
	menu->AddAction(pAction, 0, 0);

	name = QStringLiteral("Console Variables");
	cmd = QString(QStringLiteral("general.open_pane '%1'")).arg(name);
	pAction = GetIEditor()->GetICommandManager()->GetAction(cmd.toUtf8().constData(), name.toUtf8().constData());
	pAction->setIcon(CryIcon(QStringLiteral("icons:ObjectTypes/Console_Variables.ico")));
	menu->AddAction(pAction, 0, 0);
}

void CConsoleWindow::SearchBoxEnterPressed()
{
	QString text = m_searchBox.text();

	if (text.compare(m_lastSearchBoxString, Qt::CaseInsensitive) != 0)
	{
		m_pHistory->moveCursor(QTextCursor::Start);
	}

	bool searchSuccess = false;

	if (m_searchBackwards)
	{
		searchSuccess = m_pHistory->find(text, QTextDocument::FindBackward);
	}
	else
	{
		m_searchBackwards = false;

		searchSuccess = m_pHistory->find(text);

		if (!searchSuccess)
		{
			m_pHistory->moveCursor(QTextCursor::Start);
			m_pHistory->find(text);
		}
	}

	m_lastSearchBoxString = text;
	m_pHistory->ensureCursorVisible();
}

void CConsoleWindow::HighlightSelection(const QString& text)
{
	QColor highlightColor = QColor(Qt::blue).lighter(100);
	QList<QTextEdit::ExtraSelection> selections;

	while (m_pHistory->find(text))
	{
		QTextEdit::ExtraSelection extraSelection;
		extraSelection.format.setBackground(highlightColor);
		extraSelection.cursor = m_pHistory->textCursor();
		selections.append(extraSelection);
	}

	m_pHistory->setExtraSelections(selections);
	m_pHistory->moveCursor(QTextCursor::Start);
}

void CConsoleWindow::SearchBox(const QString& text)
{
	m_pHistory->moveCursor(QTextCursor::Start);

	if (text.isEmpty())
	{
		m_pHistory->moveCursor(QTextCursor::End);
		m_lastSearchBoxString = QStringLiteral("");
		QList<QTextEdit::ExtraSelection> selections;
		m_pHistory->setExtraSelections(selections);
		return;
	}

	HighlightSelection(text);
}

void CConsoleWindow::SetupUI()
{
	QWidget* m_pCentralWidget = new QWidget();

	setObjectName(QStringLiteral("ConsoleWindow"));
	resize(551, 386);
	m_pCentralWidget->setObjectName(QStringLiteral("m_pCentralWidget"));
	QVBoxLayout* verticalLayout = new QVBoxLayout(m_pCentralWidget);
	verticalLayout->setSpacing(2);
	verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
	verticalLayout->setContentsMargins(1, 2, 2, 2);

	layout()->addWidget(m_pMenuBar);

	m_pHistory = new QTextEdit();
	m_pHistory->setReadOnly(true);
	m_pHistory->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

	m_searchWidget = new QWidget(m_pHistory);
	m_searchWidget->setFixedSize(QSize(285, 30));

	QHBoxLayout* searchLayout = new QHBoxLayout();

	int buttonSize = 20;
	CryIcon iconBack;
	iconBack.addFile(QStringLiteral("icons:General/Arrow_Left.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
	QToolButton* goBack = new QToolButton(m_searchWidget);
	goBack->setIcon(iconBack);
	goBack->setCheckable(false);
	goBack->setAutoRaise(true);
	goBack->setToolTip(QStringLiteral("Search Backwards"));
	goBack->setFixedSize(buttonSize, buttonSize);
	goBack->setParent(m_searchWidget);

	CryIcon iconForward;
	iconForward.addFile(QStringLiteral("icons:General/Arrow_Right.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
	QToolButton* goForward = new QToolButton(m_searchWidget);
	goForward->setIcon(iconForward);
	goForward->setCheckable(false);
	goForward->setAutoRaise(true);
	goForward->setToolTip(QStringLiteral("Search Forward"));
	goForward->setFixedSize(buttonSize, buttonSize);
	goForward->setParent(m_searchWidget);
	goForward->setFixedSize(buttonSize, buttonSize);

	CryIcon iconClose;
	iconClose.addFile(QStringLiteral("icons:Window/Window_Close.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
	QToolButton* closeSearch = new QToolButton(m_searchWidget);
	closeSearch->setIcon(iconClose);
	closeSearch->setCheckable(false);
	closeSearch->setAutoRaise(true);
	closeSearch->setToolTip(QStringLiteral("Search Forward"));
	closeSearch->setFixedSize(buttonSize, buttonSize);
	closeSearch->setParent(m_searchWidget);
	closeSearch->setFixedSize(buttonSize, buttonSize);

	connect(goBack, &QPushButton::clicked, this, &CConsoleWindow::OnFindPrevious);
	connect(goForward, &QPushButton::clicked, this, &CConsoleWindow::OnFindNext);
	connect(closeSearch, &QPushButton::clicked, this, &CConsoleWindow::OnCloseSearch);

	m_searchWidget->setLayout(searchLayout);

	m_searchBox.setParent(m_searchWidget);
	m_searchBox.EnableContinuousSearch(true);
	m_searchBox.setFixedSize(200, 20);

	m_searchWidget->setContentsMargins(1, 1, 1, 1);

	searchLayout->setContentsMargins(1, 1, 1, 1);
	searchLayout->addWidget(&m_searchBox);
	searchLayout->addWidget(goBack);
	searchLayout->addWidget(goForward);
	searchLayout->addWidget(closeSearch);

	searchLayout->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

	verticalLayout->addWidget(m_pHistory);

	m_pInput = new QTabLineEdit(m_pCentralWidget);
	m_pInput->setObjectName(QStringLiteral("m_pInput"));
	m_pInput->setContentsMargins(2, 2, 2, 2);

	verticalLayout->addWidget(m_pInput, Qt::AlignBottom);

	setLayout(verticalLayout);

	layout()->addWidget(m_pCentralWidget);

} // setupUi

//handle new incoming log lines
void CConsoleWindow::HandleMessage(const Messages::SHistoryLine& line)
{
	QString newline = QtUtil::ToQString(line.line);

	if (!newline.isEmpty())
	{
		if (newline.toLower().startsWith(QStringLiteral("$3")))
		{
			newline.remove(0, 2);
			m_pHistory->setTextColor(GetStyleHelper()->successColor());
		}
		if (newline.toLower().startsWith(QStringLiteral("$4")))
		{
			newline.remove(0, 2);
			m_pHistory->setTextColor(GetStyleHelper()->errorColor());
		}
		else if (newline.toLower().startsWith(QStringLiteral("$6")))
		{
			newline.remove(0, 2);
			m_pHistory->setTextColor(GetStyleHelper()->warningColor());
		}
		else
		{
			m_pHistory->setTextColor(QColor("white"));
		}

		m_pHistory->append(newline);

		if (m_sticky & FLAG_STICKY)
		{
			m_pHistory->ensureCursorVisible();
		}
	}

	return;
}

//called when the input box value has changed
void CConsoleWindow::HandleInput(const QString& current)
{
	//do not handle empty or items with spaces in them
	QString query = current.trimmed();
	int spacePos = query.indexOf(QChar(L' '));
	if (spacePos > 0) query = QStringRef(&query, 0, spacePos).toString();
	auto end_char = current.cend();
	end_char--;
	if (query.isEmpty() || *(end_char) == L' ')
	{
		m_popup.hide();
		return;
	}

	Messages::SAutoCompleteRequest req;
	req.address = m_address;
	req.query = QtUtil::ToString(query);

	Messages::SAutoCompleteReply msg = CConsolePlugin::GetInstance()->HandleAutoCompleteRequest(req);
	m_popup.Set(msg);
	m_popup.Select(current);

	if (msg.matches.size() > 0 && !current.contains(QStringLiteral(" ")))
	{
		activateWindow();
		MovePopup(true);
		m_popup.show();
	}
	else
	{
		m_popup.hide();
	}

}

//called when TAB is pressed inside the input box
void CConsoleWindow::HandleTab()
{
	if (m_popup.isVisible())
	{
		QString completion = m_popup.GetSelectedItem();

		if (!completion.isEmpty() && !m_pInput->text().startsWith(completion))
		{
			if (completion.indexOf(QStringLiteral(" ")) == -1)
			{
				m_pInput->setText(completion.append(QStringLiteral(" ")));
			}
			else
			{
				m_pInput->setText(completion);
			}

		}
		else if (!completion.isEmpty() && m_pInput->text().simplified().compare(completion, Qt::CaseInsensitive)==0 )
		{
			m_pInput->setText(m_popup.SelectNext().append(QStringLiteral(" ")));
		}

	}
}

//called when ENTER is pressed inside the input box
void CConsoleWindow::HandleEnter()
{
	QString current = m_pInput->text();
	QString candidate = m_popup.GetSelectedItem();

	if (current.isEmpty() && m_popup.isHidden())
	{
		return;
	}

	QString name;
	QString value;

	if (!current.isEmpty())
	{
		name = current;
	}
	else
	{
		name = candidate;
		current = candidate;
	}

	int cutPos = current.indexOf(QStringLiteral(" "));
	if (cutPos > 0)
	{
		name.truncate(cutPos);
		value = current;
		value.replace(name, QStringLiteral(""));
		value = value.simplified();
	}

	// SHIP HACK: ToolTabManager immediately deletes all panels(and child widgets) before they can finish
	// execution. Any layout commands that are executed through the console window will very likely cause 
	// the application to crash. In this case QLineEdit hasn't finished handling of the ENTER key press,
	// when it gets destroyed and continues execution. As a solution, we will defer the handling of layout
	// commands until the next frame
	if (current.startsWith("layout."))
	{
		m_pInput->clearFocus();
		QTimer::singleShot(0, [current]()
		{
			GetIEditor()->GetICommandManager()->Execute(current.toStdString().c_str());
		});
		return;
	}

	string logString = GetIEditor()->GetICommandManager()->Execute(current.toStdString().c_str());

	if (!logString.empty())
	{
		OnWriteToConsole(logString, true);
	}

	CConsolePlugin::GetInstance()->RefreshCVarsAndCommands();

	CAutoCompletePopup::SAutoCompleteItem historyItem;
	historyItem.display = name + QStringLiteral(" ") + value;
	historyItem.name = name + QStringLiteral(" ") + value;
	historyItem.type = CAutoCompletePopup::SAutoCompleteItem::eType_Float;
	m_popup.AddHistoryItem(historyItem);

	Messages::SAutoCompleteReply::SItem match;
	match.name = historyItem.display.toStdString().c_str();
	match.value = value.toStdString().c_str();
	m_popup.AddHistoryMatch(match);
	m_pInput->clear();

	m_popup.hide();
}

//handle auto-complete selection changes
void CConsoleWindow::HandleAutoCompleteSelection(const QString& selection, CAutoCompletePopup::ESelectReason reason)
{
	if (reason == CAutoCompletePopup::eSelectReasonClick)
	{
		m_pInput->setText(selection + QStringLiteral(" "));
		m_pInput->setFocus();
	}
}

//handle auto-complete reply
void CConsoleWindow::HandleAutoComplete(const Messages::SAutoCompleteReply& reply)
{
	QString previousSelection = m_popup.GetSelectedItem();
	if (m_popup.Set(reply))
	{
		//select the first item, or a matching item if possible
		m_popup.Select(previousSelection);

		//move pop-up to input box
		MovePopup(true);
		m_popup.show();

		//re-take focus
		m_pInput->setFocus();
	}
}

//handle CVar button pressed
void CConsoleWindow::HandleCVarButtonPressed()
{
	//CConsolePlugin::GetInstance()->RefreshCVars();
	//CConsolePlugin::GetInstance()->SpawnCVarWindow();
}

//move the pop-up window
void CConsoleWindow::MovePopup(bool force)
{
	if (m_popup.isVisible() || force)
	{
		int defaultPopupHeight = m_popup.GetDefaultHeight();
		int topMarginSize = 20;
		int inputEditBorderSize = 2;

		QPoint globalYpos = m_pInput->mapToGlobal(QPoint(0, 0));

		// Resize Popup Height
		int cutSizeY = -(globalYpos.y() - defaultPopupHeight - topMarginSize - inputEditBorderSize);

		if (cutSizeY>0)
		{
			m_popup.setMinimumHeight(defaultPopupHeight - cutSizeY);
			m_popup.setMaximumHeight(defaultPopupHeight - cutSizeY);
		}
		else if (cutSizeY<=0)
		{
			m_popup.setMinimumHeight(defaultPopupHeight);
			m_popup.setMaximumHeight(defaultPopupHeight);
		}

		//move to the input box
		int newYPos = globalYpos.y() - m_popup.height() - inputEditBorderSize;
		globalYpos.setY(newYPos);
		m_popup.move(globalYpos);

		//hide if we're no longer visible or no longer have focus
		bool bHasFocus = false;
		QWidget* pWidget = QApplication::focusWidget();
		while (pWidget)
		{
			if (pWidget == this || pWidget == &m_popup)
			{
				bHasFocus = true;
				break;
			}
			pWidget = pWidget->parentWidget();
		}
	}
}

//peek at arrow keys for navigation of auto-complete, escape to close
void CConsoleWindow::keyPressEvent(QKeyEvent* e)
{
	if (m_searchWidget->isVisible())
	{
		switch (e->key())
		{
		case Qt::Key_Escape:
			if (m_popup.GetPopupType() == CAutoCompletePopup::ePopupType_History)
			{
				m_pInput->clear();
			}
			OnCloseSearch();
			break;
		}
	}

	if (m_pInput->hasFocus() && m_popup.isVisible())
	{
		int direction = 1;
		switch (e->key())
		{
		case Qt::Key_Up:
			{
				direction = -1;
			}
		case Qt::Key_Down:
			m_popup.Select(direction);
			if (m_popup.GetPopupType() == CAutoCompletePopup::ePopupType_History)
			{
				m_pInput->setText(m_popup.GetSelectedItem());
			}
			return;
		case Qt::Key_Escape:
			{
				m_popup.hide();
				if (m_popup.GetPopupType() == CAutoCompletePopup::ePopupType_History)
				{
					m_pInput->clear();
				}
			}
			return;
		}
	}
	else if (!m_popup.isVisible())
	{
		int direction = 1;
		switch (e->key())
		{
		case Qt::Key_Up:
			{
				MovePopup(true);
				direction = -1;
				m_popup.DisplayHistory();
				m_pInput->setText(m_popup.GetSelectedItem());
				return;
			}
		}
	}

	//this key press was not handled
	CDockableEditor::keyPressEvent(e);
}

bool CConsoleWindow::event(QEvent* pEvent)
{
	if (pEvent->type() == QEvent::KeyPress || pEvent->type() == QEvent::ShortcutOverride)
	{
		// Intercept the Copy shortcut event and prevent it from firing the general.copy command
		// we just want to copy the selected text to the clipboard, not fire the copy command which
		// pollutes the console
		QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(pEvent);
		if (pKeyEvent && pKeyEvent->matches(QKeySequence::Copy))
		{
			pEvent->setAccepted(true);
			return true;
		}
	}

	//TODO : This is madness!! There should not be such a process for each event
	if (m_popup.isVisible())
	{
		QWidget* pWidget = QApplication::focusWidget();

		if (pWidget)
		{
			QList<QWidget*> childrenMain = findChildren<QWidget*>();

			bool componentFound = false;
			for (int i = 0; i < childrenMain.count(); i++)
			{
				if (childrenMain.at(i) == pWidget)
				{
					componentFound = true;
					break;
				}
			}

			if (!componentFound)
			{
				m_popup.hide();
			}

		}

		switch (pEvent->type())
		{
		case QEvent::WindowDeactivate:
		    {
				m_popup.hide();
		    }
		}
	}

	if (pEvent->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(pEvent);

		const string& command = commandEvent->GetCommand();
		if (command == "general.consoleClearLog")
		{
			pEvent->setAccepted(ClearConsole());
			return true;
		}
	}

	return CDockableEditor::event(pEvent);
}

CConsoleWindow::~CConsoleWindow()
{
	gEnv->pLog->RemoveCallback(this);
};

void CConsoleWindow::OnWriteToConsole(const char* sText, bool bNewLine)
{
	QString newLine = QString::fromStdString(sText);
	newLine = newLine.simplified();

	if (gEditorGeneralPreferences.showTimeInConsole())
	{
		newLine = QStringLiteral("<") + QTime::currentTime().toString() + QStringLiteral("> ") + newLine;
	}

	Messages::SHistoryLine newHistoryLine;
	newHistoryLine.line = newLine.toStdString().c_str();
	newHistoryLine.index = m_lastConsoleLineNumber;

	HandleMessage(newHistoryLine);

	m_lastExecuteRequest = newHistoryLine.line;

	if (!m_searchWidget->isVisible())
	{
		m_pHistory->moveCursor(QTextCursor::End);
		m_pHistory->ensureCursorVisible();
	}
};

bool CConsoleWindow::OnFind()
{
	m_searchBackwards = false;

	if (!m_searchWidget->isVisible())
	{
		m_searchWidget->show();
		m_searchBox.setFocus();
	}

	return true;
}

bool CConsoleWindow::OnFindNext()
{
	m_searchBackwards = false;
	bool isEmptySearchField = m_searchBox.text().isEmpty();

	if (!m_searchWidget->isVisible())
	{
		m_searchWidget->show();
		m_searchBox.setFocus();
	}

	if (!isEmptySearchField)
	{
		SearchBoxEnterPressed();
	}
	else
	{
		OnFind();
	}

	return true;
}

bool CConsoleWindow::OnFindPrevious()
{
	m_searchBackwards = true;

	if (!m_searchWidget->isVisible())
	{
		m_searchWidget->show();
		m_searchBox.setFocus();
	}

	if (!m_searchBox.text().isEmpty())
	{
		SearchBoxEnterPressed();
	}

	return true;
}

bool CConsoleWindow::OnCloseSearch()
{
	m_searchWidget->hide();
	QList<QTextEdit::ExtraSelection> selections;
	m_pHistory->setExtraSelections(selections);
	m_pInput->setFocus();
	m_lastSearchBoxString = QStringLiteral("");
	HighlightSelection(QStringLiteral(""));

	return true;
}

bool CConsoleWindow::OnSave()
{
	QString consoleText = m_pHistory->toPlainText();

	if (consoleText.isEmpty())
		return true;

	QDir dir(QtUtil::GetAppDataFolder());

	CSystemFileDialog::RunParams runParams;
	runParams.title = tr("Export Console Log To File");
	runParams.initialDir = dir.absolutePath();
	runParams.extensionFilters << CExtensionFilter(tr("Text Files"), "txt");

	const QString path = CSystemFileDialog::RunExportFile(runParams, this);

	if (!path.isEmpty())
	{

		QFile file(path);
		if (!file.open(QIODevice::WriteOnly))
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to open path: %s", path);
			return true;
		}

		consoleText.replace(QStringLiteral("\n"), QStringLiteral("\r\n"));
		file.write(consoleText.toStdString().c_str());
		return true;
	}

	return true;
}

bool CConsoleWindow::ClearConsole()
{
	m_pHistory->clear();
	return true;
}

bool CConsoleWindow::eventFilter(QObject* o, QEvent* ev)
{
	if (o == m_pInput && ev->type() == QEvent::FocusOut)		
	{
		m_popup.hide();
	}

	return CDockableEditor::eventFilter(o, ev);
}

namespace
{
namespace Private_EditorConsoleCommands
{
void PyConsoleClearLog()
{
	CommandEvent("general.consoleClearLog").SendToKeyboardFocus();
}
}
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_EditorConsoleCommands::PyConsoleClearLog, general, consoleClearLog, "Clear", "general.consoleClearLog()");


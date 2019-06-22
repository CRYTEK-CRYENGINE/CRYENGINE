// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AutoCompletePopup.h"
#include "Messages.h"

#include <EditorFramework/Editor.h>
#include <QSearchBox.h>

class QTextEdit;
class QTabLineEdit;
class QWidget;

//the limit to number of console messages to cache
//this ensures an upper bound in memory usage and processing time in case of huge log-spam
#define CONSOLE_MAX_HISTORY (size_t)2000

//console window implementation
//see also ConsoleWindow.ui and ConsoleWindow.cpp
class CConsoleWindow : public CDockableEditor, public ILogCallback
{
	Q_OBJECT;

public:
	CConsoleWindow(QWidget* pParent = nullptr);
	~CConsoleWindow();

	virtual void Initialize() override;

	//////////////////////////////////////////////////////////
	// CDockableWindow implementation
	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_BOTTOM; }
	//////////////////////////////////////////////////////////

private:

	void RegisterActions();

	virtual const char* GetEditorName() const override { return "Console"; }

	//handle incoming update message
	void HandleMessage(const Messages::SHistoryLine& line);

	//handle incoming auto-complete reply
	void HandleAutoComplete(const Messages::SAutoCompleteReply& reply);

	//handle changed text
	void HandleInput(const QString& current);

	//handle tab pressed
	void HandleTab();

	//handle enter pressed
	void HandleEnter();

	//handle context menu on console history
	void HandleContextMenu(const QPoint& pt);

	//handle auto-complete selection change
	void HandleAutoCompleteSelection(const QString& selection, CAutoCompletePopup::ESelectReason reason);

	//handle CVar button pressed
	void HandleCVarButtonPressed();

	//move the pop-up window
	void MovePopup(bool force = false);

	//when one of the following is called, also move the pop-up window
	void keyPressEvent(QKeyEvent* e);

	void OnWriteToFile(const char* sText, bool bNewLine) {}
	void OnWriteToConsole(const char* sText, bool bNewLine);

	//setup this window
	void         SetupUI();

	virtual bool OnFind() override;
	virtual bool OnFindNext() override;
	virtual bool OnFindPrevious() override;

	bool OnSave();
	bool         ClearConsole();
	void         SearchBox(const QString& text);
	void         HighlightSelection(const QString& text);
	void         SearchBoxEnterPressed();
	bool         OnCloseSearch();
	bool m_searchBackwards;

protected:
	virtual bool eventFilter(QObject* o, QEvent* ev) Q_DECL_OVERRIDE;

private:
	bool event(QEvent* pEvent);
	void InitMenu();

private:
	//the address for this instance
	const string m_address;

	//the last execute request
	string m_lastExecuteRequest;

	//the last auto-complete request
	QString m_lastAutoCompleteRequest;

	//pop-up dialog for auto-completion
	CAutoCompletePopup m_popup;

	//timer for keeping popup in position
	//currently, move events don't work properly from MFC docking panels
	QTimer*       m_pTimer;

	QTextEdit*    m_pHistory;
	QTabLineEdit* m_pInput;

	int           m_lastConsoleLineNumber;

	//if set, keep the history scrolled down all the way at all times
	int        m_sticky;

	QSearchBox m_searchBox;
	QWidget*   m_searchWidget;
	QString    m_lastSearchBoxString;
};

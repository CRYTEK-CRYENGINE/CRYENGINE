// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/ICryPluginManager.h>
#include "CSharpOutputWindow.h"
#include "CSharpEditorPlugin.h"
#include "EditorStyleHelper.h"

#include "QtViewPane.h"

#include "CSharpOutputTextEdit.h"

REGISTER_VIEWPANE_FACTORY_AND_MENU(CCSharpOutputWindow, "C# Output", "Tools", false, "Advanced");

CCSharpOutputWindow::CCSharpOutputWindow()
	: CDockableEditor()
{
	m_pCompileTextWidget = new CCSharpOutputTextEdit();
	m_pCompileTextWidget->AddTextEventListener(this);
	m_pCompileTextWidget->setReadOnly(true);
	m_pCompileTextWidget->setUndoRedoEnabled(false);
	m_pCompileTextWidget->setTextInteractionFlags(Qt::TextBrowserInteraction | Qt::TextSelectableByKeyboard);
	SetContent(m_pCompileTextWidget);
	
	CCSharpEditorPlugin* pPlugin = CCSharpEditorPlugin::GetInstance();
	assert(pPlugin);
	if (pPlugin != nullptr)
	{
		pPlugin->RegisterMessageListener(this);
		OnMessagesUpdated(pPlugin->GetCompileMessage());
	}
}

CCSharpOutputWindow::~CCSharpOutputWindow()
{
	CCSharpEditorPlugin* pPlugin = CCSharpEditorPlugin::GetInstance();
	if (pPlugin != nullptr)
	{
		pPlugin->UnregisterMessageListener(this);
	}
	m_pCompileTextWidget->RemoveTextEventListener(this);
}

void CCSharpOutputWindow::OnMessageDoubleClicked(QMouseEvent* event)
{
	CCSharpEditorPlugin* pPlugin = CCSharpEditorPlugin::GetInstance();
	assert(pPlugin);
	if (pPlugin == nullptr)
	{
		return;
	}

	string plainText = m_pCompileTextWidget->toPlainText().toUtf8().constData();
	if (plainText.empty())
	{
		return;
	}

	QTextCursor cursor = m_pCompileTextWidget->cursorForPosition(event->pos());
	int pos = cursor.position();
	
	int start, end = 0;
	GetLineRange(plainText, pos, start, end);
	string line = GetLineFromRange(plainText, start, end);

	string path;
	int lineNumber;
	if(TryParseStackTraceLine(line, path, lineNumber))
	{
		if (lineNumber < 0)
		{
			pPlugin->OpenCSharpFile(path);
			return;
		}
		pPlugin->OpenCSharpFile(path, lineNumber);
	}
}

bool CCSharpOutputWindow::OnMouseMoved(QMouseEvent* event)
{
	if (event->buttons() != Qt::NoButton)
	{
		return false;
	}
	
	string plainText = m_pCompileTextWidget->toPlainText().toUtf8().constData();
	if (plainText.empty())
	{
		return false;
	}

	QTextCursor cursor = m_pCompileTextWidget->cursorForPosition(event->pos());
	int pos = cursor.position();
	
	int start, end = 0;
	GetLineRange(plainText, pos, start, end);
	cursor.setPosition(start);
	cursor.setPosition(end, QTextCursor::KeepAnchor);

	QList<QTextEdit::ExtraSelection> extraSelections;

	QTextEdit::ExtraSelection selection;
	QColor highlightColor = GetStyleHelper()->alternateHoverIconTint();

	selection.format.setBackground(highlightColor);
	selection.format.setProperty(QTextFormat::FullWidthSelection, true);
	selection.cursor = cursor;
	extraSelections.append(selection);

	m_pCompileTextWidget->setExtraSelections(extraSelections);

	return true;
}

void CCSharpOutputWindow::GetLineRange(const string& text, int index, int& lineStart, int& lineEnd)
{
	lineStart = lineEnd = 0;
	
	if (text[index] == '\n')
	{
		--index;
	}

	lineStart = text.rfind('\n', index);
	if (lineStart == string::npos)
	{
		lineStart = 0;
	}
	else
	{
		++lineStart;
	}

	lineEnd = text.find('\n', index);
	if (lineEnd == string::npos)
	{
		lineEnd = text.length();
	}
}

bool CCSharpOutputWindow::TryParseStackTraceLine(const string& line, string& path, int& lineNumber)
{
	lineNumber = -1;
	path = "";

	string assetDir = gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryAbsolute();
	size_t pathStartIndex = line.find(assetDir);
	if (pathStartIndex == string::npos)
	{
		return false;
	}

	size_t pathEndIndex = line.find("(", pathStartIndex);
	if (pathEndIndex == string::npos)
	{
		pathEndIndex = line.find(":", pathStartIndex);
	}

	if (pathEndIndex == string::npos)
	{
		return false;
	}

	path = line.substr(pathStartIndex, pathEndIndex - pathStartIndex);
	if (!gEnv->pCryPak->IsFileExist(path, ICryPak::eFileLocation_OnDisk))
	{
		return false;
	}

	size_t lineNumberEnd = line.find(",", pathEndIndex);
	if (lineNumberEnd == string::npos)
	{
		lineNumberEnd = line.length();
	}
	
	string lineStr = line.substr(pathEndIndex + 1, lineNumberEnd - (pathEndIndex + 1));
	for (int i : lineStr)
	{
		if (!std::isdigit(i))
		{
			// We didn't get a lineNumber, but the path should still be fine.
			return true;
		}
	}
	lineNumber = atoi(lineStr.c_str());
	return true;
}

void CCSharpOutputWindow::OnMessagesUpdated(const string messages)
{
	QList<QTextEdit::ExtraSelection> extraSelections;
	m_pCompileTextWidget->setExtraSelections(extraSelections);
	m_pCompileTextWidget->unsetCursor();
	m_pCompileTextWidget->setPlainText(messages.c_str());
}


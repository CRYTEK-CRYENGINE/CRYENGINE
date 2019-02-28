// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorFramework/Editor.h"
#include "CSharpEditorPlugin.h"
#include "CSharpOutputTextEdit.h"

//! Window designed to show compile messages from the CompiledMonoLibrary.
class CCSharpOutputWindow 
	: public CDockableEditor
	, public ICSharpMessageListener
	, public ITextEventListener
{
public:
	CCSharpOutputWindow();
	~CCSharpOutputWindow();

	virtual const char* GetEditorName() const override { return "C# Output"; };

	// ICSharpMessageListener
	virtual void OnMessagesUpdated(string messages) override;
	// ~ICSharpMessageListener

	// ITextEventListener
	virtual void OnMessageDoubleClicked(QMouseEvent* event) override;
	virtual bool OnMouseMoved(QMouseEvent* event) override;
	// ~ITextEventListener

private:
	CCSharpOutputTextEdit* m_pCompileTextWidget;
	string m_plainText;

	string GetLineFromRange(const string& text, int lineStart, int lineEnd)
	{
		string line = text.substr(lineStart, lineEnd - lineStart);
		line.Trim();
		return line;
	}

	void GetLineRange(const string& text, int index, int& lineStart, int& lineEnd);
	bool TryParseStackTraceLine(const string& line, string& path, int& lineNumber);
};

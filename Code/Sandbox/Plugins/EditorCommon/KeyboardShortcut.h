// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"

#include <QKeySequence>

class EDITOR_COMMON_API CKeyboardShortcut
{
public:

	//Standard shortcuts that will change depending on the platform.
	//Here described with their Windows shortcut for reference
	//For further information look:
	//http://doc.qt.io/qt-5/qkeysequence.html#StandardKey-enum
	//Note that CryENGINE may intentionally override default shortcuts from Qt and use its own semantics for standard keys. This is not meant to be a 1 to 1 mapping
	//To change the behavior of those keys for a specific platform, change where they are interpreted by the widget toolkit (currently QtUtil.cpp)
	enum class StandardKey
	{
		HelpWiki,     //F1, Opens the relevant wiki page
		HelpToolTip,  //Shift+F1, Opens a help tooltip that is more detailed than the basic tooltip
		Open,         //Ctrl+O
		Close,        //Ctrl+W
		Save,         //Ctrl+S
		SaveAs,       //No shortcut on windows
		New,          //Ctrl+N
		Delete,       //Del
		Cut,          //Ctrl+X
		Copy,         //Ctrl+C
		Paste,        //Ctrl+V
		Undo,         //Ctrl+Z
		Redo,         //Ctrl+Y
		Back,         //Alt+Left //TODO : bind on "back" key
		Forward,      //Alt+Right //TODO : bind on "forward" key
		Refresh,      //Ctrl+R
		ZoomIn,       //Ctrl+Plus
		ZoomOut,      //Ctrl+Minus
		Print,        //Ctrl+P
		AddTab,       //Ctrl+T
		Find,         //Ctrl+F
		FindNext,     //F3
		FindPrevious, //Shift+F3
		Replace,      //Ctrl+H
		SelectAll,    //Ctrl+A
		SelectNone,   //Ctrl+Shift+A
		Preferences,  //No shortcut on windows
		Quit,         //Alt+F4
		FullScreen,   //Alt+Enter

		Count
	};

	static StandardKey StringToStandardKey(const char* str);
	

	//Shortcut definition by string follows Qt convention:
	//http://doc.qt.io/qt-5/qkeysequence.html

	CKeyboardShortcut()
		: m_key(StandardKey::Count)
	{}

	CKeyboardShortcut(const char* string)
		: m_key(StandardKey::Count)
		, m_string(string)
	{}

	CKeyboardShortcut(StandardKey key)
		: m_key(key)
	{}

	bool          IsStandardKey() const { return m_key != StandardKey::Count; }
	StandardKey   GetKey() const        { return m_key; }
	const string& GetString() const     { return m_string; }
	QList<QKeySequence> ToKeySequence() const;

private:

	StandardKey m_key;
	string      m_string;
};


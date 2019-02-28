// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "KeyboardShortcut.h"

CKeyboardShortcut::StandardKey CKeyboardShortcut::StringToStandardKey(const char* str)
{
	if (stricmp(str, STRINGIFY(HelpWiki)) == 0) return StandardKey::HelpWiki;
	else if (stricmp(str, STRINGIFY(HelpToolTip)) == 0) return StandardKey::HelpToolTip;
	else if (stricmp(str, STRINGIFY(Open)) == 0) return StandardKey::Open;
	else if (stricmp(str, STRINGIFY(Close)) == 0) return StandardKey::Close;
	else if (stricmp(str, STRINGIFY(Save)) == 0) return StandardKey::Save;
	else if (stricmp(str, STRINGIFY(SaveAs)) == 0) return StandardKey::SaveAs;
	else if (stricmp(str, STRINGIFY(New)) == 0) return StandardKey::New;
	else if (stricmp(str, STRINGIFY(Delete)) == 0) return StandardKey::Delete;
	else if (stricmp(str, STRINGIFY(Cut)) == 0) return StandardKey::Cut;
	else if (stricmp(str, STRINGIFY(Copy)) == 0) return StandardKey::Copy;
	else if (stricmp(str, STRINGIFY(Paste)) == 0) return StandardKey::Paste;
	else if (stricmp(str, STRINGIFY(Undo)) == 0) return StandardKey::Undo;
	else if (stricmp(str, STRINGIFY(Redo)) == 0) return StandardKey::Redo;
	else if (stricmp(str, STRINGIFY(Back)) == 0) return StandardKey::Back;
	else if (stricmp(str, STRINGIFY(Forward)) == 0) return StandardKey::Forward;
	else if (stricmp(str, STRINGIFY(Refresh)) == 0) return StandardKey::Refresh;
	else if (stricmp(str, STRINGIFY(ZoomIn)) == 0) return StandardKey::ZoomIn;
	else if (stricmp(str, STRINGIFY(ZoomOut)) == 0) return StandardKey::ZoomOut;
	else if (stricmp(str, STRINGIFY(Print)) == 0) return StandardKey::Print;
	else if (stricmp(str, STRINGIFY(AddTab)) == 0) return StandardKey::AddTab;
	else if (stricmp(str, STRINGIFY(Find)) == 0) return StandardKey::Find;
	else if (stricmp(str, STRINGIFY(FindNext)) == 0) return StandardKey::FindNext;
	else if (stricmp(str, STRINGIFY(FindPrevious)) == 0) return StandardKey::FindPrevious;
	else if (stricmp(str, STRINGIFY(Replace)) == 0) return StandardKey::Replace;
	else if (stricmp(str, STRINGIFY(SelectAll)) == 0) return StandardKey::SelectAll;
	else if (stricmp(str, STRINGIFY(SelectNone)) == 0) return StandardKey::SelectNone;
	else if (stricmp(str, STRINGIFY(Preferences)) == 0) return StandardKey::Preferences;
	else if (stricmp(str, STRINGIFY(Quit)) == 0) return StandardKey::Quit;
	else if (stricmp(str, STRINGIFY(FullScreen)) == 0) return StandardKey::FullScreen;
	else return StandardKey::Count;
}

QList<QKeySequence> CKeyboardShortcut::ToKeySequence() const
{
	//Note : Some of the shortcuts may not be desired, specialize per platform to control how the standard shortcuts behave
	if (IsStandardKey())
	{
		switch (GetKey())
		{
		case CKeyboardShortcut::StandardKey::HelpWiki:
			return QKeySequence::keyBindings(QKeySequence::HelpContents);
		case CKeyboardShortcut::StandardKey::HelpToolTip:
			return QKeySequence::keyBindings(QKeySequence::WhatsThis);
		case CKeyboardShortcut::StandardKey::Open:
			return QKeySequence::keyBindings(QKeySequence::Open);
		case CKeyboardShortcut::StandardKey::Close:
		{
			QList<QKeySequence> list;
			list.append(QKeySequence("Ctrl+W"));
			return list;
		}
		case CKeyboardShortcut::StandardKey::Save:
			return QKeySequence::keyBindings(QKeySequence::Save);
		case CKeyboardShortcut::StandardKey::SaveAs:
			return QKeySequence::keyBindings(QKeySequence::SaveAs);
		case CKeyboardShortcut::StandardKey::New:
			return QKeySequence::keyBindings(QKeySequence::New);
		case CKeyboardShortcut::StandardKey::Delete:
			return QKeySequence::keyBindings(QKeySequence::Delete);
		case CKeyboardShortcut::StandardKey::Cut:
			return QKeySequence::keyBindings(QKeySequence::Cut);
		case CKeyboardShortcut::StandardKey::Copy:
			return QKeySequence::keyBindings(QKeySequence::Copy);
		case CKeyboardShortcut::StandardKey::Paste:
			return QKeySequence::keyBindings(QKeySequence::Paste);
		case CKeyboardShortcut::StandardKey::Undo:
			return QKeySequence::keyBindings(QKeySequence::Undo);
		case CKeyboardShortcut::StandardKey::Redo:
			return QKeySequence::keyBindings(QKeySequence::Redo);
		case CKeyboardShortcut::StandardKey::Back:
			return QKeySequence::keyBindings(QKeySequence::Back);
		case CKeyboardShortcut::StandardKey::Forward:
			return QKeySequence::keyBindings(QKeySequence::Forward);
		case CKeyboardShortcut::StandardKey::Refresh:
		{
			QList<QKeySequence> list;
			list.append(QKeySequence("Ctrl+R"));
			return list;
		}
		case CKeyboardShortcut::StandardKey::ZoomIn:
			return QKeySequence::keyBindings(QKeySequence::ZoomIn);
		case CKeyboardShortcut::StandardKey::ZoomOut:
			return QKeySequence::keyBindings(QKeySequence::ZoomOut);
		case CKeyboardShortcut::StandardKey::Print:
			return QKeySequence::keyBindings(QKeySequence::Print);
		case CKeyboardShortcut::StandardKey::AddTab:
			return QKeySequence::keyBindings(QKeySequence::AddTab);
		case CKeyboardShortcut::StandardKey::Find:
			return QKeySequence::keyBindings(QKeySequence::Find);
		case CKeyboardShortcut::StandardKey::FindNext:
		{
			QList<QKeySequence> list;
			list.append(QKeySequence("F3"));
			return list;
		}
		case CKeyboardShortcut::StandardKey::FindPrevious:
		{
			QList<QKeySequence> list;
			list.append(QKeySequence("Shift+F3"));
			return list;
		}
		case CKeyboardShortcut::StandardKey::Replace:
			return QKeySequence::keyBindings(QKeySequence::Replace);
		case CKeyboardShortcut::StandardKey::SelectAll:
			return QKeySequence::keyBindings(QKeySequence::SelectAll);
		case CKeyboardShortcut::StandardKey::SelectNone:
			return QKeySequence::keyBindings(QKeySequence::Deselect);
		case CKeyboardShortcut::StandardKey::Preferences:
			return QKeySequence::keyBindings(QKeySequence::Preferences);
		case CKeyboardShortcut::StandardKey::Quit:
			return QKeySequence::keyBindings(QKeySequence::Quit);
		case CKeyboardShortcut::StandardKey::FullScreen:
			return QKeySequence::keyBindings(QKeySequence::FullScreen);
		default:
			break;
		}
	}
	else if (!GetString().empty())
	{
		return QKeySequence::listFromString(GetString().c_str());
	}

	return QList<QKeySequence>();
}


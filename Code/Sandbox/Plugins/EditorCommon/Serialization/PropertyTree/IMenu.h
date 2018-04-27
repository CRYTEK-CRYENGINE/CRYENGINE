// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "sigslot.h"
#include "Rect.h"
#include "IUIFacade.h"
#include "IDrawContext.h"

namespace property_tree {


enum
{
	MENU_DISABLED = 1 << 0,
	MENU_DEFAULT = 1 << 1
};

class IMenuAction
{
public:
	sigslot::signal0 signalTriggered;
};

class IMenu
{
public:
	virtual ~IMenu() {}
	virtual bool isEmpty() = 0;
	virtual IMenu* addMenu(const char* text) = 0;
	virtual IMenu* findMenu(const char* text) = 0;
	virtual void addSeparator() = 0;
	virtual IMenuAction* addAction(const Icon& icon, const char* text, int flags = 0) = 0;
	virtual void exec(const Point& point) = 0;

	void addAction(const char* text, int flags = 0)
	{
		addAction(Icon(), text, flags);
	}

	template<class T>
	void addAction(const char* text, const char* shortcut, int flags, T* obj, void(T::*handler)())
	{
		IMenuAction* command = addAction(Icon(), text, flags);
		command->signalTriggered.connect(obj, handler);
	}

	template<class T>
	void addAction(const Icon& icon, const char* text, const char* shortcut, int flags, T* obj, void(T::*handler)())
	{
		IMenuAction* command = addAction(icon, text, flags);
		command->signalTriggered.connect(obj, handler);
	}

	template<class T>
	void addAction(const char* text, int flags, T* obj, void(T::*handler)())
	{
		IMenuAction* command = addAction(Icon(), text, flags);
		command->signalTriggered.connect(obj, handler);
	}

};
}


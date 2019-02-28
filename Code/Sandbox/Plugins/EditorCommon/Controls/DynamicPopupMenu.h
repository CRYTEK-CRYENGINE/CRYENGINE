// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

class CDynamicPopupMenu;
class QMenu;

template<class Arg1> class CPopupMenuItem1;
template<class Arg1, class Arg2> class CPopupMenuItem2;
template<class Arg1, class Arg2, class Arg3> class CPopupMenuItem3;

//TODO : solve the mystery : using _i_reference_target_t produces linker error
class EDITOR_COMMON_API CPopupMenuItem
{
	friend class CDynamicPopupMenu;
	friend class QPopupMenuItemAction;
public:
	typedef std::vector<std::shared_ptr<CPopupMenuItem> > Children;

	virtual ~CPopupMenuItem();

	CPopupMenuItem& Check(bool checked = true){ m_checked = checked; return *this; }
	bool IsChecked() const{ return m_checked; }

	CPopupMenuItem& Enable(bool enabled = true){ m_enabled = enabled; return *this; }
	bool IsEnabled() const{ return m_enabled; }

	void SetDefault(bool defaultItem = true){ m_default = defaultItem; }
	bool IsDefault() const{ return m_default; }

	const char* Text() { return m_text.c_str(); }
	CPopupMenuItem& AddSeparator();

	//! Adds a parent menu which should host a submenu as child items
	CPopupMenuItem& Add(const char* text);

	//! Please do not use other methods, std::function and std::bind make this the only Add method necessary
	CPopupMenuItem& Add(const char* text, const std::function<void()>& function);
	
	CPopupMenuItem& Add(const char* text, const char* icon,const std::function<void()>& function);

	//! Useful if a menu item is tied to an editor command. The menu item will then use all the properties of the editor command and its ui action
	CPopupMenuItem& AddCommand(const char* text, string commandToExecute);

	//! Deprecated method
	/*CPopupMenuItem& Add(const char* text, const Functor0& functor);*/

	//! Deprecated method
	template<class Arg1>
	CPopupMenuItem& Add(const char* text, const Functor1<Arg1>& functor, const Arg1& arg1);

	//! Deprecated method
	template<class Arg1, class Arg2>
	CPopupMenuItem& Add(const char* text, const Functor2<Arg1, Arg2>& functor, const Arg1& arg1, const Arg2& arg2);

	//! Deprecated method
	template<class Arg1, class Arg2, class Arg3>
	CPopupMenuItem& Add(const char* text, const Functor3<Arg1, Arg2, Arg3>& functor, const Arg1& arg1, const Arg2& arg2, const Arg3& arg3);

	CPopupMenuItem* Find(const char* text);

	CPopupMenuItem* GetParent() { return m_parent; }
	const Children& GetChildren() const { return m_children; }
	bool Empty() const { return m_children.empty(); }

	//! sets mouse hover function
	void SetHoverFunction(std::function<void()> hoverFunc)	{ m_hoverFunc = hoverFunc; }

	//TODO : handle hotkeys here
	//void SetHotkey(const CKeyboardShortcut& hotkey) { m_hotkey = hotkey; }

protected:

	CPopupMenuItem(const char* text)
		: m_parent()
		, m_text(text)
		, m_checked(false)
		, m_enabled(true)
		, m_default(false)
	{}

	void AddChildren(CPopupMenuItem* item)
	{
		m_children.push_back(std::shared_ptr<CPopupMenuItem>(item));
		item->m_parent = this;
	}

	void Call()
	{
		if (m_function)
			m_function();
	}

	void Hover()
	{
		if (m_hoverFunc)
			m_hoverFunc();
	}

	virtual bool IsEditorCommand() { return false; }

	Children& GetChildren() { return m_children; };

	bool m_default;
	bool m_checked;
	bool m_enabled;
	string m_text;
	string m_icon;
	CPopupMenuItem* m_parent;
	Children m_children;
	std::function<void()> m_function;
	std::function<void()> m_hoverFunc;
};

template<class Arg1>
CPopupMenuItem& CPopupMenuItem::Add(const char* text, const Functor1<Arg1>& functor, const Arg1& arg1)
{
	return Add(text, std::bind(functor, arg1));
}

template<class Arg1, class Arg2>
CPopupMenuItem& CPopupMenuItem::Add(const char* text, const Functor2<Arg1, Arg2>& functor, const Arg1& arg1, const Arg2& arg2)
{
	return Add(text, std::bind(functor, arg1, arg2));
}

template<class Arg1, class Arg2, class Arg3>
CPopupMenuItem& CPopupMenuItem::Add(const char* text, const Functor3<Arg1, Arg2, Arg3>& functor, const Arg1& arg1, const Arg2& arg2, const Arg3& arg3)
{
	return Add(text, std::bind(functor, arg1, arg2, arg3));
}

//! Used to fill a menu without making code dependent on QT. Can be very useful if you want to implement menu as a lib-independent visitor-like pattern.
class EDITOR_COMMON_API CDynamicPopupMenu
{
public:
	CDynamicPopupMenu();
	CPopupMenuItem& GetRoot() { return m_root; };
	const CPopupMenuItem& GetRoot() const { return m_root; };

	bool IsEmpty() { return m_root.Empty(); }
	void Clear();

	//Qt Menu
	class QMenu* CreateQMenu();
	void PopulateQMenu(class QMenu* menu);
	void PopulateQMenu(class QMenu* menu, CPopupMenuItem* parentItem);
	void Spawn( int x,int y );
	void SpawnAtCursor();

	void SetOnHideFunctor(std::function<void()> functor) { m_onHide = functor; }

private:
	CPopupMenuItem* NextItem(CPopupMenuItem* item) const;
	CPopupMenuItem m_root;

	std::function<void()> m_onHide;
};


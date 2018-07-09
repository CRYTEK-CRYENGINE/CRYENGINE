// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include "BroadcastManager.h"
#include "QtViewPane.h"
#include "DockingSystem/DockableContainer.h"
#include "Menu/MenuDesc.h"

#include <QWidget>
#include <QVariant>
#include <QMenuBar>

class CAbstractMenu;
class CMenuBarUpdater;

class QEvent;

//! Base class for Editor(s) which means this should be the base class 
//! for most if not all tools that are not simply dialogs
//! Prefer inheriting from CDockableEditor if this needs to be dockable
//! Inherit from CAssetEditor if this needs to be integrated with the AssetSystem
class EDITOR_COMMON_API CEditor : public QWidget
{
	Q_OBJECT
public:
	CEditor(QWidget* pParent = nullptr, bool bIsOnlyBackend = false);
	virtual ~CEditor();

	virtual void        customEvent(QEvent* event) override;
	virtual const char* GetEditorName() const = 0;

	CBroadcastManager&  GetBroadcastManager();

	// Serialized in the layout
	virtual void        SetLayout(const QVariantMap& state);
	virtual QVariantMap GetLayout() const;

public slots:
	virtual QMenuBar* GetMenuBar();

protected:
	void                    AddRecentFile(const QString& filePath);
	QStringList				GetRecentFiles();

	

	// Serialized in personalization
	//Properties that can be shared between all projects
	void            SetProperty(const QString& propName, const QVariant& value);
	const QVariant& GetProperty(const QString& propName);
	//Properties that belong to a project (ex: recent files are only meaningful in the context of a single project)
	void            SetProjectProperty(const QString& propName, const QVariant& value);
	const QVariant& GetProjectProperty(const QString& propName);

	template<class T>
	bool GetProperty(const QString& propName, T& outValue)
	{
		const QVariant& variant = GetProperty(propName);
		if (variant.isValid() && variant.canConvert<T>())
		{
			outValue = variant.value<T>();
			return true;
		}
		return false;
	}

	void               SetPersonalizationState(const QVariantMap& state);
	const QVariantMap& GetPersonalizationState();

	virtual void       SetContent(QWidget* content);
	virtual void       SetContent(QLayout* content);

	//! If returning false, this method prevents the Sandbox from closing this editor.
	//! Note that this might prevent the Sandbox from exiting.
	virtual bool CanQuit(std::vector<string>& unsavedChanges) { return true; }

	//Behavior override methods

	//Temporary method meant to be used only when the editor is not used for its QWidget frontend but only for its backend
	//Mainly intended at transitioning existing editors in particular the level editor
	virtual bool IsOnlyBackend() const { return m_bIsOnlybackend; }

	//File menu methods
	virtual bool OnNew()                         { return false; }
	virtual bool OnOpen()                        { return false; }
	virtual bool OnOpenFile(const QString& path) { CRY_ASSERT(0); return false; }
	virtual bool OnClose()                       { return false; }
	virtual bool OnSave()                        { return false; }
	virtual bool OnSaveAs()                      { return false; }

	//Edit menu methods
	virtual bool OnUndo()                        { return false; }
	virtual bool OnRedo()                        { return false; }
	virtual bool OnCopy()                        { return false; }
	virtual bool OnCut()                         { return false; }
	virtual bool OnPaste()                       { return false; }
	virtual bool OnDelete()                      { return false; }
	virtual bool OnFind()                        { return false; }
	//By default, these call OnFind.
	//if FindNext() is called before Find(), it is expected that the Find functionnality is called instead
	virtual bool OnFindPrevious()                { return OnFind(); }
	virtual bool OnFindNext()                    { return OnFind(); }
	virtual bool OnDuplicate()                   { return false; }
	virtual bool OnSelectAll()                   { return false; }
	virtual bool OnHelp();

	//View menu methods
	virtual bool OnZoomIn()                      { return false; }
	virtual bool OnZoomOut()                     { return false; }

	enum class MenuItems : uint8
	{
		//Top level menus
		FileMenu,
		EditMenu,
		ViewMenu,
		WindowMenu,
		HelpMenu,

		//File menu contents
		New,
		Open,
		Close,
		Save,
		SaveAs,
		RecentFiles,

		//Edit menu contents
		Undo,
		Redo,
		Copy,
		Cut,
		Paste,
		Delete,
		Find,
		FindPrevious,
		FindNext,
		SelectAll,
		Duplicate,

		//View menu contents
		ZoomIn,
		ZoomOut,

		//Help menu contents
		Help,
	};

	//! Add one of the predefined menu items to the menu
	void AddToMenu(MenuItems item);
	//! Add an array of the predefined menu items to the menu
	template<int N>
	void AddToMenu(const MenuItems(&items)[N])
	{
		AddToMenu(&items[0], N);
	}
	void AddToMenu(const MenuItems* items, int count);
	//! Add an array of the predefined menu items to the menu
	void AddToMenu(const std::vector<MenuItems>& items);
	//! Add an editor command to the menu
	//TODO : make it possible to change the UI name and icon of the command while keeping shortcut
	void AddToMenu(CAbstractMenu* pMenu, const char* command);
	//! Add an editor command to the menu
	void AddToMenu(const char* menuName, const char* command);

	//! Returns the default action for a command
	QAction* GetAction(const char* command);

	//Only use this for menus that are not handled generically by CEditor

	//! Returns the abstract menu used to populate the menu bar of this editor
	CAbstractMenu* GetRootMenu();

	//! Returns a top menu with name \p menu, if it already exists; or returns a newly created menu, otherwise. (Overload useful for tr() strings)
	CAbstractMenu* GetMenu(const QString& menuName);
	//! Returns a top menu with name \p menu, if it already exists; or returns a newly created menu, otherwise.
	CAbstractMenu* GetMenu(const char* menuName);
	CAbstractMenu* GetMenu(MenuItems menuItem);

	//! Enable the inner docking system for this tool.
	//! See methods below for configuration and usage
	void EnableDockingSystem();

	//! Use this method to register all possible dockable widgets for this editor
	//! isUnique: only one of these widgets can be spawned
	//! isInternal: the widget is not in the menu 
	void RegisterDockableWidget(QString name, std::function<QWidget * ()> factory, bool isUnique = false, bool isInternal = false);

	//! Implement this method and use CDockableContainer::SpawnWidget to define the default layout
	virtual void CreateDefaultLayout(CDockableContainer* sender) { CRY_ASSERT_MESSAGE(false, "Not implemented"); };

	//! React on docking layout change
	virtual void OnLayoutChange(const QVariantMap& state);

private:
	void PopulateRecentFilesMenu(CAbstractMenu* menu);
	void OnMainFrameAboutToClose(BroadcastEvent& event);

	void InitMenuDesc();

protected:
	QMenuBar* m_pMenuBar;

	CBroadcastManager* m_broadcastManager;
	bool               m_bIsOnlybackend;

private:
	CDockableContainer* m_dockingRegistry;
	QObject* m_pBroadcastManagerFilter;
	std::unique_ptr<MenuDesc::CDesc<MenuItems>> m_pMenuDesc;
	std::unique_ptr<CAbstractMenu> m_pMenu;
	std::unique_ptr<CMenuBarUpdater> m_pMenuBarBar;
};

//! Inherit from this class to create a dockable editor
class EDITOR_COMMON_API CDockableEditor : public CEditor, public IPane
{
public:
	CDockableEditor(QWidget* pParent = nullptr);
	virtual ~CDockableEditor();

	virtual QWidget*    GetWidget() final { return this; };
	virtual const char* GetPaneTitle() const final { return GetEditorName(); };
	virtual QVariantMap GetState() const final { return GetLayout(); }
	virtual void        SetState(const QVariantMap& state) final { SetLayout(state); }
	virtual void        LoadLayoutPersonalization();
	virtual void        SaveLayoutPersonalization();

	//! Brings this Editor on top of all otherWindows
	void Raise();

	//! UI effect that attracts attention to this window.
	void Highlight();

protected:
	void InstallReleaseMouseFilter(QObject* object);

private:    
	QObject* m_pReleaseMouseFilter;

};


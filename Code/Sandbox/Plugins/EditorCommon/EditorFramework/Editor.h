// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

// EditorCommon
#include "EditorCommonAPI.h"
#include "EditorWidget.h"
#include "DockingSystem/DockableContainer.h"
#include "Menu/MenuDesc.h"
#include "QtViewPane.h"

// CryCommon
#include <CrySandbox/CrySignal.h>

// Qt
#include <QVariant>

class BroadcastEvent;
class CAbstractMenu;
class CBroadcastManager;
class CEditorContent;
class CToolBarArea;
class CMenuUpdater;
class QEvent;

//! Base class for Editor(s) which means this should be the base class
//! for most if not all tools that are not simply dialogs
//! Prefer inheriting from CDockableEditor if this needs to be dockable
//! Inherit from CAssetEditor if this needs to be integrated with the AssetSystem
class EDITOR_COMMON_API CEditor : public CEditorWidget
{
	Q_OBJECT
public:
	CEditor(QWidget* pParent = nullptr, bool bIsOnlyBackend = false);
	virtual ~CEditor();

	virtual void            Initialize();

	virtual const char*     GetEditorName() const = 0;

	// Editor orientation when using adaptive layouts
	Qt::Orientation         GetOrientation() const        { return m_currentOrientation; }
	// Used for determining what layout direction to use if adaptive layout is turned off
	virtual Qt::Orientation GetDefaultOrientation() const { return Qt::Horizontal; }

	CBroadcastManager&      GetBroadcastManager();

	// Serialized in the layout
	virtual void        SetLayout(const QVariantMap& state);
	virtual QVariantMap GetLayout() const;

	enum class MenuItems : uint8
	{
		//Top level menus
		FileMenu,
		EditMenu,
		ViewMenu,
		WindowMenu,
		HelpMenu,
		ToolBarMenu,

		//File menu contents
		New,
		NewFolder,
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
		Rename,
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

	//! Returns a top menu with name \p menu, if it already exists; or returns a newly created menu, otherwise. (Overload useful for tr() strings)
	CAbstractMenu* GetMenu(const QString& menuName);
	//! Returns a top menu with name \p menu, if it already exists; or returns a newly created menu, otherwise.
	CAbstractMenu* GetMenu(const char* menuName);
	CAbstractMenu* GetMenu(MenuItems menuItem);

protected:
	void        customEvent(QEvent* pEvent) override;
	void        resizeEvent(QResizeEvent* pEvent) override;

	void        AddRecentFile(const QString& filePath);
	QStringList GetRecentFiles();

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

	// Must be overridden to add handling of adaptive layouts. Adaptive layouts enables editor owners to make better use of space
	virtual bool SupportsAdaptiveLayout() const  { return false; }
	bool         IsAdaptiveLayoutEnabled() const { return SupportsAdaptiveLayout() && m_isAdaptiveLayoutEnabled; }
	// Triggered on resize for editors that support adaptive layouts
	virtual void OnAdaptiveLayoutChanged();

	virtual void SetContent(QWidget* content);
	virtual void SetContent(QLayout* content);

	//! If returning false, this method prevents the Sandbox from closing this editor.
	//! Note that this might prevent the Sandbox from exiting.
	virtual bool CanQuit(std::vector<string>& unsavedChanges) { return true; }

	//Behavior override methods

	//Temporary method meant to be used only when the editor is not used for its QWidget frontend but only for its backend
	//Mainly intended at transitioning existing editors in particular the level editor
	virtual bool IsOnlyBackend() const { return m_bIsOnlybackend; }

	//File menu methods
	virtual bool OnOpenFile(const QString& path) { CRY_ASSERT(0); return false; }

	//Edit menu methods
	virtual bool OnFind()   { return false; }
	//By default, these call OnFind.
	//if FindNext() is called before Find(), it is expected that the Find functionnality is called instead
	virtual bool OnFindPrevious() { return OnFind(); }
	virtual bool OnFindNext()     { return OnFind(); }
	virtual bool OnHelp();

	//! Returns the default action for a command from editor's menu.
	QCommandAction* GetMenuAction(MenuItems item);

	//Only use this for menus that are not handled generically by CEditor

	//! Returns the abstract menu used to populate the menu bar of this editor
	CAbstractMenu* GetRootMenu();

	//! Enable the inner docking system for this tool.
	//! See methods below for configuration and usage
	void EnableDockingSystem();

	//! Use this method to register all possible dockable widgets for this editor
	//! isUnique: only one of these widgets can be spawned
	//! isInternal: the widget is not in the menu
	void RegisterDockableWidget(QString name, std::function<QWidget*()> factory, bool isUnique = false, bool isInternal = false);

	//! Implement this method and use CDockableContainer::SpawnWidget to define the default layout
	virtual void CreateDefaultLayout(CDockableContainer* sender) { CRY_ASSERT_MESSAGE(false, "Not implemented"); }

	//! React on docking layout change
	virtual void OnLayoutChange(const QVariantMap& state);

	void         ForceRebuildMenu();

private:
	void InitActions();

	void PopulateRecentFilesMenu(CAbstractMenu* menu);
	void OnMainFrameAboutToClose(BroadcastEvent& event);

	void InitMenuDesc();

	void UpdateAdaptiveLayout();
	void SetAdaptiveLayoutEnabled(bool enable);

public:
	CCrySignal<void(Qt::Orientation)> signalAdaptiveLayoutChanged;

protected:
	CBroadcastManager*  m_broadcastManager;
	bool                m_bIsOnlybackend;
	QMenu*              m_pPaneMenu;
	CDockableContainer* m_dockingRegistry;

private:
	QObject*                                    m_pBroadcastManagerFilter;
	CEditorContent*                             m_pEditorContent;
	std::unique_ptr<MenuDesc::CDesc<MenuItems>> m_pMenuDesc;
	std::unique_ptr<CAbstractMenu>              m_pMenu;
	std::unique_ptr<CMenuUpdater>               m_pMenuUpdater;

	QCommandAction*                             m_pActionAdaptiveLayout;
	Qt::Orientation                             m_currentOrientation;
	bool m_isAdaptiveLayoutEnabled;
};

//! Inherit from this class to create a dockable editor
class EDITOR_COMMON_API CDockableEditor : public CEditor, public IPane
{
	Q_OBJECT;
	Q_INTERFACES(IPane);
public:
	CDockableEditor(QWidget* pParent = nullptr);
	virtual ~CDockableEditor();

	virtual void        Initialize() override                    { CEditor::Initialize(); }
	virtual QWidget*    GetWidget() final                        { return this; }
	virtual QMenu*      GetPaneMenu() const override;
	virtual const char* GetPaneTitle() const final               { return GetEditorName(); }
	virtual QVariantMap GetState() const final                   { return GetLayout(); }
	virtual void        SetState(const QVariantMap& state) final { SetLayout(state); }
	virtual void        LoadLayoutPersonalization();
	virtual void        SaveLayoutPersonalization();

	virtual std::vector<IPane*> GetSubPanes() const override 
	{ 
		return m_dockingRegistry ? m_dockingRegistry->GetPanes() : std::vector<IPane*>();
	}

	//! Brings this Editor on top of all otherWindows
	void Raise();

	//! UI effect that attracts attention to this window.
	void Highlight();

protected:
	void InstallReleaseMouseFilter(QObject* object);

private:
	QObject* m_pReleaseMouseFilter;
};

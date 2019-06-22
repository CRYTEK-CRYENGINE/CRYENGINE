// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

#include "Commands/ICommandManager.h"
#include "EditorFramework/StateSerializable.h"
#include "IEditor.h"
#include "IEditorClassFactory.h"

#include <QMainWindow>
#include <QMenu>
#include <QRect>
#include <QSize>
#include <QWidget>

struct IPane;

struct IViewPaneClass : public IClassDesc
{
	enum EDockingDirection
	{
		DOCK_DEFAULT = 0,
		DOCK_TOP,
		DOCK_LEFT,
		DOCK_RIGHT,
		DOCK_BOTTOM,
		DOCK_FLOAT,
	};
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_VIEWPANE; }

	// Get view pane window runtime class.
	virtual CRuntimeClass* GetRuntimeClass() = 0;

	// Return text for view pane title.
	virtual const char* GetPaneTitle() = 0;

	// Return true if only one pane at a time of time view class can be created.
	virtual bool SinglePane() = 0;

	//! Notifies the application if it should create an entry in the tools menu for this pane
	virtual bool NeedsMenuItem() { return true; }

	//! This method returns the tools menu path where the tool can be spawned from.
	virtual const char* GetMenuPath() { return ""; }

	//////////////////////////////////////////////////////////////////////////
	// Creates a Qt QWidget for this ViewPane
	virtual IPane* CreatePane() const { return 0; }
};

struct EDITOR_COMMON_API IPane : IStateSerializable
{
	static CCrySignal<void(IPane*)> s_signalPaneCreated;

	virtual ~IPane() {}

	virtual void Initialize() { }

	virtual QWidget* GetWidget() = 0;

	// Return preferable initial docking position for pane.
	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const { return IViewPaneClass::DOCK_FLOAT; }

	// Return text for view pane title.
	virtual const char* GetPaneTitle() const = 0;

	virtual std::vector<IPane*> GetSubPanes() const { return {}; }

	// Initial pane size.
	virtual QRect GetPaneRect() { return QRect(0, 0, 800, 500); }

	// Get Minimal view size
	virtual QSize GetMinSize() { return QSize(0, 0); }

	// Save transient state
	virtual QVariantMap GetState() const { return QVariantMap(); }

	// Get pane menu
	virtual QMenu* GetPaneMenu() const
	{
		QMenu* pMainMenu = new QMenu();
		QMenu* pHelpMenu = pMainMenu->addMenu("Help");
		pHelpMenu->addAction(GetIEditor()->GetICommandManager()->GetAction("general.help"));
		return pMainMenu;
	}

	// Restore transient state
	virtual void SetState(const QVariantMap& state) {}

	//
	virtual void LoadLayoutPersonalization() {}

	//
	virtual void SaveLayoutPersonalization() {}
};
Q_DECLARE_INTERFACE(IPane, "EditorCommon/QTViewPane"); //Makes IPane known to QT, which is needed for qobject_cast<IPane*>

template<typename T>
class CDockableWidgetT : public T, public IPane
{
	static_assert(std::is_base_of<QWidget, T>::value, "T has to be QWidget");
public:
	CDockableWidgetT(QWidget* pParent = nullptr) : T(pParent)
	{
		QWidget::setAttribute(Qt::WA_DeleteOnClose);
	}
	virtual ~CDockableWidgetT() {}

	virtual QWidget* GetWidget() override { return this; }
};

//The split in two distinct classes is necessary, because QT can't handle Q_OBJECT in templated classes
class EDITOR_COMMON_API CDockableWidget : public CDockableWidgetT<QWidget>
{
	Q_OBJECT;
	Q_INTERFACES(IPane);
public:
	CDockableWidget(QWidget* pParent = nullptr) : CDockableWidgetT<QWidget>(pParent) {}
	virtual ~CDockableWidget() {}
};

//DEPREACTED, DO NOT USE THIS, QMainWindow is not a suitable base class for a dockable pane
class EDITOR_COMMON_API CDockableWindow : public CDockableWidgetT<QMainWindow>
{
	Q_OBJECT;
	Q_INTERFACES(IPane);
public:
	CDockableWindow(QWidget* pParent = nullptr) : CDockableWidgetT<QMainWindow>(pParent) {}
	virtual ~CDockableWindow() {}
};

// Registers the QWidget pane so that it can be opened in the editor as a tool
#define REGISTER_VIEWPANE_FACTORY_AND_MENU_IMPL(widget, name, category, unique, menuPath, needsItem) \
	class widget ## Pane : public IViewPaneClass                                                       \
	{                                                                                                  \
		virtual const char* ClassName() override { return name; }                                        \
		virtual const char* Category()  { return category; }                                             \
		virtual bool NeedsMenuItem() { return needsItem; }                                               \
		virtual const char* GetMenuPath() { return menuPath; }                                           \
		virtual CRuntimeClass* GetRuntimeClass() { return 0; }                                           \
		virtual const char* GetPaneTitle() override { return name; }                                     \
		virtual bool SinglePane()          override { return unique; }                                   \
		virtual IPane* CreatePane() const override { return new widget(); }                              \
	};                                                                                                 \
	REGISTER_CLASS_DESC(widget ## Pane);

#define REGISTER_VIEWPANE_FACTORY(widget, name, category, unique) \
	REGISTER_VIEWPANE_FACTORY_AND_MENU_IMPL(widget, name, category, unique, "", true)

#define REGISTER_VIEWPANE_FACTORY_AND_MENU(widget, name, category, unique, menuPath) \
	REGISTER_VIEWPANE_FACTORY_AND_MENU_IMPL(widget, name, category, unique, menuPath, true)

#define REGISTER_HIDDEN_VIEWPANE_FACTORY(widget, name, category, unique) \
	REGISTER_VIEWPANE_FACTORY_AND_MENU_IMPL(widget, name, category, unique, "", false)

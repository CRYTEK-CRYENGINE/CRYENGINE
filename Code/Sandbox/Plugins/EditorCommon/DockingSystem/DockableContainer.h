// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "EditorFramework/StateSerializable.h"
#include "QToolwindowManager/QToolWindowManager.h"
#include <functional>
#include <QWidget>
#include <QVariantMap>
#include <map>

class CAbstractMenu;

class EDITOR_COMMON_API CDockableContainer : public QWidget, public IStateSerializable
{
	Q_OBJECT;
	Q_INTERFACES(IStateSerializable);
	struct FactoryInfo
	{
		std::function<QWidget*()> m_factory;
		bool                      m_isUnique;
		bool                      m_isInternal;
		FactoryInfo() {};
		FactoryInfo(std::function<QWidget*()> factory, bool isUnique, bool isInternal);
	};
	struct WidgetInstance
	{
		QWidget* m_widget;
		QString  m_spawnName;
		WidgetInstance() {};
		WidgetInstance(QWidget* widget, QString spawnName);
	};

public:
	//! Constructor for CDockableContainer.
	//! \param parent Owner of this widget. Must implement IDockableProvider to register widgets, etc.
	//! \param startingLayout QVariantMap containing the layout that should be used instead of the default layout provided by the IDockableProvider.
	CDockableContainer(QWidget* parent, QVariantMap startingLayout = QVariantMap());

	~CDockableContainer();

	//! Register a widget for later spawning.
	//! The factory method \p factory must create a new instance. The dockable container will take
	//! ownership of that instance. In particular, resetting the layout will destroy all widgets
	//! owned by the dockable container. A widget must have a window title, as this is displayed in
	//! the dockable container's "Window" menu.
	//! \param name Unique name for the widget type. Used to spawn it later on; gets shown in the UI.
	//! \param factory A function which creates and returns a QWidget*.
	//! \param isUnique If true, only one instance may exist of this widget type.
	//! \param isInternal If true, this will not show up in the Window > Add tool menu.
	void Register(QString name, std::function<QWidget*()> factory, bool isUnique, bool isInternal);

	// Layout management
	virtual QVariantMap GetState() const override;
	virtual void        SetState(const QVariantMap& state) override;

	//! Spawn a widget of the named class and dock it at the specified location.
	//! Single-instance widgets will not have their position changes, but will be brought to front.
	QWidget* SpawnWidget(QString name, const QToolWindowAreaTarget& target);
	QWidget* SpawnWidget(QString name, QWidget* pTargetWidget, const QToolWindowAreaTarget& target);
	QWidget* SpawnWidget(QString name, IToolWindowArea* area = nullptr, QToolWindowAreaReference reference = QToolWindowAreaReference::Combine, int index = -1, QRect geometry = QRect());

	void     SetDefaultLayoutCallback(std::function<void(CDockableContainer*)> callback);
	void     SetMenu(CAbstractMenu* pAbstractMenu);

	//! Sets the sizes of a splitter in the layout.
	//! \param widget If widget is a QSplitter, sets the sizes directly on that splitter. Otherwise, sets the sizes on the nearest QSplitter parent of widget.
	//! \param sizes Specifies the sizes for each widget in the splitter.
	void SetSplitterSizes(QWidget* widget, QList<int> sizes);

	static QToolWindowManagerClassFactory* s_dockingFactory;

signals:
	void OnLayoutChange(const QVariantMap& layout);

private:
	typedef std::map<QString, FactoryInfo>    TNameMap;
	typedef std::map<QString, WidgetInstance> TWidgetMap;
	void showEvent(QShowEvent* event);

	// Extra spawning functions; wraps internal functionality
	QWidget* SpawnWidget(QString name, QString forceObjectName, const QToolWindowAreaTarget& target);
	QWidget* SpawnWidget(QString name, QString forceObjectName, IToolWindowArea* area = nullptr, QToolWindowAreaReference reference = QToolWindowAreaReference::Combine, int index = -1, QRect geometry = QRect());
	QWidget* Spawn(QString name, QString forceObjectName = QString());

	// Called when a widget gets removed/destroyed.
	void OnWidgetDestroyed(QObject* pObject);

	// Creates a unique name for a dockable widget.
	QString CreateObjectName(QString title) const;
	// Create and fill the window menu, if one is provided.
	void    BuildWindowMenu();
	// Resets the layout to the default.
	void    ResetLayout();
	TNameMap                                 m_registry;
	TWidgetMap                               m_spawned;
	QToolWindowManager*                      m_toolManager;
	std::function<void(CDockableContainer*)> m_defaultLayoutCallback;
	CAbstractMenu*							 m_pMenu;
	QMetaObject::Connection                  m_layoutChangedConnection;
	QVariantMap                              m_startingLayout;
};


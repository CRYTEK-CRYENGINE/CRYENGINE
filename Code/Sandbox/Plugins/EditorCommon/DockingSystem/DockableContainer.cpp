// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DockableContainer.h"
#include "QTrackingTooltip.h"
#include "Menu/AbstractMenu.h"

QToolWindowManagerClassFactory* CDockableContainer::s_dockingFactory = nullptr;

CDockableContainer::CDockableContainer(QWidget* parent, QVariantMap startingLayout /*= QVariantMap()*/)
	: QWidget(parent), m_startingLayout(startingLayout), m_toolManager(nullptr), m_defaultLayoutCallback(nullptr), m_pMenu(nullptr)
{
}

CDockableContainer::~CDockableContainer()
{
	if (m_toolManager)
	{
		disconnect(m_layoutChangedConnection);
	}

	for (auto& pair : m_spawned)
	{
		disconnect(pair.second.m_widget, 0, this, 0);
	}
	m_spawned.clear();
}

void CDockableContainer::Register(QString name, std::function<QWidget*()> factory, bool isUnique, bool isInternal)
{
	m_registry[name] = FactoryInfo(factory, isUnique, isInternal);
}

QWidget* CDockableContainer::Spawn(QString name, QString forceObjectName)
{
	auto it = m_registry.find(name);
	if (it != m_registry.end())
	{
		if (it->second.m_isUnique)
		{
			for (auto wi : m_spawned)
			{
				if (wi.second.m_spawnName == name)
				{
					// Already created, return existing instance
					return wi.second.m_widget;
				}	
			}
		}
		QWidget* w = it->second.m_factory();
		if (forceObjectName.isEmpty())
		{
			w->setObjectName(CreateObjectName(name));
		}
		else
		{
			w->setObjectName(forceObjectName);
		}
		m_spawned[w->objectName()] = WidgetInstance(w, name);
		connect(w, &QObject::destroyed, this, &CDockableContainer::OnWidgetDestroyed);

		return w;
	}
	return nullptr;
}

QWidget* CDockableContainer::SpawnWidget(QString name, const QToolWindowAreaTarget& target)
{
	return SpawnWidget(name, "", target);
}

QWidget* CDockableContainer::SpawnWidget(QString name, QWidget* pTargetWidget, const QToolWindowAreaTarget& target)
{
	IToolWindowArea* pToolArea = m_toolManager->areaOf(pTargetWidget);
	if (pToolArea)
	{
		return SpawnWidget(name, "", pToolArea, target.reference, target.index, target.geometry);
	}
	return nullptr;
}

QWidget* CDockableContainer::SpawnWidget(QString name, QString objectName, const QToolWindowAreaTarget& target)
{
	return SpawnWidget(name, objectName, target.area, target.reference, target.index, target.geometry);
}

QWidget* CDockableContainer::SpawnWidget(QString name, IToolWindowArea* area /*= nullptr*/, QToolWindowAreaReference reference /*= QToolWindowAreaTarget::Combine*/, int index /*= -1*/, QRect geometry /*= QRect()*/)
{
	return SpawnWidget(name, "", area, reference, index, geometry);
}

QWidget* CDockableContainer::SpawnWidget(QString name, QString objectName, IToolWindowArea* area /*= nullptr*/, QToolWindowAreaReference reference /*= QToolWindowManager::AreaReference::Combine*/, int index /*= -1*/, QRect geometry /*= QRect()*/)
{
	QWidget* w = Spawn(name, objectName);
	if (w)
	{
		if (!m_toolManager->ownsToolWindow(w)) //If it's already owned, this is a single-instance widget, so don't move it
		{
			m_toolManager->addToolWindow(w, area, reference, index, geometry);
		}
		m_toolManager->bringToFront(w);
	}
	return w;
}

void CDockableContainer::SetDefaultLayoutCallback(std::function<void(CDockableContainer*)> callback)
{
	m_defaultLayoutCallback = callback;
}

void CDockableContainer::SetMenu(CAbstractMenu* pAbstractMenu)
{
	m_pMenu = pAbstractMenu;
}

void CDockableContainer::SetSplitterSizes(QWidget* widget, QList<int> sizes)
{
	m_toolManager->resizeSplitter(widget, sizes);
}

QVariantMap CDockableContainer::GetState() const
{
	QVariantMap result;
	if (m_toolManager)
	{
		QVariantMap stateMap;
		for (auto sw : m_spawned)
		{
			CDockableContainer::WidgetInstance& w = sw.second;
			if (m_toolManager->areaOf(w.m_widget))
			{
				QVariantMap toolData;
				toolData.insert("class", w.m_spawnName);
				IStateSerializable* ssw = qobject_cast<IStateSerializable*>(w.m_widget);
				if (ssw)
				{
					toolData.insert("state", ssw->GetState());
				}
				stateMap[w.m_widget->objectName()] = toolData;
			}
		}
		result["Windows"] = stateMap;
		result["ToolsLayout"] = m_toolManager->saveState();
	}
	return result;
}

void CDockableContainer::SetState(const QVariantMap& state)
{
	if (!m_toolManager)
	{
		m_startingLayout = state;
		return;
	}
	auto lock = m_toolManager->getNotifyLock(false);
	m_toolManager->hide();
	m_toolManager->clear();
	m_spawned.clear();
	QVariantMap openToolsMap = state.value("Windows").toMap();
	for (QVariantMap::const_iterator iter = openToolsMap.begin(); iter != openToolsMap.end(); ++iter)
	{
		QString key = iter.key();
		QVariantMap v = iter.value().toMap();
		QString className = v.value("class").toString();
		QVariantMap toolState = v.value("state", QVariantMap()).toMap();

		QWidget* w = SpawnWidget(className, key);
		IStateSerializable* ssw = qobject_cast<IStateSerializable*>(w);
		if (w)
		{
			if (!toolState.empty() && ssw)
			{
				ssw->SetState(toolState);
			}
		}
	}

	m_toolManager->restoreState(state["ToolsLayout"]);
	BuildWindowMenu();
	m_toolManager->show();
	m_startingLayout = QVariantMap();
}

void CDockableContainer::OnWidgetDestroyed(QObject* pObject)
{
	auto it = m_spawned.find(pObject->objectName());
	if (it != m_spawned.end() && it->second.m_widget == pObject)
	{
		m_spawned.erase(it);
		BuildWindowMenu();
	}
}

QString CDockableContainer::CreateObjectName(QString title) const
{
	QString result = title;
	while (m_spawned.find(result) != m_spawned.end())
	{
		int i = qrand();
		result = QString::asprintf("%s#%d", title.toStdString().c_str(), i);
	}
	return result;
}

void CDockableContainer::showEvent(QShowEvent* event)
{
	// Construct everything in the show event to ensure owner is fully constructed
	if (m_toolManager)
	{
		return;
	}

	QVariantMap config = CEditorDialog::s_config;
	config[QTWM_RELEASE_POLICY] = rcpDelete;
	m_toolManager = new QToolWindowManager(this, config, s_dockingFactory);
	connect(m_toolManager, &QToolWindowManager::updateTrackingTooltip, [](const QString& str, const QPoint& p) { QTrackingTooltip::ShowTextTooltip(str, p); });
	setLayout(new QVBoxLayout());
	layout()->setContentsMargins(0, 0, 0, 0);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	layout()->addWidget(m_toolManager);
	if (!m_startingLayout.isEmpty())
	{
		SetState(m_startingLayout);
	}
	else
	{
		m_defaultLayoutCallback(this);
	}

	m_layoutChangedConnection = connect(m_toolManager, &QToolWindowManager::layoutChanged, [=]() { OnLayoutChange(GetState()); BuildWindowMenu(); });
	BuildWindowMenu();
}

void CDockableContainer::BuildWindowMenu()
{
	if (!m_pMenu)
	{
		return;
	}
	
	m_pMenu->Clear();

	CAbstractMenu* addMenu = nullptr;
	for (auto it : m_registry)
	{
		if (it.second.m_isInternal)
		{
			continue;
		}

		if (!addMenu)
		{
			addMenu = m_pMenu->CreateMenu("&Add Pane");
		}

		QString s = it.first;
		QAction* action = addMenu->CreateAction(s);
		connect(action, &QAction::triggered, [=]() { return SpawnWidget(s); });
	}

	QAction* action = m_pMenu->CreateAction("&Reset layout");
	connect(action, &QAction::triggered, [=] { ResetLayout(); });

	int sec = m_pMenu->GetNextEmptySection();

	foreach(QWidget* q, m_toolManager->toolWindows())
	{
		QAction* action = m_pMenu->CreateAction(q->windowTitle(), sec);
		connect(action, &QAction::triggered, [=] { m_toolManager->bringToFront(q); });
	}
}

void CDockableContainer::ResetLayout()
{
	QToolWindowManager::QTWMNotifyLock lock = m_toolManager->getNotifyLock();
	m_toolManager->hide();
	m_toolManager->clear();
	m_spawned.clear();
	m_defaultLayoutCallback(this);
	m_toolManager->show();
}

CDockableContainer::FactoryInfo::FactoryInfo(std::function<QWidget*()> factory, bool isUnique, bool isInternal) : m_factory(factory), m_isUnique(isUnique), m_isInternal(isInternal)
{
}

CDockableContainer::WidgetInstance::WidgetInstance(QWidget* widget, QString spawnName) : m_widget(widget), m_spawnName(spawnName)
{
}


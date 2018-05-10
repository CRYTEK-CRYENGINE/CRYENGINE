// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <QMainWindow>
#include "Serialization.h"
#include "DockWidgetManager.h"
#include "DockTitleBarWidget.h"
#include "Expected.h"
#include "CryIcon.h"

void DockWidgetManager::OpenWidget::Serialize(IArchive& ar)
{
	string name = GetName();

	ar(name, "name");
	ar(type, "type");

	if (DockWidgetManager* manager = ar.context<DockWidgetManager>())
	{
		IDockWidgetType* type = manager->FindType(this->type.c_str());
		if (type && ar.isInput())
		{
			if (!dockWidget)
			{
				dockWidget = manager->CreateDockWidget(type);
			}
			dockWidget->setWindowTitle(type->Title());
			dockWidget->setObjectName(name.c_str());

			if (createdType != this->type)
			{
				if (widget)
				{
					widget->setParent(0);
					widget->deleteLater();
				}
				widget = manager->CreateByTypeName(this->type.c_str(), dockWidget);
				dockWidget->setWidget(widget);
				createdType = this->type;
			}
		}

		Serialization::SStruct serializer = manager->WidgetSerializer(widget, createdType.c_str());
		if (serializer)
			ar(serializer, "widget");
	}
}

void DockWidgetManager::OpenWidget::Destroy(QMainWindow* mainWindow)
{
	if (dockWidget)
	{
		dockWidget->deleteLater();
		dockWidget = nullptr;
	}
	if (widget)
	{
		widget->deleteLater();
		widget = nullptr;
	}
}

string DockWidgetManager::OpenWidget::GetName() const 
{
	return dockWidget ? dockWidget->objectName().toLocal8Bit().data() : ""; 
}


// ---------------------------------------------------------------------------

class CSplittableDockWidget : public CCustomDockWidget
{
public:

	CSplittableDockWidget(DockWidgetManager* manager, const char* title)
		: CCustomDockWidget(title)
		, m_manager(manager)
	{
		const auto titleBar = qobject_cast<CDockTitleBarWidget*>(titleBarWidget());
		assert(titleBar);

		const auto splitButton = titleBar->AddButton(CryIcon("icons:Window/Window_Split.ico"), "Split Pane");
		EXPECTED(QObject::connect(splitButton, &QAbstractButton::clicked, [this]() { m_manager->SplitDockWidget(this); }));
	}

	void closeEvent(QCloseEvent* ev) override
	{
		m_manager->RemoveOrHideDockWidget(this);
	}

private:

	DockWidgetManager* m_manager;

};

// ---------------------------------------------------------------------------

DockWidgetManager::DockWidgetManager(QMainWindow* mainWindow)
	: m_mainWindow(mainWindow)
{
}

DockWidgetManager::~DockWidgetManager()
{
	Clear();
}

QDockWidget* DockWidgetManager::CreateDockWidget(IDockWidgetType* type)
{
	QDockWidget* dock = new CSplittableDockWidget(this, type->Title());
	return dock;
}

template<class TOpenWidget>
static int CountOpenWidgetsOfType(const std::vector<TOpenWidget>& widgets, const char* type)
{
	int result = 0;
	for (size_t i = 0; i < widgets.size(); ++i)
		if (widgets[i].type == type)
			++result;
	return result;
}

void DockWidgetManager::RemoveOrHideDockWidget(QDockWidget* dockWidget)
{
	for (size_t i = 0; i < m_openWidgets.size(); ++i)
	{
		OpenWidget& w = m_openWidgets[i];
		if (w.dockWidget == dockWidget)
		{
			int count = CountOpenWidgetsOfType(m_openWidgets, w.type.c_str());
			if (count > 1)
			{
				w.dockWidget->deleteLater();
				w.widget->deleteLater();
				m_openWidgets.erase(m_openWidgets.begin() + i);
				SignalChanged();
				return;
			}
			else
			{
				w.dockWidget->hide();
			}
		}
	}
}

QWidget* DockWidgetManager::CreateByTypeName(const char* typeName, QDockWidget* dockWidget) const
{
	for (size_t i = 0; i < m_types.size(); ++i)
	{
		IDockWidgetType* type = m_types[i].get();
		if (strcmp(type->Name(), typeName) == 0)
		{
			return type->Create(dockWidget);
		}
	}
	return 0;
}

void DockWidgetManager::Serialize(IArchive& ar)
{
	Serialization::SContext x(ar, this);
	std::vector<OpenWidget> unusedWidgets;
	if (ar.isInput())
		unusedWidgets = m_openWidgets;
	ar(m_openWidgets, "openWidgets");

	std::vector<int> widgetTypeCount(m_types.size());

	if (ar.isInput())
	{
		for (size_t i = 0; i < m_openWidgets.size(); ++i)
		{
			OpenWidget& w = m_openWidgets[i];
			int typeIndex = 0;
			IDockWidgetType* type = FindType(w.type.c_str(), &typeIndex);
			if (!type)
			{
				m_openWidgets.erase(m_openWidgets.begin() + i);
				--i;
				continue;
			}

			if (size_t(typeIndex) < widgetTypeCount.size())
				widgetTypeCount[typeIndex] += 1;

			const auto itSearch = std::find_if(unusedWidgets.begin(), unusedWidgets.end(), [&w](const OpenWidget& x) { return x.dockWidget == w.dockWidget; });
			if (itSearch == unusedWidgets.end())
			{
				m_mainWindow->addDockWidget(Qt::DockWidgetArea(type->DockArea()), w.dockWidget);
			}
			else
			{
				unusedWidgets.erase(itSearch);
			}			
		}

		for (size_t i = 0; i < unusedWidgets.size(); ++i)
		{
			unusedWidgets[i].Destroy(m_mainWindow);
		}

		for (size_t i = 0; i < widgetTypeCount.size(); ++i)
			if (widgetTypeCount[i] == 0)
				CreateDefaultWidget(m_types[i].get());
	}
}

IDockWidgetType* DockWidgetManager::FindType(const char* name, int* typeIndex)
{
	for (size_t i = 0; i < m_types.size(); ++i)
	{
		IDockWidgetType* type = m_types[i].get();
		if (strcmp(type->Name(), name) == 0)
		{
			if (typeIndex)
				*typeIndex = int(i);
			return m_types[i].get();
		}
	}
	return 0;
}

void DockWidgetManager::SplitDockWidget(QDockWidget* dockWidget)
{
	for (size_t i = 0; i < m_openWidgets.size(); ++i)
	{
		if (m_openWidgets[i].dockWidget == dockWidget)
		{
			SplitOpenWidget((int)i);
			break;
		}
	}
}

void DockWidgetManager::ResetToDefault()
{
	Clear();

	for (size_t i = 0; i < m_types.size(); ++i)
	{
		IDockWidgetType* type = m_types[i].get();
		CreateDefaultWidget(type);
	}
}

void DockWidgetManager::CreateDefaultWidget(IDockWidgetType* type)
{
	OpenWidget w;
	w.dockWidget = CreateDockWidget(type);
	w.dockWidget->setObjectName(MakeUniqueDockWidgetName(type->Name()).c_str());
	w.widget = type->Create(w.dockWidget);
	w.dockWidget->setWidget(w.widget);
	w.type = type->Name();
	w.createdType = type->Name();

	m_mainWindow->addDockWidget(Qt::DockWidgetArea(type->DockArea()), w.dockWidget);

	m_openWidgets.push_back(w);
}

string DockWidgetManager::MakeUniqueDockWidgetName(const char* type) const
{
	string name;
	for (int index = 0; true; ++index)
	{
		name = QString("%1-%2").arg(type).arg(index).toLocal8Bit().data();
		for (size_t i = 0; i < m_openWidgets.size(); ++i)
			if (m_openWidgets[i].GetName() == name)
			{
				name.clear();
				break;
			}
		if (!name.empty())
			break;
	}
	return name;
}

void DockWidgetManager::Clear()
{
	for (size_t i = 0; i < m_openWidgets.size(); ++i)
	{
		OpenWidget& w = m_openWidgets[i];
		w.Destroy(m_mainWindow);
	}
	m_openWidgets.clear();
}

void DockWidgetManager::SplitOpenWidget(int openWidgetIndex)
{
	OpenWidget& existingWidget = m_openWidgets[openWidgetIndex];

	IDockWidgetType* type = FindType(existingWidget.type.c_str());
	if (!type)
		return;
	OpenWidget newWidget = existingWidget;
	newWidget.dockWidget = CreateDockWidget(type);
	newWidget.widget = type->Create(newWidget.dockWidget);
	newWidget.dockWidget->setObjectName(MakeUniqueDockWidgetName(type->Name()).c_str());
	newWidget.dockWidget->setWidget(newWidget.widget);
	newWidget.type = type->Name();
	newWidget.createdType = type->Name();

	std::vector<char> buffer;
	Serialization::SerializeToMemory(&buffer, type->Serializer(existingWidget.widget));
	Serialization::SerializeFromMemory(type->Serializer(newWidget.widget), buffer);

	m_mainWindow->addDockWidget(m_mainWindow->dockWidgetArea(existingWidget.dockWidget), newWidget.dockWidget);

	m_openWidgets.push_back(newWidget);

	SignalChanged();
}

Serialization::SStruct DockWidgetManager::WidgetSerializer(QWidget* widget, const char* dockTypeName)
{
	IDockWidgetType* type = FindType(dockTypeName);
	if (!type)
		return Serialization::SStruct();
	return type->Serializer(widget);
}


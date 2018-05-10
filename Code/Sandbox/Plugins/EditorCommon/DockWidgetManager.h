// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/TypeID.h>
#include <QObject>
#include <QScopedPointer>
#include <QWidget>
#include <QDockWidget>

#include "Serialization.h"
#include <vector>
#include <memory>

struct IDockWidgetType
{
	virtual const char*            Name() const = 0;
	virtual const char*            Title() const = 0;
	virtual QWidget*               Create(QDockWidget* dw) = 0;
	virtual int                    DockArea() const = 0;
	virtual Serialization::SStruct Serializer(QWidget* widget) const = 0;
};

template<class T, class C>
struct DockWidgetType : IDockWidgetType
{
	typedef T* (C::* CreateFunc)();
	C*         m_creator;
	CreateFunc m_createFunc;
	string     m_title;
	int        m_dockArea;

	DockWidgetType(C* creator, CreateFunc createFunc, const char* title, int dockArea)
		: m_creator(creator)
		, m_createFunc(createFunc)
		, m_title(title)
		, m_dockArea(dockArea)
	{}

	const char* Name() const override    { return Serialization::TypeID::get<T>().name(); }
	const char* Title() const override   { return m_title; }
	QWidget*    Create(QDockWidget* dw) override
	{
		T* result = (m_creator->*m_createFunc)();
		result->SetDockWidget(dw);
		return result;
	}
	int                    DockArea() const override { return m_dockArea; }
	Serialization::SStruct Serializer(QWidget* widget) const override
	{
		return Serialization::SStruct(*static_cast<T*>(widget));
	}
};

// Controls dock-windows that can be splitted.
class EDITOR_COMMON_API DockWidgetManager : public QObject
{
	Q_OBJECT
public:
	DockWidgetManager(QMainWindow* mainWindow);
	~DockWidgetManager();

	template<class T, class C>
	void AddDockWidgetType(C* creator, typename DockWidgetType<T, C>::CreateFunc createFunc, const char* title, Qt::DockWidgetArea dockArea)
	{
		DockWidgetType<T, C>* dockWidgetType = new DockWidgetType<T, C>(creator, createFunc, title, (int)dockArea);
		m_types.push_back(std::unique_ptr<IDockWidgetType>(dockWidgetType));
	}

	QWidget*               CreateByTypeName(const char* typeName, QDockWidget* dockWidget) const;

	void                   RemoveOrHideDockWidget(QDockWidget* dockWidget);
	void                   SplitDockWidget(QDockWidget* dockWidget);

	void                   ResetToDefault();

	void                   Clear();

	void                   Serialize(IArchive& ar);
signals:
	void                   SignalChanged();
private:
	void                   CreateDefaultWidget(IDockWidgetType* type);
	string                 MakeUniqueDockWidgetName(const char* type) const;
	IDockWidgetType*       FindType(const char* name, int* typeIndex = 0);
	void                   SplitOpenWidget(int index);
	QDockWidget*           CreateDockWidget(IDockWidgetType* type);
	Serialization::SStruct WidgetSerializer(QWidget* widget, const char* dockTypeName);

	struct OpenWidget
	{
		QWidget*     widget;
		QDockWidget* dockWidget;
		string       type;
		string       createdType;

		OpenWidget() : widget(), dockWidget() {}
		void Serialize(IArchive& ar);
		void Destroy(QMainWindow* window);
		string GetName() const;
	};

	QMainWindow*                                  m_mainWindow;
	std::vector<OpenWidget>                       m_openWidgets;
	std::vector<std::unique_ptr<IDockWidgetType>> m_types;
};


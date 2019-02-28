// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QList>
#include <QVariantMap>
#include "QToolWindowManagerCommon.h"
#include "IToolWindowWrapper.h"

class QTOOLWINDOWMANAGER_EXPORT IToolWindowArea
{
public:
	virtual ~IToolWindowArea() {};
	virtual void addToolWindow(QWidget* toolWindow, int index = -1 ) = 0;
	virtual void addToolWindows(const QList<QWidget*>& toolWindows, int index = -1 ) = 0;

	virtual void removeToolWindow(QWidget* toolWindow) = 0;

	virtual QList<QWidget*> toolWindows() = 0;

	virtual QVariantMap saveState() = 0;
	virtual void restoreState(const QVariantMap &data, int stateFormat) = 0;

	virtual void adjustDragVisuals() = 0;

	virtual QWidget* getWidget() = 0;

	IToolWindowWrapper* wrapper() {
		return findClosestParent<IToolWindowWrapper*>(getWidget());
	}

	virtual bool switchAutoHide(bool newValue) = 0;

	virtual const QPalette& palette() const = 0;
	virtual void clear() = 0;
	virtual QRect rect() const = 0;
	virtual QSize size() const = 0;
	virtual int count() const = 0;
	virtual QWidget* widget(int index) const = 0;
	virtual void deleteLater() = 0;
	virtual int width() const = 0;
	virtual int height() const = 0;
	virtual const QRect geometry() const = 0;
	virtual void hide() = 0;
	virtual QObject* parent() const = 0;
	virtual void setParent(QWidget* parent) = 0;
	virtual int indexOf(QWidget* w) const = 0;
	virtual QWidget* parentWidget() const = 0;
	virtual QPoint mapFromGlobal(const QPoint & pos) const = 0;
	virtual QPoint mapToGlobal(const QPoint & pos) const = 0;
	virtual void setCurrentWidget(QWidget* w) = 0;

	virtual QPoint mapCombineDropAreaFromGlobal(const QPoint & pos) const = 0;
	virtual QRect combineAreaRect() const = 0;
	virtual QRect combineSubWidgetRect(int index) const = 0;
	virtual int subWidgetAt(const QPoint& pos) const = 0;
	virtual QTWMWrapperAreaType areaType() const = 0;
};
Q_DECLARE_INTERFACE(IToolWindowArea, "QToolWindowManager/IToolWindowArea");

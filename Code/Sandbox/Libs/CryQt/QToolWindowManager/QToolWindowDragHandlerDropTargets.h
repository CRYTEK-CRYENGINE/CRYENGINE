// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "QToolWindowManagerCommon.h"
#include "IToolWindowDragHandler.h"

#include <QMap>

class QTOOLWINDOWMANAGER_EXPORT QToolWindowDropTarget : public QWidget
{
public:
	QToolWindowDropTarget(QString imagePath, QToolWindowAreaReference areaReference);
	virtual ~QToolWindowDropTarget();
	QToolWindowAreaReference target() { return m_areaReference; }

protected:
	virtual void paintEvent(QPaintEvent *paintEvent) Q_DECL_OVERRIDE;

private:
	QPixmap* m_pixmap;
	QToolWindowAreaReference m_areaReference;
};

class QTOOLWINDOWMANAGER_EXPORT QToolWindowDragHandlerDropTargets :
	public IToolWindowDragHandler
{
public:
	QToolWindowDragHandlerDropTargets(QToolWindowManager* manager);
	~QToolWindowDragHandlerDropTargets();
	void startDrag() Q_DECL_OVERRIDE;
	void switchedArea(IToolWindowArea* lastArea, IToolWindowArea* newArea) Q_DECL_OVERRIDE;
	QToolWindowAreaTarget getTargetFromPosition(IToolWindowArea* area) const Q_DECL_OVERRIDE;
	bool isHandlerWidget(QWidget* widget) const Q_DECL_OVERRIDE;
	QToolWindowAreaTarget finishDrag(QList<QWidget*> toolWindows, IToolWindowArea* source, IToolWindowArea* destination) Q_DECL_OVERRIDE;
	IToolWindowDragHandler::SplitSizes getSplitSizes(int originalSize) const Q_DECL_OVERRIDE;

private:
	QList<QToolWindowDropTarget*> m_targets;
	void hideTargets();
	void showTargets(IToolWindowArea* area);
};



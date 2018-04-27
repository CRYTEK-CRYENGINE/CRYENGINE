// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QWidget>
#include <QRect>
#include "QToolWindowManagerCommon.h"
#include "IToolWindowArea.h"
#include "QToolWindowManager.h"

class QTOOLWINDOWMANAGER_EXPORT IToolWindowDragHandler
{
	public:
		struct SplitSizes
		{
			int oldSize;
			int newSize;
		};
		virtual ~IToolWindowDragHandler() {};
		virtual void startDrag() = 0;
		virtual QRect getRectFromCursorPos(QWidget* previewArea, IToolWindowArea* area) const
		{
			QToolWindowAreaTarget target = getTargetFromPosition(area);
			QSize s = previewArea->size();
			IToolWindowDragHandler::SplitSizes widthSplit = getSplitSizes(s.width());
			IToolWindowDragHandler::SplitSizes heightSplit = getSplitSizes(s.height());
			switch (target.reference)
			{
			case QToolWindowAreaReference::Top:
			case QToolWindowAreaReference::HSplitTop:
				s.setHeight(heightSplit.newSize);
				return QRect(QPoint(0, 0), s);
			case QToolWindowAreaReference::Bottom:
			case QToolWindowAreaReference::HSplitBottom:
				s.setHeight(heightSplit.newSize);
				return QRect(QPoint(0, heightSplit.oldSize), s);
			case QToolWindowAreaReference::Left:
			case QToolWindowAreaReference::VSplitLeft:
				s.setWidth(widthSplit.newSize);
				return QRect(QPoint(0, 0), s);
			case QToolWindowAreaReference::Right:
			case QToolWindowAreaReference::VSplitRight:
				s.setWidth(widthSplit.newSize);
				return QRect(QPoint(widthSplit.oldSize, 0), s);
			case QToolWindowAreaReference::Combine:
				if (target.index == -1)
				{
					return QRect(QPoint(0, 0), s);
				}
				else
				{
					return area->combineSubWidgetRect(target.index);
				}
			}
			return QRect();
		}
		virtual void switchedArea(IToolWindowArea* lastArea, IToolWindowArea* newArea) = 0;
		virtual QToolWindowAreaTarget getTargetFromPosition(IToolWindowArea* area) const = 0;
		virtual bool isHandlerWidget(QWidget* widget) const = 0;
		virtual QToolWindowAreaTarget finishDrag(QList<QWidget*> toolWindows, IToolWindowArea* source, IToolWindowArea* destination) = 0;
		virtual SplitSizes getSplitSizes(int originalSize) const = 0;
};
Q_DECLARE_INTERFACE(IToolWindowDragHandler, "QToolWindowManager/IToolWindowDragHandler");

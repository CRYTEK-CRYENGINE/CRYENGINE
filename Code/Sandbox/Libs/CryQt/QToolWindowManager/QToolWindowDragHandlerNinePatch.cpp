// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "QToolWindowDragHandlerNinePatch.h"

#include <QStyle>

void QToolWindowDragHandlerNinePatch::startDrag()
{
	
}

void QToolWindowDragHandlerNinePatch::switchedArea(IToolWindowArea* lastArea, IToolWindowArea* newArea)
{
	
}

QToolWindowAreaTarget QToolWindowDragHandlerNinePatch::getTargetFromPosition(IToolWindowArea* area) const
{
	QToolWindowAreaTarget result;
	result.area = area;
	result.index = -1;
	result.reference = QToolWindowAreaReference::Floating;
	if (!area)
		return result;

	QRect areaRect = area->rect();
	QRect centerRect = QRect(areaRect.x() + areaRect.width() / 3, areaRect.y() + areaRect.height() / 3, areaRect.width() / 3, areaRect.height() / 3);
	QPoint pos = area->mapFromGlobal(QCursor::pos());
	QPoint tabPos = area->mapCombineDropAreaFromGlobal(QCursor::pos());

	if (area->combineAreaRect().contains(tabPos))
	{				
		result.reference = QToolWindowAreaReference::Combine;
		result.index = area->subWidgetAt(tabPos);
	}
	else
	{
		if (pos.x() < centerRect.x()) // left region
			result.reference = QToolWindowAreaReference::VSplitLeft;
		else if (pos.x() > centerRect.x() + centerRect.width()) // right region
			result.reference = QToolWindowAreaReference::VSplitRight;
		else if (pos.y() < centerRect.y()) // top region
			result.reference = QToolWindowAreaReference::HSplitTop;
		else if (pos.y() > centerRect.y() + centerRect.height()) // bottom region
			result.reference = QToolWindowAreaReference::HSplitBottom;
	}
	
	return result;
}

bool QToolWindowDragHandlerNinePatch::isHandlerWidget(QWidget* widget) const
{
	return false;
}

QToolWindowAreaTarget QToolWindowDragHandlerNinePatch::finishDrag(QList<QWidget*> toolWindows, IToolWindowArea* source, IToolWindowArea* destination)
{
	return getTargetFromPosition(destination);
}

IToolWindowDragHandler::SplitSizes QToolWindowDragHandlerNinePatch::getSplitSizes(int originalSize) const
{
	IToolWindowDragHandler::SplitSizes result;
	result.oldSize = originalSize * 2 / 3;
	result.newSize = originalSize - result.oldSize;
	return result;
}

QRect tabSeparatorRect(QRect tabRect, int splitterWidth)
{
	return QRect(tabRect.x() - splitterWidth / 2, tabRect.y(), splitterWidth, tabRect.height());
}

QRect QToolWindowDragHandlerNinePatch::getRectFromCursorPos(QWidget* previewArea, IToolWindowArea* area) const
{
	int splitterWidth = area->getWidget()->style()->pixelMetric(QStyle::PM_SplitterWidth, 0, area->getWidget());
	QRect tabEndRect = area->combineSubWidgetRect(area->count() - 1);
	tabEndRect.setX(tabEndRect.x() + tabEndRect.width());
	QToolWindowAreaTarget target = getTargetFromPosition(area);
	if (target.reference == QToolWindowAreaReference::Combine)
	{
		if (target.index == -1)
			return tabSeparatorRect(tabEndRect, splitterWidth);
		else
			return tabSeparatorRect(area->combineSubWidgetRect(target.index), splitterWidth);
	}
	return IToolWindowDragHandler::getRectFromCursorPos(previewArea, area);
}


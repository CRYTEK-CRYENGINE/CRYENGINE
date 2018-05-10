// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "NodeHeaderWidgetStyle.h"

#include "NodeWidgetStyle.h"
#include "NodeGraphView.h"

namespace CryGraphEditor {

CNodeHeaderWidgetStyle::CNodeHeaderWidgetStyle(CNodeWidgetStyle& nodeWidgetStyle)
	: QWidget(&nodeWidgetStyle)
	, m_nodeWidgetStyle(nodeWidgetStyle)
{
	m_nameFont = QFont("Arial", 12, QFont::Bold);
	m_nameColor = QColor(94, 94, 94);

	// Icons
	m_nodeIconMenuColor = QColor(255, 100, 150);
	m_nodeIconMenuSize = QSize(16, 16);
	m_nodeIconViewSize = QSize(16, 16);

	for (int32 i = 0; i < StyleState_Count; ++i)
	{
		m_nodeIconViewColor[i] = QColor(255, 100, 150);
	}

	//
	m_height = 24;
	m_colorLeft = QColor(26, 26, 26);
	m_colorRight = QColor(26, 26, 26);
	m_margins = QMargins(7, 1, -7, 1);

	// TODO: We should have a general icon like: 'graphs/node_default.ico'!
	SetNodeIcon(QIcon("icons:schematyc/node_default.ico"));
	// ~TODO
}

void CNodeHeaderWidgetStyle::SetNodeIcon(const QIcon& icon)
{
	m_nodeIcon = icon;

	m_nodeIconMenu = CryIcon(m_nodeIcon.pixmap(m_nodeIconMenuSize), {
			{ QIcon::Mode::Normal, m_nodeIconMenuColor }
	  });

	QPixmap iconPixmap = m_nodeIcon.pixmap(m_nodeIconViewSize, QIcon::Normal);

	for (int32 i = 0; i < StyleState_Count; ++i)
	{
		GeneratePixmap(static_cast<EStyleState>(i));
	}
}

void CNodeHeaderWidgetStyle::SetNodeIconMenuColor(const QColor& color)
{
	m_nodeIconMenuColor = color;

	m_nodeIconMenu = CryIcon(m_nodeIcon.pixmap(m_nodeIconMenuSize, QIcon::Normal), {
			{ QIcon::Mode::Normal, m_nodeIconMenuColor }
	  });
}

void CNodeHeaderWidgetStyle::SetNodeIconMenuSize(const QSize& size)
{
	m_nodeIconMenuSize = size;

	m_nodeIconMenu = CryIcon(m_nodeIcon.pixmap(m_nodeIconMenuSize, QIcon::Normal), {
			{ QIcon::Mode::Normal, m_nodeIconMenuColor }
	  });
}

void CNodeHeaderWidgetStyle::SetNodeIconViewDefaultColor(const QColor& color)
{
	const EStyleState state = StyleState_Default;

	m_nodeIconViewColor[state] = color;
	GeneratePixmap(state);
}

void CNodeHeaderWidgetStyle::SetNodeIconViewSelectedColor(const QColor& color)
{
	const EStyleState state = StyleState_Selected;

	m_nodeIconViewColor[state] = color;
	GeneratePixmap(state);
}

void CNodeHeaderWidgetStyle::SetNodeIconViewHighlightedColor(const QColor& color)
{
	const EStyleState state = StyleState_Highlighted;

	m_nodeIconViewColor[state] = color;
	GeneratePixmap(state);
}

void CNodeHeaderWidgetStyle::SetNodeIconViewDeactivatedColor(const QColor& color)
{
	const EStyleState state = StyleState_Disabled;

	m_nodeIconViewColor[state] = color;
	GeneratePixmap(state);
}

void CNodeHeaderWidgetStyle::SetNodeIconViewSize(const QSize& size)
{
	m_nodeIconViewSize = size;

	QPixmap iconPixmap = m_nodeIcon.pixmap(m_nodeIconViewSize, QIcon::Normal);

	for (int32 i = 0; i < StyleState_Count; ++i)
	{
		m_nodeIconViewPixmap[i] = CryIcon(iconPixmap, {
				{ QIcon::Mode::Normal, m_nodeIconViewColor[i] }
		  }).pixmap(m_nodeIconViewSize, QIcon::Normal);
	}
}

void CNodeHeaderWidgetStyle::GeneratePixmap(EStyleState state)
{
	m_nodeIconViewPixmap[state] = CryIcon(m_nodeIcon.pixmap(m_nodeIconViewSize, QIcon::Normal), {
			{ QIcon::Mode::Normal, m_nodeIconViewColor[state] }
	  }).pixmap(m_nodeIconViewSize, QIcon::Normal);
}

}


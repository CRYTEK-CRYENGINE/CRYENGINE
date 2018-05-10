// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "NodeInfoWidgetStyle.h"

#include "NodeWidgetStyle.h"
#include "NodeGraphView.h"

namespace CryGraphEditor {

CNodeInfoWidgetStyle::CNodeInfoWidgetStyle(CNodeWidgetStyle& nodeWidgetStyle)
	: QWidget(&nodeWidgetStyle)
	, m_nodeWidgetStyle(nodeWidgetStyle)
{
	m_font = QFont("Arial", 12, QFont::Bold);
	m_errorText = "ERROR";
	m_errorTextColor = QColor(255, 255, 255);
	m_errorBackgroundColor = QColor(255, 0, 0);
	m_warningText = "WARNING";
	m_warningTextColor = QColor(0, 0, 0);
	m_warningBackgroundColor = QColor(255, 240, 0);

	m_height = 24;
};

}


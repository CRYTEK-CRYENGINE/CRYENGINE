// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ConnectionWidgetStyle.h"

namespace CryGraphEditor {

CConnectionWidgetStyle::CConnectionWidgetStyle(const char* szStyleId, CNodeGraphViewStyle& viewStyle)
	: CNodeGraphViewStyleItem(szStyleId)
{
	m_color = QColor(110, 110, 110);
	m_width = 3.0;
	m_bezier = 120.0;
	m_usePinColors = true;

	viewStyle.RegisterConnectionWidgetStyle(this);
}

}


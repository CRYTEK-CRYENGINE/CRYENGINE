// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "NodeWidgetStyle.h"

#include "NodeHeaderWidgetStyle.h"
#include "NodeContentWidgetStyle.h"
#include "NodeInfoWidgetStyle.h"

namespace CryGraphEditor {

CNodeWidgetStyle::CNodeWidgetStyle(const char* szStyleId, CNodeGraphViewStyle& viewStyle)
	: CNodeGraphViewStyleItem(szStyleId)
{
	m_borderColor = QColor(84, 84, 84);
	m_borderWidth = 1;
	m_backgroundColor = QColor(32, 32, 32);
	m_margins = QMargins(6, 3, 6, 6);

	m_pHeaderStyle = CreateHeaderWidgetStyle();
	m_pContentStyle = CreateContentWidgetStyle();
	m_pInfoWidgetStyle = CreateInfoWidgetStyle();

	viewStyle.RegisterNodeWidgetStyle(this);
}

const QIcon& CNodeWidgetStyle::GetMenuIcon() const
{
	return m_pHeaderStyle->GetNodeIconMenu();
}

CNodeHeaderWidgetStyle* CNodeWidgetStyle::CreateHeaderWidgetStyle()
{
	return new CNodeHeaderWidgetStyle(*this);
}

CNodeContentWidgetStyle* CNodeWidgetStyle::CreateContentWidgetStyle()
{
	return new CNodeContentWidgetStyle(*this);
}

CNodeInfoWidgetStyle* CNodeWidgetStyle::CreateInfoWidgetStyle()
{
	return new CNodeInfoWidgetStyle(*this);
}

}


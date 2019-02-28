// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek, 2014.
// -------------------------------------------------------------------------
//  File name:   QViewPaneHost.cpp
//  Version:     v1.00
//  Created:     29/10/2014 by timur.
//  Description: Host widget for view panes
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "QViewPaneHost.h"

#include <QLayout>

//////////////////////////////////////////////////////////////////////////
QViewPaneHost::QViewPaneHost()
{
	setLayout(new QVBoxLayout);
	setFrameStyle(StyledPanel | Plain);
	layout()->setContentsMargins(0, 0, 0, 0);
}


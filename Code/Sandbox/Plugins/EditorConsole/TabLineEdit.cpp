// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 1999-2014.
// -------------------------------------------------------------------------
//  File name:   TabLineEdit.cpp
//  Version:     v1.00
//  Created:     03/03/2014 by Matthijs vd Meide
//  Compilers:   Visual Studio 2010
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TabLineEdit.h"
#include <QEvent>
#include <QKeyEvent>

bool QTabLineEdit::event(QEvent* pEvent)
{
	if (pEvent->type() == QEvent::KeyPress)
	{
		QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(pEvent);
		if (pKeyEvent->key() == Qt::Key_Tab)
		{
			tabPressed();
			return true;
		}
	}
	return QLineEdit::event(pEvent);
}


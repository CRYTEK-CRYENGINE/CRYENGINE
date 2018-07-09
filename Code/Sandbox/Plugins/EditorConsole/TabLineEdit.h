// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 1999-2014.
// -------------------------------------------------------------------------
//  File name:   TabLineEdit.h
//  Version:     v1.00
//  Created:     03/03/2014 by Matthijs vd Meide
//  Compilers:   Visual Studio 2010
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#include <QLineEdit>

//subclassed QLineEdit that allows capturing of tab
class QTabLineEdit : public QLineEdit
{
	Q_OBJECT;

public:
	QTabLineEdit(QWidget* pWidget) : QLineEdit(pWidget) {}
	bool event(QEvent* pEvent);

signals:
	void tabPressed();
};


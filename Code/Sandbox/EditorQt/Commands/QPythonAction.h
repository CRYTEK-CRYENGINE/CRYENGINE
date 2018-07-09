// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek, 2014.
// -------------------------------------------------------------------------
//  File name: QPythonAction.h
//  Created:   26/09/2014 by timur
//  Description: Special Qt Action derived class to bind with python calls
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <QAction>

class QWidget;

//////////////////////////////////////////////////////////////////////////
class SANDBOX_API QPythonAction : public QAction
{
public:
	QPythonAction(const QString& actionName, const QString& actionText, QObject* parent, const char* pythonCall = 0);
	QPythonAction(const QString& actionName, const char* pythonCall, QObject* parent);
	~QPythonAction() {}

protected:
	void OnTriggered();
};


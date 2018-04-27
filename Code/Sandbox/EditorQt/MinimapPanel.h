// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QWidget>
#include "EditToolPanel.h"

class QMinimapButtons : public QWidget
{
public:
	QMinimapButtons(QWidget* parent = nullptr);

private:
	void AddTool(CRuntimeClass* pRuntimeClass, const char* text);
};

class QMinimapPanel : public QEditToolPanel
{
public:
	QMinimapPanel(QWidget* parent = nullptr);

protected:
	virtual bool CanEditTool(CEditTool* pTool);
};


// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QWidget>
#include "EditToolPanel.h"
#include "TerrainBrushTool.h"

class QTerrainSculptButtons : public QWidget
{
public:
	QTerrainSculptButtons(QWidget* parent = nullptr);

private:
	void AddTool(CRuntimeClass* pRuntimeClass, const char* text);

	int           m_buttonCount;
	CTerrainBrush mTerrainBrush;
};

class QTerrainSculptPanel : public QEditToolPanel
{
public:
	QTerrainSculptPanel(QWidget* parent = nullptr);

protected:
	virtual bool CanEditTool(CEditTool* pTool);
};


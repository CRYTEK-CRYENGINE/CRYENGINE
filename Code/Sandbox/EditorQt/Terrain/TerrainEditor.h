// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include "EditorFramework/Editor.h"
#include "QtViewPane.h"

class CTerrainEditor : public CDockableEditor
{
public:
	CTerrainEditor(QWidget* parent = nullptr);
	~CTerrainEditor();

	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	virtual QRect                             GetPaneRect() override               { return QRect(0, 0, 800, 500); }

	virtual const char*                       GetEditorName() const override       { return "Terrain Editor"; };
	void                                      InitTerrainMenu();

	virtual void                              SetLayout(const QVariantMap& state);
	virtual QVariantMap                       GetLayout() const override;

	static class CTerrainTextureDialog*       GetTextureLayerEditor();
};


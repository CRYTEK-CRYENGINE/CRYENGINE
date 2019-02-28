// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2014.

#pragma once

#include "IPlugin.h"
#include "TerrainMoveTool.h"

class CTrackViewExporter;
class CAnimationContext;
class CTrackViewSequenceManager;
class CTrackViewAnimNode;

class CTrackViewPlugin : public IPlugin, public ITerrainMoveToolListener
{
public:
	CTrackViewPlugin();
	~CTrackViewPlugin();

	static CTrackViewExporter*        GetExporter();
	static CAnimationContext*         GetAnimationContext();
	static CTrackViewSequenceManager* GetSequenceManager();

	int32                             GetPluginVersion() override                          { return 1; }
	const char*                       GetPluginName() override                             { return "TrackView"; }
	const char*                       GetPluginDescription() override					   { return "Adds the TrackView tool"; }

private:
	void         OnOpenObjectContextMenu(CPopupMenuItem* pMenu, const CBaseObject* pObject);
	void         OnMenuOpenTrackView(CTrackViewAnimNode* pAnimNode);

	virtual void OnMove(const Vec3 targetPos, Vec3 sourcePos, bool bIsCopy) override;

	static CTrackViewExporter*        ms_pExporter;
	static CAnimationContext*         ms_pAnimationContext;
	static CTrackViewSequenceManager* ms_pSequenceManager;
};


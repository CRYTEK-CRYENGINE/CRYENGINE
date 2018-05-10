// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorAutoLevelLoadTest.h"

CEditorAutoLevelLoadTest& CEditorAutoLevelLoadTest::Instance()
{
	static CEditorAutoLevelLoadTest levelLoadTest;
	return levelLoadTest;
}

CEditorAutoLevelLoadTest::CEditorAutoLevelLoadTest()
{
	GetIEditorImpl()->RegisterNotifyListener(this);
}

CEditorAutoLevelLoadTest::~CEditorAutoLevelLoadTest()
{
	GetIEditorImpl()->UnregisterNotifyListener(this);
}

void CEditorAutoLevelLoadTest::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnEndSceneOpen:
		CLogFile::WriteLine("[LevelLoadFinished]");
		exit(0);
		break;
	}
}


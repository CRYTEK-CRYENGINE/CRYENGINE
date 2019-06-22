// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "TagLocations.h"

// Sandbox
#include "CryEditDoc.h"
#include "ViewManager.h"

// EditorCommon
#include <EditorFramework/Events.h>
#include <QtUtil.h>
#include <Util/Math.h>

// System
#include <CrySystem/Profilers/IStatoscope.h>

// CryCommon
#include <CrySerialization/yasli/JSONOArchive.h>
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/STL.h>

// Qt
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>

static const char* s_tagsPath = "/tags.json";

CTagLocations::CTagLocations()
{
	m_locations.resize(12);
}

void CTagLocations::TagLocation(int locationNum)
{
	CViewport* pRenderViewport = GetIEditorImpl()->GetViewManager()->GetGameViewport();
	if (!pRenderViewport)
		return;

	Vec3 vPosVec = pRenderViewport->GetViewTM().GetTranslation();

	int index = locationNum - 1;
	m_locations[index].position = vPosVec;
	m_locations[index].angles = Ang3::GetAnglesXYZ(pRenderViewport->GetViewTM());

	SaveTagLocations();
}

void CTagLocations::GotoTagLocation(int locationNum)
{
	string sTagConsoleText("");

	int index = locationNum - 1;
	Vec3 pos = m_locations[index].position;

	if (!IsVectorsEqual(pos, Vec3(0, 0, 0)))
	{
		// Change render viewport view TM to the stored one.
		CViewport* pRenderViewport = GetIEditorImpl()->GetViewManager()->GetGameViewport();
		if (pRenderViewport)
		{

			if (gEnv->pStatoscope && gEnv->IsEditorGameMode())
			{
				Vec3 oldPos = pRenderViewport->GetViewTM().GetTranslation();
				char buffer[100];
				cry_sprintf(buffer, "Teleported from (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f)", oldPos.x, oldPos.y, oldPos.z, pos.x, pos.y, pos.z);
				gEnv->pStatoscope->AddUserMarker("Player", buffer);
			}

			Matrix34 tm = Matrix34::CreateRotationXYZ(m_locations[index].angles);
			tm.SetTranslation(pos);
			pRenderViewport->SetViewTM(tm);

			GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_BEAM_PLAYER_TO_CAMERA_POS, (UINT_PTR)&tm, 0);
		}
	}
	else
	{
		sTagConsoleText.Format("Camera Tag Point %d not set", index);
	}

	if (!sTagConsoleText.IsEmpty())
		GetIEditorImpl()->WriteToConsole(sTagConsoleText);
}

void CTagLocations::SaveTagLocations()
{
	SaveToTxt();

	QFileInfo fileInfo(GetIEditorImpl()->GetDocument()->GetPathName().c_str());
	string fullPath;
	fullPath.Format("%s%s", QtUtil::ToString(fileInfo.absolutePath()).c_str(), s_tagsPath);

	QFile file(fullPath.c_str());
	if (!file.open(QIODevice::WriteOnly))
	{
		string message;
		message.Format("Failed to open path: %s", fullPath.c_str());
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, message.c_str());
		return;
	}

	const QByteArray blob = file.readAll();

	yasli::JSONOArchive ar;
	ar(*this, "tagLocations", "tagLocations");
	file.write(ar.buffer());
}

void CTagLocations::LoadTagLocations()
{
	// Use level location to find tags file
	QFileInfo fileInfo(GetIEditorImpl()->GetDocument()->GetPathName().c_str());
	string fullPath;
	fullPath.Format("%s%s", QtUtil::ToString(fileInfo.absolutePath()).c_str(), s_tagsPath);

	QFile file(fullPath.c_str());
	if (!file.open(QIODevice::ReadOnly))
	{
		string message;
		message.Format("Failed to open path: %s", fullPath.c_str());
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_COMMENT, message.c_str());
		return;
	}

	const QByteArray blob = file.readAll();

	yasli::JSONIArchive ar;
	ar.open(blob, blob.length());
	ar(*this, "tagLocations", "tagLocations");
}

void CTagLocations::SaveToTxt()
{
	// To be deprecated, CryAction depends on this file still
	char filename[_MAX_PATH];
	cry_strcpy(filename, GetIEditorImpl()->GetDocument()->GetPathName());
	QFileInfo fileInfo(filename);
	QString fullPath(fileInfo.absolutePath() + "/tags.txt");
	SetFileAttributes(fullPath.toStdString().c_str(), FILE_ATTRIBUTE_NORMAL);
	FILE* f = fopen(fullPath.toStdString().c_str(), "wt");
	if (f)
	{
		for (int i = 0; i < 12; i++)
		{
			fprintf(f, "%f,%f,%f,%f,%f,%f\n",
				m_locations[i].position.x, m_locations[i].position.y, m_locations[i].position.z,
				m_locations[i].angles.x, m_locations[i].angles.y, m_locations[i].angles.z);
		}
		fclose(f);
	}
}

namespace Private_TagLocationCommands
{
void TagLocation(int slot)
{
	char buffer[64];
	cry_sprintf(buffer, "level.tag_location %i", slot);
	CommandEvent(buffer).SendToKeyboardFocus();
}

void GoToTagLocation(int slot)
{
	char buffer[64];
	cry_sprintf(buffer, "level.go_to_tag_location %i", slot);
	CommandEvent(buffer).SendToKeyboardFocus();
}
}

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_TagLocationCommands::TagLocation, level, tag_location,
                                   CCommandDescription("Saves current location to specified slot").Param("slot", "Specified save slot for location tag"))

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_TagLocationCommands::GoToTagLocation, level, go_to_tag_location,
                                   CCommandDescription("Goes to specified saved location").Param("slot", "Specified save slot for location tag"))

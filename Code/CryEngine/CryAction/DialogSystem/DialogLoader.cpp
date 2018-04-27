// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   DialogLoader.cpp
//  Version:     v1.00
//  Created:     07/07/2006 by AlexL
//  Compilers:   Visual Studio.NET
//  Description: Dialog Loader
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "DialogLoader.h"
#include "DialogCommon.h"
#include <CryString/StringUtils.h>

////////////////////////////////////////////////////////////////////////////
CDialogLoader::CDialogLoader(CDialogSystem* pDS) : m_pDS(pDS)
{

}

////////////////////////////////////////////////////////////////////////////
CDialogLoader::~CDialogLoader()
{

}

////////////////////////////////////////////////////////////////////////////
bool CDialogLoader::LoadScriptsFromPath(const string& path, TDialogScriptMap& outScriptMap)
{
	ICryPak* pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	int numLoaded = 0;

	string realPath(path);
	realPath.TrimRight("/\\");
	string search(realPath);
	search += "/*.xml";

	intptr_t handle = pCryPak->FindFirst(search.c_str(), &fd);
	if (handle != -1)
	{
		do
		{
			// fd.name contains the profile name
			string filename = realPath;
			filename += "/";
			filename += fd.name;
			bool ok = LoadScript(filename, outScriptMap);
			if (ok)
				++numLoaded;
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}

	return numLoaded > 0;
}

////////////////////////////////////////////////////////////////////////////
bool CDialogLoader::LoadScript(const string& filename, TDialogScriptMap& outScriptMap)
{
	// parse MS Excel spreadsheet
	XmlNodeRef rootNode = GetISystem()->LoadXmlFromFile(filename);
	if (!rootNode)
	{
		GameWarning("[DIALOG] CDialogLoader::LoadScripts: Cannot find file '%s'", filename.c_str());
		return false;
	}

	// iterate over all children and load all worksheets
	int nChilds = rootNode->getChildCount();
	if (nChilds == 0)
	{
		GameWarning("[DIALOG] CDialogLoader::LoadScripts: Cannot find any 'Worksheet's in file '%s'", filename.c_str());
		return false;
	}

	int numScripts = 0;

	string baseName = PathUtil::GetFileName(filename);

	for (int i = 0; i < nChilds; ++i)
	{
		XmlNodeRef childNode = rootNode->getChild(i);
		if (childNode && childNode->isTag("Worksheet"))
		{
			const char* wsName = childNode->getAttr("ss:Name");
			XmlNodeRef tableNode = childNode->findChild("Table");
			if (!tableNode)
			{
				GameWarning("[DIALOG] CDialogLoader::LoadScripts: Worksheet '%s' in file '%s' has no Table", wsName ? wsName : "<noname>", filename.c_str());
			}
			else
			{
				const string& groupName = baseName; // maybe we add the Worksheets name later on!
				numScripts = LoadFromTable(tableNode, groupName, outScriptMap);
				break; // only load first worksheet
			}
		}
	}

	return numScripts > 0;
}

static const int MAX_CELL_COUNT = 16;  // must fit into unsigned char!

static const char* sColumnNames[] =
{
	"Dialog",
	"Actor",
	"Sound",
	"Animation",
	"Facial Expression",
	"LookAtTarget",
	"Delay",
	"",
};

enum eAttrType
{
	ATTR_DIALOG = 0,
	ATTR_ACTOR,
	ATTR_AUDIO,
	ATTR_ANIM,
	ATTR_FACIAL,
	ATTR_LOOKAT,
	ATTR_DELAY,
	ATTR_SKIP
};

void FillMapping(XmlNodeRef row, unsigned char* pIndexToAttrMap)
{
	int nCellIndex = 0;
	int nNewIndex = 0;
	for (int cell = 0; cell < row->getChildCount(); ++cell)
	{
		if (cell >= MAX_CELL_COUNT)
			continue;

		XmlNodeRef nodeCell = row->getChild(cell);
		if (!nodeCell->isTag("Cell"))
			continue;

		if (nodeCell->getAttr("ss:Index", nNewIndex))
		{
			// Check if some cells are skipped.
			nCellIndex = nNewIndex - 1;
		}

		XmlNodeRef nodeCellData = nodeCell->findChild("Data");
		if (!nodeCellData)
		{
			++nCellIndex;
			continue;
		}

		const char* sCellContent = nodeCellData->getContent();

		for (int i = 0; i < sizeof(sColumnNames) / sizeof(*sColumnNames); ++i)
		{
			// this is a begins-with-check!
			if (CryStringUtils::stristr(sCellContent, sColumnNames[i]) == sCellContent)
			{
				pIndexToAttrMap[nCellIndex] = i;
				break;
			}
		}
		++nCellIndex;
	}
}

////////////////////////////////////////////////////////////////////////////
int CDialogLoader::LoadFromTable(XmlNodeRef tableNode, const string& groupName, TDialogScriptMap& outScriptMap)
{
	unsigned char nCellIndexToType[MAX_CELL_COUNT];
	memset(nCellIndexToType, ATTR_SKIP, sizeof(nCellIndexToType));

	IMScript theScript;

	int nNumGoodScripts = 0;
	int nRowIndex = 0;
	int nChilds = tableNode->getChildCount();
	for (int i = 0; i < nChilds; ++i)
	{
		XmlNodeRef rowNode = tableNode->getChild(i);
		if (!rowNode || !rowNode->isTag("Row"))
			continue;

		++nRowIndex;

		// skip first row as it should only contain column description
		if (nRowIndex == 1)
		{
			FillMapping(rowNode, nCellIndexToType);
			continue;
		}

		IMScriptLine scriptLine;

		bool bLineValid = false;
		int nColCount = rowNode->getChildCount();
		int nCellIndex = 0;
		for (int j = 0; j < nColCount; ++j)
		{
			XmlNodeRef cellNode = rowNode->getChild(j);
			if (!cellNode || !cellNode->isTag("Cell"))
				continue;

			int tmpIndex = 0;
			if (cellNode->getAttr("ss:Index", tmpIndex))
			{
				nCellIndex = tmpIndex - 1;
			}

			if (nCellIndex < 0 || nCellIndex >= MAX_CELL_COUNT)
				break;

			XmlNodeRef cellDataNode = cellNode->findChild("Data");
			if (!cellDataNode)
			{
				++nCellIndex;
				continue;
			}

			unsigned char nCellType = nCellIndexToType[nCellIndex];

			const char* szContent = cellDataNode->getContent();

			// nRowIndex and nCellIndex should be correct now [1-based, not 0-based!]
			switch (nCellType)
			{
			case ATTR_SKIP:
				break;
			case ATTR_DIALOG:
				if (theScript.IsValid())
				{
					const bool ok = ProcessScript(theScript, groupName, outScriptMap);
					if (ok)
						++nNumGoodScripts;
					theScript.Reset();
				}
				theScript.name = szContent;
				break;
			case ATTR_ACTOR:
				scriptLine.actor = szContent;
				bLineValid = true;
				break;
			case ATTR_AUDIO:
				if (bLineValid)
				{
					if (szContent == nullptr)
					{
						scriptLine.audioID = CryAudio::InvalidControlId;
					}
					else
					{
						scriptLine.audioID = CryAudio::StringToId(szContent);
					}
				}
				break;
			case ATTR_ANIM:
				if (bLineValid)
					scriptLine.anim = szContent;
				break;
			case ATTR_FACIAL:
				if (bLineValid)
				{
					size_t n = strcspn(szContent, ":; ");
					if (n == strlen(szContent))
					{
						scriptLine.facial = szContent;
						scriptLine.facialWeight = 0.5f;
						scriptLine.facialFadeTime = 0.5f;
					}
					else
					{
						scriptLine.facial.assign(szContent, n);
						float w = 0.5f;
						float t = 0.5f;
						int nGood = sscanf(szContent + n + 1, "%f%*[:; ]%f", &w, &t);
						if (nGood != 1 && nGood != 2)
						{
							GameWarning("[DIALOG] CDialogLoader::LoadFromTable: DialogScript '%s' has invalid Facial Expression Content '%s'. Using weight=%f fadetime=%f.", groupName.c_str(), szContent, w, t);
						}
						scriptLine.facialWeight = w;
						scriptLine.facialFadeTime = t;
					}
				}
				break;
			case ATTR_LOOKAT:
				if (bLineValid)
					scriptLine.lookat = szContent;
				break;
			case ATTR_DELAY:
				if (bLineValid)
				{
					float val = 0.0f;
					int n = sscanf(szContent, "%f", &val);
					if (n == 1)
					{
						scriptLine.delay = val;
					}
				}
				break;
			default:
				break;
			}

			++nCellIndex;
		}
		if (scriptLine.IsValid())
		{
			theScript.lines.push_back(scriptLine);
		}
	}
	if (theScript.IsValid())
	{
		const bool ok = ProcessScript(theScript, groupName, outScriptMap);
		if (ok)
			++nNumGoodScripts;
	}

	return nNumGoodScripts;
}

// returns actor's ID in outID [1 based]
// or 0, when not found
////////////////////////////////////////////////////////////////////////////
bool CDialogLoader::GetActor(const char* actor, int& outID)
{
	static const char* actorPrefix = "actor";
	static const int actorPrefixLen = strlen(actorPrefix);

	assert(actor != 0 && *actor != '\0');
	if (actor == 0 || *actor == '\0') // safety
	{
		outID = 0;
		return true;
	}

	const char* found = CryStringUtils::stristr(actor, actorPrefix);
	if (found && sscanf(found + actorPrefixLen, "%d", &outID) == 1)
		return true;
	outID = 0;
	return false;
}

// returns actor's ID in outID [1 based]
// or 0, when not found
////////////////////////////////////////////////////////////////////////////
bool CDialogLoader::GetLookAtActor(const char* actor, int& outID, bool& outSticky)
{
	static const char* actorPrefix = "actor";
	static const int actorPrefixLen = strlen(actorPrefix);

	assert(actor != 0 && *actor != '\0');
	if (actor == 0 || *actor == '\0') // safety
	{
		outID = 0;
		outSticky = false;
		return true;
	}

	if (actor[0] == '$')
	{
		outSticky = true;
		++actor;
	}
	else
		outSticky = false;

	const char* found = CryStringUtils::stristr(actor, actorPrefix);
	if (found && sscanf(found + actorPrefixLen, "%d", &outID) == 1)
		return true;
	outID = 0;
	return false;
}

////////////////////////////////////////////////////////////////////////////
bool CDialogLoader::ProcessScript(IMScript& script, const string& groupName, TDialogScriptMap& outScriptMap)
{
#if 0
	DiaLOG::Log(DiaLOG::eAlways, "Script: %s with %d lines", script.name, script.lines.size());
	for (int i = 0; i < script.lines.size(); ++i)
	{
		IMScriptLine& line = script.lines[i];
		DiaLOG::Log(DiaLOG::eAlways, "actor=%s sound=%s anim=%s facial=%s lookat=%s delay=%f",
		            line.actor, line.sound, line.anim, line.facial, line.lookat, line.delay);
	}
#endif

	string scriptName(groupName);
	scriptName += ".";
	scriptName += script.name;

	if (script.lines.empty())
	{
		GameWarning("[DIALOG] CDialogLoader::ProcessScript: DialogScript '%s' has no lines. Discarded.", scriptName.c_str());
		return false;
	}

	CDialogScript* pScript = new CDialogScript(scriptName);
	pScript->SetVersionFlags(CDialogScript::VF_EXCEL_BASED, true);

	bool bDiscard = false;
	int nLine = 1;
	std::vector<IMScriptLine>::iterator iter = script.lines.begin();
	std::vector<IMScriptLine>::iterator end = script.lines.end();

	string animName;

	for (; iter != end; ++iter, ++nLine)
	{
		// process the line
		const IMScriptLine& line = *iter;
		int actor = 0;
		int lookAt = 0;

		// Actor
		if (!GetActor(line.actor, actor))
		{
			bDiscard = true;
			GameWarning("[DIALOG] CDialogLoader::ProcessScript '%s': Line %d: Cannot parse 'Actor' statement. Discarding.", scriptName.c_str(), nLine);
			break;
		}
		if (actor < 1 || actor > CDialogScript::MAX_ACTORS)
		{
			bDiscard = true;
			GameWarning("[DIALOG] CDialogLoader::ProcessScript '%s': Line %d: Actor%d given. Must be within [1..%d]. Discarding.", scriptName.c_str(), nLine, actor, CDialogScript::MAX_ACTORS);
			break;
		}

		bool bUseAGEP = false;
		bool bUseAGSignal = false;
		bool bSoundStopsAnim = false;

		const char* lineAnimName = line.anim;
		if (CryStringUtils::stristr(lineAnimName, "ex_") == lineAnimName)
		{
			// use AG Exact Positioning
			bUseAGEP = true;
			lineAnimName += 3;

			if (*lineAnimName == 0)
			{
				bDiscard = true;
				GameWarning("[DIALOG] CDialogLoader::ProcessScript '%s': Line %d: Invalid AnimName '%s'. Discarding", scriptName.c_str(), nLine, line.anim);
				break;
			}
		}

		if (lineAnimName[0] == '$')
		{
			// use AG Signal
			bUseAGSignal = true;
			++lineAnimName;

			if (*lineAnimName == 0)
			{
				bDiscard = true;
				GameWarning("[DIALOG] CDialogLoader::ProcessScript '%s': Line %d: Invalid AnimName '%s'. Discarding", scriptName.c_str(), nLine, line.anim);
				break;
			}
		}

		int animNameLen = strlen(lineAnimName);
		if (animNameLen >= 2 && lineAnimName[animNameLen - 1] == '$')
		{
			bSoundStopsAnim = true;
			--animNameLen;
		}
		animName.assign(lineAnimName, animNameLen);

		// LookAtTarget
		bool bLookAtGiven = line.lookat && line.lookat[0] != '\0';
		bool bLookAtSticky = false;
		bool bResetLookAt = false;
		if (bLookAtGiven && CryStringUtils::stristr(line.lookat, "$reset") != 0)
		{
			bResetLookAt = true;
			bLookAtGiven = false;
		}
		else if (bLookAtGiven && !GetLookAtActor(line.lookat, lookAt, bLookAtSticky))
		{
			bDiscard = true;
			GameWarning("[DIALOG] CDialogLoader::ProcessScript: %s Line %d: Cannot parse 'LookAtTarget' statement. Discarding.", scriptName.c_str(), nLine);
			break;
		}
		if (bLookAtGiven && (lookAt < 1 || lookAt > CDialogScript::MAX_ACTORS))
		{
			bDiscard = true;
			GameWarning("[DIALOG] CDialogLoader::ProcessScript '%s': Line %d: LookAtTarget Actor%d given. Must be within [1..%d]. Discarding.", scriptName.c_str(), nLine, lookAt, CDialogScript::MAX_ACTORS);
			break;
		}

		const char* facialExpression = line.facial.c_str();
		bool bResetFacial = false;
		if (CryStringUtils::stristr(facialExpression, "$reset") != 0)
		{
			bResetFacial = true;
			facialExpression = "";
		}

		const CDialogScript::TActorID actorID = static_cast<CDialogScript::TActorID>(actor - 1);
		const CDialogScript::TActorID lookAtID = lookAt <= 0 ? CDialogScript::NO_ACTOR_ID : static_cast<CDialogScript::TActorID>(lookAt - 1);
		const bool bSuccess = pScript->AddLine(actorID, line.audioID, animName.c_str(), facialExpression, lookAtID, line.delay, line.facialWeight, line.facialFadeTime, bLookAtSticky, bResetFacial, bResetLookAt, bSoundStopsAnim, bUseAGSignal, bUseAGEP);
		if (!bSuccess)
		{
			bDiscard = true;
			GameWarning("[DIALOG] CDialogLoader::ProcessScript '%s': Cannot add line %d. Discarding", scriptName.c_str(), nLine);
			break;
		}
	}

	if (bDiscard == false)
	{
		// try to complete the script
		if (pScript->Complete() == true)
		{
			// add it to the map
			std::pair<TDialogScriptMap::iterator, bool> inserted =
			  outScriptMap.insert(TDialogScriptMap::value_type(pScript->GetID(), pScript));
			if (inserted.second == false)
			{
				bDiscard = true;
				GameWarning("[DIALOG] CDialogLoader::ProcessScript '%s': Script already defined. Discarded", scriptName.c_str());
			}
		}
		// completion not successful -> discard
		else
		{
			bDiscard = true;
			GameWarning("[DIALOG] CDialogLoader::ProcessScript '%s': Cannot complete Script. Discarded.", scriptName.c_str());
		}
	}

	// discard pScript
	if (bDiscard)
	{
		delete pScript;
	}

	return bDiscard == false;
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GoalPipeXMLReader.h"
#include "AILog.h"
#include <CryAISystem/IAgent.h>
#include "TacticalPointSystem/TacticalPointSystem.h"

CGoalPipeXMLReader::CGoalPipeXMLReader()
{
	m_dictBranchType.Add("IF_ACTIVE_GOALS", IF_ACTIVE_GOALS);
	m_dictBranchType.Add("IF_ACTIVE_GOALS_HIDE", IF_ACTIVE_GOALS_HIDE);
	m_dictBranchType.Add("IF_NO_PATH", IF_NO_PATH);
	m_dictBranchType.Add("IF_PATH_STILL_FINDING", IF_PATH_STILL_FINDING);
	m_dictBranchType.Add("IF_IS_HIDDEN", IF_IS_HIDDEN);
	m_dictBranchType.Add("IF_CAN_HIDE", IF_CAN_HIDE);
	m_dictBranchType.Add("IF_CANNOT_HIDE", IF_CANNOT_HIDE);
	m_dictBranchType.Add("IF_STANCE_IS", IF_STANCE_IS);
	m_dictBranchType.Add("IF_FIRE_IS", IF_FIRE_IS);
	m_dictBranchType.Add("IF_HAS_FIRED", IF_HAS_FIRED);
	m_dictBranchType.Add("IF_NO_LASTOP", IF_NO_LASTOP);
	m_dictBranchType.Add("IF_SEES_LASTOP", IF_SEES_LASTOP);
	m_dictBranchType.Add("IF_SEES_TARGET", IF_SEES_TARGET);
	m_dictBranchType.Add("IF_CAN_SHOOT_TARGET", IF_CAN_SHOOT_TARGET);
	m_dictBranchType.Add("IF_CAN_MELEE", IF_CAN_MELEE);
	m_dictBranchType.Add("IF_NO_ENEMY_TARGET", IF_NO_ENEMY_TARGET);
	m_dictBranchType.Add("IF_PATH_LONGER", IF_PATH_LONGER);
	m_dictBranchType.Add("IF_PATH_SHORTER", IF_PATH_SHORTER);
	m_dictBranchType.Add("IF_PATH_LONGER_RELATIVE", IF_PATH_LONGER_RELATIVE);
	m_dictBranchType.Add("IF_NAV_WAYPOINT_HUMAN", IF_NAV_WAYPOINT_HUMAN);
	m_dictBranchType.Add("IF_NAV_TRIANGULAR", IF_NAV_TRIANGULAR);
	m_dictBranchType.Add("IF_TARGET_DIST_LESS", IF_TARGET_DIST_LESS);
	m_dictBranchType.Add("IF_TARGET_DIST_LESS_ALONG_PATH", IF_TARGET_DIST_LESS_ALONG_PATH);
	m_dictBranchType.Add("IF_TARGET_DIST_GREATER", IF_TARGET_DIST_GREATER);
	m_dictBranchType.Add("IF_TARGET_IN_RANGE", IF_TARGET_IN_RANGE);
	m_dictBranchType.Add("IF_TARGET_OUT_OF_RANGE", IF_TARGET_OUT_OF_RANGE);
	m_dictBranchType.Add("IF_TARGET_TO_REFPOINT_DIST_LESS", IF_TARGET_TO_REFPOINT_DIST_LESS);
	m_dictBranchType.Add("IF_TARGET_TO_REFPOINT_DIST_GREATER", IF_TARGET_TO_REFPOINT_DIST_GREATER);
	m_dictBranchType.Add("IF_TARGET_LOST_TIME_MORE", IF_TARGET_LOST_TIME_MORE);
	m_dictBranchType.Add("IF_TARGET_LOST_TIME_LESS", IF_TARGET_LOST_TIME_LESS);
	m_dictBranchType.Add("IF_LASTOP_DIST_LESS", IF_LASTOP_DIST_LESS);
	m_dictBranchType.Add("IF_LASTOP_DIST_LESS_ALONG_PATH", IF_LASTOP_DIST_LESS_ALONG_PATH);
	m_dictBranchType.Add("IF_TARGET_MOVED_SINCE_START", IF_TARGET_MOVED_SINCE_START);
	m_dictBranchType.Add("IF_TARGET_MOVED", IF_TARGET_MOVED);
	m_dictBranchType.Add("IF_EXPOSED_TO_TARGET", IF_EXPOSED_TO_TARGET);
	m_dictBranchType.Add("IF_COVER_COMPROMISED", IF_COVER_COMPROMISED);
	m_dictBranchType.Add("IF_COVER_NOT_COMPROMISED", IF_COVER_NOT_COMPROMISED);
	m_dictBranchType.Add("IF_COVER_SOFT", IF_COVER_SOFT);
	m_dictBranchType.Add("IF_COVER_NOT_SOFT", IF_COVER_NOT_SOFT);
	m_dictBranchType.Add("IF_CAN_SHOOT_TARGET_PRONED", IF_CAN_SHOOT_TARGET_PRONED);
	m_dictBranchType.Add("IF_CAN_SHOOT_TARGET_CROUCHED", IF_CAN_SHOOT_TARGET_CROUCHED);
	m_dictBranchType.Add("IF_CAN_SHOOT_TARGET_STANDING", IF_CAN_SHOOT_TARGET_STANDING);
	m_dictBranchType.Add("IF_COVER_FIRE_ENABLED", IF_COVER_FIRE_ENABLED);
	m_dictBranchType.Add("IF_RANDOM", IF_RANDOM);
	m_dictBranchType.Add("IF_CHANCE", IF_RANDOM);
	m_dictBranchType.Add("IF_LASTOP_FAILED", IF_LASTOP_FAILED);
	m_dictBranchType.Add("IF_LASTOP_SUCCEED", IF_LASTOP_SUCCEED);
}

bool CGoalPipeXMLReader::LoadGoalPipesFromXmlFile(const char* filename)
{
	const XmlNodeRef root = GetISystem()->LoadXmlFromFile(filename);
	if (!root)
	{
		AIError("Unable to load goal pipes XML file: %s", filename);
		return false;
	}

	AILogLoading("Loading goal pipes XML file: %s...", filename);
	return LoadGoalPipesFromXmlNode(root);
}

bool CGoalPipeXMLReader::LoadGoalPipesFromXmlNode(const XmlNodeRef& root)
{
	for (int iGoalPipe = 0, nGoalPipes = root->getChildCount(); iGoalPipe < nGoalPipes; ++iGoalPipe)
	{
		const XmlNodeRef goalPipeNode = root->getChild(iGoalPipe);
		if (goalPipeNode->isTag("GoalPipe"))
		{
			const char* szGoalPipeName = NULL;
			if (!goalPipeNode->getAttr("name", &szGoalPipeName))
			{
				AIError("Encountered tag <GoalPipe> without attribute 'name'; skipping.");
				continue;
			}

			ParseGoalPipe(szGoalPipeName, goalPipeNode);
		}
	}

	return true;
}

void CGoalPipeXMLReader::ParseGoalOps(IGoalPipe* pGoalPipe, const XmlNodeRef& parentNode,
                                      string sIfLabel, bool b_IfElseEnd_Halves_ReverseOrder)
{
	if (sIfLabel.empty())
	{
		// We are NOT within If-tag; just parse all the goalops in order.

		for (int iGoalOp = 0, nGoalOps = parentNode->getChildCount(); iGoalOp < nGoalOps; ++iGoalOp)
		{
			const XmlNodeRef goalOpNode = parentNode->getChild(iGoalOp);
			ParseGoalOp(pGoalPipe, goalOpNode);
		}

		m_currentGrouping = IGoalPipe::eGT_NOGROUP;
	}
	else
	{
		// We are within tag <IfOr>.  Conditional branch(-es) to szIfLabel has (have) already been added.
		// 1. Add the goalops AFTER tag <Else/>
		// 2. Add unconditional branch to the end (tag </If>)
		// 3. Add label szIfLabel
		// 4. Add the goalops BEFORE tag <Else/>
		// That is the reversed order used for tag <IfOr> (b_IfElseEnd_Halves_ReverseOrder == true)
		//
		// <IfAnd> (or simply <If>) is based on <IfOr> using formula 'NOT(A AND B) = NOT(A) OR NOT(B).
		// In this case, sections "AFTER" and "BEFORE" from above should be swapped.
		// Hence the need for parameter 'b_IfElseEnd_Halves_ReverseOrder'.

		int nGoalOps = parentNode->getChildCount();
		int iElseIndex = nGoalOps;

		for (int iGoalOp = 0; iGoalOp < nGoalOps; ++iGoalOp)
		{
			if (!stricmp(parentNode->getChild(iGoalOp)->getTag(), "Else"))
			{
				iElseIndex = iGoalOp;
				break;
			}
		}

		if (b_IfElseEnd_Halves_ReverseOrder)
		{
			for (int iGoalOp = iElseIndex + 1; iGoalOp < nGoalOps; ++iGoalOp)
			{
				const XmlNodeRef goalOpNode = parentNode->getChild(iGoalOp);
				ParseGoalOp(pGoalPipe, goalOpNode);
			}
		}
		else
		{
			for (int iGoalOp = 0; iGoalOp < iElseIndex; ++iGoalOp)
			{
				const XmlNodeRef goalOpNode = parentNode->getChild(iGoalOp);
				ParseGoalOp(pGoalPipe, goalOpNode);
			}
		}

		string sEndIfLabel = sIfLabel + "End";

		GoalParameters params;
		params.nValue = BRANCH_ALWAYS;
		params.str = sEndIfLabel;
		pGoalPipe->PushGoal(eGO_BRANCH, false, IGoalPipe::eGT_NOGROUP, params);

		pGoalPipe->PushLabel(sIfLabel.c_str());

		if (b_IfElseEnd_Halves_ReverseOrder)
		{
			for (int iGoalOp = 0; iGoalOp < iElseIndex; ++iGoalOp)
			{
				const XmlNodeRef goalOpNode = parentNode->getChild(iGoalOp);
				ParseGoalOp(pGoalPipe, goalOpNode);
			}
		}
		else
		{
			for (int iGoalOp = iElseIndex + 1; iGoalOp < nGoalOps; ++iGoalOp)
			{
				const XmlNodeRef goalOpNode = parentNode->getChild(iGoalOp);
				ParseGoalOp(pGoalPipe, goalOpNode);
			}
		}

		pGoalPipe->PushLabel(sEndIfLabel.c_str());
	}
}

void CGoalPipeXMLReader::ParseGoalOp(IGoalPipe* pGoalPipe, const XmlNodeRef& goalOpNode)
{
	GoalParameters params;

	const char* szGoalOpName = goalOpNode->getTag();

	if (!stricmp(szGoalOpName, "Else"))
	{
		AIError("Misplaced 'Else' encountered.");
		return;
	}

	if (!stricmp(szGoalOpName, "Group"))
	{
		m_currentGrouping = IGoalPipe::eGT_GROUPED;
		ParseGoalOps(pGoalPipe, goalOpNode);
		return;
	}

	if (!stricmp(szGoalOpName, "If") || !stricmp(szGoalOpName, "IfAnd") || !stricmp(szGoalOpName, "IfOr"))
	{
		// <IfOr condition1 condition2 ...> ... <Else/> ... </IfOr> is converted to:
		// AI.PushGoal("branch", 0, sLabel, condition1);
		// AI.PushGoal("branch", 0, sLabel, condition2);
		// ...
		// Translation of section <Else/> ... </IfOr>
		// Goto label (sLabel + "End")
		// Label sLabel
		// Translation of section <IfOr> ... <Else/>
		// Label (sLabel + "End")

		// <IfAnd> (or simply <If>) is based on <IfOr> using formula 'NOT(A AND B) = NOT(A) OR NOT(B).
		bool bOr = !stricmp(szGoalOpName, "IfOr");

		string sLabel = GenerateUniqueLabelName();
		params.str = sLabel;

		// Each attribute is a condition
		for (int i = 0, n = goalOpNode->getNumAttributes(); i < n; ++i)
		{
			params.fValueAux = -1.f;

			const char* szKey;
			const char* szValue;
			goalOpNode->getAttributeByIndex(i, &szKey, &szValue);

			if (!strnicmp(szKey, "not_", 4))
			{
				szKey += 4;
				params.nValue = bOr ? NOT : 0;  // Treat "not " as 'not' :) if the tag is <IfOr>
			}
			else
			{
				params.nValue = bOr ? 0 : NOT;  // Treat "yes" as 'yes' :) if the tag is <IfOr>
			}

			int nBranchType;
			if (!m_dictBranchType.Get(szKey, nBranchType))
			{
				AIError("Unrecognized attribute no. %d of tag %s of goal pipe %s: %s; skipping.",
				        i, szGoalOpName, pGoalPipe->GetName(), szKey);
				continue;
			}

			params.nValue |= nBranchType;
			params.fValue = static_cast<float>(atof(szValue));

			// See if we can get the 2nd parameter
			if (i + 1 < n)
			{
				goalOpNode->getAttributeByIndex(i + 1, &szKey, &szValue);
				if (!m_dictBranchType.Get(szKey, nBranchType))
				{
					params.fValueAux = static_cast<float>(atof(szValue));
				}
			}

			pGoalPipe->PushGoal(eGO_BRANCH, false, IGoalPipe::eGT_NOGROUP, params);
		}

		ParseGoalOps(pGoalPipe, goalOpNode, sLabel.c_str(), bOr); // the label for half "Else-EndIf" of "If-Else-EndIf"

		return;
	}

	if (!stricmp(szGoalOpName, "Loop"))
	{
		string sLabel = GenerateUniqueLabelName();
		pGoalPipe->PushLabel(sLabel.c_str());

		ParseGoalOps(pGoalPipe, goalOpNode);

		params.nValue = BRANCH_ALWAYS;
		params.str = sLabel;
		pGoalPipe->PushGoal(eGO_BRANCH, false, IGoalPipe::eGT_NOGROUP, params);
		return;
	}

	if (!stricmp(szGoalOpName, "SubGoalPipe"))
	{
		const char* szSubGoalPipeName;
		if (!goalOpNode->getAttr("name", &szSubGoalPipeName))
		{
			AIError("Unable to get mandatory attribute 'name' of node 'SubGoalPipe'.");
			szSubGoalPipeName = "";
		}

		if (!gAIEnv.pPipeManager->OpenGoalPipe(szSubGoalPipeName))
		{
			AIWarning("Tried to push a goal pipe to '%s' that is not yet defined: '%s'.",
			          pGoalPipe->GetName(), szSubGoalPipeName);
		}
		pGoalPipe->PushPipe(szSubGoalPipeName, true, IGoalPipe::eGT_NOGROUP, params);

		m_currentGrouping = IGoalPipe::eGT_NOGROUP;

		//AILogLoading("Added call to sub-goal pipe '%s'.", szSubGoalPipeName);
		return;
	}

	pGoalPipe->PushGoal(goalOpNode, m_currentGrouping);

	if (m_currentGrouping == IGoalPipe::eGT_GROUPED)
	{
		m_currentGrouping = IGoalPipe::eGT_GROUPWITHPREV;
	}

	//AILogLoading("Added goalop %s", szGoalOpName);
}

string CGoalPipeXMLReader::GenerateUniqueLabelName()
{
	char szLabel[100];
	cry_sprintf(szLabel, "Label%d", m_nextUniqueLabelID++);
	return szLabel;
}

void CGoalPipeXMLReader::ParseGoalPipe(const char* szGoalPipeName, const XmlNodeRef goalPipeNode
                                       , CPipeManager::ActionToTakeWhenDuplicateFound actionToTakeWhenDuplicateFound /*= CPipeManager::ReplaceDuplicateAndReportError*/)
{
	CPipeManager* pPipeManager = gAIEnv.pPipeManager;

	IGoalPipe* pGoalPipe = pPipeManager->CreateGoalPipe(
	  szGoalPipeName, actionToTakeWhenDuplicateFound);

	if (!pGoalPipe)
	{
		AIError("Could not create goal pipe '%s'.", szGoalPipeName);
		return;
	}

	//AILogLoading("Created goal pipe '%s'.", szGoalPipeName);

	m_nextUniqueLabelID = 0;
	m_currentGrouping = IGoalPipe::eGT_NOGROUP;

	ParseGoalOps(pGoalPipe, goalPipeNode);
}

bool CGoalPipeXMLReader::CXMLAttrReader::Get(const char* szKey, int& nValue)
{
	for (std::vector<TRecord>::iterator it = m_dictionary.begin(), end = m_dictionary.end(); it != end; ++it)
	{
		if (!stricmp(it->first, szKey))
		{
			nValue = it->second;
			return true;
		}
	}

	return false;
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Script Binding for Item

   -------------------------------------------------------------------------
   History:
   - 17:12:2007   11:49 : Created by Mieszko Zielinski

*************************************************************************/
#ifndef __SCRIPTBLACKCBOARD_H__
#define __SCRIPTBLACKCBOARD_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>
#include <CryAISystem/IBlackBoard.h>

class CBlackBoard : public IBlackBoard
{
public:
	virtual ~CBlackBoard(){}

	CBlackBoard();

	virtual SmartScriptTable& GetForScript() { return m_BB; }
	virtual void              SetFromScript(SmartScriptTable& sourceBB);
	virtual void              Clear()        { m_BB->Clear(); }

private:
	SmartScriptTable m_BB;
};

#endif // __SCRIPTBLACKCBOARD_H__

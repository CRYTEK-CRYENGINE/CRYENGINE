// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 17:12:2007   11:29 : Created by Mieszko Zielinski

*************************************************************************/
#include "StdAfx.h"
#include "BlackBoard.h"

CBlackBoard::CBlackBoard()
{
	m_BB.Create(gEnv->pSystem->GetIScriptSystem());
}

void CBlackBoard::SetFromScript(SmartScriptTable& sourceBB)
{
	m_BB->Clone(sourceBB, true);
}

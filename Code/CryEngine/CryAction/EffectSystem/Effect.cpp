// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	Base class for effects managed by the effect system

   -------------------------------------------------------------------------
   History:
   - 17:01:2006:		Created by Marco Koegler

*************************************************************************/
#include "StdAfx.h"
#include "Effect.h"

bool CEffect::Activating(float delta)
{
	return true;
}
bool CEffect::Update(float delta)
{
	return true;
}
bool CEffect::Deactivating(float delta)
{
	return true;
}

bool CEffect::OnActivate()
{
	return true;
}

bool CEffect::OnDeactivate()
{
	return true;
}

void CEffect::SetState(EEffectState state)
{
	m_state = state;
}

EEffectState CEffect::GetState()
{
	return m_state;
}

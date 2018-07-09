// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DoubleMagazine.h"
#include "ICryMannequin.h"



CDoubleMagazine::CDoubleMagazine()
	:	m_reloadFaster(true)
{
}



void CDoubleMagazine::OnAttach(bool attach)
{
	m_reloadFaster = true;
}



void CDoubleMagazine::OnParentReloaded()
{
	m_reloadFaster = !m_reloadFaster;
}



void CDoubleMagazine::SetAccessoryReloadTags(CTagState& fragTags)
{
	if (m_reloadFaster)
	{
		TagID clipRemaining = fragTags.GetDef().Find(CItem::sFragmentTagCRCs.doubleclip_fast);
		if(clipRemaining != TAG_ID_INVALID)
			fragTags.Set(clipRemaining, true);
	}
}

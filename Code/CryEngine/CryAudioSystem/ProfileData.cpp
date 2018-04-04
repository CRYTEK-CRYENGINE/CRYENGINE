// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "stdafx.h"
#include "ProfileData.h"

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
char const* CProfileData::GetImplName() const
{
	return m_implName.c_str();
}

//////////////////////////////////////////////////////////////////////////
void CProfileData::SetImplName(char const* const szName)
{
	m_implName = szName;
}
} // namespace CryAudio

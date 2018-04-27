// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYSIMPLEFILEGUARD__
#define __CRYSIMPLEFILEGUARD__

#include "../Common.h"
#include <string>

class CCrySimpleFileGuard
{
	std::string	m_FileName;
public:
							CCrySimpleFileGuard(const std::string& rFileName):
							m_FileName(rFileName)
							{
							}
							~CCrySimpleFileGuard()
							{
								remove(m_FileName.c_str());
							}
};
#endif

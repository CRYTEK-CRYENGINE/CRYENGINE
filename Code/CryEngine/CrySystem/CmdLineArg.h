// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements the command line argument interface ICmdLineArg.

   -------------------------------------------------------------------------
   History:
   - 30:7:2004   17:43 : Created by Márcio Martins

*************************************************************************/
#ifndef __CMDLINEARG_H__
#define __CMDLINEARG_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CrySystem/ICmdLine.h>

class CCmdLineArg :
	public ICmdLineArg
{
public:
	CCmdLineArg(const char* name, const char* value, ECmdLineArgType type);
	virtual ~CCmdLineArg();

	const char*           GetName() const;
	const char*           GetValue() const;
	const ECmdLineArgType GetType() const;
	const float           GetFValue() const;
	const int             GetIValue() const;

private:

	ECmdLineArgType m_type;
	string          m_name;
	string          m_value;
};

#endif //__CMDLINEARG_H__

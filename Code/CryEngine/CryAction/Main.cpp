// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 2:8:2004   10:38 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"

#define CRYACTION_API DLL_EXPORT

extern "C"
{
	CRYACTION_API IGameFramework* CreateGameFramework()
	{
		// at this point... we have no dynamic memory allocation, and we cannot
		// rely on atexit() doing the right thing; the only recourse is to
		// have a static buffer that we use for this object
		static char cryAction_buffer[sizeof(CCryAction)];
		return new((void*)cryAction_buffer)CCryAction();
	}
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	Dummy font implementation (dedicated server)
   -------------------------------------------------------------------------
   History:
   - Jun 30,2006:	Created by Sascha Demetrio

*************************************************************************/

#include "StdAfx.h"

#if defined(USE_NULLFONT)

	#include "NullFont.h"

CNullFont CCryNullFont::ms_nullFont;

#endif // USE_NULLFONT

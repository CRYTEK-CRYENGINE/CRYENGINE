// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__0B7BFEE0_95B3_4DD3_956A_33AD2ADB212D__INCLUDED_)
#define AFX_STDAFX_H__0B7BFEE0_95B3_4DD3_956A_33AD2ADB212D__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_Movie

#define CRYMOVIE_EXPORTS

#include <CryCore/Platform/platform.h>

#include <vector>
#include <list>
#include <map>
#include <algorithm>

#include <CryMath/Cry_Math.h>
#include <CrySystem/XML/IXml.h>
#include <CrySystem/ISystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryMovie/IMovieSystem.h>
#include <CryCore/smartptr.h>
#include <CryMemory/CrySizer.h>

#include <CrySerialization/IArchive.h>
#include <CrySerialization/IntrusiveFactory.h>
#include <CrySerialization/Enum.h>
#include <CrySerialization/Color.h>
#include <CrySerialization/Math.h>
#include <CrySerialization/SmartPtr.h>

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__0B7BFEE0_95B3_4DD3_956A_33AD2ADB212D__INCLUDED_)

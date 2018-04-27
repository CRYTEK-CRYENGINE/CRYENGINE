// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios
// -------------------------------------------------------------------------
//  Created:     13/2/2003 by Timur.
//  Description: Main header included by every file in Editor.
//
////////////////////////////////////////////////////////////////////////////
#include <Include/SandboxAPI.h>

//////////////////////////////////////////////////////////////////////////
// optimize away function call, in favor of inlining asm code
//////////////////////////////////////////////////////////////////////////
#pragma intrinsic( memset,memcpy,memcmp )
#pragma intrinsic( strcat,strcmp,strcpy,strlen,_strset )
//#pragma intrinsic( abs,fabs,fmod,sin,cos,tan,log,exp,atan,atan2,log10,sqrt,acos,asin )

// Warnings in STL
#pragma warning (disable : 4786) // identifier was truncated to 'number' characters in the debug information.
#pragma warning (disable : 4244) // conversion from 'long' to 'float', possible loss of data
#pragma warning (disable : 4018) // signed/unsigned mismatch
#pragma warning (disable : 4800) // BOOL bool conversion

// Disable warning when a function returns a value inside an __asm block
#pragma warning (disable : 4035)

//////////////////////////////////////////////////////////////////////////
// 64-bits related warnings.
#pragma warning (disable : 4267) // conversion from 'size_t' to 'int', possible loss of data

//////////////////////////////////////////////////////////////////////////
// Simple type definitions.
//////////////////////////////////////////////////////////////////////////
#include <CryCore/BaseTypes.h>

// If #defined, then shapes of newly created AITerritoryShapes will be limited to a single point, which is less confusing for level-designers nowadays
// as the actual shape has no practical meaning anymore (C1 and maybe C2 may have used the shape, but it was definitely no longer used for C3).
#define USE_SIMPLIFIED_AI_TERRITORY_SHAPE

//////////////////////////////////////////////////////////////////////////
// C runtime lib includes
//////////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>

/////////////////////////////////////////////////////////////////////////////
// STL
/////////////////////////////////////////////////////////////////////////////
#include <memory>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <algorithm>
#include <functional>

/////////////////////////////////////////////////////////////////////////////
// VARIOUS MACROS AND DEFINES
/////////////////////////////////////////////////////////////////////////////
#ifdef new
	#undef new
#endif

#ifndef SAFE_DELETE
	#define SAFE_DELETE(p) { if (p) { delete (p);   (p) = nullptr; } }
#endif

#ifndef SAFE_DELETE_ARRAY
	#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p);   (p) = nullptr; } }
#endif

#ifndef SAFE_RELEASE
	#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }
#endif

/////////////////////////////////////////////////////////////////////////////
// CRY Stuff.
/////////////////////////////////////////////////////////////////////////////
#include <CryCore/Platform/platform.h>
#include <CryMath/Cry_Geo.h>
#include <CryMath/Range.h>
#include <CryCore/StlUtils.h>

#include <CryCore/smartptr.h>
#define TSmartPtr                 _smart_ptr

#ifndef ON_WM_INPUT
// MFC does not define this one.
	#define ON_WM_INPUT() \
	  { WM_INPUT, 0, 0, 0, AfxSig_vwl, (AFX_PMSG)(AFX_PMSGW)(static_cast<void(AFX_MSG_CALL CWnd::*)(UINT, HRAWINPUT)>(&ThisClass::OnRawInput)) },
#endif

#define CRY_ENABLE_FBX_SDK

/////////////////////////////////////////////////////////////////////////////
// Interfaces.
/////////////////////////////////////////////////////////////////////////////
#include <CryRenderer/IRenderer.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CryScriptSystem/IScriptSystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/IIndexedMesh.h>
#include <CrySystem/ITimer.h>
#include <CryPhysics/IPhysics.h>
#include <CryAISystem/IAISystem.h>
#include <CrySystem/XML/IXml.h>
#include <CryMovie/IMovieSystem.h>
#include <CryCore/functor.h>

//////////////////////////////////////////////////////////////////////////
// Commonly used Editor includes.
//////////////////////////////////////////////////////////////////////////

// Utility classes.
#include <Util/EditorUtils.h>
#include "Util/FileEnum.h"
#include "Util/Math.h"
#include "Util/AffineParts.h"

// Xml support.
#include "Util/XmlArchive.h"
#include "Util/XmlTemplate.h"

// Utility classes.
#include "Util/BitArray.h"
#include "Util/MemoryBlock.h"
#include "Util/FileUtil.h"

// Variable.
#include "Util/Variable.h"

//////////////////////////////////////////////////////////////////////////
// Editor includes.
//////////////////////////////////////////////////////////////////////////

// Utility routines.
#include "Util/Image.h"
#include "Util/ImageUtil.h"
#include <CryCore/ToolsHelpers/GuidUtil.h>

// Main Editor interface definition.
#include "IEditorImpl.h"
#include "IEditorClassFactory.h"

// Undo support.
#include "IUndoObject.h"

// Log file access
#include "LogFile.h"

//Editor Settings.
#include "Settings.h"

// View panes.
#include "QtViewPane.h"

#include "UsedResources.h"

// Command Manager.
#include "Commands/CommandManager.h"
#include "Objects/ObjectManager.h"


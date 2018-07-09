// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Project/CryModuleDefs.h>
#include <CryCore/Platform/platform.h>

#define CRY_USE_XT
#include <CryCore/Platform/CryAtlMfc.h>

#include "Resource.h"

#include <CryCore/functor.h>
#include <CryCore/smartptr.h>
#include <CryMath/Cry_Geo.h>
#include <CrySystem/ISystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryEntitySystem/IEntity.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryExtension/CryGUID.h>

// STL headers.
#include <vector>
#include <list>
#include <map>
#include <set>
#include <algorithm>

#include <QString>

#include "IEditor.h"

#include "Settings.h"
#include "LogFile.h"

#include "EditorCommon.h"

#include <CryCore/smartptr.h>
#define TSmartPtr                 _smart_ptr

#include "IUndoObject.h"

#include "Util/EditorUtils.h"
#include "Util/Variable.h"
#include "Util/XmlArchive.h"
#include "Util/Math.h"
#include "Util/FileUtil.h"

#include "Objects/ObjectManager.h"
#include "Objects/SelectionGroup.h"
#include "Objects/BaseObject.h"

#include "IDisplayViewport.h"
#include "EditTool.h"

#include "Core/Declaration.h"
#include "Core/Common.h"
#include "Util/DesignerSettings.h"


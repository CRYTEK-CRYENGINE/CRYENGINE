#pragma once
#include <CryCore/Project/CryModuleDefs.h>
#include <CryCore/Platform/platform.h>
#define CRY_USE_MFC
#include <CryCore/Platform/CryAtlMfc.h>
#include "EditorCommonAPI.h"
#include "SandboxAPI.h"
#include "PluginAPI.h"
#include "Resource.h"
#include <CryCore/functor.h>
#include <CryMath/Cry_Geo.h>
#include <CrySystem/ISystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryEntitySystem/IEntity.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryCore/smartptr.h>
// STL headers.
#include <vector>
#include <list>
#include <map>	
#include <set>
#include <algorithm>

#include <QString>

#include "IEditor.h"

#include "Settings.h"
//#include "LogFile.h"

#include "IUndoObject.h"

//#include "Util/RefCountBase.h"
//#include "Util/fastlib.h"
//#include "Util/EditorUtils.h"
//#include "Util/PathUtil.h"
//#include "Util/SmartPtr.h"
//#include "Util/Variable.h"
//#include "Util/XmlArchive.h"
//#include "Util/Math.h"

#include "Controls/ColorCtrl.h"
#include "Controls/PropertyItem.h"
#include "Controls/PropertyCtrl.h"
#include "Controls/NumberCtrl.h"

#include "Objects/ObjectManager.h"
#include "Objects/SelectionGroup.h"
#include "Objects/BaseObject.h"

#include "IDisplayViewport.h"
#include "EditTool.h"

IEditor* GetIEditor();
extern HINSTANCE g_hInst;


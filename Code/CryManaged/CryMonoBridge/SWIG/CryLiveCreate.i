%include "CryEngine.swig"

%import "CryCommon.i"

%{
#include <CryLiveCreate/ILiveCreateCommon.h>
#include <CryLiveCreate/ILiveCreateHost.h>
#include <CryLiveCreate/ILiveCreateManager.h>
#include <CryLiveCreate/ILiveCreatePlatform.h>
%}

%ignore ILiveCreateEngineModule;

%include "../../../../CryEngine/CryCommon/CryLiveCreate/ILiveCreateCommon.h"
%include "../../../../CryEngine/CryCommon/CryLiveCreate/ILiveCreateHost.h"
%include "../../../../CryEngine/CryCommon/CryLiveCreate/ILiveCreateManager.h"
%include "../../../../CryEngine/CryCommon/CryLiveCreate/ILiveCreatePlatform.h"
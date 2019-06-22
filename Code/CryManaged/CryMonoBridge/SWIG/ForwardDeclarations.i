%include "cstring.i"
%cstring_output_maxsize(char *szOutPath, int pathSize);

%include "CryEngine.swig"

%{
#define SWIG_INCLUDE_SWIG_RUNTIME_FUNCTIONS
%}

// Required by platform.h:
// for now we only handle windows source and target . . and pretend to be vs 2012
#define _MSC_VER 1900
#if defined(_WIN64)
  #define _M_X64
#else
  #error Only 64-bit platforms are supported.
#endif

%include "../StdAfx.h"
%include "../../../../CryEngine/CryCommon/CryCore/Platform/CryPlatformDefines.h"
%include "../../../../CryEngine/CryCommon/CryCore/Project/ProjectDefines.h"
%include "../../../../CryEngine/CryCommon/CryGame/ExtensionDefines.h"
%include "../../../../CryEngine/CryCommon/CryCore/Compiler/MSVCspecific.h"

// Platform-specific header files

#if defined(_WIN64) && !defined(DURANGO)
 #define _declspec(WINDOWS_EXTENDED_ATTRIBUTE)
 %include "../../../../CryEngine/CryCommon/CryCore/Platform/WindowsSpecific.h"
 %include <windows.i>
#endif
#if defined(LINUX)
 %include "../../../../CryEngine/CryCommon/CryCore/Platform/LinuxSpecific.h"
#endif
#if defined(ANDROID)
// HACKY architecture definition . . only for nVidia shield (Tegra K1):
 #define __ARM_NEON__
 #define __GNUC__
 %include "../../../../CryEngine/CryCommon/CryCore/Platform/AndroidSpecific.h"
#endif
#if defined(DURANGO)
 %include "../../../../CryEngine/CryCommon/CryCore/Platform/DurangoSpecific.h"
#endif
#if defined(ORBIS)
 %include "../../../../CryEngine/CryCommon/CryCore/Platform/OrbisSpecific.h"
#endif 
#if defined(MAC)
 %include "../../../../CryEngine/CryCommon/CryCore/Platform/MacSpecific.h"
#endif
#if defined(IOS)
 %include "../../../CryEngine/CryCommon/CryCore/Platform/iOSSpecific.h"
#endif

%inline {
void CryGetExecutableFolder(char *szOutPath, int pathSize) 
{
	CryGetExecutableFolder(pathSize, szOutPath);
} 
}

%include "../../../../CryEngine/CryCommon/CryCore/Platform/platform.h"
%include "../../../../CryEngine/CryCommon/CryCore/smartptr.h"

%include "../../../CryEngine/CryCommon/CryExtension/CryTypeID.h"
%include "../../../../CryEngine/CryCommon/CryThreading/CryThreadSafePushContainer.h"

#define COMPILE_TIME_ASSERT(...)

%ignore TypeInfo;

//%nodefault CTypeInfo::CVarInfo;


%include "../../../../CryEngine/CryCommon/CryCore/TypeInfo_decl.h"
%include "../../../../CryEngine/CryCommon/CryCore/TypeInfo_impl.h"

%include "../../../../CryEngine/CryCommon/CryCore/BaseTypes.h"
%include "../../../../CryEngine/CryCommon/CryCore/Common_TypeInfo.h"
%include "../../../../CryEngine/CryCommon/CryMemory/CrySizer.h"

// calling convention stuff from windows.i ; we also want to use that for non-windows platforms . . .
#define __cdecl
#define __fastcall
#define __far
#define __forceinline
#define __fortran
#define __inline
#define __pascal
#define __stdcall
#define __syscall
#define __restrict
#define _cdecl
#define _fastcall
#define _inline
#define _pascal
#define _stdcall
#define WINAPI
#define __declspec(WINDOWS_EXTENDED_ATTRIBUTE)
#define __w64
#define __attribute__(...)

%ignore *::Serialize;
  
//Smart Pointer templates:
%define SMART_PTR_TEMPLATE(class)
%template(##class##Ptr) _smart_ptr<class>;
%enddef

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//#define DEFINE_MODULE_NAME "CrySystem"

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_System

#define CRYSYSTEM_EXPORTS

#include <CryCore/Platform/platform.h>
#include <CryCore/Assert/CryAssert.h>

#ifdef USE_PCH

// Ensure included first to prevent windows.h from being included by certain standard library headers, e.g. <future> on Durango
#include <CryCore/Platform/CryWindows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cfenv>
#include <cfloat>
#include <chrono>
#include <cinttypes>
#include <climits>
#include <clocale>
#include <cmath>
#include <codecvt>
#include <complex>
#include <condition_variable>
#include <csetjmp>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <cwctype>
#include <deque>
#include <exception>
#include <forward_list>
#include <fstream>
#include <functional>
#include <future>
#include <initializer_list>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <iterator>
#include <limits>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <numeric>
#include <ostream>
#include <queue>
#include <random>
#include <ratio>
#include <regex>
#include <scoped_allocator>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <system_error>
#include <thread>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <valarray>
#include <vector>

//////////////////////////////////////////////////////////////////////////
// CRT
//////////////////////////////////////////////////////////////////////////

#include <cstring>
#include <stdio.h>
#include <stdarg.h>

#if !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE && !CRY_PLATFORM_ORBIS
	#include <memory.h>
	#include <malloc.h>
#endif
#include <fcntl.h>

#if !CRY_PLATFORM_ORBIS && !CRY_PLATFORM_APPLE && !CRY_PLATFORM_ANDROID
	#if CRY_PLATFORM_LINUX
		#include <sys/io.h>
	#else
		#include <io.h>
	#endif
#endif
#include <cmath>

#if CRY_PLATFORM_WINDOWS
	#include <winsock2.h>
	#include <shlobj.h>
#endif

/////////////////////////////////////////////////////////////////////////////
// CRY Stuff ////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Camera.h>
#include <CryMath/Random.h>
#include <CryMath/Range.h>
#include <CryMath/Angle.h>
#include <CryMath/ISplines.h>
#include <CryMath/Rotation.h>
#include <CryMath/Transform.h>

#include <CryMemory/CrySizer.h>
#include <CryMemory/AddressHelpers.h>
#include <CryMemory/HeapAllocator.h>
#include <CryMemory/BucketAllocator.h>
#include <CryMemory/STLGlobalAllocator.h>

#include <CryCore/BoostHelpers.h>

#include <CryCore/smartptr.h>
#include <CryCore/CryEnumMacro.h>
#include <CryCore/StlUtils.h>
#include <CryCore/stridedptr.h>
#include <CryCore/functor.h>
#include <CryCore/RingBuffer.h>
#include <CryCore/SmallFunction.h>
#include <CryCore/BitMask.h>
#include <CryCore/CryVariant.h>
#include <CryCore/optional.h>
#include <CryCore/CountedValue.h>
#include <CryCore/CryCustomTypes.h>
#include <CryCore/CryTypeInfo.h>
#include <CryCore/Containers/CryArray.h>
#include <CryCore/Containers/CryFixedArray.h>
#include <CryCore/Containers/CryListenerSet.h>
#include <CryCore/Containers/MiniQueue.h>
#include <CryCore/Containers/VectorSet.h>
#include <CryCore/Containers/VectorMap.h>
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>
#include <CryCore/ToolsHelpers/SettingsManagerHelpers.h>
#include <CryCore/TypeInfo_impl.h>

#include <CryString/CryString.h>
#include <CryString/CryFixedString.h>
#include <CryString/CryName.h>
#include <CryString/CryPath.h>

#include <CrySerialization/STL.h>
#include <CrySerialization/DynArray.h>
#include <CrySerialization/Color.h>
#include <CrySerialization/ColorImpl.h>
#include <CrySerialization/Math.h>
#include <CrySerialization/MathImpl.h>

#include <CryExtension/ClassWeaver.h>
#include <CryExtension/Conversion.h>
#include <CryExtension/RegFactoryNode.h>

#include <CryRenderer/IScaleform.h> // expensive header
#include <CryRenderer/IRenderer.h> // expensive header

#ifdef INCLUDE_SCALEFORM_SDK
	#include <GRefCount.h>
	#include <GMemory.h>
	#include <GMemoryHeap.h>
	#include <GAtomic.h>
	#include <GStats.h>
	#include <GTimer.h>
	#include <GList.h>
	#include <GSysAllocMalloc.h>
	#include <GArray.h>
	#include <GAllocator.h>
	#include <GMath.h>
	#include <GFxPlayerStats.h>
	#include <GColor.h>
	#include <GColorMacros.h>
	#include <GMatrix2D.h>
	#include <GTypes2DF.h>
	#include <GMatrix3D.h>
	#include <GPoint3.h>
	#include <GImage.h>
	#include <GRendererEventHandler.h>
#endif

#endif // USE_PCH

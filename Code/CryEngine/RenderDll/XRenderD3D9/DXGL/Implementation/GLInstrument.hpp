// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLInstrument.hpp
//  Version:     v1.00
//  Created:     29/04/2015 by Valerio Guagliumi.
//  Description: Wrappers for OpenGL function call data and error logging
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GLINSTRUMENT__
#define __GLINSTRUMENT__

#if defined(DXGL_USE_GLAD)
	#define GLAD_INSTRUMENT_CALL(_Name, ...) NCryOpenGL::Instrument<glad_tag_ ## _Name>::Call( # _Name, glad_ ## _Name, ## __VA_ARGS__)
	#define DXGL_UNWRAPPED_FUNCTION(_Name)   glad_ ## _Name
#else
	#error "Not implemented in this configuration"
#endif //defined(DXGL_USE_GLAD)

namespace NCryOpenGL
{

template<typename T>
struct Array
{
	T*     m_pElements;
	size_t m_iSize;

	Array(T* pElements, size_t iSize)
		: m_pElements(pElements)
		, m_iSize(iSize)
	{
	}
};

#if DXGL_TRACE_CALLS

void                             CallTracePrintf(const char* szFormat, ...) PRINTF_PARAMS(1, 2);
void                             CallTraceFlush();

template<typename T> inline void TraceValue(const T&)              { CallTracePrintf("[?]"); }
template<typename T> inline void TraceValue(const T*)              { CallTracePrintf("[ptr]"); }
inline void                      TraceValue(char cValue)           { CallTracePrintf("%c", cValue); }
inline void                      TraceValue(unsigned int uValue)   { CallTracePrintf("%u", uValue); }
inline void                      TraceValue(int iValue)            { CallTracePrintf("%i", iValue); }
inline void                      TraceValue(uint64_t uValue)       { CallTracePrintf("%" PRIu64, uValue); }
inline void                      TraceValue(int64_t iValue)        { CallTracePrintf("%" PRId64, iValue); }
inline void                      TraceValue(double fValue)         { CallTracePrintf("%f", fValue); }
inline void                      TraceValue(unsigned short uValue) { TraceValue((unsigned int)uValue); }
inline void                      TraceValue(unsigned char uValue)  { TraceValue((unsigned int)uValue); }
inline void                      TraceValue(short iValue)          { TraceValue((int)iValue); }
inline void                      TraceValue(signed char iValue)    { TraceValue((int)iValue); }
inline void                      TraceValue(float fValue)          { TraceValue((double)fValue); }

template<typename T>
inline void TraceValue(const Array<T>& kArray)
{
	CallTracePrintf("[");
	for (size_t i = 0; i < kArray.m_iSize; ++i)
	{
		if (i > 0)
			CallTracePrintf(", ");
		TraceValue(kArray.m_pElements[i]);
	}
	CallTracePrintf("]");
}

inline void TraceFunction(const char* szName)
{
	CallTracePrintf("%s(", szName);
}

template<typename T>
inline void TraceArgument(uint32 uIndex, const T& kValue)
{
	if (uIndex > 0)
		CallTracePrintf(", ");
	TraceValue(kValue);
}

inline void TracePreArgument(uint32 uIndex, const char* szName)
{
	if (uIndex > 0)
		CallTracePrintf(", ");
	CallTracePrintf("[%s]", szName);
}

template<typename T>
inline void TracePostArgument(const char* szName, const T& kValue)
{
	CallTracePrintf(", %s = ", szName);
	TraceValue(kValue);
}

inline void TraceCall()
{
	CallTracePrintf(")");
	#if DXGL_TRACE_CALLS_FLUSH
	CallTraceFlush();
	#endif
}

template<typename T>
inline void TraceReturn(const T& kValue)
{
	CallTracePrintf(" = ");
	TraceValue(kValue);
	#if DXGL_TRACE_CALLS_FLUSH
	CallTraceFlush();
	#endif
}

inline void TraceNext()
{
	CallTracePrintf("\n");
}

#endif //DXGL_TRACE_CALLS

#if DXGL_CHECK_ERRORS
void CheckErrors();
#endif //DXGL_CHECK_ERRORS

#if DXGL_TRACE_CALLS
	#define TRACE_FUNCTION(_Name)              TraceFunction(_Name)
	#define TRACE_ARGUMENT(_Index, _Value)     TraceArgument(_Index, _Value)
	#define TRACE_PRE_ARGUMENT(_Index, _Name)  TracePreArgument(_Index, _Name)
	#define TRACE_POST_ARGUMENT(_Name, _Value) TracePostArgument(_Name, _Value)
	#define TRACE_CALL()                       TraceCall()
	#define TRACE_RETURN(_Value)               TraceReturn(_Value)
	#define TRACE_NEXT()                       TraceNext()
#else
	#define TRACE_FUNCTION(_Name)
	#define TRACE_ARGUMENT(_Index, _Value)
	#define TRACE_PRE_ARGUMENT(_Index, _Name)
	#define TRACE_POST_ARGUMENT(_Name, _Value)
	#define TRACE_CALL()
	#define TRACE_RETURN(_Value)
	#define TRACE_NEXT()
#endif

#if DXGL_CHECK_ERRORS
	#define CHECK_ERRORS() CheckErrors()
#else
	#define CHECK_ERRORS()
#endif

template<typename T>
struct Instrument
{
	inline static void Call(const char* szName, void(pfCall)())
	{
		TRACE_FUNCTION(szName);
		TRACE_CALL();
		pfCall();
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0>
	inline static void Call(const char* szName, void(pfCall)(Arg0), Arg0 kArg0)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_CALL();
		pfCall(kArg0);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1), Arg0 kArg0, Arg1 kArg1)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_CALL();
		pfCall(kArg0, kArg1);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12, Arg13 kArg13)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_ARGUMENT(13, kArg13);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12, kArg13);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12, Arg13 kArg13, Arg14 kArg14)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_ARGUMENT(13, kArg13);
		TRACE_ARGUMENT(14, kArg14);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12, kArg13, kArg14);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14, typename Arg15>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12, Arg13 kArg13, Arg14 kArg14, Arg15 kArg15)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_ARGUMENT(13, kArg13);
		TRACE_ARGUMENT(14, kArg14);
		TRACE_ARGUMENT(15, kArg15);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12, kArg13, kArg14, kArg15);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14, typename Arg15, typename Arg16>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12, Arg13 kArg13, Arg14 kArg14, Arg15 kArg15, Arg16 kArg16)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_ARGUMENT(13, kArg13);
		TRACE_ARGUMENT(14, kArg14);
		TRACE_ARGUMENT(15, kArg15);
		TRACE_ARGUMENT(16, kArg16);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12, kArg13, kArg14, kArg15, kArg16);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14, typename Arg15, typename Arg16, typename Arg17>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16, Arg17), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12, Arg13 kArg13, Arg14 kArg14, Arg15 kArg15, Arg16 kArg16, Arg17 kArg17)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_ARGUMENT(13, kArg13);
		TRACE_ARGUMENT(14, kArg14);
		TRACE_ARGUMENT(15, kArg15);
		TRACE_ARGUMENT(16, kArg16);
		TRACE_ARGUMENT(17, kArg17);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12, kArg13, kArg14, kArg15, kArg16, kArg17);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14, typename Arg15, typename Arg16, typename Arg17, typename Arg18>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16, Arg17, Arg18), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12, Arg13 kArg13, Arg14 kArg14, Arg15 kArg15, Arg16 kArg16, Arg17 kArg17, Arg18 kArg18)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_ARGUMENT(13, kArg13);
		TRACE_ARGUMENT(14, kArg14);
		TRACE_ARGUMENT(15, kArg15);
		TRACE_ARGUMENT(16, kArg16);
		TRACE_ARGUMENT(17, kArg17);
		TRACE_ARGUMENT(18, kArg18);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12, kArg13, kArg14, kArg15, kArg16, kArg17, kArg18);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14, typename Arg15, typename Arg16, typename Arg17, typename Arg18, typename Arg19>
	inline static void Call(const char* szName, void(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16, Arg17, Arg18, Arg19), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12, Arg13 kArg13, Arg14 kArg14, Arg15 kArg15, Arg16 kArg16, Arg17 kArg17, Arg18 kArg18, Arg19 kArg19)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_ARGUMENT(13, kArg13);
		TRACE_ARGUMENT(14, kArg14);
		TRACE_ARGUMENT(15, kArg15);
		TRACE_ARGUMENT(16, kArg16);
		TRACE_ARGUMENT(17, kArg17);
		TRACE_ARGUMENT(18, kArg18);
		TRACE_ARGUMENT(19, kArg19);
		TRACE_CALL();
		pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12, kArg13, kArg14, kArg15, kArg16, kArg17, kArg18, kArg19);
		TRACE_NEXT();
		CHECK_ERRORS();
	}

	template<typename Ret>
	inline static Ret Call(const char* szName, Ret(pfCall)())
	{
		TRACE_FUNCTION(szName);
		TRACE_CALL();
		Ret kResult = pfCall();
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0), Arg0 kArg0)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1), Arg0 kArg0, Arg1 kArg1)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12, Arg13 kArg13)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_ARGUMENT(13, kArg13);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12, kArg13);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12, Arg13 kArg13, Arg14 kArg14)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_ARGUMENT(13, kArg13);
		TRACE_ARGUMENT(14, kArg14);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12, kArg13, kArg14);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14, typename Arg15>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12, Arg13 kArg13, Arg14 kArg14, Arg15 kArg15)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_ARGUMENT(13, kArg13);
		TRACE_ARGUMENT(14, kArg14);
		TRACE_ARGUMENT(15, kArg15);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12, kArg13, kArg14, kArg15);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14, typename Arg15, typename Arg16>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12, Arg13 kArg13, Arg14 kArg14, Arg15 kArg15, Arg16 kArg16)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_ARGUMENT(13, kArg13);
		TRACE_ARGUMENT(14, kArg14);
		TRACE_ARGUMENT(15, kArg15);
		TRACE_ARGUMENT(16, kArg16);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12, kArg13, kArg14, kArg15, kArg16);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14, typename Arg15, typename Arg16, typename Arg17>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16, Arg17), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12, Arg13 kArg13, Arg14 kArg14, Arg15 kArg15, Arg16 kArg16, Arg17 kArg17)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_ARGUMENT(13, kArg13);
		TRACE_ARGUMENT(14, kArg14);
		TRACE_ARGUMENT(15, kArg15);
		TRACE_ARGUMENT(16, kArg16);
		TRACE_ARGUMENT(17, kArg17);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12, kArg13, kArg14, kArg15, kArg16, kArg17);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14, typename Arg15, typename Arg16, typename Arg17, typename Arg18>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16, Arg17, Arg18), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12, Arg13 kArg13, Arg14 kArg14, Arg15 kArg15, Arg16 kArg16, Arg17 kArg17, Arg18 kArg18)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_ARGUMENT(13, kArg13);
		TRACE_ARGUMENT(14, kArg14);
		TRACE_ARGUMENT(15, kArg15);
		TRACE_ARGUMENT(16, kArg16);
		TRACE_ARGUMENT(17, kArg17);
		TRACE_ARGUMENT(18, kArg18);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12, kArg13, kArg14, kArg15, kArg16, kArg17, kArg18);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}

	template<typename Ret, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14, typename Arg15, typename Arg16, typename Arg17, typename Arg18, typename Arg19>
	inline static Ret Call(const char* szName, Ret(pfCall)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16, Arg17, Arg18, Arg19), Arg0 kArg0, Arg1 kArg1, Arg2 kArg2, Arg3 kArg3, Arg4 kArg4, Arg5 kArg5, Arg6 kArg6, Arg7 kArg7, Arg8 kArg8, Arg9 kArg9, Arg10 kArg10, Arg11 kArg11, Arg12 kArg12, Arg13 kArg13, Arg14 kArg14, Arg15 kArg15, Arg16 kArg16, Arg17 kArg17, Arg18 kArg18, Arg19 kArg19)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, kArg0);
		TRACE_ARGUMENT(1, kArg1);
		TRACE_ARGUMENT(2, kArg2);
		TRACE_ARGUMENT(3, kArg3);
		TRACE_ARGUMENT(4, kArg4);
		TRACE_ARGUMENT(5, kArg5);
		TRACE_ARGUMENT(6, kArg6);
		TRACE_ARGUMENT(7, kArg7);
		TRACE_ARGUMENT(8, kArg8);
		TRACE_ARGUMENT(9, kArg9);
		TRACE_ARGUMENT(10, kArg10);
		TRACE_ARGUMENT(11, kArg11);
		TRACE_ARGUMENT(12, kArg12);
		TRACE_ARGUMENT(13, kArg13);
		TRACE_ARGUMENT(14, kArg14);
		TRACE_ARGUMENT(15, kArg15);
		TRACE_ARGUMENT(16, kArg16);
		TRACE_ARGUMENT(17, kArg17);
		TRACE_ARGUMENT(18, kArg18);
		TRACE_ARGUMENT(19, kArg19);
		TRACE_CALL();
		Ret kResult = pfCall(kArg0, kArg1, kArg2, kArg3, kArg4, kArg5, kArg6, kArg7, kArg8, kArg9, kArg10, kArg11, kArg12, kArg13, kArg14, kArg15, kArg16, kArg17, kArg18, kArg19);
		TRACE_RETURN(kResult);
		TRACE_NEXT();
		CHECK_ERRORS();
		return kResult;
	}
};

struct InstrumentCategoryGen
{
	template<typename Count, typename Name>
	inline static void Call(const char* szName, void(pfCall)(Count, Name*), Count iCount, Name* pNames)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, iCount);
		TRACE_PRE_ARGUMENT(1, "names");
		TRACE_CALL();
		pfCall(iCount, pNames);
		TRACE_POST_ARGUMENT("names", Array<Name>(pNames, iCount));
		TRACE_NEXT();
		CHECK_ERRORS();
	}
};

struct InstrumentCategoryDelete
{
	template<typename Count, typename Name>
	inline static void Call(const char* szName, void(pfCall)(Count, Name*), Count iCount, Name* pNames)
	{
		TRACE_FUNCTION(szName);
		TRACE_ARGUMENT(0, iCount);
		TRACE_ARGUMENT(1, Array<Name>(pNames, iCount));
		TRACE_CALL();
		pfCall(iCount, pNames);
		TRACE_NEXT();
		CHECK_ERRORS();
	}
};

#undef TRACE_FUNCTION
#undef TRACE_ARGUMENT
#undef TRACE_CALL
#undef TRACE_RETURN
#undef TRACE_ERRORS

}

#define CUSTOM_INSTRUMENT(_Category, _Name)                                              \
  struct glad_tag_ ## _Name;                                                             \
  namespace NCryOpenGL                                                                   \
  {                                                                                      \
  template<> struct Instrument<glad_tag_ ## _Name> : InstrumentCategory ## _Category {}; \
  }

CUSTOM_INSTRUMENT(Gen, glGenTextures)
CUSTOM_INSTRUMENT(Gen, glGenQueries)
CUSTOM_INSTRUMENT(Gen, glGenBuffers)
CUSTOM_INSTRUMENT(Gen, glGenRenderbuffers)
CUSTOM_INSTRUMENT(Gen, glGenFramebuffers)
CUSTOM_INSTRUMENT(Gen, glGenVertexArrays)
CUSTOM_INSTRUMENT(Gen, glGenSamplers)
CUSTOM_INSTRUMENT(Gen, glGenTransformFeedbacks)
CUSTOM_INSTRUMENT(Gen, glGenProgramPipelines)

CUSTOM_INSTRUMENT(Delete, glDeleteTextures)
CUSTOM_INSTRUMENT(Delete, glDeleteQueries)
CUSTOM_INSTRUMENT(Delete, glDeleteBuffers)
CUSTOM_INSTRUMENT(Delete, glDeleteRenderbuffers)
CUSTOM_INSTRUMENT(Delete, glDeleteFramebuffers)
CUSTOM_INSTRUMENT(Delete, glDeleteVertexArrays)
CUSTOM_INSTRUMENT(Delete, glDeleteSamplers)
CUSTOM_INSTRUMENT(Delete, glDeleteTransformFeedbacks)
CUSTOM_INSTRUMENT(Delete, glDeleteProgramPipelines)

#endif //__GLINSTRUMENT__

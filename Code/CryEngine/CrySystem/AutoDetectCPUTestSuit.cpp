// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*
   Written by:

   Bruce Dawson, Cygnus Software      Author of Fractal eXtreme for Win32
   http://www.cygnus-software.com/           comments@cygnus-software.com

   Last modified, March 2001


   This source code is supplied on an as-is basis.  You are free
   to use it and modify it, as long as this notice remains intact, and
   as long as Cygnus Software receives credit for any use where it is
   a substantial portion of the information involved.

   This source code has little in common with the high precision
   math code used for Fractal eXtreme, which has been optimized much
   more.


   This source file declares and defines a partially functional
   high precision math class, then uses it to calculate PI to high
   precision.


   For more information on the algorithms see:


   http://www.cygnus-software.com/misc/pidigits.htm


   In a real application, the class declaration, definition,
   and usage would be in three separate files.

   This code was compiled with MSVC5.0.  It should compile on
   MSVC6.0, and many other compilers.

   The platform specific code has been isolated at the top of
   the file, to allow adjusting to different sizes of ints, and
   little endian versus big endian byte ordering.

   This is not a complete math class.  In particular, it does
   not have full support for negative numbers.

   Exercises:

   1) Fully implement this class, to properly handle negative numbers,
   multiplication and division of InfPrec numbers, cos, sin, etc. of
   InfPrec numbers.

   2) Optimize this class.

   3) Template this class on its precision.
 */

#include "StdAfx.h"

#if CRY_PLATFORM_WINDOWS

	#include <CryCore/Assert/CryAssert.h>
	#include <CryCore/BaseTypes.h>
	#include "AutoDetectCPUTestSuit.h"

//#define ENABLE_STREAMOUT

	#ifdef ENABLE_STREAMOUT
		#include <math.h>
	#endif

	#if CRY_PLATFORM_WINDOWS
		#define LITTLE_ENDIAN
	#endif

const int BITSPERLONG(32);

inline uint16 Loword(uint32 x)
{
	return x & 0xFFFF;
}

inline uint16 Hiword(uint32 x)
{
	return (x >> 16) & 0xFFFF;
}

inline uint32 Makelong(uint16 h, uint16 l)
{
	return (h << 16) | l;
}

template<int PRECISION>
class InfPrec
{
public:
	static const int NUMLONGS = PRECISION;
	static const int NUMWORDS = PRECISION * 2;

public:
	InfPrec(int Value = 0) { Init(Value); }
	InfPrec(const InfPrec& rhs) { Init(rhs); }
	InfPrec& operator=(const InfPrec& rhs) { Init(rhs); return *this; }

	bool     operator==(int rhs) const;
	bool     operator<(int rhs) const;
	bool     operator!=(int rhs) const { return !(*this == rhs); }
	bool     operator<=(int rhs) const { return *this < rhs || *this == rhs; }
	bool     operator>=(int rhs) const { return !(*this < rhs); }
	bool     operator>(int rhs) const  { return !(*this <= rhs); }

	InfPrec& operator*=(int rhs);
	InfPrec  operator*(int rhs) const { return InfPrec(*this) *= rhs; }

	InfPrec& operator/=(int rhs);
	InfPrec  operator/(int rhs) const { return InfPrec(*this) /= rhs; }

	InfPrec& operator+=(const InfPrec& rhs);
	InfPrec  operator+(const InfPrec& rhs) const { return InfPrec(*this) += rhs; }

	InfPrec  operator-() const;
	InfPrec& operator-=(const InfPrec& rhs);
	InfPrec  operator-(const InfPrec& rhs) const { return InfPrec(*this) -= rhs; }

	#ifdef ENABLE_STREAMOUT
	void Print();
	#endif

private:
	void Init(int Value);
	void Init(const InfPrec& rhs);

	#ifdef  LITTLE_ENDIAN
	uint32&       GetLong(int Index)       { return mNumber[NUMLONGS - 1 - Index]; }
	const uint32& GetLong(int Index) const { return mNumber[NUMLONGS - 1 - Index]; }
	uint16&       GetWord(int Index)       { return reinterpret_cast<uint16*>(mNumber)[NUMWORDS - 1 - Index]; }
	const uint16& GetWord(int Index) const { return reinterpret_cast<const uint16*>(mNumber)[NUMWORDS - 1 - Index]; }
	#else
	uint32&       GetLong(int Index)       { return mNumber[Index]; }
	const uint32& GetLong(int Index) const { return mNumber[Index]; }
	uint16&       GetWord(int Index)       { return reinterpret_cast<uint16*>(mNumber)[Index]; }
	const uint16& GetWord(int Index) const { return reinterpret_cast<const uint16*>(mNumber)[Index]; }
	#endif

	uint32 mNumber[NUMLONGS];
};

template<int PRECISION>
bool InfPrec<PRECISION >::operator==(int rhs) const
{
	if ((int)GetLong(0) != rhs)
		return false;
	for (int i = 1; i < NUMLONGS; i++)
		if (GetLong(i) != 0)
			return false;
	return true;
}

template<int PRECISION>
bool InfPrec<PRECISION >::operator<(int rhs) const
{
	return (int)GetLong(0) < rhs;
}

template<int PRECISION>
InfPrec<PRECISION>& InfPrec<PRECISION >::operator*=(int rhs)
{
	assert(rhs >= 0);
	assert(*this >= 0);
	uint32 accum = 0;
	for (int i = NUMWORDS - 1; i >= 0; i--)
	{
		accum += GetWord(i) * rhs;
		GetWord(i) = Loword(accum);
		accum = Hiword(accum);
	}
	return *this;
}

template<int PRECISION>
InfPrec<PRECISION>& InfPrec<PRECISION >::operator/=(int rhs)
{
	assert(rhs > 0);
	assert(*this >= 0);
	uint16 remainder = 0;
	for (int i = 0; i < NUMWORDS; i++)
	{
		uint32 divisor = Makelong(remainder, GetWord(i));
		GetWord(i) = (uint16)(divisor / rhs);
		remainder = (uint16)(divisor % rhs);
	}
	return *this;
}

template<int PRECISION>
InfPrec<PRECISION>& InfPrec<PRECISION >::operator+=(const InfPrec& rhs)
{
	uint32 accum = 0;
	for (int i = NUMWORDS - 1; i >= 0; i--)
	{
		accum += GetWord(i) + rhs.GetWord(i);
		GetWord(i) = Loword(accum);
		accum = Hiword(accum);
	}
	return *this;
}

template<int PRECISION>
InfPrec<PRECISION> InfPrec<PRECISION >::operator-() const
{
	InfPrec Result = *this;
	for (int i = 0; i < NUMLONGS; i++)
		Result.GetLong(i) = ~Result.GetLong(i);
	bool carry = true;
	for (int i = NUMLONGS - 1; i >= 0 && carry; i--)
		if (++Result.GetLong(i) != 0)
			carry = false;
	return Result;
}

template<int PRECISION>
InfPrec<PRECISION>& InfPrec<PRECISION >::operator-=(const InfPrec& rhs)
{
	*this += -rhs;
	return *this;
}

template<int PRECISION>
void InfPrec<PRECISION >::Init(int Value)
{
	GetLong(0) = Value;
	for (int i = 1; i < NUMLONGS; i++)
		GetLong(i) = 0;
}

template<int PRECISION>
void InfPrec<PRECISION >::Init(const InfPrec& rhs)
{
	for (int i = 0; i < NUMLONGS; i++)
		mNumber[i] = rhs.mNumber[i];
}

template<int PRECISION>
InfPrec<PRECISION> ataninvint(int x)
{
	InfPrec<PRECISION> Result = InfPrec<PRECISION>(1) / x;
	int XSquared = x * x;

	int Divisor = 1;
	InfPrec<PRECISION> Term = Result;
	InfPrec<PRECISION> TempTerm;
	while (Term != 0)
	{
		Divisor += 2;
		Term /= XSquared;
		Result -= Term / Divisor;

		Divisor += 2;
		Term /= XSquared;
		Result += Term / Divisor;
	}
	return Result;
}

	#ifdef ENABLE_STREAMOUT
template<int PRECISION>
void InfPrec<PRECISION >::Print()
{
	InfPrec<PRECISION> Copy = *this; //rhs*
	printf("%d.", Copy.GetLong(0));
	Copy.GetLong(0) = 0;
	//int NumDigits = int((NUMLONGS - 1) * BITSPERLONG * (log(2.0) / log(10.0)));
	int NumDigits = (int) ((NUMLONGS - 1) * BITSPERLONG * (log(2.0) / log(10.0)));
	while (Copy != 0 && NumDigits-- > 0)
	{
		Copy *= 10;
		printf("%d", Copy.GetLong(0));
		Copy.GetLong(0) = 0;
	}
}
	#endif

int CCPUTestSuite::RunTest()
{
	const int PERFPREC(1024);
	InfPrec<PERFPREC> PI((ataninvint<PERFPREC>(5) * 4 - ataninvint<PERFPREC>(239)) * 4);
	//PI.Print();
	return 0;
}

#endif // #if CRY_PLATFORM_WINDOWS

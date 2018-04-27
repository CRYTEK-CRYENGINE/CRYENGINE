// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
MaskedVar.h

Description: 
	- templated data type that attempts to protect itself from being identifiable to cheat tools by obfuscating its value with a randomly generated bitmask
	- implemented with heavy use of operator overloads so it can be dropped-in in place of existing variable declarations with minimal or no changes required to code that uses the variable. this also allows for easy #if'ing dependant on platform
	- templated to allow for generalisation across base data types

-------------------------------------------------------------------------
History:
-	[23/08/2011] : Created by Tom Houghton

*************************************************************************/

#ifndef __MASKEDVAR_H__
#define __MASKEDVAR_H__

#ifdef _RELEASE
#define MASKEDVAR_DEBUG_VAL_ENABLED  (0)
#else
#define MASKEDVAR_DEBUG_VAL_ENABLED  (1)
#endif

template <class TVarType, typename TMaskType, TMaskType TMaxMaskValue>
class CMaskedVarT
{
	private:
		union UVal
		{
			TVarType  asVarType;
			TMaskType  asMaskType;
		};

	private:
		UVal  m_val;
		TMaskType  m_mask;
#if MASKEDVAR_DEBUG_VAL_ENABLED
		TVarType  m_dbgVal;
#endif

	public:
		CMaskedVarT() :
			m_mask(0)
		{
#if MASKEDVAR_DEBUG_VAL_ENABLED
			m_dbgVal = 0;
#endif
			static_assert(sizeof(TVarType) <= sizeof(TMaskType), "Invalid type size!");  // assert that the size of the Mask type is the same or bigger than the Var size so the XOR masking can work correctly
			static_assert(std::numeric_limits<TMaskType>::is_integer==true && std::numeric_limits<TMaskType>::is_signed==false, "'TMaskType' must be an unsigned integer type!");  // assert that the Mask type is not a float and is unsigned so that the XOR masking can work correctly
			static_assert(((TMaskType)TMaxMaskValue) == TMaxMaskValue, "Unexpected array size!");  // assert that the Mask type is large enough to hold the MaxMask value. (in practise this isn't actually needed, as the use of TMaskType as TMaxMaskValue's template paramater type means that on 360 and PC the compiler will error when trying to pass in a paramater too big for the type)
		}

		CMaskedVarT(const TVarType v)
		{
			Set(v);
		}

		operator TVarType () const
		{
			return this->Get();
		}

		CMaskedVarT& operator += (const TVarType rhs)
		{
			TVarType  v = this->Get();
			v += rhs;
			this->Set(v);
			return (*this);
		}

		CMaskedVarT& operator -= (const TVarType rhs)
		{
			TVarType  v = this->Get();
			v -= rhs;
			this->Set(v);
			return (*this);
		}

		CMaskedVarT& operator *= (const TVarType rhs)
		{
			TVarType  v = this->Get();
			v *= rhs;
			this->Set(v);
			return (*this);
		}

		CMaskedVarT& operator /= (const TVarType rhs)
		{
			TVarType  v = this->Get();
			v /= rhs;
			this->Set(v);
			return (*this);
		}

	private:
		void Set(const TVarType v)
		{
#if MASKEDVAR_DEBUG_VAL_ENABLED
			m_dbgVal = v;
#endif
			const TMaskType  m = cry_random((TMaskType)1, TMaxMaskValue);
			UVal  w;
			w.asVarType = v;
			m_val.asMaskType = (w.asMaskType ^ m);
			m_mask = m;
		}

		TVarType Get() const
		{
			UVal  w;
			w.asMaskType = 0;
			if (m_mask > 0)
			{
				w.asMaskType = (m_val.asMaskType ^ m_mask);
			}
			return w.asVarType;
		}
};

typedef CMaskedVarT<float, uint32, 0xffffffff>  TMaskedFloat;
typedef CMaskedVarT<int, uint32, 0xffffffff>  TMaskedInt;
typedef CMaskedVarT<uint32, uint32, 0xffffffff> TMaskedUInt;

#endif // __MASKEDVAR_H__


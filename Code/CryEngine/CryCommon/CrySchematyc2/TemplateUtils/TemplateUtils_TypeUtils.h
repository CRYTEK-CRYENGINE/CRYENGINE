// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// TODO : Replace type traits with std::traits?

#pragma once

#include <CryExtension/TypeList.h>

#include "TemplateUtils_PreprocessorUtils.h"
#include <CryString/StringUtils.h>

namespace TemplateUtils
{
	template <bool SELECT, typename LHS, typename RHS> struct Select;

	template <typename LHS, typename RHS> struct Select<true, LHS, RHS>
	{
		typedef LHS Result;
	};

	template <typename LHS, typename RHS> struct Select<false, LHS, RHS>
	{
		typedef RHS Result;
	};

	template <typename TYPE> struct ReferenceTraits
	{
		static const bool IsReference = false;

		typedef TYPE RemoveReference;
	};

	template <typename TYPE> struct ReferenceTraits<TYPE &>
	{
		static const bool IsReference = true;

		typedef TYPE RemoveReference;
	};

	template <typename TYPE> struct PointerTraits
	{
		static const bool IsPointer = false;

		typedef TYPE RemovePointer;
	};

	template <typename TYPE> struct PointerTraits<TYPE *>
	{
		static const bool IsPointer = true;

		typedef TYPE RemovePointer;
	};

	template <typename TYPE> struct VolatileTraits
	{
		static const bool IsVolatile = false;

		typedef TYPE RemoveVolatile;
	};

	template <typename TYPE> struct VolatileTraits<volatile TYPE>
	{
		static const bool IsVolatile = true;

		typedef TYPE RemoveVolatile;
	};

	template <typename TYPE> struct ConstTraits
	{
		static const bool IsConst = false;

		typedef TYPE RemoveConst;
	};

	template <typename TYPE> struct ConstTraits<const TYPE>
	{
		static const bool IsConst = true;

		typedef TYPE RemoveConst;
	};

	template <typename TYPE> struct RemoveReference
	{
		typedef typename ReferenceTraits<TYPE>::RemoveReference Result;
	};

	template <typename TYPE> struct RemoveReferenceAndConst
	{
		typedef typename ConstTraits<typename ReferenceTraits<TYPE>::RemoveReference>::RemoveConst Result;
	};

	template <typename TYPE> struct RemovePointer
	{
		typedef typename PointerTraits<TYPE>::RemovePointer Result;
	};

	template <typename TYPE> struct RemovePointerAndConst
	{
		typedef typename ConstTraits<typename PointerTraits<TYPE>::RemovePointer>::RemoveConst Result;
	};

	template <typename TYPE> struct GetUnqualifiedType
	{
		typedef typename ConstTraits<typename VolatileTraits<typename PointerTraits<typename ReferenceTraits<TYPE>::RemoveReference>::RemovePointer>::RemoveVolatile>::RemoveConst Result;
	};

	template <typename LHS_TYPE, typename RHS_TYPE> struct Conversion
	{
	private:

		typedef char True;
		typedef long False;

		static LHS_TYPE MakeLHSType();
		static True Test(RHS_TYPE);
		static False Test(...);

	public:

		static const bool Exists         = sizeof((Test(MakeLHSType()))) == sizeof(True);
		static const bool ExistsBothWays = Exists && Conversion<LHS_TYPE, RHS_TYPE>::Exists;
		static const bool SameType       = false;
	};

	template <class TYPE> struct Conversion<TYPE, TYPE>
	{
		static const bool Exists         = true;
		static const bool ExistsBothWays = true;
		static const bool SameType       = true;
	};

	template <class TYPE> struct Conversion<TYPE, void>
	{
		static const bool Exists         = false;
		static const bool ExistsBothWays = false;
		static const bool SameType       = false;
	};

	template <class TYPE> struct Conversion<void, TYPE>
	{
		static const bool Exists         = false;
		static const bool ExistsBothWays = false;
		static const bool SameType       = false;
	};

	template <> struct Conversion<void, void>    
	{
		static const bool Exists         = true;
		static const bool ExistsBothWays = true;
		static const bool SameType       = true;
	};

	template <typename TYPE> struct IsClass
	{
	private:

		typedef char True;
		typedef long False;

		template <class U> static True Test(void (U::*)());
		template <class U> static False Test(...);

	public:

		static const bool Result = (sizeof(Test<TYPE>(0)) == sizeof(True));
	};

	template <class LHS, class RHS> struct ListConversion;

	template <> struct ListConversion<TL::NullType, TL::NullType> 
	{
		static const bool Exists = true;
	};

	template <class HEAD, class TAIL> struct ListConversion<TL::Typelist<HEAD, TAIL>,TL::NullType>
	{
		static const bool Exists = false;
	};

	template <class HEAD, class TAIL> struct ListConversion<TL::NullType, TL::Typelist<HEAD, TAIL> >
	{
		static const bool Exists = false;
	};
	
	template <class LHS_HEAD, class LHS_TAIL, class RHS_HEAD, class RHS_TAIL> struct ListConversion<TL::Typelist<LHS_HEAD, LHS_TAIL>, TL::Typelist<RHS_HEAD, RHS_TAIL> >
	{
		static const bool Exists = (Conversion<LHS_HEAD, RHS_HEAD>::Exists && ListConversion<LHS_TAIL, RHS_TAIL>::Exists);
	};

	namespace Private
	{
		// MSVC's macro __FUNCSIG__ produces something like this:
		//const char *__cdecl TemplateUtils::Private::GetFunctionName<void (__cdecl*)(int),&void __cdecl SchematycLegacy::Entity::TestFunction(int)>::operator ()(void) const
		inline const char* GetFunctionNameImplMSVC(const char* szFunctionName, size_t& outLength)
		{
			const char* szPrefix = "TemplateUtils::Private::GetFunctionName<";
			const char* szStartOfFunctionSignature = strstr(szFunctionName, szPrefix);

			if (szStartOfFunctionSignature)
			{
				static const char* szSuffix = ">::operator";
				const char*        szEndOfFunctionName = strstr(szStartOfFunctionSignature + strlen(szPrefix), szSuffix);

				if (szEndOfFunctionName)
				{
					int32 scope = 0;
					while (true)
					{
						--szEndOfFunctionName;
						if (*szEndOfFunctionName == ')')
						{
							++scope;
						}
						else if (*szEndOfFunctionName == '(')
						{
							--scope;
							if (scope == 0)
							{
								break;
							}
						}
					}

					const char* szStartOfFunctionName = szEndOfFunctionName - 1;
					while (*(szStartOfFunctionName - 1) != ' ')
					{
						--szStartOfFunctionName;
					}

					outLength = (size_t)(szEndOfFunctionName - szStartOfFunctionName);
					return szStartOfFunctionName;
				}
			}

			CRY_ASSERT_MESSAGE(false, "Failed to extract function name!");

			outLength = 0;
			return nullptr;
		}



		// Use GetFunctionNameImplGCC() or GetFunctionNameImplClang() below
		inline const char* GetFunctionNameImplClangGCC(const char* szFunctionName, const char* szPrefix, size_t& outLength)
		{
			const char* szStartOfFunctionSignature = strstr(szFunctionName, szPrefix);

			if (szStartOfFunctionSignature)
			{
				const char* szStartOfFunctionName = szStartOfFunctionSignature + strlen(szPrefix);

				size_t len = ::strlen(szStartOfFunctionName);
				if (len > 1 && szStartOfFunctionName[len - 1] == ']')
				{
					outLength = len - 1;
					return szStartOfFunctionName;
				}
			}

			CRY_ASSERT_MESSAGE(false, "Failed to extract function name!");

			outLength = 0;
			return nullptr;
		}

		// GCC's macro __PRETTY_FUNCTION__ produces something like this:
		// const char* TemplateUtils::Private::GetFunctionName<FUNCTION_PTR_TYPE, FUNCTION_PTR>::operator()() const [with FUNCTION_PTR_TYPE = void (*)(int); FUNCTION_PTR_TYPE FUNCTION_PTR = SchematycLegacy::Entity::TestFunction]
		inline const char* GetFunctionNameImplGCC(const char* szFunctionName, size_t& outLength)
		{
			const char* szPrefix = "FUNCTION_PTR = ";
			return GetFunctionNameImplClangGCC(szFunctionName, szPrefix, outLength);
		}

		// Clang's macro __PRETTY_FUNCTION__ produces something like this:
		// const char *TemplateUtils::Private::GetFunctionName<void (*)(int), &SchematycLegacy::Entity::TestFunction>::operator()() const [FUNCTION_PTR_TYPE = void (*)(int), FUNCTION_PTR = &SchematycLegacy::Entity::TestFunction]
		inline const char* GetFunctionNameImplClang(const char* szFunctionName, size_t& outLength)
		{
			const char* szPrefix = "FUNCTION_PTR = &";
			return GetFunctionNameImplClangGCC(szFunctionName, szPrefix, outLength);
		}

		// Structure for extracting function name from function pointer.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		template <typename FUNCTION_PTR_TYPE, FUNCTION_PTR_TYPE FUNCTION_PTR> struct GetFunctionName
		{
			inline const char* operator () () const
			{
				// TODO pavloi 2017.09.04: would be nice to be able to store and return string_view here
				static const char* szResult = "";
				if (!szResult[0])
				{
					const char* szFunctionName = COMPILE_TIME_FUNCTION_NAME;
					static char szStorage[256] = "";

					size_t length = 0;
#if CRY_COMPILER_MSVC
					const char* szNameBegin = GetFunctionNameImplMSVC(szFunctionName, *&length);
#elif CRY_COMPILER_CLANG
					const char* szNameBegin = GetFunctionNameImplClang(szFunctionName, *&length);
#elif CRY_COMPILER_GCC
					const char* szNameBegin = GetFunctionNameImplGCC(szFunctionName, *&length);
#else
					const char* szNameBegin = nullptr;
#endif 

					if (length > 0)
					{
						CRY_ASSERT_MESSAGE(length < sizeof(szStorage), "Not enough space for function name storage - name will be clamped");

						cry_strcpy(szStorage, szNameBegin, length);
						szResult = szStorage;
					}
				}

				return szResult;
			}
		};
	} // namespace Private

	// Get function name.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	template <typename FUNCTION_PTR_TYPE, FUNCTION_PTR_TYPE FUNCTION_PTR> inline const char* GetFunctionName()
	{
		return Private::GetFunctionName<FUNCTION_PTR_TYPE, FUNCTION_PTR>()();
	}

	namespace Private
	{
		// Structure for extracting type name from type.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		template <typename TYPE> struct GetTypeName
		{
			inline const char* operator () () const
			{
				static const char* szResult = "";
				if(!szResult[0])
				{
					const char* szFunctionName = COMPILE_TIME_FUNCTION_NAME;
					const char* szPrefix = "TemplateUtils::Private::GetTypeName<";
					const char* szStartOfTypeName = strstr(szFunctionName, szPrefix);
					if(szStartOfTypeName)
					{
						szStartOfTypeName += strlen(szPrefix);
						const char* keyWords[] = { "struct ", "class ", "enum "	};
						for(size_t iKeyWord = 0; iKeyWord < CRY_ARRAY_COUNT(keyWords); ++ iKeyWord)
						{
							const char*		keyWord = keyWords[iKeyWord];
							const size_t	keyWordLength = strlen(keyWord);
							if(strncmp(szStartOfTypeName, keyWord, keyWordLength) == 0)
							{
								szStartOfTypeName += keyWordLength;
								break;
							}
						}
						static const char* szSffix = ">::operator";
						const char*        szEndOfTypeName = strstr(szStartOfTypeName, szSffix);
						if(szEndOfTypeName)
						{
							while(*(szEndOfTypeName - 1) == ' ')
							{
								-- szEndOfTypeName;
							}
							static char storage[8192] = "";
							cry_strcpy(storage, szStartOfTypeName, (size_t)(szEndOfTypeName - szStartOfTypeName));
							szResult = storage;
						}
					}
					CRY_ASSERT_MESSAGE(szResult[0], "Failed to extract type name!");
				}
				return szResult;
			}
		};
	}

	// Get type name.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	template <typename TYPE> inline const char* GetTypeName()
	{
		return Private::GetTypeName<TYPE>()();
	}

	// Get member offset.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	template <typename TYPE, typename MEMBER_TYPE> inline ptrdiff_t GetMemberOffset(MEMBER_TYPE TYPE::* pMember)
	{
		return reinterpret_cast<const uint8*>(&(reinterpret_cast<const TYPE*>(0x1)->*pMember)) - reinterpret_cast<const uint8*>(0x1);
	}
}
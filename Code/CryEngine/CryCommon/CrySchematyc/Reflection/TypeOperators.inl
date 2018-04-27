// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Utils/TypeUtils.h"

namespace Schematyc
{
namespace Adapters
{

template<typename TYPE, bool TYPE_IS_DEFAULT_CONSTRUCTIBLE = std::is_default_constructible<TYPE>::value> struct SDefaultConstruct;

template<typename TYPE> struct SDefaultConstruct<TYPE, true>
{
	static inline void DefaultConstruct(void* pPlacement)
	{
		new(pPlacement) TYPE();
	}

	static inline STypeOperators::DefaultConstruct Select()
	{
		return &DefaultConstruct;
	}
};

template<typename TYPE> struct SDefaultConstruct<TYPE, false>
{
	static inline STypeOperators::DefaultConstruct Select()
	{
		return nullptr;
	}
};

template<typename TYPE> struct SDestruct
{
	static inline void Destruct(void* pPlacement)
	{
		return static_cast<TYPE*>(pPlacement)->~TYPE();
	}

	static inline STypeOperators::Destruct Select()
	{
		return &Destruct;
	}
};

template<typename TYPE, bool TYPE_IS_COPY_CONSTRUCTIBLE = std::is_copy_constructible<TYPE>::value> struct SCopyConstruct;

template<typename TYPE> struct SCopyConstruct<TYPE, true>
{
	static inline void CopyConstruct(void* pPlacement, const void* pRHS)
	{
		const TYPE& rhs = *static_cast<const TYPE*>(pRHS);
		new(pPlacement) TYPE(rhs);
	}

	static inline STypeOperators::CopyConstruct Select()
	{
		return &CopyConstruct;
	}
};

template<typename TYPE> struct SCopyConstruct<TYPE, false>
{
	static inline STypeOperators::CopyConstruct Select()
	{
		return nullptr;
	}
};

template<typename TYPE, bool TYPE_HAS_COPY_ASSIGNMENT_OPERATOR = std::is_copy_assignable<TYPE>::value> struct SCopyAssign;

template<typename TYPE> struct SCopyAssign<TYPE, true>
{
	static inline void CopyAssign(void* pLHS, const void* pRHS)
	{
		TYPE& lhs = *static_cast<TYPE*>(pLHS);
		const TYPE& rhs = *static_cast<const TYPE*>(pRHS);
		lhs = rhs;
	}

	static inline STypeOperators::CopyAssign Select()
	{
		return &CopyAssign;
	}
};

template<typename TYPE> struct SCopyAssign<TYPE, false>
{
	static inline STypeOperators::CopyAssign Select()
	{
		return nullptr;
	}
};

template<typename TYPE, bool TYPE_HAS_EQUALS_OPRATOR = HasOperator::SEquals<TYPE>::value> struct SEquals;

template<typename TYPE> struct SEquals<TYPE, true>
{
	static inline bool Equals(const void* pLHS, const void* pRHS)
	{
		const TYPE& lhs = *static_cast<const TYPE*>(pLHS);
		const TYPE& rhs = *static_cast<const TYPE*>(pRHS);
		return lhs == rhs;
	}

	static inline STypeOperators::Equals Select()
	{
		return &Equals;
	}
};

template<typename TYPE> struct SEquals<TYPE, false>
{
	static inline STypeOperators::Equals Select()
	{
		return nullptr;
	}
};

template<typename TYPE, bool TYPE_IS_SERIALIZEABLE = Serialization::IsSerializeable<TYPE>()> struct SSerialize;

template<typename TYPE> struct SSerialize<TYPE, true>
{
	static inline bool Serialize(Serialization::IArchive& archive, void* pValue, const char* szName, const char* szLabel)
	{
		TYPE& value = *static_cast<TYPE*>(pValue);
		return archive(value, szName, szLabel);
	}

	static inline STypeOperators::Serialize Select()
	{
		return &Serialize;
	}
};

template<typename TYPE> struct SSerialize<TYPE, false>
{
	static inline STypeOperators::Serialize Select()
	{
		return nullptr;
	}
};

template<typename FUNCTION_PTR_TYPE, FUNCTION_PTR_TYPE FUNCTION_PTR> struct SToString;

template<typename TYPE, void(* FUNCTION_PTR)(IString&, const TYPE&)> struct SToString<void (*)(IString&, const TYPE&), FUNCTION_PTR>
{
	static inline void ToString(IString& output, const void* pInput)
	{
		(*FUNCTION_PTR)(output, *static_cast<const TYPE*>(pInput));
	}

	static inline STypeOperators::ToString Select()
	{
		return &ToString;
	}
};

template<typename TYPE, void(TYPE::* FUNCTION_PTR)(IString&) const> struct SToString<void (TYPE::*)(IString&) const, FUNCTION_PTR>
{
	static inline void ToString(IString& output, const void* pInput)
	{
		(static_cast<const TYPE*>(pInput)->*FUNCTION_PTR)(output);
	}

	static inline STypeOperators::ToString Select()
	{
		return &ToString;
	}
};

template<typename FUNCTION_PTR_TYPE, FUNCTION_PTR_TYPE FUNCTION_PTR> struct SStringGetChars;

template<typename TYPE, const char*(* FUNCTION_PTR)(const TYPE&)> struct SStringGetChars<const char*(*)(const TYPE&), FUNCTION_PTR>
{
	static inline const char* GetChars(const void* pString)
	{
		return (*FUNCTION_PTR)(*static_cast<const TYPE*>(pString));
	}

	static inline SStringOperators::GetChars Select()
	{
		return &GetChars;
	}
};

template<typename TYPE, const char*(TYPE::* FUNCTION_PTR)() const> struct SStringGetChars<const char* (TYPE::*)() const, FUNCTION_PTR>
{
	static inline const char* GetChars(const void* pString)
	{
		return (static_cast<const TYPE*>(pString)->*FUNCTION_PTR)();
	}

	static inline SStringOperators::GetChars Select()
	{
		return &GetChars;
	}
};

template<typename FUNCTION_PTR_TYPE, FUNCTION_PTR_TYPE FUNCTION_PTR> struct SArraySize;

template<typename TYPE, uint32(* FUNCTION_PTR)(const TYPE&)> struct SArraySize<uint32 (*)(const TYPE&), FUNCTION_PTR>
{
	static inline uint32 Size(const void* pArray)
	{
		return (*FUNCTION_PTR)(*static_cast<const TYPE*>(pArray));
	}

	static inline SArrayOperators::Size Select()
	{
		return &Size;
	}
};

template<typename TYPE, uint32(TYPE::* FUNCTION_PTR)() const> struct SArraySize<uint32 (TYPE::*)() const, FUNCTION_PTR>
{
	static inline uint32 Size(const void* pArray)
	{
		return (static_cast<const TYPE*>(pArray)->*FUNCTION_PTR)();
	}

	static inline SArrayOperators::Size Select()
	{
		return &Size;
	}
};

template<typename FUNCTION_PTR_TYPE, FUNCTION_PTR_TYPE FUNCTION_PTR> struct SArrayAt;

template<typename TYPE, typename ELEMENT_TYPE, ELEMENT_TYPE& (* FUNCTION_PTR)(TYPE&, uint32)> struct SArrayAt<ELEMENT_TYPE& (*)(TYPE&, uint32), FUNCTION_PTR>
{
	static inline void* At(void* pArray, uint32 pos)
	{
		return &((*FUNCTION_PTR)(*static_cast<TYPE*>(pArray), pos));
	}

	static inline SArrayOperators::At Select()
	{
		return &At;
	}
};

template<typename TYPE, typename ELEMENT_TYPE, ELEMENT_TYPE& (TYPE::* FUNCTION_PTR)(uint32)> struct SArrayAt<ELEMENT_TYPE& (TYPE::*)(uint32), FUNCTION_PTR>
{
	static inline void* At(void* pArray, uint32 pos)
	{
		return &((static_cast<TYPE*>(pArray)->*FUNCTION_PTR)(pos));
	}

	static inline SArrayOperators::At Select()
	{
		return &At;
	}
};

template<typename FUNCTION_PTR_TYPE, FUNCTION_PTR_TYPE FUNCTION_PTR> struct SArrayAtConst;

template<typename TYPE, typename ELEMENT_TYPE, const ELEMENT_TYPE& (* FUNCTION_PTR)(const TYPE&, uint32)> struct SArrayAtConst<const ELEMENT_TYPE& (*)(const TYPE&, uint32), FUNCTION_PTR>
{
	static inline const void* AtConst(const void* pArray, uint32 pos)
	{
		return &((*FUNCTION_PTR)(*static_cast<const TYPE*>(pArray), pos));
	}

	static inline SArrayOperators::AtConst Select()
	{
		return &AtConst;
	}
};

template<typename TYPE, typename ELEMENT_TYPE, const ELEMENT_TYPE& (TYPE::* FUNCTION_PTR)(uint32) const> struct SArrayAtConst<const ELEMENT_TYPE& (TYPE::*)(uint32) const, FUNCTION_PTR>
{
	static inline const void* AtConst(const void* pArray, uint32 pos)
	{
		return &((static_cast<const TYPE*>(pArray)->*FUNCTION_PTR)(pos));
	}

	static inline SArrayOperators::AtConst Select()
	{
		return &AtConst;
	}
};

template<typename FUNCTION_PTR_TYPE, FUNCTION_PTR_TYPE FUNCTION_PTR> struct SArrayPushBack;

template<typename TYPE, typename ELEMENT_TYPE, void(* FUNCTION_PTR)(TYPE&, const ELEMENT_TYPE&)> struct SArrayPushBack<void (*)(TYPE&, const ELEMENT_TYPE&), FUNCTION_PTR>
{
	static inline void PushBack(void* pArray, const void* pElement)
	{
		(*FUNCTION_PTR)(*static_cast<TYPE*>(pArray), *static_cast<const ELEMENT_TYPE*>(pElement));
	}

	static inline SArrayOperators::PushBack Select()
	{
		return &PushBack;
	}
};

template<typename TYPE, typename ELEMENT_TYPE, void(TYPE::* FUNCTION_PTR)(const ELEMENT_TYPE&)> struct SArrayPushBack<void (TYPE::*)(const ELEMENT_TYPE&), FUNCTION_PTR>
{
	static inline void PushBack(void* pArray, const void* pElement)
	{
		(static_cast<TYPE*>(pArray)->*FUNCTION_PTR)(*static_cast<const ELEMENT_TYPE*>(pElement));
	}

	static inline SArrayOperators::PushBack Select()
	{
		return &PushBack;
	}
};

} // Adapters
} // Schematyc

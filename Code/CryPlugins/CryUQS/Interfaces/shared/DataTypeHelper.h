// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Shared
	{

		//===========================================================================================================================================================================
		//
		// SDataTypeHelper
		//
		// - have const and non-const pointers yield the same CTypeInfo
		// - use-case: there's a global parameter that represents an entity-pointer (non-const pointer, mind you!), and a function that takes a const-entity-pointer and wants to access this global param
		// - TODO: should only work from "non-const source" to "const destination" (e. g. passing a non-const pointer to a function that takes a const pointer), but not the other way around
		// - suppress references completely
		//
		//===========================================================================================================================================================================

		template <class T>
		struct SDataTypeHelper
		{
			static const CTypeInfo& GetTypeInfo()
			{
				return CTypeInfo::Get<T>();
			}
		};

		template <class T>
		struct SDataTypeHelper<const T*>
		{
			static const CTypeInfo& GetTypeInfo()
			{
				return CTypeInfo::Get<const T*>();
			}
		};

		template <class T>
		struct SDataTypeHelper<T*>
		{
			static const CTypeInfo& GetTypeInfo()
			{
				return CTypeInfo::Get<const T*>();	// !! notice same type as 'const T*' (so that const and non-const pointers are always stored with the same typeid) !!
			}
		};

		template <class T>
		struct SDataTypeHelper<T&>
		{
		};

		template <class T>
		struct SDataTypeHelper<const T&>
		{
		};

	}
}

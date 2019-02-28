// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Cry
{
	namespace Memory
	{
		//! Get offset of a member variable
		template<typename TYPE, typename MEMBER_TYPE> inline ptrdiff_t GetMemberOffset(MEMBER_TYPE TYPE::* pMember)
		{
			return reinterpret_cast<uint8*>(&(reinterpret_cast<TYPE*>(1)->*pMember)) - reinterpret_cast<uint8*>(1);
		}

		// Get offset of a base structure / class
		template<typename TYPE, typename BASE_TYPE> inline ptrdiff_t GetBaseOffset()
		{
			return reinterpret_cast<uint8*>(static_cast<BASE_TYPE*>(reinterpret_cast<TYPE*>(1))) - reinterpret_cast<uint8*>(1);
		}
	}
}
// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRY_HASH_UTIL_H__
#define __CRY_HASH_UTIL_H__

#include <CryString/StringUtils.h>

//-----------------------------------------------------------------------------------
// HASH Tools.
// from Frd's code-base courtesy of AW. /FH
typedef uint32 CryHash;

struct CryHashStringId
{
	CryHashStringId()
		: id (0)
	{

	}

	CryHashStringId(CryHash _id)
		: id (_id)
	{

	}

	CryHashStringId( const char* _name )
	{
		Set(_name);
	}

	CryHashStringId(const CryHashStringId& _otherId)
	{
#if defined(_DEBUG)
		debugName = _otherId.debugName;
#endif
		id = _otherId.id;
	}

	void Set( const char* _name )
	{
		CRY_ASSERT(_name);
#if defined(_DEBUG)
		debugName = _name;
#endif
		id = CryStringUtils::HashString(_name);
	}

	ILINE bool operator == (const CryHashStringId& rhs) const
	{
		return (id == rhs.id);
	}

	ILINE bool operator != (const CryHashStringId& rhs) const
	{
		return (id != rhs.id);
	}

	ILINE bool operator <= (const CryHashStringId& rhs) const
	{
		return (id <= rhs.id);
	}

	ILINE bool operator < (const CryHashStringId& rhs) const
	{
		return (id < rhs.id);
	}

	ILINE bool operator >= (const CryHashStringId& rhs) const
	{
		return (id >= rhs.id);
	}

	ILINE bool operator > (const CryHashStringId& rhs) const
	{
		return (id > rhs.id);
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
#if defined(_DEBUG)
		pSizer->AddObject(debugName);
#endif
	}

	const char* GetDebugName() const
	{
#if defined(_DEBUG)
		return debugName.c_str();
#else
		return "";
#endif
	}

	//This is useful for look ups in debug, so we don't allocate strings
	static CryHashStringId GetIdForName(const char* _name);

#if defined(_DEBUG)
	string debugName;
#endif
	CryHash id;
};

#endif // __CRY_HASH_UTIL_H__

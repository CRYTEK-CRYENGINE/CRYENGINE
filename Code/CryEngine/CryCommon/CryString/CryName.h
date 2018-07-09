// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CrySystem/ISystem.h>
#include <CryCore/StlUtils.h>
#include <CryMemory/CrySizer.h>
#include <CryCore/CryCrc32.h>
#include <CryMemory/STLGlobalAllocator.h>

class CNameTable;

struct INameTable
{
	virtual ~INameTable(){}

	//! Name entry header, immediately after this header in memory starts actual string data.
	struct SNameEntry
	{
		//! Reference count of this string.
		int nRefCount;

		//! Current length of string.
		int nLength;

		//! Size of memory allocated at the end of this class.
		int nAllocSize;
		// Here in memory starts character buffer of size nAllocSize.
		//char data[nAllocSize].

		const char* GetStr()         { return (char*)(this + 1); }
		void        AddRef()         { nRefCount++; /*InterlockedIncrement(&_header()->nRefCount);*/ };
		int         Release()        { return --nRefCount; };
		int         GetMemoryUsage() { return sizeof(SNameEntry) + strlen(GetStr()); }
		int         GetLength()      { return nLength; }
	};

	//! Finds an existing name table entry, or creates a new one if not found.
	virtual INameTable::SNameEntry* GetEntry(const char* str) = 0;

	//! Only finds an existing name table entry, return 0 if not found.
	virtual INameTable::SNameEntry* FindEntry(const char* str) = 0;

	//! Release existing name table entry.
	virtual void Release(SNameEntry* pEntry) = 0;
	virtual int  GetMemoryUsage() = 0;
	virtual int  GetNumberOfEntries() = 0;

	//! Output all names from the table to log.
	virtual void LogNames() = 0;

	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;
};

//////////////////////////////////////////////////////////////////////////
class CNameTable : public INameTable
{
private:
	typedef std::unordered_map<const char*, SNameEntry*, stl::hash_stricmp<const char*>, stl::hash_stricmp<const char*>, stl::STLGlobalAllocator<std::pair<const char* const, SNameEntry*>>> NameMap;
	NameMap m_nameMap;

public:
	CNameTable() {}

	~CNameTable()
	{
		for (NameMap::iterator it = m_nameMap.begin(); it != m_nameMap.end(); ++it)
		{
			free(it->second);
		}
	}

	//! Only finds an existing name table entry
	//! \return 0 if not found.
	virtual INameTable::SNameEntry* FindEntry(const char* str)
	{
		SNameEntry* pEntry = stl::find_in_map(m_nameMap, str, 0);
		return pEntry;
	}

	//! Finds an existing name table entry, or creates a new one if not found.
	virtual INameTable::SNameEntry* GetEntry(const char* str)
	{
		SNameEntry* pEntry = stl::find_in_map(m_nameMap, str, 0);
		if (!pEntry)
		{
			//! Create a new entry.
			unsigned int nLen = strlen(str);
			unsigned int allocLen = sizeof(SNameEntry) + (nLen + 1) * sizeof(char);
			pEntry = (SNameEntry*)malloc(allocLen);
			assert(pEntry != NULL);
			pEntry->nRefCount = 0;
			pEntry->nLength = nLen;
			pEntry->nAllocSize = allocLen;

			//! Copy string to the end of name entry.
			char* pEntryStr = const_cast<char*>(pEntry->GetStr());
			memcpy(pEntryStr, str, nLen + 1);
			// put in map.
			//m_nameMap.insert( NameMap::value_type(pEntry->GetStr(),pEntry) );
			m_nameMap[pEntry->GetStr()] = pEntry;
		}
		return pEntry;
	}

	//! Release existing name table entry.
	virtual void Release(SNameEntry* pEntry)
	{
		assert(pEntry);
		m_nameMap.erase(pEntry->GetStr());
		free(pEntry);
	}
	virtual int GetMemoryUsage()
	{
		int nSize = 0;
		NameMap::iterator it;
		int n = 0;
		for (it = m_nameMap.begin(); it != m_nameMap.end(); it++)
		{
			nSize += strlen(it->first);
			nSize += it->second->GetMemoryUsage();
			n++;
		}
		nSize += n * 8;

		return nSize;
	}
	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_nameMap);
	}
	virtual int GetNumberOfEntries()
	{
		return m_nameMap.size();
	}

	//! Log all names inside CryName table.
	virtual void LogNames()
	{
		NameMap::iterator it;
		for (it = m_nameMap.begin(); it != m_nameMap.end(); ++it)
		{
			SNameEntry* pNameEntry = it->second;
			CryLog("[%4d] %s", pNameEntry->nLength, pNameEntry->GetStr());
		}
	}

};

class CCryName
{
public:
	CCryName();
	CCryName(const CCryName& n);
	explicit CCryName(const char* s);
	CCryName(const char* s, bool bOnlyFind);
	~CCryName();

	CCryName&   operator=(const CCryName& n);
	CCryName&   operator=(const char* s);

	bool        operator==(const CCryName& n) const;
	bool        operator!=(const CCryName& n) const;

	bool        operator==(const char* s) const;
	bool        operator!=(const char* s) const;

	bool        operator<(const CCryName& n) const;
	bool        operator>(const CCryName& n) const;

	bool        empty() const { return !m_str || !m_str[0]; }
	void        reset()       { _release(m_str);  m_str = 0; }
	void        addref()      { _addref(m_str); }

	const char* c_str() const
	{
		return (m_str) ? m_str : "";
	}
	int         length() const        { return _length(); };

	static bool find(const char* str) { return GetNameTable()->FindEntry(str) != 0; }
	void        GetMemoryUsage(ICrySizer* pSizer) const
	{
		//pSizer->AddObject(m_str);
		pSizer->AddObject(GetNameTable());   // cause for slowness?
	}
	static int GetMemoryUsage()
	{
#ifdef USE_STATIC_NAME_TABLE
		CNameTable* pTable = GetNameTable();
#else
		INameTable* pTable = GetNameTable();
#endif
		return pTable->GetMemoryUsage();
	}
	static int GetNumberOfEntries()
	{
#ifdef USE_STATIC_NAME_TABLE
		CNameTable* pTable = GetNameTable();
#else
		INameTable* pTable = GetNameTable();
#endif
		return pTable->GetNumberOfEntries();
	}

	//! Compare functor for sorting CCryNames lexically.
	struct CmpLex
	{
		bool operator()(const CCryName& n1, const CCryName& n2) const
		{
			return strcmp(n1.c_str(), n2.c_str()) < 0;
		}
	};

private:
	typedef INameTable::SNameEntry SNameEntry;

#ifdef USE_STATIC_NAME_TABLE
	static CNameTable* GetNameTable()
	{
		//! \note We cannot use a 'static CNameTable sTable' here, because that
		//! implies a static destruction order depenency - the name table is
		//! accessed from static destructor calls.
		static CNameTable* table = NULL;

		if (table == NULL)
			table = new CNameTable();
		return table;
	}
#else
	//static INameTable* GetNameTable() { return GetISystem()->GetINameTable(); }
	static INameTable* GetNameTable()
	{
		assert(gEnv && gEnv->pNameTable);
		return gEnv->pNameTable;
	}
#endif

	SNameEntry* _entry(const char* pBuffer) const { assert(pBuffer); return ((SNameEntry*)pBuffer) - 1; }
	void        _release(const char* pBuffer)
	{
		if (pBuffer && _entry(pBuffer)->Release() <= 0 && gEnv)
			GetNameTable()->Release(_entry(pBuffer));
	}
	int  _length() const              { return (m_str) ? _entry(m_str)->nLength : 0; };
	void _addref(const char* pBuffer) { if (pBuffer) _entry(pBuffer)->AddRef(); }

	const char* m_str;
};

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CryName
//////////////////////////////////////////////////////////////////////////
inline CCryName::CCryName()
{
	m_str = 0;
}

//////////////////////////////////////////////////////////////////////////
inline CCryName::CCryName(const CCryName& n)
{
	_addref(n.m_str);
	m_str = n.m_str;
}

//////////////////////////////////////////////////////////////////////////
inline CCryName::CCryName(const char* s)
{
	m_str = 0;
	*this = s;
}

//////////////////////////////////////////////////////////////////////////
inline CCryName::CCryName(const char* s, bool bOnlyFind)
{
	assert(s);
	m_str = 0;
	if (*s) // if not empty
	{
		SNameEntry* pNameEntry = GetNameTable()->FindEntry(s);
		if (pNameEntry)
		{
			m_str = pNameEntry->GetStr();
			_addref(m_str);
		}
	}
}

inline CCryName::~CCryName()
{
	_release(m_str);
}

//////////////////////////////////////////////////////////////////////////
inline CCryName& CCryName::operator=(const CCryName& n)
{
	if (m_str != n.m_str)
	{
		_release(m_str);
		m_str = n.m_str;
		_addref(m_str);
	}
	return *this;
}

//////////////////////////////////////////////////////////////////////////
inline CCryName& CCryName::operator=(const char* s)
{
	assert(s);
	const char* pBuf = 0;
	if (s && *s) // if not empty
	{
		pBuf = GetNameTable()->GetEntry(s)->GetStr();
	}
	if (m_str != pBuf)
	{
		_release(m_str);
		m_str = pBuf;
		_addref(m_str);
	}
	return *this;
}

//////////////////////////////////////////////////////////////////////////
inline bool CCryName::operator==(const CCryName& n) const
{
	return m_str == n.m_str;
}

inline bool CCryName::operator!=(const CCryName& n) const
{
	return !(*this == n);
}

inline bool CCryName::operator==(const char* str) const
{
	return m_str && stricmp(m_str, str) == 0;
}

inline bool CCryName::operator!=(const char* str) const
{
	if (!m_str)
		return true;
	return stricmp(m_str, str) != 0;
}

inline bool CCryName::operator<(const CCryName& n) const
{
	return m_str < n.m_str;
}

inline bool CCryName::operator>(const CCryName& n) const
{
	return m_str > n.m_str;
}

inline bool operator==(const string& s, const CCryName& n)
{
	return n == s;
}
inline bool operator!=(const string& s, const CCryName& n)
{
	return n != s;
}

inline bool operator==(const char* s, const CCryName& n)
{
	return n == s;
}
inline bool operator!=(const char* s, const CCryName& n)
{
	return n != s;
}

class CCryNameCRC
{
public:
	CCryNameCRC();
	CCryNameCRC(const CCryNameCRC& n);
	explicit CCryNameCRC(const char* s);
	CCryNameCRC(const char* s, bool bOnlyFind);
	explicit CCryNameCRC(uint32 n) { m_nID = n; }
	~CCryNameCRC();

	CCryNameCRC& operator=(const CCryNameCRC& n);
	CCryNameCRC& operator=(const char* s);

	bool         operator==(const CCryNameCRC& n) const;
	bool         operator!=(const CCryNameCRC& n) const;

	bool         operator==(const char* s) const;
	bool         operator!=(const char* s) const;

	bool         operator<(const CCryNameCRC& n) const;
	bool         operator>(const CCryNameCRC& n) const;

	bool         empty() const { return m_nID == 0; }
	void         reset()       { m_nID = 0; }
	uint32       get() const   { return m_nID; }
	void         add(int nAdd) { m_nID += nAdd; }

	AUTO_STRUCT_INFO;

	void GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }
private:

	uint32 m_nID;

};

inline CCryNameCRC::CCryNameCRC()
{
	m_nID = 0;
}

//////////////////////////////////////////////////////////////////////////
inline CCryNameCRC::CCryNameCRC(const CCryNameCRC& n)
{
	m_nID = n.m_nID;
}

//////////////////////////////////////////////////////////////////////////
inline CCryNameCRC::CCryNameCRC(const char* s)
{
	m_nID = 0;
	*this = s;
}

inline CCryNameCRC::~CCryNameCRC()
{
	m_nID = 0;
}

//////////////////////////////////////////////////////////////////////////
inline CCryNameCRC& CCryNameCRC::operator=(const CCryNameCRC& n)
{
	m_nID = n.m_nID;
	return *this;
}

//////////////////////////////////////////////////////////////////////////
inline CCryNameCRC& CCryNameCRC::operator=(const char* s)
{
	assert(s);
	if (*s) // if not empty
	{
		m_nID = CCrc32::ComputeLowercase(s);
	}
	return *this;
}

//////////////////////////////////////////////////////////////////////////
inline bool CCryNameCRC::operator==(const CCryNameCRC& n) const
{
	return m_nID == n.m_nID;
}

inline bool CCryNameCRC::operator!=(const CCryNameCRC& n) const
{
	return !(*this == n);
}

inline bool CCryNameCRC::operator==(const char* str) const
{
	assert(str);
	if (*str) // if not empty
	{
		uint32 nID = CCrc32::ComputeLowercase(str);
		return m_nID == nID;
	}
	return m_nID == 0;
}

inline bool CCryNameCRC::operator!=(const char* str) const
{
	if (!m_nID)
		return true;
	if (*str) // if not empty
	{
		uint32 nID = CCrc32::ComputeLowercase(str);
		return m_nID != nID;
	}
	return false;
}

inline bool CCryNameCRC::operator<(const CCryNameCRC& n) const
{
	return m_nID < n.m_nID;
}

inline bool CCryNameCRC::operator>(const CCryNameCRC& n) const
{
	return m_nID > n.m_nID;
}

inline bool operator==(const string& s, const CCryNameCRC& n)
{
	return n == s;
}
inline bool operator!=(const string& s, const CCryNameCRC& n)
{
	return n != s;
}

inline bool operator==(const char* s, const CCryNameCRC& n)
{
	return n == s;
}
inline bool operator!=(const char* s, const CCryNameCRC& n)
{
	return n != s;
}

//! \endcond
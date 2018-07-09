// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <string.h>     // memcpy
#include <algorithm>    // std::min

namespace SettingsManagerHelpers
{

namespace Utils
{
inline size_t strlen(const char* p)
{
	return p ? ::strlen(p) : 0;
}

inline size_t strcmp(const char* p0, const char* p1)
{
	return ::strcmp(p0, p1);
}

inline size_t strlen(const wchar_t* p)
{
	return p ? ::wcslen(p) : 0;
}

inline size_t strcmp(const wchar_t* p0, const wchar_t* p1)
{
	return ::wcscmp(p0, p1);
}

//! Copy of CryStringUtils_Internal::strcpy_with_clamp
inline bool strcpy_with_clamp(char* const dst, size_t const dst_size_in_bytes, const char* const src, size_t const src_size_in_bytes = (size_t)-1)
{
	if (!dst || dst_size_in_bytes < sizeof(char))
	{
		return false;
	}

	if (!src || src_size_in_bytes < sizeof(char))
	{
		dst[0] = 0;
		return src != 0;    // we return true for non-null src without characters
	}

	const size_t src_n = src_size_in_bytes;
	const size_t n = (std::min)(dst_size_in_bytes - 1, src_n);

	for (size_t i = 0; i < n; ++i)
	{
		dst[i] = src[i];
		if (!src[i])
		{
			return true;
		}
	}

	dst[n] = 0;
	return n >= src_n || src[n] == 0;
}
}

template<class T>
class CBuffer
{
public:
	typedef T element_type;

private:
	element_type* m_ptr;
	size_t        m_sizeInBytes;

public:
	CBuffer(element_type* ptr, size_t sizeInBytes)
		: m_ptr(ptr)
		, m_sizeInBytes(ptr ? sizeInBytes : 0)
	{
	}

	const element_type* getPtr() const
	{
		return m_ptr;
	}

	element_type* getPtr()
	{
		return m_ptr;
	}

	size_t getSizeInElements() const
	{
		return m_sizeInBytes / sizeof(element_type);
	}

	size_t getSizeInBytes() const
	{
		return m_sizeInBytes;
	}

	const element_type& operator[](size_t pos) const
	{
		return m_ptr[pos];
	}

	element_type& operator[](size_t pos)
	{
		return m_ptr[pos];
	}
};

typedef CBuffer<char>    CCharBuffer;
typedef CBuffer<wchar_t> CWCharBuffer;

template<class T, size_t CAPACITY>
class CFixedString
{
public:
	typedef T char_type;
	static const size_t npos = ~size_t(0);
private:
	size_t              m_count;                //!< Number of characters (not counting trailing zero).
	char_type           m_buffer[CAPACITY + 1]; //!< '+ 1' is for trailing zero.

public:
	CFixedString()
	{
		clear();
	}

	CFixedString(const char_type* p)
	{
		set(p);
	}

	CFixedString& operator=(const char_type* p)
	{
		set(p);
		return *this;
	}

	CFixedString& operator=(const CFixedString& s)
	{
		if (&s != this)
		{
			set(s.m_buffer, s.m_count);
		}
		return *this;
	}

	CBuffer<char_type> getBuffer()
	{
		return CBuffer<char_type>(m_buffer, (CAPACITY + 1) * sizeof(char_type));
	}

	char_type operator[](const size_t i) const
	{
		return m_buffer[i];
	}

	const char_type* c_str() const
	{
		return &m_buffer[0];
	}

	const size_t length() const
	{
		return m_count;
	}

	void clear()
	{
		m_count = 0;
		m_buffer[m_count] = 0;
	}

	void setLength(size_t n)
	{
		m_count = (n <= CAPACITY) ? n : CAPACITY;
		m_buffer[m_count] = 0;
	}

	char_type* ptr()
	{
		return &m_buffer[0];
	}

	CFixedString substr(size_t pos = 0, size_t n = npos) const
	{
		CFixedString s;
		if (pos < m_count && n > 0)
		{
			if (n > m_count || pos + n > m_count)
			{
				n = m_count - pos;
			}
			s.set(&m_buffer[pos], n);
		}
		return s;
	}

	void set(const char_type* p, size_t n)
	{
		if (p == 0 || n <= 0)
		{
			m_count = 0;
		}
		else
		{
			m_count = (n > CAPACITY) ? CAPACITY : n;
			// memmove() is used because p may point to m_buffer
			memmove(m_buffer, p, m_count * sizeof(*p));
		}
		m_buffer[m_count] = 0;
	}

	void set(const char_type* p)
	{
		if (p && p[0])
		{
			set(p, Utils::strlen(p));
		}
		else
		{
			clear();
		}
	}

	void append(const char_type* p, size_t n)
	{
		if (p && n > 0)
		{
			if (n > CAPACITY || m_count + n > CAPACITY)
			{
				// assert(0);
				n = CAPACITY - m_count;
			}
			if (n > 0)
			{
				memcpy(&m_buffer[m_count], p, n * sizeof(*p));
				m_count += n;
				m_buffer[m_count] = 0;
			}
		}
	}

	void append(const char_type* p)
	{
		if (p && p[0])
		{
			append(p, Utils::strlen(p));
		}
	}

	void appendAscii(const char* p, size_t n)
	{
		if (p && n > 0)
		{
			if (n > CAPACITY || m_count + n > CAPACITY)
			{
				// assert(0);
				n = CAPACITY - m_count;
			}
			if (n > 0)
			{
				for (size_t i = 0; i < n; ++i)
				{
					m_buffer[m_count + i] = p[i];
				}
				m_count += n;
				m_buffer[m_count] = 0;
			}
		}
	}

	void appendAscii(const char* p)
	{
		if (p && p[0])
		{
			appendAscii(p, Utils::strlen(p));
		}
	}

	bool equals(const char_type* p) const
	{
		return (p == 0 || p[0] == 0)
		       ? (m_count == 0)
		       : (Utils::strcmp(m_buffer, p) == 0);
	}

	void trim()
	{
		size_t begin = 0;
		while (begin < m_count && (m_buffer[begin] == ' ' || m_buffer[begin] == '\r' || m_buffer[begin] == '\t' || m_buffer[begin] == '\n'))
		{
			++begin;
		}

		if (begin >= m_count)
		{
			clear();
			return;
		}

		size_t end = m_count - 1;
		while (end > begin && (m_buffer[end] == ' ' || m_buffer[end] == '\r' || m_buffer[end] == '\t' || m_buffer[end] == '\n'))
		{
			--end;
		}

		m_count = end + 1;
		m_buffer[m_count] = 0;

		if (begin > 0)
		{
			set(&m_buffer[begin], m_count - begin);
		}
	}
};

struct SKeyValue
{
	CFixedString<char, 256>    key;
	CFixedString<wchar_t, 256> value;
};

template<size_t CAPACITY>
class CKeyValueArray
{
private:
	size_t    count;
	SKeyValue data[CAPACITY];

public:
	CKeyValueArray()
		: count(0)
	{
	}

	size_t size() const
	{
		return count;
	}

	const SKeyValue& operator[](size_t i) const
	{
		return data[i];
	}

	void clear()
	{
		count = 0;
	}

	const SKeyValue* find(const char* key) const
	{
		for (size_t i = 0; i < count; ++i)
		{
			if (data[i].key.equals(key))
			{
				return &data[i];
			}
		}
		return 0;
	}

	SKeyValue* find(const char* key)
	{
		for (size_t i = 0; i < count; ++i)
		{
			if (data[i].key.equals(key))
			{
				return &data[i];
			}
		}
		return 0;
	}

	SKeyValue* set(const char* key, const wchar_t* value)
	{
		SKeyValue* p = find(key);

		if (!p)
		{
			if (count >= CAPACITY)
			{
				return 0;
			}
			p = &data[count++];
			p->key = key;
		}

		p->value = value;

		return p;
	}

	SKeyValue* set(const char* key, const char* value)
	{
		SKeyValue* p = find(key);

		if (!p)
		{
			if (count >= CAPACITY)
			{
				return 0;
			}
			p = &data[count++];
			p->key = key;
		}

		p->value.clear();
		p->value.appendAscii(value);

		return p;
	}
};

#if defined(CRY_ENABLE_RC_HELPER)

bool Utf16ContainsAsciiOnly(const wchar_t* wstr);

void ConvertUtf16ToUtf8(const wchar_t* src, CCharBuffer dst);

void ConvertUtf8ToUtf16(const char* src, CWCharBuffer dst);

template<size_t CAPACITY>
void AddPathSeparator(CFixedString<wchar_t, CAPACITY>& wstr)
{
	if (wstr.length() <= 0)
	{
		return;
	}

	if (wstr[wstr.length() - 1] == L'/' || wstr[wstr.length() - 1] == L'\\')
	{
		return;
	}

	wstr.appendAscii("/");
}

void GetAsciiFilename(const wchar_t* wfilename, CCharBuffer buffer);

#endif // #if defined(CRY_ENABLE_RC_HELPER)

}

#if defined(CRY_ENABLE_RC_HELPER)

//! Provides settings and functions to make calls to RC.
class CEngineSettingsManager;
class CSettingsManagerTools
{
public:
	CSettingsManagerTools(const wchar_t* szModuleName = 0);
	~CSettingsManagerTools();

private:
	CEngineSettingsManager* m_pSettingsManager;

public:
	CEngineSettingsManager* GetEngineSettingsManager()
	{
		return m_pSettingsManager;
	}

	//! Loads EngineSettingsManager to get settings information.
	void GetRootPathUtf16(bool pullFromRegistry, SettingsManagerHelpers::CWCharBuffer wbuffer);
	void GetRootPathAscii(bool pullFromRegistry, SettingsManagerHelpers::CCharBuffer buffer);

	bool GetInstalledBuildPathUtf16(const int index, SettingsManagerHelpers::CWCharBuffer name, SettingsManagerHelpers::CWCharBuffer path);
	bool GetInstalledBuildPathAscii(const int index, SettingsManagerHelpers::CCharBuffer name, SettingsManagerHelpers::CCharBuffer path);

	void CallSettingsManagerExe(void* hParent);
};

#endif // #if defined(CRY_ENABLE_RC_HELPER)

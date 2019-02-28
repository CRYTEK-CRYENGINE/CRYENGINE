// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma  once
//////////////////////////////////  CRYTEK  ////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2011.
// -------------------------------------------------------------------------
//  Author           : Jaewon Jung
//  Time of creation : 8/15/2011   16:05
//  Description      : A simple string class which is safe over a DLL boundary
//  Notice           :
//		- This uses LocalAlloc()/LocalFree() to guarantee consistent memory operations
//			even with differently-configured modules.
//		- Intentionally minimal
//		-- This is developed with the use for editor plug-in DLLs in mind.
//		-- It's recommended to convert this to a more capable string implementation
//			once crossed a module boundary.
//
////////////////////////////////////////////////////////////////////////////

template<typename T>
class dll_string_tpl
{
public:
	typedef T                 value_type;
	typedef int               size_type;
	typedef const value_type* const_iterator;
	static const unsigned long kGranularity = 32;

	dll_string_tpl()
		: m_length(0)
	{
		m_data = construct(0, m_capacity);
	}

	dll_string_tpl(const value_type* str)
	{
		const size_type len = strlen(str);
		m_data = construct(len, m_capacity);
		memcpy(m_data, str, len * sizeof(value_type));
		m_length = len;
		m_data[len] = 0;
	}

	dll_string_tpl(const dll_string_tpl<T>& str)
	{
		const size_type len = str.length();
		m_data = construct(len, m_capacity);
		memcpy(m_data, str.c_str(), len * sizeof(value_type));
		m_length = len;
		m_data[len] = 0;
	}

	~dll_string_tpl()
	{
		destruct();
	}

	const value_type* c_str() const
	{
		return m_data;
	}

	size_type length() const
	{
		return m_length;
	}

	bool empty() const
	{
		return length() == 0;
	}

	size_type capacity() const
	{
		return m_capacity;
	}

	void clear()
	{
		destruct();
		m_data = construct(0, m_capacity);
		m_length = 0;
	}

	const_iterator begin() const
	{
		return c_str();
	}

	const_iterator end() const
	{
		return c_str() + length();
	}

	dll_string_tpl<T>& operator=(const dll_string_tpl<T>& rhs)
	{
		if (m_data != rhs.c_str())
		{
			assign(rhs.c_str(), rhs.length());
		}
		return *this;
	}

	dll_string_tpl<T>& operator=(const value_type* str)
	{
		assign(str, strlen(str));
		return *this;
	}

	int compare(const dll_string_tpl<T>& rhs) const
	{
		const size_type thisLen = length();
		const size_type rhsLen = rhs.length();
		if (thisLen < rhsLen)
			return -1;
		if (thisLen > rhsLen)
			return 1;

		return strcompare(c_str(), rhs.c_str(), thisLen);
	}

private:
	T*        m_data;
	T         m_end_of_data;
	size_type m_capacity;
	size_type m_length;

	value_type* construct(size_type length, size_type& out_capacity)
	{
		value_type* data(0);
		size_type capacity = length;
		if (capacity != 0)
		{
			capacity = (capacity + kGranularity - 1) & ~(kGranularity - 1);
			if (capacity < kGranularity)
				capacity = kGranularity;

			const size_type toAlloc = sizeof(value_type) * (capacity + 1);
			void* mem = LocalAlloc(0, toAlloc);
			memset(mem, 0, toAlloc);
			data = static_cast<value_type*>(mem);
		}
		else  // empty string, no allocation needed. Use our internal buffer.
		{
			data = &m_end_of_data;
		}

		out_capacity = capacity;
		*data = 0;
		return data;
	}

	void destruct()
	{
		if (m_capacity != 0)
		{
			assert(m_data != &m_end_of_data);
			LocalFree(m_data);
		}
	}

	void assign(const value_type* str, size_type len)
	{
		// Do not use with str = str.c_str()!
		assert(str != m_data);
		if (m_capacity <= len + 1)
		{
			destruct();
			m_data = construct(len, m_capacity);
		}
		memcpy(m_data, str, len * sizeof(value_type));
		m_length = len;
		m_data[len] = 0;
	}

	static int strlen(const T* str)
	{
		int len(0);
		while (*str++)
			++len;
		return len;
	}

	static int strcompare(const T* s1, const T* s2, size_type len)
	{
		for (/**/; len != 0; --len)
		{
			const T c1 = *s1;
			const T c2 = *s2;
			if (c1 != c2)
				return (c1 < c2 ? -1 : 1);

			++s1;
			++s2;
		}
		return 0;
	}
};

template<typename T>
bool operator==(const dll_string_tpl<T>& lhs, const dll_string_tpl<T>& rhs)
{
	return lhs.compare(rhs) == 0;
}

template<typename T>
bool operator!=(const dll_string_tpl<T>& lhs, const dll_string_tpl<T>& rhs)
{
	return !(lhs == rhs);
}

typedef dll_string_tpl<char> dll_string;


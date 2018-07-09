// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Schematyc
{
template<typename TYPE, uint32 SIZE> class CRingBuffer
{
public:

	inline CRingBuffer()
		: m_size(0)
		, m_pos(0)
	{
		std::fill(m_data, m_data + SIZE, TYPE());
	}

	inline void PushBack(const TYPE& value)
	{
		if (m_size < SIZE)
		{
			m_data[m_size++] = value;
		}
		else
		{
			m_data[m_pos] = value;
			m_pos = Wrap(m_pos + 1);
		}
	}

	inline TYPE& Front()
	{
		return (*this)[0];
	}

	inline const TYPE& Front() const
	{
		return (*this)[0];
	}

	inline TYPE& Back()
	{
		return (*this)[m_size - 1];
	}

	inline const TYPE& Back() const
	{
		return (*this)[m_size - 1];
	}

	inline TYPE& operator[](uint32 idx)
	{
		CRY_ASSERT(idx < m_size);
		return m_data[Wrap(m_pos + idx)];
	}

	inline const TYPE& operator[](uint32 idx) const
	{
		CRY_ASSERT(idx < m_size);
		return m_data[Wrap(m_pos + idx)];
	}

	inline uint32 Size() const
	{
		return m_size;
	}

	inline void Clear()
	{
		std::fill(m_data, m_data + SIZE, TYPE());
		m_size = 0;
		m_pos = 0;
	}

private:

	inline uint32 Wrap(uint32 pos) const
	{
		return pos < m_size ? pos : pos - m_size;
	}

private:

	TYPE   m_data[SIZE];
	uint32 m_size;
	uint32 m_pos;
};
} // Schematyc

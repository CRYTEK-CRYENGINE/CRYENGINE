// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SERIALIZATION_HELPER_H__
#define __SERIALIZATION_HELPER_H__

//-----------------------------------------------------------------------------
class CSerializationHelper
{
public:
	//-----------------------------------------------------------------------------
	CSerializationHelper(int size)
		: m_pos(0)
		, m_size(size)
		, m_bBufferOverflow(false)
		, m_bWriting(true)
	{
		m_pBuffer = new char[size];
	}

	CSerializationHelper(char* buffer, int size)
		: m_pos(0)
		, m_size(size)
		, m_bBufferOverflow(false)
		, m_bWriting(false)
	{
		m_pBuffer = buffer;
	}

	//-----------------------------------------------------------------------------
	~CSerializationHelper()
	{
		if (m_bWriting)
		{
			delete[] m_pBuffer;
		}
	}

	//-----------------------------------------------------------------------------
	char* GetBuffer()         { return m_pBuffer; }
	int   GetUsedSize() const { return m_pos; }
	bool  Overflow() const    { return m_bBufferOverflow; }
	bool  IsWriting() const   { return m_bWriting; }
	void  Clear()             { m_pos = 0; m_bBufferOverflow = false; }

	void  ExpandBuffer(int spaceRequired)
	{
		while (spaceRequired > m_size)
		{
			m_size *= 2;
		}
		char* pNewBuffer = new char[m_size];
		memcpy(pNewBuffer, m_pBuffer, m_pos);
		SAFE_DELETE_ARRAY(m_pBuffer);
		m_pBuffer = pNewBuffer;
	}

	//-----------------------------------------------------------------------------
	template<class T>
	void Write(const T& constData)
	{
		CRY_ASSERT(m_bWriting);

		T data = constData;

		if (eBigEndian)
		{
			SwapEndian(data, eBigEndian);   //swap to Big Endian
		}

		if (m_pos + (int)sizeof(T) > m_size)
		{
			ExpandBuffer(m_pos + (int)sizeof(T));
		}

		memcpy(m_pBuffer + m_pos, &data, sizeof(T));
		m_pos += sizeof(T);
	}

	//-----------------------------------------------------------------------------
	void WriteString(const char* pString)
	{
		CRY_ASSERT(m_bWriting);

		int length = strlen(pString);
		// Write the length of the string followed by the string itself
		Write(length);
		if (m_pos + length + 1 > m_size)
		{
			ExpandBuffer(m_pos + length + 1);
		}
		memcpy(m_pBuffer + m_pos, pString, length);
		m_pBuffer[m_pos + length] = '\0';
		m_pos += length + 1;
	}

	//-----------------------------------------------------------------------------
	void WriteBuffer(const char* pData, int length)
	{
		if (m_pos + length > m_size)
		{
			ExpandBuffer(m_pos + length);
		}
		memcpy(m_pBuffer + m_pos, pData, length);
		m_pos += length;
	}

	//-----------------------------------------------------------------------------
	template<class T>
	void Read(T& data)
	{
		CRY_ASSERT(!m_bWriting);

		if (m_pos + (int)sizeof(T) <= m_size)
		{
			memcpy(&data, m_pBuffer + m_pos, sizeof(T));
			m_pos += sizeof(T);
			if (eBigEndian)
			{
				SwapEndian(data, eBigEndian);   //swap to Big Endian
			}
		}
		else
		{
			m_bBufferOverflow = true;
			CRY_ASSERT_MESSAGE(false, "Buffer size is not large enough");
		}
	}

	//-----------------------------------------------------------------------------
	const char* ReadString()
	{
		CRY_ASSERT(!m_bWriting);

		const char* pResult = NULL;
		int length = 0;
		// Read the length of the string followed by the string itself
		Read(length);
		if (m_pos + length <= m_size)
		{
			pResult = m_pBuffer + m_pos;
			m_pos += length + 1;
		}
		else
		{
			m_bBufferOverflow = true;
			CRY_ASSERT_MESSAGE(false, "Buffer size is not large enough");
		}
		return pResult;
	}

private:
	//-----------------------------------------------------------------------------
	char* m_pBuffer;
	int   m_pos;
	int   m_size;
	bool  m_bBufferOverflow;
	bool  m_bWriting;
};

#endif  // __SERIALIZATION_HELPER_H__

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _ARRAY2D_H_
#define _ARRAY2D_H_

// Dynamic replacement for static 2d array
template<class T> struct Array2d
{
	Array2d()
	{
		m_nSize = 0;
		m_pData = 0;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_pData, m_nSize * m_nSize * sizeof(T));
	}

	int  GetSize()     { return m_nSize; }
	int  GetDataSize() { return m_nSize * m_nSize * sizeof(T); }

	T*   GetData()     { return m_pData; }

	T*   GetDataEnd()  { return &m_pData[m_nSize * m_nSize]; }

	void SetData(T* pData, int nSize)
	{
		Allocate(nSize);
		memcpy(m_pData, pData, nSize * nSize * sizeof(T));
	}

	void Allocate(int nSize)
	{
		if (m_nSize == nSize)
			return;

		delete[] m_pData;

		m_nSize = nSize;
		m_pData = new T[nSize * nSize];
		memset(m_pData, 0, nSize * nSize * sizeof(T));
	}

	~Array2d()
	{
		delete[] m_pData;
	}

	void Reset()
	{
		delete[] m_pData;
		m_pData = 0;
		m_nSize = 0;
	}

	T*  m_pData;
	int m_nSize;

	T* operator[](const int& nPos) const
	{
		assert(nPos >= 0 && nPos < m_nSize);
		return &m_pData[nPos * m_nSize];
	}

	Array2d& operator=(const Array2d& other)
	{
		Allocate(other.m_nSize);
		memcpy(m_pData, other.m_pData, m_nSize * m_nSize * sizeof(T));
		return *this;
	}
};

#endif // _ARRAY2D_H_

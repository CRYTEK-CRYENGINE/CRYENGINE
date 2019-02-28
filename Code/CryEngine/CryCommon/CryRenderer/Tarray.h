// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CrySystem/ILog.h>
#include <CrySystem/File/ICryPak.h> //impl of fxopen
#include <CryMath/Cry_Math.h>

#ifndef CLAMP
	#define CLAMP(X, mn, mx) ((X) < (mn) ? (mn) : ((X) < (mx) ? (X) : (mx)))
#endif

#ifndef SATURATE
	#define SATURATE(X) clamp_tpl(X, 0.f, 1.f)
#endif

#ifndef SATURATEB
	#define SATURATEB(X) CLAMP(X, 0, 255)
#endif

#ifndef LERP
	#define LERP(A, B, Alpha) ((A) + (Alpha) * ((B)-(A)))
#endif

//! General array class.
//! Can refer to a general (unowned) region of memory (m_nAllocatedCount = 0).
//! Can allocate, grow, and shrink an array.
//! Does not deep copy.
template<class T> class TArray
{
protected:
	T*           m_pElements;
	unsigned int m_nCount;
	unsigned int m_nAllocatedCount;

public:
	typedef T value_type;

	//! Empty array.
	TArray()
	{
		MEMSTAT_REGISTER_CONTAINER(this, EMemStatContainerType::MSC_Vector, T);
		ClearArr();
	}

	//! Create a new array, delete it on destruction.
	TArray(int Count)
	{
		MEMSTAT_REGISTER_CONTAINER(this, EMemStatContainerType::MSC_Vector, T);
		m_nCount = Count;
		m_nAllocatedCount = Count;
		MEMSTAT_USAGE(begin(), MemSize());
		m_pElements = NULL;
		Realloc(0);
	}
	TArray(int Use, int Max)
	{
		MEMSTAT_REGISTER_CONTAINER(this, EMemStatContainerType::MSC_Vector, T);
		m_nCount = Use;
		m_nAllocatedCount = Max;
		MEMSTAT_USAGE(begin(), MemSize());
		m_pElements = NULL;
		Realloc(0);
	}

	//! References pre-existing memory. Does not delete it.
	TArray(T* Elems, int Count)
	{
		MEMSTAT_REGISTER_CONTAINER(this, EMemStatContainerType::MSC_Vector, T);
		m_pElements = Elems;
		m_nCount = Count;
		m_nAllocatedCount = 0;
		MEMSTAT_BIND_TO_CONTAINER(this, m_pElements);
		MEMSTAT_USAGE(begin(), MemSize());
	}
	~TArray()
	{
		Free();
		MEMSTAT_UNREGISTER_CONTAINER(this);
	}

	void Free()
	{
		m_nCount = 0;
		MEMSTAT_USAGE(begin(), MemSize());
		if (m_nAllocatedCount)
		{
			CryModuleMemalignFree(m_pElements);
		}
		m_nAllocatedCount = 0;
		m_pElements = NULL;
	}

	void Create(int Count)
	{
		MEMSTAT_USAGE(begin(), 0);
		MEMSTAT_UNBIND_FROM_CONTAINER(this, m_pElements);
		m_pElements = NULL;
		m_nCount = Count;
		m_nAllocatedCount = Count;
		Realloc(0);
		Clear();
	}
	void Copy(const TArray<T>& src)
	{
		MEMSTAT_USAGE(begin(), 0);
		MEMSTAT_UNBIND_FROM_CONTAINER(this, m_pElements);
		Free();
		m_pElements = NULL;
		m_nCount = m_nAllocatedCount = src.Num();
		Realloc(0);
		PREFAST_ASSUME(m_pElements); // realloc asserts if it fails - so this is safe
		memcpy(m_pElements, src.m_pElements, src.Num() * sizeof(T));
	}
	void Copy(const T* src, unsigned int numElems)
	{
		int nOffs = m_nCount;
		Grow(numElems);
		memcpy(&m_pElements[nOffs], src, numElems * sizeof(T));
	}
	void Align4Copy(const T* src, unsigned int& numElems)
	{
		int nOffs = m_nCount;
		Grow((numElems + 3) & ~3);
		memcpy(&m_pElements[nOffs], src, numElems * sizeof(T));
		if (numElems & 3)
		{
			int nSet = 4 - (numElems & 3);
			memset(&m_pElements[nOffs + numElems], 0, nSet);
			numElems += nSet;
		}
	}

	void Realloc(int nOldAllocatedCount)
	{
		if (!m_nAllocatedCount)
		{
			MEMSTAT_USAGE(begin(), 0);
			MEMSTAT_UNBIND_FROM_CONTAINER(this, m_pElements);
			m_pElements = NULL;
		}
		else
		{
			PREFAST_SUPPRESS_WARNING(6308) m_pElements = (T*)CryModuleReallocAlign(m_pElements, m_nAllocatedCount * sizeof(T), alignof(T));
			MEMSTAT_BIND_TO_CONTAINER(this, m_pElements);
			MEMSTAT_USAGE(begin(), MemSize());
			assert(m_pElements);
		}
	}

	void Remove(unsigned int Index, unsigned int Count = 1)
	{
		if (Count)
		{
			memmove(m_pElements + Index, m_pElements + (Index + Count), sizeof(T) * (m_nCount - Index - Count));
			m_nCount -= Count;
			MEMSTAT_USAGE(begin(), MemSize());
		}
	}

	void Shrink()
	{
		if (m_nCount == 0 || m_nAllocatedCount == 0)
			return;
		assert(m_nAllocatedCount >= m_nCount);
		if (m_nAllocatedCount != m_nCount)
		{
			int nOldAllocatedCount = m_nAllocatedCount;
			m_nAllocatedCount = m_nCount;
			Realloc(nOldAllocatedCount);
		}
	}

	void _Remove(unsigned int Index, unsigned int Count)
	{
		assert(Index >= 0);
		assert(Index <= m_nCount);
		assert((Index + Count) <= m_nCount);

		Remove(Index, Count);
	}

	unsigned int Num(void) const                { return m_nCount; }
	unsigned int Capacity(void) const           { return m_nAllocatedCount; }
	unsigned int MemSize(void) const            { return m_nCount * sizeof(T); }
	void         SetNum(unsigned int n)         { m_nCount = m_nAllocatedCount = n; MEMSTAT_USAGE(begin(), MemSize()); }
	void         SetUse(unsigned int n)         { m_nCount = n; MEMSTAT_USAGE(begin(), MemSize()); }
	void         Alloc(unsigned int n)          { int nOldAllocatedCount = m_nAllocatedCount; m_nAllocatedCount = n; Realloc(nOldAllocatedCount); }
	void         Reserve(unsigned int n)        { int nOldAllocatedCount = m_nAllocatedCount; SetNum(n); Realloc(nOldAllocatedCount); Clear(); }
	void         ReserveNoClear(unsigned int n) { int nOldAllocatedCount = m_nAllocatedCount; SetNum(n); Realloc(nOldAllocatedCount); }
	void         Expand(unsigned int n)
	{
		if (n > m_nAllocatedCount)
			ReserveNew(n);
	}
	void ReserveNew(unsigned int n)
	{
		int num = m_nCount;
		if (n > m_nAllocatedCount)
		{
			int nOldAllocatedCount = m_nAllocatedCount;
			m_nAllocatedCount = n * 2;
			Realloc(nOldAllocatedCount);
		}
		m_nCount = n;
		MEMSTAT_USAGE(begin(), MemSize());
		memset(&m_pElements[num], 0, sizeof(T) * (m_nCount - num));
	}
	T* Grow(unsigned int n)
	{
		int nStart = m_nCount;
		m_nCount += n;
		MEMSTAT_USAGE(begin(), MemSize());
		if (m_nCount > m_nAllocatedCount)
		{
			int nOldAllocatedCount = m_nAllocatedCount;
			m_nAllocatedCount = m_nCount * 2;
			Realloc(nOldAllocatedCount);
		}
		return &m_pElements[nStart];
	}
	T* GrowReset(unsigned int n)
	{
		int num = m_nAllocatedCount;
		T* Obj = AddIndex(n);
		if (num != m_nAllocatedCount)
			memset(&m_pElements[num], 0, sizeof(T) * (m_nAllocatedCount - num));
		return Obj;
	}

	unsigned int* GetNumAddr(void)           { return &m_nCount; }
	T**           GetDataAddr(void)          { return &m_pElements; }

	T*            Data(void) const           { return m_pElements; }
	T&            Get(unsigned int id) const { return m_pElements[id]; }

	void          Assign(TArray& fa)
	{
		MEMSTAT_USAGE(begin(), 0);
		MEMSTAT_UNBIND_FROM_CONTAINER(this, m_pElements);
		m_pElements = fa.m_pElements;
		m_nCount = fa.m_nCount;
		m_nAllocatedCount = fa.m_nAllocatedCount;
		MEMSTAT_BIND_TO_CONTAINER(this, m_pElements);
		MEMSTAT_USAGE(begin(), MemSize());
	}

	/*const TArray operator=(TArray fa) const
	   {
	   TArray<T> t = TArray(fa.m_nCount,fa.m_nAllocatedCount);
	   for ( int i=0; i<fa.Num(); i++ )
	   {
	    t.AddElem(fa[i]);
	   }
	   return t;
	   }*/

	const T&  operator[](unsigned int i) const { assert(i < m_nCount); return m_pElements[i]; }
	T&        operator[](unsigned int i)       { assert(i < m_nCount); return m_pElements[i]; }
	T&        operator*()                      { assert(m_nCount > 0); return *m_pElements;   }

	TArray<T> operator()(unsigned int Start)
	{
		assert(Start < m_nCount);
		return TArray<T>(m_pElements + Start, m_nCount - Start);
	}
	TArray<T> operator()(unsigned int Start, unsigned int Count)
	{
		assert(Start < m_nCount);
		assert(Start + Count <= m_nCount);
		return TArray<T>(m_pElements + Start, Count);
	}

	//! For simple types only.
	TArray(const TArray<T>& cTA)
	{
		m_pElements = NULL;
		m_nCount = m_nAllocatedCount = cTA.Num();
		MEMSTAT_USAGE(begin(), MemSize());
		Realloc(0);
		if (m_pElements)
			memcpy(m_pElements, &cTA[0], m_nCount * sizeof(T));
		/*for (unsigned int i=0; i<cTA.Num(); i++ )
		   {
		   AddElem(cTA[i]);
		   }*/
	}
	inline TArray& operator=(const TArray& cTA)
	{
		Free();
		new(this)TArray(cTA);
		return *this;
	}
	void ClearArr(void)
	{
		m_nCount = 0;
		m_nAllocatedCount = 0;
		MEMSTAT_USAGE(begin(), MemSize());
		MEMSTAT_UNBIND_FROM_CONTAINER(this, m_pElements);
		m_pElements = NULL;
	}

	void Clear(void)
	{
		memset(m_pElements, 0, m_nCount * sizeof(T));
	}

	void AddString(const char* szStr)
	{
		assert(szStr);
		int nLen = strlen(szStr) + 1;
		T* pDst = Grow(nLen);
		memcpy(pDst, szStr, nLen);
	}

	void Set(unsigned int m)
	{
		memset(m_pElements, m, m_nCount * sizeof(T));
	}

	ILINE T* AddIndex(unsigned int inc)
	{
		unsigned int nIndex = m_nCount;
		unsigned int nNewCount = m_nCount + inc;

		if (nNewCount > m_nAllocatedCount)
		{
			int nOldAllocatedCount = m_nAllocatedCount;
			m_nAllocatedCount = nNewCount + (nNewCount >> 1) + 10;
			Realloc(nOldAllocatedCount);
		}

		m_nCount = nNewCount;
		MEMSTAT_USAGE(begin(), MemSize());
		return &m_pElements[nIndex];
	}

	T& Insert(unsigned int nIndex, unsigned int inc = 1)
	{
		m_nCount += inc;
		MEMSTAT_USAGE(begin(), MemSize());
		if (m_nCount > m_nAllocatedCount)
		{
			int nOldAllocatedCount = m_nAllocatedCount;
			m_nAllocatedCount = m_nCount + (m_nCount >> 1) + 32;
			Realloc(nOldAllocatedCount);
		}
		memmove(&m_pElements[nIndex + inc], &m_pElements[nIndex], (m_nCount - inc - nIndex) * sizeof(T));

		return m_pElements[nIndex];
	}

	void AddIndexNoCache(unsigned int inc)
	{
		m_nCount += inc;
		MEMSTAT_USAGE(begin(), MemSize());
		if (m_nCount > m_nAllocatedCount)
		{
			int nOldAllocatedCount = m_nAllocatedCount;
			m_nAllocatedCount = m_nCount;
			Realloc(nOldAllocatedCount);
		}
	}

	void Add(const T& elem) { AddElem(elem); }
	void AddElem(const T& elem)
	{
		unsigned int m = m_nCount;
		AddIndex(1);
		m_pElements[m] = elem;
	}
	void AddElemNoCache(const T& elem)
	{
		unsigned int m = m_nCount;
		AddIndexNoCache(1);
		m_pElements[m] = elem;
	}

	int Find(const T& p)
	{
		for (unsigned int i = 0; i < m_nCount; i++)
		{
			if (p == (*this)[i])
				return i;
		}
		return -1;
	}

	void Delete(unsigned int n) { DelElem(n); }
	void DelElem(unsigned int n)
	{
		//    memset(&m_pElements[n],0,sizeof(T));
		_Remove(n, 1);
	}

	void Load(const char* file_name)
	{
		Clear();
		FILE* f = fxopen(file_name, "rb");
		if (!f)
			return;

		int size = 0;
		fread(&size, 4, 1, f);

		while (!feof(f) && sizeof(T) == size)
		{
			T tmp;
			if (fread(&tmp, 1, sizeof(T), f) == sizeof(T))
				AddElem(tmp);
		}

		fclose(f);
	}

	//! Standard compliance interface.
	//! This is for those who don't want to learn the non standard and thus not very
	//! convenient interface of TArray, but are unlucky enough not to be able to avoid using it.
	void clear()                    { Free(); }
	void resize(unsigned int nSize) { reserve(nSize); m_nCount = nSize; MEMSTAT_USAGE(begin(), MemSize()); }
	void reserve(unsigned int nSize)
	{
		if (nSize > m_nAllocatedCount)
			Alloc(nSize);
	}
	unsigned size() const                { return m_nCount; }
	unsigned capacity() const            { return m_nAllocatedCount; }
	bool     empty() const               { return size() == 0; }
	void     push_back(const T& rSample) { Add(rSample); }
	void     pop_back()                  { m_nCount--; MEMSTAT_USAGE(begin(), MemSize()); }
	void     erase(T* pElem)
	{
		int n = int(pElem - m_pElements);
		assert(n >= 0 && n < m_nCount);
		_Remove(n, 1);
	}
	T*       begin()                { return m_pElements; }
	T*       end()                  { return m_pElements + m_nCount; }
	T        last()                 { return m_pElements[m_nCount - 1]; }
	const T* begin() const          { return m_pElements; }
	const T* end() const            { return m_pElements + m_nCount; }

	int      GetMemoryUsage() const { return (int)(m_nAllocatedCount * sizeof(T)); }
};

template<class T> inline void Exchange(T& X, T& Y)
{
	const T Tmp = X;
	X = Y;
	Y = Tmp;
}

//! \endcond
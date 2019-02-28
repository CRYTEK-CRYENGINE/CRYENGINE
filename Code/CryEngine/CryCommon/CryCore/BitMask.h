// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   BitMask.h
//  Created:     2015-7-31 by Anton.
//  Description: BitMask with templatized max length and either fixed or allocatable storage
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef _BITMASK_H
#define _BITMASK_H

#include <CryCore/BaseTypes.h>

struct bitmaskPtr
{
	bitmaskPtr() { size = 0; data = 0; refCounted = 0; }
	~bitmaskPtr() { setptr(0); }
	uint* data;
	uint  size       : 31;
	uint  refCounted : 1;
	uint        operator[](int i) const { return data[i]; }
	uint&       operator[](int i)       { return data[i]; }

	int         getsize() const         { return size; }
	bitmaskPtr& setsize(int newSize)
	{
		if (size != newSize || refCounted && size > 0 && newSize > 0 && data[-1] > 1)
		{
			uint* newData = 0;
			if (newSize)
				memcpy(newData = (new uint[newSize + 1]) + 1, data, min(newSize, (int)size) * sizeof(uint));
			setptr(newData);
			if (newSize)
				newData[-1] = 1, refCounted = 1;
		}
		for (int i = size; i < newSize; i++) data[i] = 0;
		size = newSize;
		return *this;
	}

	bitmaskPtr& setptr(uint* newData)
	{
		if (data && refCounted)
		{
			assert((int)data[-1] > 0);
			if (CryInterlockedDecrement((volatile int*)data - 1) == 0)
				delete[] (data - 1);
		}
		data = newData;
		refCounted = 0;
		return *this;
	}
};

template<int MaxSize> struct bitmaskBuf
{
	bitmaskBuf() { size = 0; }
	uint data[MaxSize];
	int  size;
	uint        operator[](int i) const { return data[i]; }
	uint&       operator[](int i)       { return data[i]; }

	int         getsize() const         { return size; }
	bitmaskBuf& setsize(int newSize)
	{
		for (int i = size; i < newSize; i++) data[i] = 0;
		size = newSize;
		return *this;
	}
};

struct bitmaskOneBit
{
	bitmaskOneBit() { index = 0; }
	uint index;
	uint operator[](int i) const { return -iszero(i - ((int)index >> 5)) & 1u << (index & 31); }
	int  getsize() const         { return (index >> 5) + 1; }
};

ILINE int bitsUsed(uint64 x)
{
	if (!x) return 0;
	union
	{
		float f;
		uint  i;
	} u;
	u.f = (float)x;
	int nbits = (u.i >> 23) - 127;
	return nbits - (((1LL << nbits) - 1 - (int64)x) >> 63);
}

template<class Data, int MaxSize> struct bitmask_t
{
	Data data;

	bitmask_t() {}
	bitmask_t(uint64 mask) { bmset(data, mask); }
	bitmask_t(const bitmask_t& src) { bmref(src.data, data); }
	bitmask_t(bitmask_t&& src) { bmcopy(src.data, data); } //!< C++11 constructor from temp rvalue.
	bitmask_t& operator=(uint64 mask)          { bmset(data, mask); return *this; }
	bitmask_t& operator=(const bitmask_t& src) { bmcopy(src.data, data); return *this; }
	template<class Data1> bitmask_t(const bitmask_t<Data1, MaxSize>& src) { bmref(src.data, data); }
	template<class Data1> bitmask_t(bitmask_t<Data1, MaxSize>&& src) { bmcopy(src.data, data); }
	template<class Data1> bitmask_t& operator=(const bitmask_t<Data1, MaxSize>& src) { bmcopy(src.data, data); return *this; }

	bool                             operator!() const                               { int i, j; for (i = j = 0; i < data.getsize(); j |= data[i++]) ; return !j; }
	bool                             operator!=(int) const                           { return !!*this; } //!< Should only be used to compare with 0.
	bitmask_t&                       operator<<=(int shift)                          { return *this = *this << shift; }
	bitmask_t&                       operator<<=(uint shift)                         { return *this = *this << (int)shift; }
	template<class Data1> bitmask_t& operator&=(const bitmask_t<Data1, MaxSize>& op) { for (int i = 0; i < data.getsize(); i++) data[i] &= op.data[i]; return *this; }
	template<class Data1> bitmask_t& operator|=(const bitmask_t<Data1, MaxSize>& op)
	{
		data.setsize(max(data.getsize(), op.data.getsize()));
		for (int i = 0; i < op.data.getsize(); i++) data[i] |= op.data[i];
		return *this;
	}
	bitmask_t<bitmaskBuf<MaxSize>, MaxSize> operator~() const
	{
		bitmask_t<bitmaskBuf<MaxSize>, MaxSize> res;
		res.data.setsize(MaxSize);
		int i;
		for (i = 0; i < data.getsize(); i++) res.data[i] = ~data[i];
		for (; i < MaxSize; i++) res.data[i] = ~0;
		return res;
	}
};

template<class Data> ILINE void bmset(Data& dst, uint64 mask)
{
	int nbits = bitsUsed(mask);
	if (!mask) dst.setsize(0);
	else if (nbits < 33)
		dst.setsize(1)[0] = (uint)mask;
	else *(uint64*)&dst.setsize(2)[0] = mask;
}
ILINE void                                    bmset(bitmaskOneBit& dst, uint64 mask) { dst.index = mask == 1 ? 0 : ilog2(mask); }

template<class Data1, class Data2> ILINE void bmcopy(const Data1& src, Data2& dst)
{
	dst.setsize(src.getsize());
	for (int i = 0; i < dst.getsize(); i++)
		dst[i] = src[i];
}
ILINE void bmcopy(const bitmaskPtr& src, bitmaskPtr& dst)
{
	if (src.refCounted && src.size)
	{
		dst.setptr(src.data);
		dst.size = src.size;
		CryInterlockedIncrement((volatile int*)src.data - 1);
		dst.refCounted = 1;
	}
	else
	{
		dst.setsize(src.getsize());
		for (int i = 0; i < dst.getsize(); i++)
			dst[i] = src[i];
	}
}
ILINE void                                    bmcopy(const bitmaskOneBit& src, bitmaskOneBit& dst)   { dst.index = src.index; }

template<class Data1, class Data2> ILINE void bmref(const Data1& src, Data2& dst)                    { bmcopy(src, dst); }
template<int MaxSize> ILINE void              bmref(const bitmaskBuf<MaxSize>& src, bitmaskPtr& dst) { dst.setptr((uint*)src.data); dst.size = src.size; }
ILINE void                                    bmref(const bitmaskPtr& src, bitmaskPtr& dst)
{
	if (!src.refCounted)
		dst.setptr((uint*)src.data), dst.size = src.size;
	else bmcopy(src, dst);
}

template<class Data1, class Data2, int MaxSize> ILINE bitmask_t<bitmaskBuf<MaxSize>, MaxSize> operator&(const bitmask_t<Data1, MaxSize>& op1, const bitmask_t<Data2, MaxSize>& op2)
{
	bitmask_t<bitmaskBuf<MaxSize>, MaxSize> res;
	res.data.setsize(min(op1.data.getsize(), op2.data.getsize()));
	for (int i = 0; i < res.data.getsize(); i++)
		res.data[i] = op1.data[i] & op2.data[i];
	return res;
}

template<class Data1, class Data2, int MaxSize> ILINE bitmask_t<bitmaskBuf<MaxSize>, MaxSize> operator^(const bitmask_t<Data1, MaxSize>& op1, const bitmask_t<Data2, MaxSize>& op2)
{
	bitmask_t<bitmaskBuf<MaxSize>, MaxSize> res;
	res.data.setsize(max(op1.data.getsize(), op2.data.getsize()));
	int i;
	if (op1.data.getsize() > op2.data.getsize())
		for (i = op1.data.getsize() - 1; i >= op2.data.getsize(); i--)
			res.data[i] = op1.data[i];
	else
		for (i = op2.data.getsize() - 1; i >= op1.data.getsize(); i--)
			res.data[i] = op2.data[i];
	for (; i >= 0; i--)
		res.data[i] = op1.data[i] ^ op2.data[i];
	return res;
}

template<class Data1, class Data2, int MaxSize> ILINE bool operator<(const bitmask_t<Data1, MaxSize>& op1, const bitmask_t<Data2, MaxSize>& op2)
{
	int i;
	for (i = op2.data.getsize() - 1; i >= op1.data.getsize(); i--)
		if (op2.data[i]) return true;
	for (; i >= 0; i--)
		if (op1.data[i] < op2.data[i]) return true;
		else if (op1.data[i] > op2.data[i])
			return false;
	return false;
}

template<class Data1, class Data2, int MaxSize> ILINE bool operator!=(const bitmask_t<Data1, MaxSize>& op1, const bitmask_t<Data2, MaxSize>& op2)
{
	int i;
	for (i = op2.data.getsize() - 1; i >= op1.data.getsize(); i--)
		if (op2.data[i]) return true;
	for (i = op1.data.getsize() - 1; i >= op2.data.getsize(); i--)
		if (op1.data[i]) return true;
	for (; i >= 0; i--)
		if (op1.data[i] != op2.data[i]) return true;
	return false;
}

template<class Data, int MaxSize> ILINE bitmask_t<bitmaskBuf<MaxSize>, MaxSize> operator<<(const bitmask_t<Data, MaxSize>& op, int shift)
{
	bitmask_t<bitmaskBuf<MaxSize>, MaxSize> res;
	int size = op.data.getsize(), nbits = 0;
	while (size > 0 && !op.data[size - 1])
		size--;
	nbits = size > 0 ? bitsUsed(op.data[size - 1]) + (size - 1) * 32 : 0;
	res.data.setsize(((nbits - 1 + shift) >> 5) + 1);
	if (size > 0)
	{
		uint64 top = (uint64)op.data[size - 1] << (shift & 31);
		res.data[size - 1 + (shift >> 5)] |= (uint)top;
		if (size + (shift >> 5) < res.data.getsize())
			res.data[size + (shift >> 5)] |= (uint)(top >> 32);
		for (int i = size - 2; i >= 0; i--)
			*(uint64*)&res.data[i + (shift >> 5)] |= (uint64)op.data[i] << (shift & 31);
	}
	return res;
}
template<int MaxSize> bitmask_t<bitmaskOneBit, MaxSize> ILINE operator<<(const bitmask_t<bitmaskOneBit, MaxSize>& op, int shift)
{
	bitmask_t<bitmaskOneBit, MaxSize> res;
	res.data.index = op.data.index + shift;
	return res;
}
template<class Data, int MaxSize> ILINE bitmask_t<bitmaskBuf<MaxSize>, MaxSize> operator<<(const bitmask_t<Data, MaxSize>& op, uint shift)          { return op << (int)shift; }
template<int MaxSize> ILINE bitmask_t<bitmaskOneBit, MaxSize>                   operator<<(const bitmask_t<bitmaskOneBit, MaxSize>& op, uint shift) { return op << (int)shift; }

template<class Data, int MaxSize> ILINE bitmask_t<bitmaskBuf<MaxSize>, MaxSize> operator-(const bitmask_t<Data, MaxSize>& op, int sub)
{
	bitmask_t<bitmaskBuf<MaxSize>, MaxSize> res = op;
	int i;
	for (i = 0; i < res.data.getsize(); i++)
	{
		uint64 j = (uint64)res.data[i] - sub;
		res.data[i] = (uint)j;
		sub = -(int)(j >> 32);
	}
	if (sub)
		for (res.data.setsize(MaxSize); i < MaxSize; i++)
			res.data[i] = ~0u;
	return res;
}
template<class Data, int MaxSize> ILINE bitmask_t<bitmaskBuf<MaxSize>, MaxSize> operator-(const bitmask_t<Data, MaxSize>& op, uint sub) { return op - (int)sub; }
template<class Data, int MaxSize> ILINE bitmask_t<bitmaskBuf<MaxSize>, MaxSize> operator+(const bitmask_t<Data, MaxSize>& op, int add)  { return op - (-add); }

template<class Data, int MaxSize> ILINE int                                     ilog2(const bitmask_t<Data, MaxSize>& mask)
{
	int i;
	for (i = mask.data.getsize() - 1; i > 0 && !mask.data[i]; i--)
		;
	return ilog2(mask.data[i]) + i * 32;
}
template<int MaxSize> ILINE int ilog2(const bitmask_t<bitmaskOneBit, MaxSize>& mask) { return mask.data.index; }

//! Assumes radix 16.
template<int MaxSize> ILINE char* _ui64toa(bitmask_t<bitmaskPtr, MaxSize> mask, char* str, int radix)
{
	int len = mask.data.getsize();
	for (int i = 0; i < len; i++)
	{
		sprintf(str + i * 8, "%.8x", mask.data[len - 1 - i]);
	}
	if (!len)
	{
		*str++ = '0';
	}
	str[len * 8] = 0;
	return str;
}

#if 0 // fallback version
typedef uint64 hidemask;
typedef uint64 hidemaskLoc;
typedef uint64 hidemaskOneBit;
const uint64 hidemask1 = 1ul;
#else
typedef bitmask_t<bitmaskPtr, 8>    hidemask;
typedef bitmask_t<bitmaskBuf<8>, 8> hidemaskLoc;
typedef bitmask_t<bitmaskOneBit, 8> hidemaskOneBit;
	#define hidemask1 hidemaskOneBit(1)
#endif

#endif

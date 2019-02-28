// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __bitarray_h__
#define __bitarray_h__

#if _MSC_VER > 1000
	#pragma once
#endif

//////////////////////////////////////////////////////////////////////////
//
// CBitArray is similar to std::vector but faster to clear.
//
//////////////////////////////////////////////////////////////////////////
class CBitArray
{
public:
	struct BitReference
	{
		uint32* p;
		uint32  mask;
		BitReference(uint32* __x, uint32 __y)
			: p(__x), mask(__y) {}

	public:
		BitReference() : p(0), mask(0) {}

		operator bool() const {
			return !(!(*p & mask));
		}
		BitReference& operator=(bool __x)
		{
			if (__x)
				*p |= mask;
			else
				*p &= ~mask;
			return *this;
		}
		BitReference& operator=(const BitReference& __x)        { return *this = bool(__x);  }
		bool          operator==(const BitReference& __x) const { return bool(*this) == bool(__x); }
		bool          operator<(const BitReference& __x) const  { return !bool(*this) && bool(__x); }
		BitReference& operator|=(bool __x)
		{
			if (__x)
				*p |= mask;
			return *this;
		}
		BitReference& operator&=(bool __x)
		{
			if (!__x)
				*p &= ~mask;
			return *this;
		}
		void flip() {* p ^= mask; }
	};

	CBitArray() { m_base = NULL; m_bits = NULL; m_size = 0; m_numBits = 0; };
	CBitArray(int numBits)  { resize(numBits); };
	~CBitArray()  { if (m_base) free(m_base); };

	void resize(int c)
	{
		m_numBits = c;
		int newSize = ((c + 63) & (~63)) >> 5;
		if (newSize > m_size)
			Alloc(newSize);
	}
	int  size() const  { return m_numBits; };
	bool empty() const { return m_numBits == 0; };

	//////////////////////////////////////////////////////////////////////////
	void set()
	{
		memset(m_bits, 0xFFFFFFFF, m_size * sizeof(uint32));  // Set all bits.
	}
	//////////////////////////////////////////////////////////////////////////
	void set(int numBits)
	{
		int num = (numBits >> 3) + 1;
		if (num > (m_size * sizeof(uint32))) num = m_size * sizeof(uint32);
		memset(m_bits, 0xFFFFFFFF, num);  // Reset num bits.
	}

	//////////////////////////////////////////////////////////////////////////
	void clear()
	{
		memset(m_bits, 0, m_size * sizeof(uint32)); // Reset all bits.
	}

	//////////////////////////////////////////////////////////////////////////
	void clear(int numBits)
	{
		int num = (numBits >> 3) + 1;
		if (num > (m_size * sizeof(uint32))) num = m_size * sizeof(uint32);
		memset(m_bits, 0, num); // Reset num bits.
	}

	//////////////////////////////////////////////////////////////////////////
	// Check if all bits are 0.
	bool is_zero() const
	{
		for (int i = 0; i < m_size; i++)
			if (m_bits[i] != 0)
				return false;
		return true;
	}

	// Count number of set bits.
	int count_bits()  const
	{
		int c = 0;
		for (int i = 0; i < m_size; i++)
		{
			uint32 v = m_bits[i];
			for (int j = 0; j < 32; j++)
			{
				if (v & (1 << (j & 0x1F))) c++; // if bit set increase bit count.
			}
		}
		return c;
	}

	BitReference       operator[](int pos)       { return BitReference(&m_bits[index(pos)], shift(pos)); }
	const BitReference operator[](int pos) const { return BitReference(&m_bits[index(pos)], shift(pos)); }

	//////////////////////////////////////////////////////////////////////////
	void swap(CBitArray& bitarr)
	{
		std::swap(m_base, bitarr.m_base);
		std::swap(m_bits, bitarr.m_bits);
		std::swap(m_size, bitarr.m_size);
	}

	CBitArray& operator=(const CBitArray& b)
	{
		if (m_size != b.m_size)
		{
			Alloc(b.m_size);
		}
		memcpy(m_bits, b.m_bits, m_size * sizeof(uint32));
		return *this;
	}

	bool checkByte(int pos) const { return reinterpret_cast<char*>(m_bits)[pos] != 0; };

	//////////////////////////////////////////////////////////////////////////
	// Compresses this bit array into the specified one.
	// Uses run length encoding compression.
	void compress(CBitArray& b) const
	{
		int i, countz, compsize, bsize;
		char* out;
		char* in;

		bsize = m_size * 4;
		compsize = 0;
		in = (char*)m_bits;
		for (i = 0; i < bsize; i++)
		{
			compsize++;
			if (in[i] == 0)
			{
				countz = 1;
				while (++i < bsize)
				{
					if (in[i] == 0 && countz != 255) countz++; else break;
				}
				i--;
				compsize++;
			}
		}
		b.resize((compsize + 1) << 3);
		out = (char*)b.m_bits;
		in = (char*)m_bits;
		*out++ = bsize;
		for (i = 0; i < bsize; i++)
		{
			*out++ = in[i];
			if (in[i] == 0)
			{
				countz = 1;
				while (++i < bsize)
				{
					if (in[i] == 0 && countz != 255) countz++; else break;
				}
				i--;
				*out++ = countz;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Decompress specified bit array in to this one.
	// Uses run length encoding compression.
	void decompress(CBitArray& b)
	{
		int raw, decompressed, c;
		char* out, * in;

		in = (char*)m_bits;
		out = (char*)b.m_bits;
		decompressed = 0;
		raw = *in++;
		while (decompressed < raw)
		{
			if (*in != 0)
			{
				*out++ = *in++;
				decompressed++;
			}
			else
			{
				in++;
				c = *in++;
				decompressed += c;
				while (c) { *out++ = 0; c--; };
			}
		}
		m_numBits = decompressed;
	}

	void CopyFromMem(const char* src, int size)
	{
		Alloc(size);
		memcpy(m_bits, src, size);
	}
	int CopyToMem(char* trg)
	{
		memcpy(trg, m_bits, m_size);
		return m_size;
	}

private:
	void*   m_base;
	uint32* m_bits;
	int     m_size;
	int     m_numBits;

	void Alloc(int s)
	{
		if (m_base) free(m_base);
		m_size = s;
		m_base = (char*)malloc(m_size * sizeof(uint32) + 32);
		m_bits = (uint32*)(((UINT_PTR)m_base + 31) & (~31)); // align by 32.
		memset(m_bits, 0, m_size * sizeof(uint32));          // Reset all bits.
	}
	uint32 shift(int pos) const
	{
		return (1 << (pos & 0x1F));
	}
	uint32 index(int pos) const
	{
		return pos >> 5;
	}

	friend  int concatBitarray(CBitArray& b1, CBitArray& b2, CBitArray& test, CBitArray& res);
};

inline int concatBitarray(CBitArray& b1, CBitArray& b2, CBitArray& test, CBitArray& res)
{
	unsigned int b, any;
	any = 0;
	for (int i = 0; i < b1.size(); i++)
	{
		b = b1.m_bits[i] & b2.m_bits[i];
		any |= (b & (~test.m_bits[i])); // test if any different from test(i) bit set.
		res.m_bits[i] = b;
	}
	return any;
}

#endif // __bitarray_h__


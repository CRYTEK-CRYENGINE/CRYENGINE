// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __BRANCHLESS_MASK__
#define __BRANCHLESS_MASK__

///////////////////////////////////////////
// helper functions for branch elimination
//
// msb/lsb - most/less significant byte
//
// mask - 0xFFFFFFFF
// nz   - not zero
// zr   - is zero

ILINE const uint32 nz2msb(const uint32 x)
{
	return -(int32)x | x;
}

ILINE const uint32 msb2mask(const uint32 x)
{
	return (int32)(x) >> 31;
}

ILINE const uint32 nz2one(const uint32 x)
{
	return nz2msb(x) >> 31; // int((bool)x);
}

ILINE const uint32 nz2mask(const uint32 x)
{
	return (int32)msb2mask(nz2msb(x)); // -(int32)(bool)x;
}

//! Select integer with mask (0xFFFFFFFF or 0x0 only!).
ILINE const uint32 iselmask(const uint32 mask, uint32 x, const uint32 y)
{
	return (x & mask) | (y & ~mask);
}

//! mask if( x != 0 && y != 0)
ILINE const uint32 mask_nz_nz(const uint32 x, const uint32 y)
{
	return msb2mask(nz2msb(x) & nz2msb(y));
}

//! mask if( x != 0 && y == 0)
ILINE const uint32 mask_nz_zr(const uint32 x, const uint32 y)
{
	return msb2mask(nz2msb(x) & ~nz2msb(y));
}

//! mask if( x == 0 && y == 0)
ILINE const uint32 mask_zr_zr(const uint32 x, const uint32 y)
{
	return ~nz2mask(x | y);
}

#endif//__BRANCHLESS_MASK__

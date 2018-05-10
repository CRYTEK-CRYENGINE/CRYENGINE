// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  CryEngine Source File.
//  Copyright (C), Crytek.
// -------------------------------------------------------------------------
//  File name:   SwapEndianness.h
//  Version:     v1.00
//  Created:     08/01/2008 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SwapEndianness_h__
#define __SwapEndianness_h__

#pragma once


inline void SwapEndians_(void* pData, size_t nCount, size_t nSizeCheck)
{
	// Primitive type.
	switch (nSizeCheck)
	{
	case 1:
		break;
	case 2:
		{
			while (nCount--)
			{
				uint16& i = *((uint16*&)pData)++;
				i = ((i>>8) + (i<<8)) &0xFFFF;
			}
			break;
		}
	case 4:
		{
			while (nCount--)
			{
				uint32& i = *((uint32*&)pData)++;
				i = (i>>24) + ((i>>8)&0xFF00) + ((i&0xFF00)<<8) + (i<<24);
			}
			break;
		}
	case 8:
		{
			while (nCount--)
			{
				uint64& i = *((uint64*&)pData)++;
				i = (i>>56) + ((i>>40)&0xFF00) + ((i>>24)&0xFF0000) + ((i>>8)&0xFF000000)
					+ ((i&0xFF000000)<<8) + ((i&0xFF0000)<<24) + ((i&0xFF00)<<40) + (i<<56);
			}
			break;
		}
	default:
		assert(0);
	}
}


template<class T>
void SwapEndianness(T* t, std::size_t count)
{
	SwapEndians_(t, count, sizeof(T));
}


template<class T>
void SwapEndianness(T& t)
{
	SwapEndianness(&t, 1);
}


#endif // __SwapEndianness_h__
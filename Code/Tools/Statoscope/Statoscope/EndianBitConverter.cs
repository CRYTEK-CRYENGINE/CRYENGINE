// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Statoscope
{
  static class EndianBitConverter
  {
    private static readonly EEndian m_destEndian = BitConverter.IsLittleEndian ? EEndian.LITTLE_ENDIAN : EEndian.BIG_ENDIAN;

    public static Int16 ToInt16(byte[] b, int p, EEndian srcEndian)
    {
      Int16 i = BitConverter.ToInt16(b, p);
      if (srcEndian != m_destEndian)
      {
        unsafe
        {
          SwapEndian2((ushort*)&i);
        }
      }
      return i;
    }

    public static UInt16 ToUInt16(byte[] b, int p, EEndian srcEndian)
    {
      UInt16 i = BitConverter.ToUInt16(b, p);
      if (srcEndian != m_destEndian)
      {
        unsafe
        {
          SwapEndian2((ushort*)&i);
        }
      }
      return i;
    }

    public static Int32 ToInt32(byte[] b, int p, EEndian srcEndian)
    {
      Int32 i = BitConverter.ToInt32(b, p);
      if (srcEndian != m_destEndian)
      {
        unsafe
        {
          SwapEndian4((uint*)&i);
        }
      }
      return i;
    }

    public static UInt32 ToUInt32(byte[] b, int p, EEndian srcEndian)
    {
      UInt32 i = BitConverter.ToUInt32(b, p);
      if (srcEndian != m_destEndian)
      {
        unsafe
        {
          SwapEndian4((uint*)&i);
        }
      }
      return i;
    }

    public static Int64 ToInt64(byte[] b, int p, EEndian srcEndian)
    {
      Int64 i = BitConverter.ToInt64(b, p);
      if (srcEndian != m_destEndian)
      {
        unsafe
        {
          SwapEndian8((ulong*)&i);
        }
      }
      return i;
    }

    public static UInt64 ToUInt64(byte[] b, int p, EEndian srcEndian)
    {
      UInt64 i = BitConverter.ToUInt64(b, p);
      if (srcEndian != m_destEndian)
      {
        unsafe
        {
          SwapEndian8((ulong*)&i);
        }
      }
      return i;
    }

    public static float ToSingle(byte[] b, int p, EEndian srcEndian)
    {
      if (srcEndian != m_destEndian)
      {
        UInt32 i = BitConverter.ToUInt32(b, p);
        unsafe
        {
          SwapEndian4((uint*)&i);
          return *(float*)&i;
        }
      }

      return BitConverter.ToSingle(b, p);
    }

    unsafe public static void SwapEndian8(UInt64* pVal)
    {
      UInt64 v = *pVal;
      v = ((v & 0xff00ff00ff00ff00) >> 8) | ((v & 0x00ff00ff00ff00ff) << 8);
      v = ((v & 0xffff0000ffff0000) >> 16) | ((v & 0x0000ffff0000ffff) << 16);
      v = ((v & 0xffffffff00000000) >> 32) | ((v & 0x00000000ffffffff) << 32);
      *pVal = v;
    }

    unsafe public static void SwapEndian4(uint *pVal)
    {
      *pVal = ( (*pVal & 0xff000000) >> 24 |
                (*pVal & 0x00ff0000) >> 8 |
                (*pVal & 0x0000ff00) << 8 |
                (*pVal & 0x000000ff) << 24 );
    }

    unsafe public static void SwapEndian2(UInt16* pVal)
    {
      *pVal = (UInt16)((*pVal << 8) | (*pVal >> 8));
    }
  }
}

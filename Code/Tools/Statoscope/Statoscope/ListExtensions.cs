// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Collections.Generic;

namespace Statoscope
{
  static class ListExtensions
  {
    public delegate bool LessThan<T>(T a, T b);
    public delegate X Map<T, X>(T v);

    public static int MappedBinarySearchIndex<T, X>(this List<T> lst, X v, Map<T, X> map, LessThan<X> lt)
    {
      int l = 0, u = lst.Count;

      while ((u - l) > 1)
      {
        int d = u - l;
        int m = l + d / 2;

        if (lt(v, map(lst[m])))
        {
          u = m;
        }
        else
        {
          l = m;
        }
      }

      return l;
    }
  }

  static class ArrayExtensions
  {
    public static int MappedBinarySearchIndex<T, X>(this T[] lst, X v, ListExtensions.Map<T, X> map, ListExtensions.LessThan<X> lt)
    {
      int l = 0, u = lst.Length;

      while ((u - l) > 1)
      {
        int d = u - l;
        int m = l + d / 2;

        if (lt(v, map(lst[m])))
        {
          u = m;
        }
        else
        {
          l = m;
        }
      }

      return l;
    }
  }
}

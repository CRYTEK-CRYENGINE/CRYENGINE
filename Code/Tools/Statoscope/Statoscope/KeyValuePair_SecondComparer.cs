// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Statoscope
{
  class KeyValuePair_SecondComparer<K, V> : IComparer<KeyValuePair<K, V>> where V : IComparable
  {
    public int Compare(KeyValuePair<K, V> x, KeyValuePair<K, V> y)
    {
      return x.Value.CompareTo(y.Value);
    }
  }
}

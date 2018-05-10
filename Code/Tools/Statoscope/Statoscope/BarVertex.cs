// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Statoscope
{
  [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Explicit)]
  struct BarVertex
  {
    public BarVertex(float x, float y, float z, uint color)
    {
      this.x = x;
      this.y = y;
      this.z = z;
      this.color = color;
    }

    [System.Runtime.InteropServices.FieldOffset(0)]
    public float x;
    [System.Runtime.InteropServices.FieldOffset(4)]
    public float y;
    [System.Runtime.InteropServices.FieldOffset(8)]
    public float z;
    [System.Runtime.InteropServices.FieldOffset(12)]
    public uint color;
  }
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Statoscope
{
  struct RectD
  {
    public double Left, Top, Right, Bottom;
    public double Width { get { return Right - Left; } }
    public double Height { get { return Bottom - Top; } }
    public RectD(double l, double t, double r, double b)
    {
      Left = l;
      Top = t;
      Right = r;
      Bottom = b;
    }

    public PointD Size
    {
      get { return new PointD(Right - Left, Bottom - Top); }
    }
  }
}

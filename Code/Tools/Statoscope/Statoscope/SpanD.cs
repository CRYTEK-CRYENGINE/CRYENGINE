// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Statoscope
{
  struct SpanD
  {
    public double Min, Max;

    public double Length
    {
      get { return Max - Min; }
    }

    public SpanD(double min, double max)
    {
      Min = min;
      Max = max;
    }
  }
}

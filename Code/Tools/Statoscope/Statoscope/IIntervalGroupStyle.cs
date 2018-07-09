// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Drawing;

namespace Statoscope
{
  public interface IIntervalGroupStyle
  {
    Color GetIntervalColor(Interval iv);
    int[] ReorderRails(IntervalTree tree, int[] rails, double selectionStart, double selectionEnd);
  }
}

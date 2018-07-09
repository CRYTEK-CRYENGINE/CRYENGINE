// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Drawing;

namespace Statoscope
{
  class StreamingTextureIntervalGroupStyle : IIntervalGroupStyle
  {
    #region IIntervalGroupStyle Members

    public Color GetIntervalColor(Interval iv)
    {
      StreamingTextureInterval siv = (StreamingTextureInterval)iv;
      if (!siv.InUse)
      {
        return Color.Red;
      }
      else
      {
        return Color.Green;
      }
    }

    public int[] ReorderRails(IntervalTree tree, int[] rails, double selectionStart, double selectionEnd)
    {
      return rails;
    }

    #endregion
  }
}

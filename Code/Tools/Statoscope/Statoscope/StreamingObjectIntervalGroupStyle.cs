// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Drawing;

namespace Statoscope
{
  class StreamingObjectIntervalGroupStyle : IIntervalGroupStyle
  {
    #region IIntervalGroupStyle Members

    public Color GetIntervalColor(Interval iv)
    {
      StreamingObjectInterval siv = (StreamingObjectInterval)iv;
      switch (siv.Stage)
      {
        case StreamingObjectStage.Unloaded: return Color.Blue;
        case StreamingObjectStage.Requested: return Color.Violet;
        case StreamingObjectStage.LoadedUnused: return Color.Red;
        case StreamingObjectStage.LoadedUsed: return Color.Green;
      }
      return Color.Red;
    }

    public int[] ReorderRails(IntervalTree tree, int[] rails, double selectionStart, double selectionEnd)
    {
      return rails;
    }

    #endregion
  }
}

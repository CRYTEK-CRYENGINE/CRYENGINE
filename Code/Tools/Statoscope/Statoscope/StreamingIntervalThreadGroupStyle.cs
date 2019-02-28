// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Drawing;

namespace Statoscope
{
  class StreamingIntervalThreadGroupStyle : IIntervalGroupStyle
  {
    private const float MaxPriority = 6.0f;

    #region IIntervalGroupStyle Members

    public System.Drawing.Color GetIntervalColor(Interval iv)
    {
      StreamingInterval siv = (StreamingInterval)iv;
			//float hue = 0.75f - (siv.PerceptualImportance / MaxPriority) * 0.75f;
			float hue = 0.75f - ((float)siv.Source / (float)StreamTaskType.Count) * 0.75f;
			
      return new RGB(new HSV(hue, 1.0f, 1.0f)).ConvertToSysDrawCol();
    }

    public int[] ReorderRails(IntervalTree tree, int[] rails, double selectionStart, double selectionEnd)
    {
      return rails;
    }

    #endregion
  }
}

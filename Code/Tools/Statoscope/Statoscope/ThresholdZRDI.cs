// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Xml.Serialization;

namespace Statoscope
{
	// Fixme: the zones currently update frame by frame and don't go back
	// this means that when new frames affect moving averages of existing frames, the zones can be out of date
	public abstract class ThresholdZRDI : ZoneHighlighterRDI
	{
		[XmlAttribute] public float MinThreshold, MaxThreshold;

		public ThresholdZRDI()
		{
		}

		public ThresholdZRDI(string basePath, string name, RGB colour, float minThreshold, float maxThreshold)
			: base(basePath, name, colour)
		{
			MinThreshold = minThreshold;
			MaxThreshold = maxThreshold;
		}

		public override void UpdateInstance(ZoneHighlighterInstance zhi, int fromIdx, int toIdx)
		{
			if (!IsValid(zhi.LogView))
				return;

			ZoneHighlighterInstance.XRange activeRange = zhi.ActiveRange;
			FrameRecord previousFR, fr = null;
			ViewFrameRecord vfr;

			for (int i = fromIdx; i <= toIdx; i++)
			{
				previousFR = fr;
				fr = zhi.LogView.m_baseLogData.FrameRecords[i];
				vfr = zhi.LogView.m_baseViewFRs[i];

				float value = GetValue(fr, vfr, zhi.LogView);

				if (activeRange == null)
				{
					if (value > MinThreshold && (value < MaxThreshold || MaxThreshold == -1.0f))
						activeRange = zhi.StartRange(fr);
						//activeRange = zhi.StartRange(previousFR != null ? previousFR : fr);
				}
				else
				{
					if (value < MinThreshold || (value > MaxThreshold && MaxThreshold != -1.0f))
						activeRange = zhi.EndRange(fr, activeRange);
				}
			}
		}

		protected abstract float GetValue(FrameRecord fr, ViewFrameRecord vfr, LogView logView);
		protected virtual bool IsValid(LogView logView) { return true; }
	}
}

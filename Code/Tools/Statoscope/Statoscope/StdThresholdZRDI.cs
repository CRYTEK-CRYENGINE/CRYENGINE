// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Xml.Serialization;

namespace Statoscope
{
	public class StdThresholdZRDI : ThresholdZRDI
	{
		[XmlAttribute] public string TrackedPath = "";
		[XmlAttribute] public bool ValuesAreFPS = false;

		public StdThresholdZRDI()
		{
		}

		public StdThresholdZRDI(string basePath, string name)
			: base(basePath, name, RGB.RandomHueRGB(), 0.0f, 0.0f)
		{
		}

		public StdThresholdZRDI(string basePath, string name, RGB colour, string trackedPath, float minThreshold, float maxThreshold)
			: base(basePath, name, colour, minThreshold, maxThreshold)
		{
			TrackedPath = trackedPath;
		}

		protected override float GetValue(FrameRecord fr, ViewFrameRecord vfr, LogView logView)
		{
			OverviewRDI ordi = logView.m_logControl.m_ordiTree[TrackedPath];
			IReadOnlyFRVs frvs = ordi.ValueIsPerLogView ? vfr.Values : fr.Values;
			float value = frvs[ordi.ValuesPath];
			return (ValuesAreFPS && value != 0.0f) ? (1000.0f / value) : value;
		}

		protected override bool IsValid(LogView logView)
		{
			return logView.m_logControl.m_ordiTree.ContainsPath(TrackedPath);
		}
	}
}

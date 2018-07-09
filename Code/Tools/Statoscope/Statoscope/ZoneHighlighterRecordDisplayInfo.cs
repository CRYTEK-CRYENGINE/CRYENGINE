// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Xml.Serialization;

namespace Statoscope
{
	[XmlInclude(typeof(StdThresholdZRDI))]
	public class ZoneHighlighterRDI : RecordDisplayInfo<ZoneHighlighterRDI>
	{
		[XmlAttribute] public bool NameFromPath = true;

		public ZoneHighlighterRDI()
		{
		}

		public ZoneHighlighterRDI(string basePath, string name)
			: base(basePath, name, 1.0f)
		{
			CanHaveChildren = true;
		}

		public ZoneHighlighterRDI(string basePath, string name, RGB colour)
			: base(basePath, name, 1.0f)
		{
			Colour = colour;
		}

		// Update zhi's XRanges
		public virtual void UpdateInstance(ZoneHighlighterInstance zhi, int fromIdx, int toIdx)
		{
		}
	}
}

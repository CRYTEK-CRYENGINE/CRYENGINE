// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Statoscope
{
	class UserMarkerZRDI : ZoneHighlighterRDI
	{
		private readonly UserMarker FromMarker, ToMarker;

		public UserMarkerZRDI(string basePath, string name, RGB colour, UserMarker fromMarker, UserMarker toMarker)
			: base(basePath, name, colour)
		{
			FromMarker = fromMarker;
			ToMarker = toMarker;
		}

		public override void UpdateInstance(ZoneHighlighterInstance zhi, int fromIdx, int toIdx)
		{
			ZoneHighlighterInstance.XRange activeRange = zhi.ActiveRange;

			for (int i = fromIdx; i <= toIdx; i++)
			{
				FrameRecord fr = zhi.LogView.m_logData.FrameRecords[i];

				foreach (UserMarkerLocation uml in fr.UserMarkers)
				{
					if (activeRange == null)
					{
						if (FromMarker != null && FromMarker.Matches(uml))
							activeRange = zhi.StartRange(fr);
					}
					else
					{
						if (ToMarker != null && ToMarker.Matches(uml))
							activeRange = zhi.EndRange(fr, activeRange);
					}
				}
			}
		}
	}
}

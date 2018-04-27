// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Windows.Forms;

namespace Statoscope
{
	class ZoneHighlighterControl<T> : RDIInfoControl<ZoneHighlighterRDI> where T : ZoneHighlighterRDI
	{
		protected bool ZHIResetEnabled = true;

		protected override ZoneHighlighterRDI RDI
		{
			set
			{
				base.RDI = value;
				ZRDI = value as T;
			}
		}

		protected virtual T ZRDI
		{
			get { return RDI as T; }
			set { }
		}

		protected ZoneHighlighterControl(LogControl logControl, ZoneHighlighterRDI zrdiTree, RDICheckboxTreeView<ZoneHighlighterRDI> zrdiCTV)
			: base(logControl, zrdiTree, zrdiCTV)
		{
		}

		protected void ResetZHIs()
		{
			if (!ZHIResetEnabled)
				return;

			LogControl.ResetZoneHighlighter(ZRDI);
		}
	}
}
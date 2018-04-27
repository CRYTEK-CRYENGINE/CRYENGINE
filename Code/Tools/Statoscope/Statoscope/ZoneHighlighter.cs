// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace Statoscope
{
	partial class LogControl
	{
		public void AddZoneHighlighter(ZoneHighlighterRDI zrdi)
		{
			foreach (LogView logView in m_logViews)
				logView.AddZoneHighlighterInstance(zrdi);

			InvalidateGraphControl();
		}

		void AddZoneHighlighters(LogView logView)
		{
			foreach (ZoneHighlighterRDI zrdi in m_zrdiTree.GetEnumerator(ERDIEnumeration.OnlyLeafNodes))
				logView.AddZoneHighlighterInstance(zrdi);

			InvalidateGraphControl();
		}

		public void RemoveZoneHighlighter(ZoneHighlighterRDI zrdi)
		{
			foreach (LogView logView in m_logViews)
				logView.RemoveZoneHighlighterInstance(zrdi);

			InvalidateGraphControl();
		}

		protected void UpdateZoneHighlighters(LogView logView, int fromIdx, int toIdx)
		{
			foreach (ZoneHighlighterRDI zrdi in m_zrdiTree.GetEnumerator(ERDIEnumeration.OnlyLeafNodes))
				logView.UpdateZoneHighlighter(zrdi, fromIdx, toIdx);

			InvalidateGraphControl();
		}

		public void ResetZoneHighlighter(ZoneHighlighterRDI zrdi)
		{
			foreach (LogView logView in m_logViews)
				logView.ResetZoneHighlighterInstance(zrdi);

			InvalidateGraphControl();
		}

		public void ResetZoneHighlighters()
		{
			foreach (ZoneHighlighterRDI zrdi in m_zrdiTree.GetEnumerator(ERDIEnumeration.OnlyLeafNodes))
				ResetZoneHighlighter(zrdi);
		}
	}
}
// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace Statoscope
{
	partial class LogControl
	{
		UserMarkerRDI GetFirstURDIMatching(UserMarker um, LogView logView)
		{
			if (um == null)
				return null;

			UserMarkerRDI baseURDI = m_urdiTree[um.Path];
			if (baseURDI == null)
				return null;

			foreach (UserMarkerRDI childURDI in baseURDI.Children)
				if ((childURDI.DisplayName == um.Name) && (childURDI.m_logView == logView))
					return childURDI;

			return null;
		}

		void SerializeInLogViewStates(SerializeState serializeState)
		{
			if (serializeState.LogViewStates == null)
				return;

			for (int i=0; i<m_logViews.Count; i++)
			{
				LogView logView = m_logViews[i];
				LogViewSerializeState lvss;

				lock (logView.m_logData)
				{
					if (i < serializeState.LogViewStates.Length)
					{
						lvss = serializeState.LogViewStates[i];

						if ((m_logViews.Count > 1) && (lvss.SingleORDIColour != null))
						{
							logView.m_singleOrdiColour = lvss.SingleORDIColour;
						}
					}
					else
					{
						lvss = serializeState.LogViewStates.Last();
					}

					if (!(logView.m_logData is SocketLogData))	// this is only a temporary quick fix - fixing this properly will take some time
					{
						UserMarkerRDI startURDI = GetFirstURDIMatching(lvss.StartUM, logView);
						UserMarkerRDI endURDI = GetFirstURDIMatching(lvss.EndUM, logView);
						FrameRecordRange frr = logView.GetFrameRecordRange();
						int startIdx = (startURDI != null) ? startURDI.m_uml.m_fr.Index : frr.StartIdx;
						int endIdx = (endURDI != null) ? endURDI.m_uml.m_fr.Index : frr.EndIdx;

						logView.SetFrameRecordRange(Math.Min(startIdx, endIdx), Math.Max(startIdx, endIdx));

						logView.m_startUM = (startURDI != null) ? startURDI.DisplayUserMarker : null;
						logView.m_endUM = (endURDI != null) ? endURDI.DisplayUserMarker : null;
					}

					foreach (Control lliControl in logListTableLayoutPanel.Controls)
					{
						LogListItem lli = (LogListItem)lliControl;
						lli.UpdateContents();
					}

					SetUMTreeViewNodeColours();
				}
			}
		}

		public bool LoadProfile(string filename)
		{
			SerializeState serializeState = null;
			try
			{
				serializeState = SerializeState.LoadFromFile(filename);
			}
			catch (FileNotFoundException)
			{
				MessageBox.Show(string.Format("Unable to find file: \"{0}\"", filename), "Error loading profile", MessageBoxButtons.OK, MessageBoxIcon.Error);
			}
			catch (InvalidOperationException)
			{
				MessageBox.Show(string.Format("An error was encountered while loading profile: \"{0}\"", filename), "Error loading profile", MessageBoxButtons.OK, MessageBoxIcon.Error);
			}

			if (serializeState == null)
				return false;

			m_ordiTreeView.SerializeInRDIs(serializeState.ORDIs);

			CalculateMAandLMs();

			m_prdiTreeView.SerializeInRDIs(serializeState.PRDIs);
			m_urdiTreeView.SerializeInRDIs(serializeState.URDIs);
			m_trdiTreeView.SerializeInRDIs(serializeState.TRDIs);
			m_zrdiTreeView.SerializeInRDIs(serializeState.ZRDIs);

			ResetZoneHighlighters();

			SerializeInLogViewStates(serializeState);

			InvalidateGraphControl();
			RefreshItemInfoPanel();

			return true;
		}

		public void SaveProfile(string filename)
		{
			SerializeState serializeState = new SerializeState();

			serializeState.ORDIs = m_ordiTreeView.SerializeOutRDIs();
			serializeState.PRDIs = m_prdiTreeView.SerializeOutRDIs();
			serializeState.URDIs = m_urdiTreeView.SerializeOutRDIs();
			serializeState.TRDIs = m_trdiTreeView.SerializeOutRDIs();
			serializeState.ZRDIs = m_zrdiTreeView.SerializeOutRDIs();

			serializeState.LogViewStates = m_logViews.Select(lv => lv.SerializeState).ToArray();

			serializeState.SaveToFile(filename);
		}
	}
}

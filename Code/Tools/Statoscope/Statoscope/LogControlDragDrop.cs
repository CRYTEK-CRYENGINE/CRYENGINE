// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Windows.Forms;

namespace Statoscope
{
	partial class LogControl
	{
		public void LogControl_DragEnter(object sender, DragEventArgs e)
		{
			LogControl sourceLogControl = GetDragEventSourceLogControl(e);
			LogRange sourceLogRange = GetDragEventSourceLogRange(e);

			if ((sourceLogControl != null && sourceLogControl != this) || sourceLogRange != null)
				e.Effect = DragDropEffects.Copy;
			else
				m_statoscopeForm.StatoscopeForm_DragEnter(sender, e);
		}

		LogControl GetDragEventSourceLogControl(DragEventArgs e)
		{
			return (LogControl)e.Data.GetData(typeof(LogControl));
		}

		LogRange GetDragEventSourceLogRange(DragEventArgs e)
		{
			return (LogRange)e.Data.GetData(typeof(LogRange));
		}

		public void LogControl_DragDrop(object sender, DragEventArgs e)
		{
			LogControl sourceLogControl = GetDragEventSourceLogControl(e);
			LogRange sourceLogRange = GetDragEventSourceLogRange(e);

			if (sourceLogControl != null || sourceLogRange != null)
			{
				foreach (LogView logView in m_logViews)
				{
					logView.SetSingleOrdiColour(RGB.RandomHueRGB());
				}

				foreach (Control lliControl in logListTableLayoutPanel.Controls)
				{
					LogListItem lli = (LogListItem)lliControl;
					lli.UpdateContents();
				}

				logListTableLayoutPanel.SuspendLayout();
				{
					if (sourceLogControl != null)
					{
						foreach (LogView logView in sourceLogControl.m_logViews)
						{
							FrameRecordRange frr = logView.m_logData == logView.m_baseLogData ? null : logView.m_logData.FrameRecordRange;
							LogRange logRange = new LogRange(logView.m_baseLogData, frr);
							AddNewLogRange(logRange);
						}
					}
					else if (sourceLogRange != null)
					{
						AddNewLogRange(sourceLogRange);
					}
				}
				logListTableLayoutPanel.ResumeLayout();

				m_statoscopeForm.SetSessionInfoList(this);

				RefreshItemInfoPanel();
				UpdateControls(0);
				SetUMTreeViewNodeColours();

				foreach (LogView logView in m_logViews)
				{
					// this is required as in calculating value stats in UpdateControls(), an OnPaint could happen and use invalid data
					logView.DeleteDisplayListsEndingIn(".MA");
					logView.DeleteDisplayListsEndingIn(".LM");
				}
			}
			else
			{
				m_statoscopeForm.StatoscopeForm_DragDrop(sender, e);
			}
		}
	}
}
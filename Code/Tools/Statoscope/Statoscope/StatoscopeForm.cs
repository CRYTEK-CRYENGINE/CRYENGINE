// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace Statoscope
{
	public partial class StatoscopeForm : Form
	{
		public static Random s_rng = new Random(0);
		public static bool m_shiftIsBeingPressed;
		public static bool m_ctrlIsBeingPressed;
		bool m_bCancelTreeViewExpand;

		public StatoscopeForm(string logURI)
		{
			InitializeComponent();
			Text = "Statoscope " + GetVersionStr();

			KeyPreview = true;
			KeyDown += new KeyEventHandler(StatoscopeForm_KeyDown);
			KeyUp += new KeyEventHandler(StatoscopeForm_KeyUp);
			m_shiftIsBeingPressed = false;
			m_ctrlIsBeingPressed = false;

			PopulateProfileMenu();
			OpenLog(logURI);
		}

		public string GetVersionStr()
		{
			// major.minor.minor dd/mm/yy
			return "v3.7.5 03/03/16";
		}

		void StatoscopeForm_KeyDown(object sender, KeyEventArgs e)
		{
			if (e.KeyCode == Keys.ShiftKey)
			{
				m_shiftIsBeingPressed = true;
			}

			if (e.KeyCode == Keys.ControlKey)
			{
				m_ctrlIsBeingPressed = true;
			}

			if (e.Control && e.KeyCode == Keys.C)
			{
				LogControl currentLogControl = GetActiveLogControl();

				if (currentLogControl != null)
				{
					currentLogControl.m_OGLGraphingControl.CopyInfoLabelTextToClipboard();
				}
			}
		}

		void StatoscopeForm_KeyUp(object sender, KeyEventArgs e)
		{
			if (e.KeyCode == Keys.ShiftKey)
			{
				m_shiftIsBeingPressed = false;
			}

			if (e.KeyCode == Keys.ControlKey)
			{
				m_ctrlIsBeingPressed = false;
			}
		}

		void Reset()
		{
			s_rng = new Random(0);
		}

		private void ProcessLogLoadRequest(FileSessionInfo[] sessions)
		{
			using(ProfileSection p = new ProfileSection("Loading log"))

			try
			{
				List<LogRange> logRanges = new List<LogRange>();
				TreeNode sessionLogsBaseNode = null;

				foreach (FileSessionInfo session in sessions)
				{
					Reset();

					FileLogProcessor logProcessor = new FileLogProcessor(session);
					LogData logData = logProcessor.ProcessLog();
					LogRange logRange = new LogRange(logData, null);

					logRanges.Add(logRange);
					sessionLogsBaseNode = AddToLogRangeList(logRange);
				}

				int numTabPages = tabControl1.TabPages.Count;
				string tabName = logRanges.Count == 1 ? logRanges[0].m_logData.Name : string.Format("{0} log compare", logRanges.Count);
				TreeNode sessionTabsBaseNode = CreateLogControlTabPage(logRanges, tabName, null);

				if (logRanges.Count == 1)
				{
					foreach (FrameRecordRange frr in logRanges[0].m_logData.LevelRanges)
					{
						List<LogRange> lrs = new List<LogRange>();
						LogRange logRange = new LogRange(logRanges[0].m_logData, frr);

						lrs.Add(logRange);
						AddToLogRangeList(logRange, sessionLogsBaseNode, sessionTabsBaseNode);
					}
				}

				SetSessionInfoList(logRanges[0]);

				tabControl1.SelectTab(numTabPages);	// select the newly opened page
			}
			catch (Exception e)
			{
				System.Console.WriteLine(e);
			}
		}

		class LogRangeTreeEntry
		{
			public readonly LogRange LogRange;
			public readonly TreeNode ParentTabNode;

			public LogRangeTreeEntry(LogRange logRange, TreeNode parentTabNode)
			{
				LogRange = logRange;
				ParentTabNode = parentTabNode;
			}
		}

		public TreeNode AddToLogRangeList(LogRange logRange)
		{
			return AddToLogRangeList(logRange, null, null);
		}

		public TreeNode AddToLogRangeList(LogRange logRange, TreeNode sessionParentLogNode, TreeNode sessionParentTabNode)
		{
			string name = logRange.m_frr != null ? logRange.m_frr.Name : logRange.m_logData.Name;
			TreeNode treeNode = new TreeNode(name);

			treeNode.Tag = new LogRangeTreeEntry(logRange, sessionParentTabNode);

			TreeNode logsTreeNode = sessionTreeView.Nodes["Logs"];

			if (sessionParentLogNode == null)
			{
				sessionParentLogNode = logsTreeNode;
			}

			sessionParentLogNode.Nodes.Add(treeNode);
			logsTreeNode.Expand();
			SetSessionInfoList(logRange);

			return treeNode;
		}

		public void OpenLog(string logURI)
		{
			if (logURI == "")
			{
				return;
			}

			string tempFile = "";
			string ssProtocolStr = "statoscope://";
			FileSessionInfo session;

			try
			{
				if (logURI.StartsWith(ssProtocolStr))
				{
					Dictionary<string, string> args = null;
					string argsString = logURI.Substring(ssProtocolStr.Length);	// remove statoscope://
					string sourceLocation;
					string localFile = "";

					if (argsString.Contains('='))
					{
						// argsString = "logFile=statoscope.bin&platform=ps3" -> args = { logFile: statoscope.bin, platform: ps3 }
						args = argsString.Split('&').ToDictionary(s => s.Split('=')[0], s => s.Split('=')[1]);
						sourceLocation = args["logFile"];
					}
					else
					{
						// support for the old format
						sourceLocation = argsString;
					}


					DialogResult result = DialogResult.No; // MessageBox.Show("Download request for:\n\n" + sourceLocation + "\n\nWould you like to save a local copy?", "Save downloaded log", MessageBoxButtons.YesNoCancel);

					switch (result)
					{
					case DialogResult.Cancel:
						return;

					case DialogResult.Yes:
						SaveFileDialog fd = new SaveFileDialog();
						fd.CheckFileExists = false;
						fd.FileName = "statoscope.bin";
						fd.Filter = "Binary log files (*.bin)|*.bin|All files (*.*)|*.*";
						fd.FilterIndex = 0;

						if (fd.ShowDialog() == DialogResult.OK)
						{
							localFile = fd.FileName;
						}

						break;
					}

					if (localFile == "")
					{
						tempFile = Path.GetTempFileName();
						localFile = tempFile;
					}

					if (localFile == "")
					{
						return;
					}

					LogDownload.DownloadFile(sourceLocation, localFile);

					if (args != null)
					{
						session = new TelemetrySessionInfo(localFile, sourceLocation, args["peerName"], args["platform"], args["peerType"]);
					}
					else
					{
						session = new FileSessionInfo(localFile);
					}
				}
				else
				{
					session = new FileSessionInfo(logURI);
				}

				ProcessLogLoadRequest(new FileSessionInfo[] { session });
			}
			finally
			{
				if (tempFile != "")
				{
					File.Delete(tempFile);
				}
			}
		}

		public TreeNode CreateLogControlTabPage(List<LogRange> logRanges, string name, TreeNode sessionParentTabNode)
		{
			LogControl logControl = new LogControl(logRanges, this);
			TabPage tabPage = new TabPage(name);
			TreeNode sessionTabsNode = new TreeNode(tabPage.Text);

			sessionTabsNode.Tag = tabPage;

			TreeNode tabsTreeNode = sessionTreeView.Nodes["Tabs"];

			if (sessionParentTabNode == null)
			{
				sessionParentTabNode = tabsTreeNode;
			}
			else if (!tabsTreeNode.Nodes.Contains(sessionParentTabNode))
			{
				tabsTreeNode.Nodes.Add(sessionParentTabNode);
			}

			sessionParentTabNode.Nodes.Add(sessionTabsNode);
			tabsTreeNode.Expand();

			tabPage.Tag = sessionTabsNode;
			tabPage.Controls.Add(logControl);	// see GetLogControlFromTabPage()

			tabControl1.TabPages.Add(tabPage);
			tabControl1.SelectTab(tabPage);	// need to select it so the target labels set their initial positions correctly

			logControl.InvalidateGraphControl();

			return sessionTabsNode;
		}

		LogControl GetLogControlFromTabPage(TabPage tabPage)
		{
			return tabPage != null ? tabPage.Controls[0] as LogControl : null;
		}

		void tabControl1_MouseClick(object sender, MouseEventArgs e)
		{
			if (e.Button == MouseButtons.Left)
			{
				for (int i = 0; i < tabControl1.TabPages.Count; i++)
				{
					System.Drawing.Rectangle tabRect = tabControl1.GetTabRect(i);

					if (tabRect.Contains(e.Location))
					{
						LogControl logControl = GetLogControlFromTabPage(tabControl1.TabPages[i]);
						SetSessionInfoList(logControl);
						break;
					}
				}
			}
			else if (e.Button == MouseButtons.Middle)
			{
				for (int i = 0; i < tabControl1.TabPages.Count; i++)
				{
					System.Drawing.Rectangle tabRect = tabControl1.GetTabRect(i);

					if (tabRect.Contains(e.Location))
					{
						TreeNode sessionTabsNode = (TreeNode)tabControl1.TabPages[i].Tag;

						if (sessionTabsNode.Nodes.Count == 0)
						{
							TreeNode parentNode = sessionTabsNode.Parent;	// this will be the node corresponding to the file
							TabPage parentTabPage = (TabPage)parentNode.Tag;

							if (parentNode.Nodes.Count == 1 && (parentTabPage != null) && !tabControl1.TabPages.Contains(parentTabPage))
							{
								parentNode.Nodes.Remove(sessionTabsNode);
								sessionTabsNode.Nodes.Remove(parentNode);
							}
							else
							{
								sessionTreeView.Nodes.Remove(sessionTabsNode);
							}
						}

						tabControl1.TabPages.RemoveAt(i);
						break;
					}
				}
			}
		}

		private void tabControl1_Selected(object sender, TabControlEventArgs e)
		{
			if (e.TabPage != null)
			{
				LogControl logControl = GetLogControlFromTabPage(e.TabPage);
				SetSessionInfoList(logControl);
			}
			else
			{
				ClearSessionInfoList();
			}
		}

		private void openToolStripMenuItem1_Click(object sender, EventArgs e)
		{
			DialogResult result = openFileDialog.ShowDialog();

			if (result == DialogResult.OK)
			{
				ProcessLogLoadRequest(new FileSessionInfo[] { new FileSessionInfo(openFileDialog.FileName) });
			}
		}

		public static void SetDoubleBuffered(Control control)
		{
			// set instance non-public property with name "DoubleBuffered" to true
			typeof(Control).InvokeMember("DoubleBuffered",
			                             System.Reflection.BindingFlags.SetProperty | System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic,
			                             null, control, new object[] { true });
		}

		public void StatoscopeForm_DragEnter(object sender, DragEventArgs e)
		{
			if (e.Data.GetDataPresent(DataFormats.FileDrop) || e.Data.GetDataPresent(DataFormats.Text))
			{
				e.Effect = DragDropEffects.Copy;
			}
			else
			{
				e.Effect = DragDropEffects.None;
			}
		}

		public void StatoscopeForm_DragDrop(object sender, DragEventArgs e)
		{
			if (e.Data.GetDataPresent(DataFormats.FileDrop))
			{
				string[] droppedFilePaths = e.Data.GetData(DataFormats.FileDrop, false) as string[];
				FileSessionInfo[] sessions = new FileSessionInfo[droppedFilePaths.Length];

				for (int i = 0; i < droppedFilePaths.Length; i++)
				{
					sessions[i] = new FileSessionInfo(droppedFilePaths[i]);
				}

				ProcessLogLoadRequest(sessions);
			}
			else if (e.Data.GetDataPresent(DataFormats.Text))
			{
				OpenLog(e.Data.GetData(DataFormats.Text) as string);
			}
		}

		private void exitToolStripMenuItem1_Click(object sender, EventArgs e)
		{
			Application.Exit();
		}

		private void uploadLogsToolStripMenuItem_Click(object sender, EventArgs e)
		{
			if (tabControl1.TabPages.Count > 0)
			{
				if (tabControl1.TabPages[0].Controls.Count == 1)
				{
					LogControl logControl = GetLogControlFromTabPage(tabControl1.TabPages[0]);

					if (logControl != null)
					{
						using(ConfluenceService confluenceService = new ConfluenceService(logControl.m_logViews[0].m_logData))
						{
							if (confluenceService.TryLogin(false, "", ""))
							{
								confluenceService.PromptForUpload("" /*m_logfilepath*/);
							}
						}
					}
				}
			}
		}

		private void toolStripMenuItem1_Click(object sender, EventArgs e)
		{
			Reset();

			SocketLogProcessor logProcessor = new SocketLogProcessor();
			logProcessor.ProcessLog(this);
		}

		private void sessionTreeView_ItemDrag(object sender, ItemDragEventArgs e)
		{
			TreeNode treeNode = (TreeNode)e.Item;
			TabPage tabPage = treeNode.Tag as TabPage;
			LogRangeTreeEntry logRangeTreeEntry = treeNode.Tag as LogRangeTreeEntry;
			object data = null;

			sessionTreeView.SelectedNode = treeNode;

			if (tabPage != null)
			{
				data = GetLogControlFromTabPage(tabPage);
			}
			else if (logRangeTreeEntry != null)
			{
				data = logRangeTreeEntry.LogRange;
			}

			if (data != null)
			{
				sessionTreeView.DoDragDrop(data, DragDropEffects.Copy);
			}
		}

		private void sessionTreeView_AfterSelect(object sender, TreeViewEventArgs e)
		{
			TabPage tabPage = e.Node.Tag as TabPage;
			LogRangeTreeEntry logRangeTreeEntry = e.Node.Tag as LogRangeTreeEntry;

			if (tabPage != null && tabControl1.TabPages.Contains(tabPage))
			{
				if (tabControl1.SelectedTab == tabPage)
				{
					// select the session info anyway (tabControl1_Selected() won't be called)
					LogControl logControl = GetLogControlFromTabPage(tabPage);
					SetSessionInfoList(logControl);
				}

				tabControl1.SelectedTab = tabPage;
				sessionTreeView.Focus();
			}
			else if (logRangeTreeEntry != null)
			{
				LogRange logRange = logRangeTreeEntry.LogRange;
				SetSessionInfoList(logRange);
			}
		}

		private void sessionTreeView_NodeMouseDoubleClick(object sender, TreeNodeMouseClickEventArgs e)
		{
			TabPage tabPage = e.Node.Tag as TabPage;
			LogRangeTreeEntry logRangeTreeEntry = e.Node.Tag as LogRangeTreeEntry;

			if (tabPage != null && tabControl1.TabPages.Contains(tabPage))
			{
				tabControl1.SelectedTab = tabPage;
			}
			else if (logRangeTreeEntry != null)
			{
				LogRange logRange = logRangeTreeEntry.LogRange;
				List<LogRange> logRanges = new List<LogRange>();
				logRanges.Add(logRange);
				string name = logRange.m_frr != null ? logRange.m_frr.Name : logRange.m_logData.Name;
				TreeNode sessionTabsBaseNode = CreateLogControlTabPage(logRanges, name, logRangeTreeEntry.ParentTabNode);
			}
		}

		private void sessionTreeView_BeforeCollapse(object sender, TreeViewCancelEventArgs e)
		{
			e.Cancel = m_bCancelTreeViewExpand;
			m_bCancelTreeViewExpand = false;
		}

		private void sessionTreeView_BeforeExpand(object sender, TreeViewCancelEventArgs e)
		{
			e.Cancel = m_bCancelTreeViewExpand;
			m_bCancelTreeViewExpand = false;
		}

		private void sessionTreeView_MouseDown(object sender, MouseEventArgs e)
		{
			m_bCancelTreeViewExpand = e.Clicks > 1;
		}

		private void scaleAboutCentreToolStripMenuItem_CheckedChanged(object sender, EventArgs e)
		{
			Properties.Settings.Default.GraphControl_ScaleAboutCentre = scaleAboutCentreToolStripMenuItem.Checked;
		}

		protected override void OnCreateControl()
		{
			base.OnCreateControl();

			scaleAboutCentreToolStripMenuItem.Checked = Properties.Settings.Default.GraphControl_ScaleAboutCentre;

			Application.Idle += new EventHandler(Application_Idle);
		}

		protected override void OnClosed(EventArgs e)
		{
			Application.Idle -= new EventHandler(Application_Idle);

			Properties.Settings.Default.Save();

			base.OnClosed(e);
		}

		private void WakeUp(object sender, EventArgs e)
		{
		}

		private volatile bool m_updateControlsRequested;

		public void RequestUpdateControls()
		{
			if (!m_updateControlsRequested)
			{
				m_updateControlsRequested = true;

				// Issue a dummy message to retrigger the idle event
				BeginInvoke(new EventHandler(WakeUp), this, EventArgs.Empty);
			}
		}

		void Application_Idle(object sender, EventArgs e)
		{
			m_updateControlsRequested = false;

			foreach (TabPage tp in tabControl1.TabPages)
			{
				LogControl lc = GetLogControlFromTabPage(tp);

				if (lc != null)
				{
					lc.UpdateControls();
				}
			}
		}

		void SetSessionInfoList(LogRange logRange)
		{
			SetSessionInfoList(new LogData[] { logRange.m_logData });
		}

		public void SetSessionInfoList(LogControl logControl)
		{
			LogData[] logDatas = new LogData[logControl.m_logViews.Count];

			for (int i = 0; i < logControl.m_logViews.Count; i++)
			{
				logDatas[i] = logControl.m_logViews[i].m_logData;
			}

			SetSessionInfoList(logDatas);
		}

		void SetSessionInfoList(LogData[] logDatas)
		{
			sessionInfoComboBox.Items.Clear();
			sessionInfoComboBox.Items.AddRange(logDatas);
			sessionInfoComboBox.SelectedIndex = 0;
		}

		void ClearSessionInfoList()
		{
			sessionInfoComboBox.Items.Clear();
			sessionInfoComboBox.Text = "";
			sessionInfoPanel.Controls.Clear();
		}

		public void InvalidateSessionInfoPanel()
		{
			sessionInfoPanel.Invalidate(true);
		}

		private void sessionInfoComboBox_SelectedIndexChanged(object sender, EventArgs e)
		{
			sessionInfoPanel.Hide();
			sessionInfoPanel.Controls.Clear();
			sessionInfoPanel.Controls.Add(new SessionInfoListView(((LogData)sessionInfoComboBox.SelectedItem).SessionInfo));
			sessionInfoPanel.Show();
		}

		LogControl GetActiveLogControl()
		{
			return GetLogControlFromTabPage(tabControl1.SelectedTab);
		}

		private void screenshotGraphToolStripMenuItem_Click(object sender, EventArgs e)
		{
			LogControl logControl = GetActiveLogControl();

			if (logControl != null)
			{
				logControl.ScreenshotGraphToClipboard();
			}
		}

		private void resetToolStripMenuItem_Click(object sender, EventArgs e)
		{
			LogControl logControl = GetActiveLogControl();

			if (logControl != null)
			{
				foreach (LogView logView in logControl.m_logViews)
				{
					logView.SetFrameRecordRange(0, logView.m_maxValidBaseIdx);
				}
			}
		}

		private void cropLogRangeFromViewToolStripMenuItem_Click(object sender, EventArgs e)
		{
			LogControl logControl = GetActiveLogControl();

			if (logControl != null)
			{
				int maxStartIdxChange = int.MinValue;

				foreach (LogView logView in logControl.m_logViews)
				{
					int leftIdx  = logControl.m_OGLGraphingControl.LeftIndex(logView);
					int rightIdx = logControl.m_OGLGraphingControl.RightIndex(logView);

					FrameRecordRange frr = logView.GetFrameRecordRange();
					int newStartIdx = Math.Min(Math.Max(0, leftIdx  + frr.StartIdx), logView.m_maxValidBaseIdx);
					int newEndIdx   = Math.Min(Math.Max(0, rightIdx + frr.StartIdx), logView.m_maxValidBaseIdx);
					int startIdxChange = newStartIdx - frr.StartIdx;

					logView.SetFrameRecordRange(newStartIdx, newEndIdx);

					if (startIdxChange > maxStartIdxChange)
					{
						maxStartIdxChange = startIdxChange;
					}
				}

				logControl.m_OGLGraphingControl.AdjustCentreX(-maxStartIdxChange);
			}
		}

		private void fitToFrameRecordsToolStripMenuItem_Click(object sender, EventArgs e)
		{
			LogControl logControl = GetActiveLogControl();

			if (logControl != null)
			{
				logControl.m_OGLGraphingControl.FitViewToFrameRecords();
			}
		}

		private void saveLogToolStripMenuItem_Click(object sender, EventArgs e)
		{
			LogControl logControl = GetActiveLogControl();

			if (logControl == null)
			{
				MessageBox.Show("No Log open", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
				return;
			}

			foreach (LogView logView in logControl.m_logViews)
			{
				SocketLogData socketLogData = logView.m_baseLogData as SocketLogData;

				if (socketLogData != null)
				{
					SaveFileDialog fd = new SaveFileDialog();
					fd.CheckFileExists = false;
					fd.FileName = "log.bin";
					fd.Filter = "Binary log files (*.bin)|*.bin|All files (*.*)|*.*";
					fd.FilterIndex = 0;

					if (fd.ShowDialog() == DialogResult.OK)
					{
						socketLogData.SaveToFile(fd.FileName);
					}
				}
				else
				{
					MessageBox.Show("Selected log is not a socket logging session (probably a file opened from disk)", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
				}
			}
		}
	}

	class ProfileSection : IDisposable
	{
		string m_name;
		System.Diagnostics.Stopwatch m_stopwatch;

		public ProfileSection(string name)
		{
			m_name = name;
			m_stopwatch = new System.Diagnostics.Stopwatch();
			m_stopwatch.Start();
		}

		public void Dispose()
		{
			m_stopwatch.Stop();
			System.Console.WriteLine("'{0}' took: {1}", m_name, m_stopwatch.Elapsed);
		}
	}
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace Statoscope
{
	public partial class LogControl : UserControl
	{
		public readonly List<LogView> m_logViews = new List<LogView>();

		public readonly OpenGLGraphingControl m_OGLGraphingControl;
		readonly IntervalControl m_intervalControl;
		readonly ProfilerTreemapControl m_treemapControl;
		readonly StatoscopeForm m_statoscopeForm;

		public readonly ORDICheckboxTreeView m_ordiTreeView;
		public readonly PRDICheckboxTreeView m_prdiTreeView;
		public readonly URDICheckboxTreeView m_urdiTreeView;
		public readonly TRDICheckboxTreeView m_trdiTreeView;
		public readonly ZRDICheckboxTreeView m_zrdiTreeView;

		public readonly SpikeFinder m_spikeFinder;

		public readonly OverviewRDI m_ordiTree;
		public readonly ProfilerRDI m_prdiTree;
		public readonly UserMarkerRDI m_urdiTree;
		public readonly TargetLineRDI m_trdiTree;
		public readonly ZoneHighlighterRDI m_zrdiTree;

		ItemInfoControl m_activeItemInfoControl;
		Dictionary<Type, ItemInfoControl> m_itemInfoControls = new Dictionary<Type, ItemInfoControl>();

		List<ProfilerRDI> m_lastDisplayedPrdis = new List<ProfilerRDI>();

		//bool m_nonNumberEntered;

		public LogControl(List<LogRange> logRanges, StatoscopeForm statoscopeForm)
		{
			m_statoscopeForm = statoscopeForm;

			InitializeComponent();
			InitializeDisplayScales();

			if (components == null)
			{
				components = new Container();
			}

			intervalPath.Text = "";

			Dock = DockStyle.Fill;

			m_ordiTree = new OverviewRDI();
			m_prdiTree = new ProfilerRDI();
			m_urdiTree = new UserMarkerRDI();
			m_trdiTree = new TargetLineRDI();
			m_zrdiTree = new ZoneHighlighterRDI();

			m_ordiTreeView = new ORDICheckboxTreeView(this, m_ordiTree);
			m_prdiTreeView = new PRDICheckboxTreeView(this, m_prdiTree);
			m_urdiTreeView = new URDICheckboxTreeView(this, m_urdiTree);
			m_trdiTreeView = new TRDICheckboxTreeView(this, m_trdiTree);
			m_zrdiTreeView = new ZRDICheckboxTreeView(this, m_zrdiTree);

			InitializeItemInfoControls();

			foreach (LogRange logRange in logRanges)
			{
				LogView logView = new LogView(this, logRange);

				components.Add(logView);
				m_logViews.Add(logView);
			}

			m_intervalControl = new IntervalControl();
			m_intervalControl.Dock = DockStyle.Fill;
			m_intervalControl.Visible = false;

			m_intervalControl.Tree = m_logViews[0].m_logData.IntervalTree;

			m_intervalControl.ViewChanged += new EventHandler(m_intervalControl_ViewChanged);
			m_intervalControl.ActiveIntervalChanged += new EventHandler(m_intervalControl_ActiveIntervalChanged);

			intervalTreemapTableLayoutPanel.Controls.Add(m_intervalControl, 0, 0);

			m_treemapControl = new ProfilerTreemapControl(this);
			m_treemapControl.Dock = DockStyle.Fill;
			m_treemapControl.Visible = false;

			m_treemapControl.SelectionChanged += new EventHandler(m_treemapControl_SelectionChanged);

			intervalTreemapTableLayoutPanel.Controls.Add(m_treemapControl, 0, 1);

			m_OGLGraphingControl = new OpenGLGraphingControl(this);
			m_OGLGraphingControl.Dock = DockStyle.Fill;
			m_OGLGraphingControl.SelectionChanged += new EventHandler(m_OGLGraphingControl_SelectionChanged);
			m_OGLGraphingControl.SelectedHighlight += new EventHandler(m_OGLGraphingControl_SelectedHighlight);

			displayTableLayoutPanel.Controls.Add(m_OGLGraphingControl, 0, 1);

			logListTableLayoutPanel.SuspendLayout();

			foreach (LogView logView in m_logViews)
			{
				if (m_logViews.Count > 1)
				{
					logView.SetSingleOrdiColour(RGB.RandomHueRGB());
				}

				LogListItem logListItem = new LogListItem(logView);
				logListTableLayoutPanel.Controls.Add(logListItem);
			}

			logListTableLayoutPanel.ResumeLayout();

			m_spikeFinder = new SpikeFinder(this);

			overviewTabPage.Controls.Add(new CTVControl(m_ordiTreeView));
			functionProfileTabPage.Controls.Add(new CTVControl(m_prdiTreeView));
			userMarkersTabPage.Controls.Add(new CTVControl(m_urdiTreeView));
			targetLinesTabPage.Controls.Add(new CTVControl(m_trdiTreeView, new TargetLineCTVControlPanel(m_trdiTreeView)));
			spikeFinderTabPage.Controls.Add(m_spikeFinder);
			zoneHighlightersTabPage.Controls.Add(new CTVControl(m_zrdiTreeView, new ZoneHighlighterCTVControlPanel(m_zrdiTreeView)));

			//m_nonNumberEntered = false;

			InitializeTargetLines();
			InitializeZoneHighlighters();

			UpdateControls(0);

			m_OGLGraphingControl.FitViewToFrameRecords();

			AutoEnableTargetLines();
			//CreateSummaryTabPage();	// this will be replaced with a much better Dr Statoscope feature later
		}

		void InitializeItemInfoControls()
		{
			m_itemInfoControls.Add(typeof(OverviewRDI), new OverviewItemInfoControl(this, m_ordiTree, m_ordiTreeView));
			m_itemInfoControls.Add(typeof(ProfilerRDI), new ProfilerItemInfoControl(this, m_prdiTree, m_prdiTreeView));
			m_itemInfoControls.Add(typeof(TargetLineRDI), new TargetLineItemInfoControl(this, m_trdiTree, m_trdiTreeView));
			m_itemInfoControls.Add(typeof(StdThresholdZRDI), new StdThresholdZoneHighlighterControl(this, m_zrdiTree, m_zrdiTreeView));
		}

		public void UpdateCallstacks(FrameRecord fr)
		{
			callstackTreeView.Nodes.Clear();

			if (fr == null || fr.Callstacks == null)
			{
				return;
			}

			foreach (Callstack callstack in fr.Callstacks)
			{
				bool foundExistingCallstackNode = false;

				foreach (TreeNode existingCallstackNode in callstackTreeView.Nodes)
				{
					int numExistingFunctions = existingCallstackNode.Nodes.Count;
					int numNewFunctions = callstack.m_functions.Count;

					if ((numExistingFunctions == numNewFunctions) && (existingCallstackNode.Text == callstack.m_tag))
					{
						bool different = false;

						for (int i = 0; i < numExistingFunctions; i++)
						{
							if (existingCallstackNode.Nodes[i].Text != callstack.m_functions[i])
							{
								different = true;
								break;
							}
						}

						if (!different)
						{
							existingCallstackNode.Tag = (int)existingCallstackNode.Tag + 1;
							foundExistingCallstackNode = true;
							break;
						}
					}
				}

				if (!foundExistingCallstackNode)
				{
					TreeNode callstackNode = new TreeNode(callstack.m_tag);
					callstackNode.Tag = 1;

					foreach (string functionName in callstack.m_functions)
					{
						callstackNode.Nodes.Add(functionName);
					}

					callstackTreeView.Nodes.Add(callstackNode);
				}
			}

			foreach (TreeNode callstackNode in callstackTreeView.Nodes)
			{
				callstackNode.Text += " (" + callstackNode.Tag + ")";
			}
		}

		void AddNewLogRange(LogRange logRange)
		{
			LogView newLogView = new LogView(this, logRange);
			newLogView.SetSingleOrdiColour(RGB.RandomHueRGB());
			m_logViews.Add(newLogView);

			LogListItem logListItem = new LogListItem(newLogView);
			logListTableLayoutPanel.Controls.Add(logListItem);

			AddZoneHighlighters(newLogView);
		}

		public void SelectItemInfo(object item)
		{
			Type itemType = item.GetType();

			while (!m_itemInfoControls.ContainsKey(itemType))
			{
				itemType = itemType.BaseType;

				if (itemType == null)
				{
					m_activeItemInfoControl = null;
					itemInfoTabPage.Controls.Clear();
					return;
				}
			}

			if (m_activeItemInfoControl != m_itemInfoControls[itemType])
			{
				m_activeItemInfoControl = m_itemInfoControls[itemType];
				itemInfoTabPage.Controls.Clear();
				itemInfoTabPage.Controls.Add(m_activeItemInfoControl);
			}

			m_activeItemInfoControl.SetItem(item);
			lowerTabControl.SelectedTab = itemInfoTabPage;
		}

		public void SelectTreeViewTabPage(string name)
		{
			TabPage tabPage = treeViewTabControl.TabPages[name];

			if (tabPage != null)
			{
				treeViewTabControl.SelectedTab = tabPage;
			}
		}

		public void NewRecordsAvailable()
		{
			m_statoscopeForm.RequestUpdateControls();
		}

		delegate void UpdateFRData(int fromIdx);

		public void UpdateControls()
		{
			UpdateControls(0);
		}

		public void UpdateDisplayScale(string path)
		{
			if (m_ordiTree.ContainsPath(path))
			{
				m_ordiTree[path].DisplayScale = GetDisplayScale(path);
			}

			this.m_OGLGraphingControl.Refresh();
		}

		public void UpdateControls(int inFromIdx)
		{
			if (InvokeRequired)
			{
				BeginInvoke(new UpdateFRData(UpdateControls), new object[] { inFromIdx });
				return;
			}

			foreach (LogView logView in m_logViews)
			{
				LogData baseLogData = logView.m_baseLogData;
				int toIdx;
				double logStart;
				double logEnd;

				lock (baseLogData)
				{
					toIdx = baseLogData.FrameRecords.Count - 1;
					logStart = baseLogData.LogStartInSeconds;
					logEnd = baseLogData.LogEndInSeconds;
				}

				int invalidBegin = logView.m_maxValidBaseIdx + 1;
				int fromIdx = Math.Max(0, Math.Max(inFromIdx, invalidBegin));

				if (fromIdx <= toIdx)
				{
					logView.ExpandBaseVFRs(toIdx);
					LogView.CDIResult cdiRes = logView.CreateDisplayInfo(fromIdx, toIdx);
					logView.CalculateDisplayedValuesAndValueStats(fromIdx, toIdx);

					CalculateMAandLM(logView, fromIdx, toIdx);
					UpdateZoneHighlighters(logView, fromIdx, toIdx);
					CreateDisplayInfo(logView, fromIdx, toIdx, cdiRes);

					AddNewBucketListTabPages(baseLogData);
					OnLogChanged();
					m_statoscopeForm.InvalidateSessionInfoPanel();

					m_intervalControl.TimeRange = new SpanD(logStart, logEnd);
					m_intervalControl.RefreshTree();

					logView.UpdateValidIdxs(toIdx);
				}
			}
		}

		protected void CalculateMAandLMs()
		{
			foreach (LogView logView in m_logViews)
			{
				CalculateMAandLM(logView, 0, -1);
			}
		}

		protected void CalculateMAandLM(LogView logView, int fromIdx, int toIdx)
		{
			foreach (OverviewRDI ordi in m_ordiTree.GetEnumerator(ERDIEnumeration.OnlyLeafNodes))
			{
				logView.CalculateMovingAverage(ordi, fromIdx, toIdx);
				logView.CalculateLocalMax(ordi, fromIdx, toIdx);
			}
		}

		public void AddPRDIs(IEnumerable<ProfilerRDI> newToAdd)
		{
			m_lastDisplayedPrdis.AddRange(newToAdd.Where(prdi => prdi.Displayed));
		}

		public void SetFrameLengths()
		{
			List<ProfilerRDI> displayedPrdis = m_prdiTree.GetEnumerator(ERDIEnumeration.Full).Where(prdi => prdi.Displayed).ToList();

			var onlyInLastDisplayedPrdis = m_lastDisplayedPrdis.Except(displayedPrdis);
			var onlyInDisplayedPrdis = displayedPrdis.Except(m_lastDisplayedPrdis);
			var changedPrdis = onlyInLastDisplayedPrdis.Union(onlyInDisplayedPrdis).ToArray();
			var changedPrdiAncestors = changedPrdis.SelectMany(prdi => prdi.Ancestors()).Distinct();
			var changedPaths = changedPrdiAncestors.Select(prdi => prdi.LeafPath);

			m_lastDisplayedPrdis = displayedPrdis;

			foreach (LogView logView in m_logViews)
			{
				logView.ResetValueStats(changedPaths);
				logView.CalculateDisplayedValuesAndValueStats(0, logView.m_maxValidBaseIdx, changedPaths);
			}
		}

		public void AddNewBucketListTabPages(LogData logData)
		{
			lock (logData.m_bucketSets)
			{
				foreach (KeyValuePair<string, BucketSet> bucketSetPair in logData.m_bucketSets)
				{
					if (bucketSetPair.Value.IsEmpty)
					{
						continue;
					}

					if (!bucketsTabControl.TabPages.ContainsKey(bucketSetPair.Key))
					{
						AddBucketListTabPage(bucketSetPair.Key, bucketSetPair.Value);
					}
					else
					{
						bucketsTabControl.TabPages[bucketSetPair.Key].Invalidate(true);
					}
				}
			}
		}

		void m_treemapControl_SelectionChanged(object sender, EventArgs e)
		{
			TreemapNode node = m_treemapControl.SelectedNode;

			if (node != null)
			{
				RDIElementValue<ProfilerRDI> prdiEv = node.Tag as RDIElementValue<ProfilerRDI>;

				if (prdiEv != null)
				{
					SelectItemInfo(prdiEv.m_rdi);
				}
			}
		}

		void m_OGLGraphingControl_SelectionChanged(object sender, EventArgs e)
		{
			OpenGLGraphingControl ctl = (OpenGLGraphingControl)sender;
			m_intervalControl.View = new RectD(ctl.SelectionStartInSeconds, m_intervalControl.View.Top, ctl.SelectionEndInSeconds, m_intervalControl.View.Bottom);
		}

		void m_OGLGraphingControl_SelectedHighlight(object sender, EventArgs e)
		{
			OpenGLGraphingControl ctl = (OpenGLGraphingControl)sender;
			m_treemapControl.FrameRecord = ctl.HighlightedFrameRecord;
			m_treemapControl.InvalidateTree();
		}

		void m_intervalControl_ViewChanged(object sender, EventArgs e)
		{
			IntervalControl ctl = (IntervalControl)sender;
			m_OGLGraphingControl.SelectionStartInSeconds = (float)ctl.View.Left;
			m_OGLGraphingControl.SelectionEndInSeconds = (float)ctl.View.Right;
		}

		void m_intervalControl_ActiveIntervalChanged(object sender, EventArgs e)
		{
			LogData logData = m_logViews[0].m_logData;

			lock (logData.IntervalTree)
			{
				IntervalControl ctl = (IntervalControl)sender;

				if (ctl.ActiveInterval != null)
				{
					intervalPath.Text = logData.IntervalTree.GetRailName(ctl.ActiveRailId);
					intervalProperties.SelectedObject = ctl.ActiveInterval;
				}
				else
				{
					intervalPath.Text = string.Empty;
					intervalProperties.SelectedObject = null;
				}
			}
		}

		void m_intervalTreeView_AfterCheck(object sender, TreeViewEventArgs e)
		{
			RailGroup rg = e.Node.Tag as RailGroup;

			if (rg != null)
			{
				m_intervalControl.ShowGroup(rg, e.Node.Checked);
			}
			else
			{
				KeyValuePair<RailGroup, int> rgi = (KeyValuePair<RailGroup, int>)e.Node.Tag;
				m_intervalControl.ShowGroupRail(rgi.Key, rgi.Value, e.Node.Checked);
			}
		}

		void m_intervalTreeView_NodeMouseClick(object sender, TreeNodeMouseClickEventArgs e)
		{
			if (e.Button == MouseButtons.Left)
			{
				if (StatoscopeForm.m_ctrlIsBeingPressed)
				{
					intervalTreeView.IsolateSubTree(e.Node);
				}
			}
			else if (e.Button == MouseButtons.Right)
			{
				intervalTreeView.ToggleSubTree(e.Node);
			}
		}

		private void xScaleFramesRadioButton_CheckedChanged(object sender, EventArgs e)
		{
			m_OGLGraphingControl.m_againstFrameTime = xScaleTimeRadioButton.Checked;

			foreach (LogView logView in m_logViews)
			{
				logView.DeleteDisplayLists();
			}

			InvalidateGraphControl();
			m_OGLGraphingControl.FitViewToFrameRecords();
		}

		public void InvalidateGraphControl()
		{
			if (m_OGLGraphingControl != null)
			{
				m_OGLGraphingControl.Invalidate(true);
			}
		}

		public void OnLogChanged()
		{
			if (m_activeItemInfoControl != null)
			{
				m_activeItemInfoControl.OnLogChanged();
			}
		}

		public void InvalidateLogListGroupBox()
		{
			foreach (Control logListView in logListTableLayoutPanel.Controls)
			{
				logListView.Invalidate(true);
			}
		}

		public void PRDIsInvalidated()
		{
			SetFrameLengths();
			OnLogChanged();
			InvalidateGraphControl();
			m_treemapControl.InvalidateTree();
		}

		public void RefreshItemInfoPanel()
		{
			if (m_activeItemInfoControl != null)
			{
				m_activeItemInfoControl.RefreshItem();
			}
		}

		void UpdateTableLayouts()
		{
			intervalTreemapTableLayoutPanel.RowStyles[0].Height = showIntervalsCheckBox.Checked ? 100 : 0;
			intervalTreemapTableLayoutPanel.RowStyles[1].Height = showTreemapCheckBox.Checked ? 100 : 0;

			if (showIntervalsCheckBox.Checked || showTreemapCheckBox.Checked)
			{
				displayTableLayoutPanel.RowStyles[0].Height = 80;
				displayTableLayoutPanel.RowStyles[1].Height = 20;
			}
			else
			{
				displayTableLayoutPanel.RowStyles[0].Height = 0;
				displayTableLayoutPanel.RowStyles[1].Height = 100;
			}
		}

		private void showIntervalsCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			if (showIntervalsCheckBox.Checked)
			{
				m_intervalControl.View = m_intervalControl.PhysExtents;
				m_intervalControl.Visible = true;
			}
			else
			{
				m_intervalControl.Visible = false;
			}

			UpdateTableLayouts();
			InvalidateGraphControl();
		}

		private void showTreemapCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			if (showTreemapCheckBox.Checked)
			{
				//m_treemapControl.View = m_treemapControl.PhysExtents;
				m_treemapControl.Visible = true;
			}
			else
			{
				m_treemapControl.Visible = false;
			}

			UpdateTableLayouts();
			InvalidateGraphControl();
		}

		public void ScreenshotGraphToClipboard()
		{
			Control control = displayTableLayoutPanel;
			Bitmap bitmap = new Bitmap(control.Width, control.Height);

			using(Graphics g = Graphics.FromImage(bitmap))
			{
				g.CopyFromScreen(control.PointToScreen(Point.Empty), Point.Empty, this.Size);
			}

			Clipboard.SetImage(bitmap);
		}

		public void SetTracking(LogView logView, bool bAutoTracking)
		{
			m_OGLGraphingControl.SetTracking(logView, bAutoTracking);
		}
	}
}

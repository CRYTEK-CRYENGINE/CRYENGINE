// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace Statoscope
{
	partial class LogControl
	{
		/// <summary> 
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary> 
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Component Designer generated code

		/// <summary> 
		/// Required method for Designer support - do not modify 
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.displayTableLayoutPanel = new System.Windows.Forms.TableLayoutPanel();
            this.intervalTreemapTableLayoutPanel = new System.Windows.Forms.TableLayoutPanel();
            this.rhsTableLayoutPanel = new System.Windows.Forms.TableLayoutPanel();
            this.lowerTabControl = new System.Windows.Forms.TabControl();
            this.itemInfoTabPage = new System.Windows.Forms.TabPage();
            this.intervalInfoTabPage = new System.Windows.Forms.TabPage();
            this.intervalProperties = new System.Windows.Forms.PropertyGrid();
            this.intervalGroupBox = new System.Windows.Forms.GroupBox();
            this.intervalPath = new System.Windows.Forms.Label();
            this.screenshotTabPage = new System.Windows.Forms.TabPage();
            this.screenshotPictureBox = new System.Windows.Forms.PictureBox();
            this.bucketsTabPage = new System.Windows.Forms.TabPage();
            this.bucketsTabControl = new System.Windows.Forms.TabControl();
            this.topFlowLayoutPanel = new System.Windows.Forms.FlowLayoutPanel();
            this.xScaleGroupBox = new System.Windows.Forms.GroupBox();
            this.xScaleTimeRadioButton = new System.Windows.Forms.RadioButton();
            this.xScaleFramesRadioButton = new System.Windows.Forms.RadioButton();
            this.viewGroupBox = new System.Windows.Forms.GroupBox();
            this.showTreemapCheckBox = new System.Windows.Forms.CheckBox();
            this.showIntervalsCheckBox = new System.Windows.Forms.CheckBox();
            this.treeViewTabControl = new System.Windows.Forms.TabControl();
            this.overviewTabPage = new System.Windows.Forms.TabPage();
            this.functionProfileTabPage = new System.Windows.Forms.TabPage();
            this.intervalTabPage = new System.Windows.Forms.TabPage();
            this.userMarkersTabPage = new System.Windows.Forms.TabPage();
            this.targetLinesTabPage = new System.Windows.Forms.TabPage();
            this.spikeFinderTabPage = new System.Windows.Forms.TabPage();
            this.callStackTabPage = new System.Windows.Forms.TabPage();
            this.zoneHighlightersTabPage = new System.Windows.Forms.TabPage();
            this.logListGroupBox = new System.Windows.Forms.GroupBox();
            this.logListTableLayoutPanel = new System.Windows.Forms.TableLayoutPanel();
            this.intervalTreeView = new Statoscope.CheckboxTreeView();
            this.callstackTreeView = new Statoscope.CheckboxTreeView();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            this.displayTableLayoutPanel.SuspendLayout();
            this.rhsTableLayoutPanel.SuspendLayout();
            this.lowerTabControl.SuspendLayout();
            this.intervalInfoTabPage.SuspendLayout();
            this.intervalGroupBox.SuspendLayout();
            this.screenshotTabPage.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.screenshotPictureBox)).BeginInit();
            this.bucketsTabPage.SuspendLayout();
            this.topFlowLayoutPanel.SuspendLayout();
            this.xScaleGroupBox.SuspendLayout();
            this.viewGroupBox.SuspendLayout();
            this.treeViewTabControl.SuspendLayout();
            this.intervalTabPage.SuspendLayout();
            this.callStackTabPage.SuspendLayout();
            this.logListGroupBox.SuspendLayout();
            this.SuspendLayout();
            // 
            // splitContainer1
            // 
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.Location = new System.Drawing.Point(0, 0);
            this.splitContainer1.Name = "splitContainer1";
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this.displayTableLayoutPanel);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.rhsTableLayoutPanel);
            this.splitContainer1.Size = new System.Drawing.Size(1221, 859);
            this.splitContainer1.SplitterDistance = 900;
            this.splitContainer1.TabIndex = 5;
            // 
            // displayTableLayoutPanel
            // 
            this.displayTableLayoutPanel.ColumnCount = 1;
            this.displayTableLayoutPanel.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.displayTableLayoutPanel.Controls.Add(this.intervalTreemapTableLayoutPanel, 0, 0);
            this.displayTableLayoutPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.displayTableLayoutPanel.Location = new System.Drawing.Point(0, 0);
            this.displayTableLayoutPanel.Margin = new System.Windows.Forms.Padding(0);
            this.displayTableLayoutPanel.Name = "displayTableLayoutPanel";
            this.displayTableLayoutPanel.RowCount = 2;
            this.displayTableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 0F));
            this.displayTableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.displayTableLayoutPanel.Size = new System.Drawing.Size(900, 859);
            this.displayTableLayoutPanel.TabIndex = 17;
            // 
            // intervalTreemapTableLayoutPanel
            // 
            this.intervalTreemapTableLayoutPanel.ColumnCount = 1;
            this.intervalTreemapTableLayoutPanel.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.intervalTreemapTableLayoutPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.intervalTreemapTableLayoutPanel.Location = new System.Drawing.Point(3, 3);
            this.intervalTreemapTableLayoutPanel.Name = "intervalTreemapTableLayoutPanel";
            this.intervalTreemapTableLayoutPanel.RowCount = 2;
            this.intervalTreemapTableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.intervalTreemapTableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.intervalTreemapTableLayoutPanel.Size = new System.Drawing.Size(894, 1);
            this.intervalTreemapTableLayoutPanel.TabIndex = 2;
            // 
            // rhsTableLayoutPanel
            // 
            this.rhsTableLayoutPanel.ColumnCount = 1;
            this.rhsTableLayoutPanel.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.rhsTableLayoutPanel.Controls.Add(this.lowerTabControl, 0, 3);
            this.rhsTableLayoutPanel.Controls.Add(this.topFlowLayoutPanel, 0, 0);
            this.rhsTableLayoutPanel.Controls.Add(this.treeViewTabControl, 0, 2);
            this.rhsTableLayoutPanel.Controls.Add(this.logListGroupBox, 0, 1);
            this.rhsTableLayoutPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.rhsTableLayoutPanel.Location = new System.Drawing.Point(0, 0);
            this.rhsTableLayoutPanel.Name = "rhsTableLayoutPanel";
            this.rhsTableLayoutPanel.RowCount = 4;
            this.rhsTableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 62F));
            this.rhsTableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.rhsTableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.rhsTableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 320F));
            this.rhsTableLayoutPanel.Size = new System.Drawing.Size(317, 859);
            this.rhsTableLayoutPanel.TabIndex = 15;
            // 
            // lowerTabControl
            // 
            this.lowerTabControl.Controls.Add(this.itemInfoTabPage);
            this.lowerTabControl.Controls.Add(this.intervalInfoTabPage);
            this.lowerTabControl.Controls.Add(this.screenshotTabPage);
            this.lowerTabControl.Controls.Add(this.bucketsTabPage);
            this.lowerTabControl.Dock = System.Windows.Forms.DockStyle.Fill;
            this.lowerTabControl.Location = new System.Drawing.Point(3, 512);
            this.lowerTabControl.Name = "lowerTabControl";
            this.lowerTabControl.SelectedIndex = 0;
            this.lowerTabControl.Size = new System.Drawing.Size(311, 344);
            this.lowerTabControl.TabIndex = 0;
            // 
            // itemInfoTabPage
            // 
            this.itemInfoTabPage.Location = new System.Drawing.Point(4, 22);
            this.itemInfoTabPage.Name = "itemInfoTabPage";
            this.itemInfoTabPage.Size = new System.Drawing.Size(303, 318);
            this.itemInfoTabPage.TabIndex = 2;
            this.itemInfoTabPage.Text = "Item Info";
            this.itemInfoTabPage.UseVisualStyleBackColor = true;
            // 
            // intervalInfoTabPage
            // 
            this.intervalInfoTabPage.Controls.Add(this.intervalProperties);
            this.intervalInfoTabPage.Controls.Add(this.intervalGroupBox);
            this.intervalInfoTabPage.Location = new System.Drawing.Point(4, 22);
            this.intervalInfoTabPage.Name = "intervalInfoTabPage";
            this.intervalInfoTabPage.Padding = new System.Windows.Forms.Padding(3);
            this.intervalInfoTabPage.Size = new System.Drawing.Size(303, 318);
            this.intervalInfoTabPage.TabIndex = 4;
            this.intervalInfoTabPage.Text = "Interval Info";
            this.intervalInfoTabPage.UseVisualStyleBackColor = true;
            // 
            // intervalProperties
            // 
            this.intervalProperties.Location = new System.Drawing.Point(6, 51);
            this.intervalProperties.Name = "intervalProperties";
            this.intervalProperties.Size = new System.Drawing.Size(322, 249);
            this.intervalProperties.TabIndex = 2;
            this.intervalProperties.ViewForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(1)))), ((int)(((byte)(1)))), ((int)(((byte)(1)))));
            // 
            // intervalGroupBox
            // 
            this.intervalGroupBox.Controls.Add(this.intervalPath);
            this.intervalGroupBox.Location = new System.Drawing.Point(6, 6);
            this.intervalGroupBox.Name = "intervalGroupBox";
            this.intervalGroupBox.Size = new System.Drawing.Size(322, 39);
            this.intervalGroupBox.TabIndex = 1;
            this.intervalGroupBox.TabStop = false;
            this.intervalGroupBox.Text = "Path";
            // 
            // intervalPath
            // 
            this.intervalPath.AutoSize = true;
            this.intervalPath.Location = new System.Drawing.Point(9, 16);
            this.intervalPath.Name = "intervalPath";
            this.intervalPath.Size = new System.Drawing.Size(57, 13);
            this.intervalPath.TabIndex = 1;
            this.intervalPath.Text = "/blah/blah";
            // 
            // screenshotTabPage
            // 
            this.screenshotTabPage.Controls.Add(this.screenshotPictureBox);
            this.screenshotTabPage.Location = new System.Drawing.Point(4, 22);
            this.screenshotTabPage.Name = "screenshotTabPage";
            this.screenshotTabPage.Padding = new System.Windows.Forms.Padding(3);
            this.screenshotTabPage.Size = new System.Drawing.Size(303, 318);
            this.screenshotTabPage.TabIndex = 0;
            this.screenshotTabPage.Text = "Screenshot";
            this.screenshotTabPage.UseVisualStyleBackColor = true;
            // 
            // screenshotPictureBox
            // 
            this.screenshotPictureBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.screenshotPictureBox.Location = new System.Drawing.Point(3, 3);
            this.screenshotPictureBox.Name = "screenshotPictureBox";
            this.screenshotPictureBox.Size = new System.Drawing.Size(297, 312);
            this.screenshotPictureBox.SizeMode = System.Windows.Forms.PictureBoxSizeMode.Zoom;
            this.screenshotPictureBox.TabIndex = 0;
            this.screenshotPictureBox.TabStop = false;
            // 
            // bucketsTabPage
            // 
            this.bucketsTabPage.Controls.Add(this.bucketsTabControl);
            this.bucketsTabPage.Location = new System.Drawing.Point(4, 22);
            this.bucketsTabPage.Name = "bucketsTabPage";
            this.bucketsTabPage.Padding = new System.Windows.Forms.Padding(3);
            this.bucketsTabPage.Size = new System.Drawing.Size(303, 318);
            this.bucketsTabPage.TabIndex = 1;
            this.bucketsTabPage.Text = "Buckets";
            this.bucketsTabPage.UseVisualStyleBackColor = true;
            // 
            // bucketsTabControl
            // 
            this.bucketsTabControl.Dock = System.Windows.Forms.DockStyle.Fill;
            this.bucketsTabControl.Location = new System.Drawing.Point(3, 3);
            this.bucketsTabControl.Name = "bucketsTabControl";
            this.bucketsTabControl.SelectedIndex = 0;
            this.bucketsTabControl.Size = new System.Drawing.Size(297, 312);
            this.bucketsTabControl.TabIndex = 1;
            // 
            // topFlowLayoutPanel
            // 
            this.topFlowLayoutPanel.Controls.Add(this.xScaleGroupBox);
            this.topFlowLayoutPanel.Controls.Add(this.viewGroupBox);
            this.topFlowLayoutPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.topFlowLayoutPanel.Location = new System.Drawing.Point(3, 3);
            this.topFlowLayoutPanel.Name = "topFlowLayoutPanel";
            this.topFlowLayoutPanel.Size = new System.Drawing.Size(311, 56);
            this.topFlowLayoutPanel.TabIndex = 13;
            // 
            // xScaleGroupBox
            // 
            this.xScaleGroupBox.Controls.Add(this.xScaleTimeRadioButton);
            this.xScaleGroupBox.Controls.Add(this.xScaleFramesRadioButton);
            this.xScaleGroupBox.Location = new System.Drawing.Point(3, 3);
            this.xScaleGroupBox.Name = "xScaleGroupBox";
            this.xScaleGroupBox.Size = new System.Drawing.Size(121, 52);
            this.xScaleGroupBox.TabIndex = 12;
            this.xScaleGroupBox.TabStop = false;
            this.xScaleGroupBox.Text = "X Scale";
            // 
            // xScaleTimeRadioButton
            // 
            this.xScaleTimeRadioButton.AutoSize = true;
            this.xScaleTimeRadioButton.Location = new System.Drawing.Point(68, 22);
            this.xScaleTimeRadioButton.Name = "xScaleTimeRadioButton";
            this.xScaleTimeRadioButton.Size = new System.Drawing.Size(48, 17);
            this.xScaleTimeRadioButton.TabIndex = 11;
            this.xScaleTimeRadioButton.TabStop = true;
            this.xScaleTimeRadioButton.Text = "Time";
            this.xScaleTimeRadioButton.UseVisualStyleBackColor = true;
            // 
            // xScaleFramesRadioButton
            // 
            this.xScaleFramesRadioButton.AutoSize = true;
            this.xScaleFramesRadioButton.Checked = true;
            this.xScaleFramesRadioButton.Location = new System.Drawing.Point(6, 22);
            this.xScaleFramesRadioButton.Name = "xScaleFramesRadioButton";
            this.xScaleFramesRadioButton.Size = new System.Drawing.Size(59, 17);
            this.xScaleFramesRadioButton.TabIndex = 10;
            this.xScaleFramesRadioButton.TabStop = true;
            this.xScaleFramesRadioButton.Text = "Frames";
            this.xScaleFramesRadioButton.UseVisualStyleBackColor = true;
            this.xScaleFramesRadioButton.CheckedChanged += new System.EventHandler(this.xScaleFramesRadioButton_CheckedChanged);
            // 
            // viewGroupBox
            // 
            this.viewGroupBox.Controls.Add(this.showTreemapCheckBox);
            this.viewGroupBox.Controls.Add(this.showIntervalsCheckBox);
            this.viewGroupBox.Location = new System.Drawing.Point(130, 3);
            this.viewGroupBox.Name = "viewGroupBox";
            this.viewGroupBox.Size = new System.Drawing.Size(91, 52);
            this.viewGroupBox.TabIndex = 14;
            this.viewGroupBox.TabStop = false;
            this.viewGroupBox.Text = "View";
            // 
            // showTreemapCheckBox
            // 
            this.showTreemapCheckBox.AutoSize = true;
            this.showTreemapCheckBox.Location = new System.Drawing.Point(6, 30);
            this.showTreemapCheckBox.Name = "showTreemapCheckBox";
            this.showTreemapCheckBox.Size = new System.Drawing.Size(68, 17);
            this.showTreemapCheckBox.TabIndex = 1;
            this.showTreemapCheckBox.Text = "Treemap";
            this.showTreemapCheckBox.UseVisualStyleBackColor = true;
            this.showTreemapCheckBox.CheckedChanged += new System.EventHandler(this.showTreemapCheckBox_CheckedChanged);
            // 
            // showIntervalsCheckBox
            // 
            this.showIntervalsCheckBox.AutoSize = true;
            this.showIntervalsCheckBox.Location = new System.Drawing.Point(6, 14);
            this.showIntervalsCheckBox.Name = "showIntervalsCheckBox";
            this.showIntervalsCheckBox.Size = new System.Drawing.Size(66, 17);
            this.showIntervalsCheckBox.TabIndex = 0;
            this.showIntervalsCheckBox.Text = "Intervals";
            this.showIntervalsCheckBox.UseVisualStyleBackColor = true;
            this.showIntervalsCheckBox.CheckedChanged += new System.EventHandler(this.showIntervalsCheckBox_CheckedChanged);
            // 
            // treeViewTabControl
            // 
            this.treeViewTabControl.Appearance = System.Windows.Forms.TabAppearance.Buttons;
            this.treeViewTabControl.Controls.Add(this.overviewTabPage);
            this.treeViewTabControl.Controls.Add(this.functionProfileTabPage);
            this.treeViewTabControl.Controls.Add(this.intervalTabPage);
            this.treeViewTabControl.Controls.Add(this.userMarkersTabPage);
            this.treeViewTabControl.Controls.Add(this.targetLinesTabPage);
            this.treeViewTabControl.Controls.Add(this.spikeFinderTabPage);
            this.treeViewTabControl.Controls.Add(this.callStackTabPage);
            this.treeViewTabControl.Controls.Add(this.zoneHighlightersTabPage);
            this.treeViewTabControl.Dock = System.Windows.Forms.DockStyle.Fill;
            this.treeViewTabControl.Location = new System.Drawing.Point(3, 90);
            this.treeViewTabControl.Multiline = true;
            this.treeViewTabControl.Name = "treeViewTabControl";
            this.treeViewTabControl.SelectedIndex = 0;
            this.treeViewTabControl.Size = new System.Drawing.Size(311, 416);
            this.treeViewTabControl.SizeMode = System.Windows.Forms.TabSizeMode.FillToRight;
            this.treeViewTabControl.TabIndex = 0;
            // 
            // overviewTabPage
            // 
            this.overviewTabPage.Location = new System.Drawing.Point(4, 49);
            this.overviewTabPage.Name = "overviewTabPage";
            this.overviewTabPage.Size = new System.Drawing.Size(303, 363);
            this.overviewTabPage.TabIndex = 0;
            this.overviewTabPage.Text = "Overview";
            this.overviewTabPage.UseVisualStyleBackColor = true;
            // 
            // functionProfileTabPage
            // 
            this.functionProfileTabPage.Location = new System.Drawing.Point(4, 49);
            this.functionProfileTabPage.Name = "functionProfileTabPage";
            this.functionProfileTabPage.Size = new System.Drawing.Size(303, 363);
            this.functionProfileTabPage.TabIndex = 1;
            this.functionProfileTabPage.Text = "Function Profile";
            this.functionProfileTabPage.UseVisualStyleBackColor = true;
            // 
            // intervalTabPage
            // 
            this.intervalTabPage.Controls.Add(this.intervalTreeView);
            this.intervalTabPage.Location = new System.Drawing.Point(4, 49);
            this.intervalTabPage.Name = "intervalTabPage";
            this.intervalTabPage.Size = new System.Drawing.Size(303, 363);
            this.intervalTabPage.TabIndex = 2;
            this.intervalTabPage.Text = "Interval";
            this.intervalTabPage.UseVisualStyleBackColor = true;
            // 
            // userMarkersTabPage
            // 
            this.userMarkersTabPage.Location = new System.Drawing.Point(4, 49);
            this.userMarkersTabPage.Name = "userMarkersTabPage";
            this.userMarkersTabPage.Size = new System.Drawing.Size(303, 363);
            this.userMarkersTabPage.TabIndex = 3;
            this.userMarkersTabPage.Text = "User Markers";
            this.userMarkersTabPage.UseVisualStyleBackColor = true;
            // 
            // targetLinesTabPage
            // 
            this.targetLinesTabPage.Location = new System.Drawing.Point(4, 49);
            this.targetLinesTabPage.Name = "targetLinesTabPage";
            this.targetLinesTabPage.Size = new System.Drawing.Size(303, 363);
            this.targetLinesTabPage.TabIndex = 7;
            this.targetLinesTabPage.Text = "Target Lines";
            this.targetLinesTabPage.UseVisualStyleBackColor = true;
            // 
            // spikeFinderTabPage
            // 
            this.spikeFinderTabPage.Location = new System.Drawing.Point(4, 49);
            this.spikeFinderTabPage.Name = "spikeFinderTabPage";
            this.spikeFinderTabPage.Size = new System.Drawing.Size(303, 363);
            this.spikeFinderTabPage.TabIndex = 4;
            this.spikeFinderTabPage.Text = "Spike Finder";
            this.spikeFinderTabPage.UseVisualStyleBackColor = true;
            // 
            // callStackTabPage
            // 
            this.callStackTabPage.Controls.Add(this.callstackTreeView);
            this.callStackTabPage.Location = new System.Drawing.Point(4, 49);
            this.callStackTabPage.Name = "callStackTabPage";
            this.callStackTabPage.Size = new System.Drawing.Size(303, 363);
            this.callStackTabPage.TabIndex = 5;
            this.callStackTabPage.Text = "Callstack";
            this.callStackTabPage.UseVisualStyleBackColor = true;
            // 
            // zoneHighlightersTabPage
            // 
            this.zoneHighlightersTabPage.Location = new System.Drawing.Point(4, 49);
            this.zoneHighlightersTabPage.Name = "zoneHighlightersTabPage";
            this.zoneHighlightersTabPage.Size = new System.Drawing.Size(303, 363);
            this.zoneHighlightersTabPage.TabIndex = 6;
            this.zoneHighlightersTabPage.Text = "Zone Highlighters";
            this.zoneHighlightersTabPage.UseVisualStyleBackColor = true;
            // 
            // logListGroupBox
            // 
            this.logListGroupBox.AutoSize = true;
            this.logListGroupBox.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.logListGroupBox.Controls.Add(this.logListTableLayoutPanel);
            this.logListGroupBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.logListGroupBox.Location = new System.Drawing.Point(3, 65);
            this.logListGroupBox.Name = "logListGroupBox";
            this.logListGroupBox.Size = new System.Drawing.Size(311, 19);
            this.logListGroupBox.TabIndex = 0;
            this.logListGroupBox.TabStop = false;
            this.logListGroupBox.Text = "Logs";
            // 
            // logListTableLayoutPanel
            // 
            this.logListTableLayoutPanel.AutoSize = true;
            this.logListTableLayoutPanel.ColumnCount = 1;
            this.logListTableLayoutPanel.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.logListTableLayoutPanel.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.logListTableLayoutPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.logListTableLayoutPanel.Location = new System.Drawing.Point(3, 16);
            this.logListTableLayoutPanel.Name = "logListTableLayoutPanel";
            this.logListTableLayoutPanel.RowCount = 1;
            this.logListTableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.logListTableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.logListTableLayoutPanel.Size = new System.Drawing.Size(305, 0);
            this.logListTableLayoutPanel.TabIndex = 0;
            // 
            // intervalTreeView
            // 
            this.intervalTreeView.CheckBoxes = true;
            this.intervalTreeView.Dock = System.Windows.Forms.DockStyle.Fill;
            this.intervalTreeView.Location = new System.Drawing.Point(0, 0);
            this.intervalTreeView.Name = "intervalTreeView";
            this.intervalTreeView.PathSeparator = "/";
            this.intervalTreeView.Size = new System.Drawing.Size(303, 387);
            this.intervalTreeView.TabIndex = 0;
            this.intervalTreeView.AfterCheck += new System.Windows.Forms.TreeViewEventHandler(this.m_intervalTreeView_AfterCheck);
            this.intervalTreeView.NodeMouseClick += new System.Windows.Forms.TreeNodeMouseClickEventHandler(this.m_intervalTreeView_NodeMouseClick);
            // 
            // callstackTreeView
            // 
            this.callstackTreeView.CheckBoxes = true;
            this.callstackTreeView.Dock = System.Windows.Forms.DockStyle.Fill;
            this.callstackTreeView.Location = new System.Drawing.Point(0, 0);
            this.callstackTreeView.Name = "callstackTreeView";
            this.callstackTreeView.PathSeparator = "/";
            this.callstackTreeView.Size = new System.Drawing.Size(303, 363);
            this.callstackTreeView.TabIndex = 0;
            // 
            // LogControl
            // 
            this.AllowDrop = true;
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.splitContainer1);
            this.Name = "LogControl";
            this.Size = new System.Drawing.Size(1221, 859);
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            this.splitContainer1.ResumeLayout(false);
            this.displayTableLayoutPanel.ResumeLayout(false);
            this.rhsTableLayoutPanel.ResumeLayout(false);
            this.rhsTableLayoutPanel.PerformLayout();
            this.lowerTabControl.ResumeLayout(false);
            this.intervalInfoTabPage.ResumeLayout(false);
            this.intervalGroupBox.ResumeLayout(false);
            this.intervalGroupBox.PerformLayout();
            this.screenshotTabPage.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.screenshotPictureBox)).EndInit();
            this.bucketsTabPage.ResumeLayout(false);
            this.topFlowLayoutPanel.ResumeLayout(false);
            this.xScaleGroupBox.ResumeLayout(false);
            this.xScaleGroupBox.PerformLayout();
            this.viewGroupBox.ResumeLayout(false);
            this.viewGroupBox.PerformLayout();
            this.treeViewTabControl.ResumeLayout(false);
            this.intervalTabPage.ResumeLayout(false);
            this.callStackTabPage.ResumeLayout(false);
            this.logListGroupBox.ResumeLayout(false);
            this.logListGroupBox.PerformLayout();
            this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.SplitContainer splitContainer1;
		private System.Windows.Forms.TableLayoutPanel displayTableLayoutPanel;
		private System.Windows.Forms.TableLayoutPanel intervalTreemapTableLayoutPanel;
		private System.Windows.Forms.TableLayoutPanel rhsTableLayoutPanel;
		public System.Windows.Forms.TabControl lowerTabControl;
		public System.Windows.Forms.TabPage itemInfoTabPage;
		private System.Windows.Forms.TabPage intervalInfoTabPage;
		private System.Windows.Forms.PropertyGrid intervalProperties;
		private System.Windows.Forms.GroupBox intervalGroupBox;
		private System.Windows.Forms.Label intervalPath;
		private System.Windows.Forms.TabPage screenshotTabPage;
		public System.Windows.Forms.PictureBox screenshotPictureBox;
		private System.Windows.Forms.TabPage bucketsTabPage;
		public System.Windows.Forms.TabControl bucketsTabControl;
		private System.Windows.Forms.FlowLayoutPanel topFlowLayoutPanel;
		private System.Windows.Forms.GroupBox xScaleGroupBox;
		private System.Windows.Forms.RadioButton xScaleTimeRadioButton;
		private System.Windows.Forms.RadioButton xScaleFramesRadioButton;
		private System.Windows.Forms.GroupBox viewGroupBox;
		private System.Windows.Forms.CheckBox showTreemapCheckBox;
		private System.Windows.Forms.CheckBox showIntervalsCheckBox;
		private System.Windows.Forms.TabControl treeViewTabControl;
		private System.Windows.Forms.TabPage overviewTabPage;
		private System.Windows.Forms.TabPage functionProfileTabPage;
		private System.Windows.Forms.TabPage intervalTabPage;
		private CheckboxTreeView intervalTreeView;
		private System.Windows.Forms.TabPage userMarkersTabPage;
		private System.Windows.Forms.TabPage targetLinesTabPage;
		private System.Windows.Forms.TabPage spikeFinderTabPage;
		private System.Windows.Forms.TabPage callStackTabPage;
		public CheckboxTreeView callstackTreeView;
		private System.Windows.Forms.TabPage zoneHighlightersTabPage;
		private System.Windows.Forms.GroupBox logListGroupBox;
		private System.Windows.Forms.TableLayoutPanel logListTableLayoutPanel;


	}
}

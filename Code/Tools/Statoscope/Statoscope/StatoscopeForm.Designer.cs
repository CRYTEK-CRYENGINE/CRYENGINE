// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace Statoscope
{
	partial class StatoscopeForm
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

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(StatoscopeForm));
            System.Windows.Forms.TreeNode treeNode1 = new System.Windows.Forms.TreeNode("Tabs");
            System.Windows.Forms.TreeNode treeNode2 = new System.Windows.Forms.TreeNode("Logs");
            this.menuStrip1 = new System.Windows.Forms.MenuStrip();
            this.fileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.openToolStripMenuItem1 = new System.Windows.Forms.ToolStripMenuItem();
            this.saveLogToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripMenuItem1 = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
            this.exitToolStripMenuItem1 = new System.Windows.Forms.ToolStripMenuItem();
            this.editToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.resetToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.cropLogRangeFromViewToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.viewToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.graphToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.scaleAboutCentreToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.fitToFrameRecordsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.screenshotGraphToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.profilesToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.openToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.saveToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
            this.openFileDialog = new System.Windows.Forms.OpenFileDialog();
            this.tabControl1 = new System.Windows.Forms.TabControl();
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.sessionSplitContainer = new System.Windows.Forms.SplitContainer();
            this.sessionTreeView = new System.Windows.Forms.TreeView();
            this.splitContainer2 = new System.Windows.Forms.SplitContainer();
            this.sessionInfoComboBox = new System.Windows.Forms.ComboBox();
            this.sessionInfoPanel = new System.Windows.Forms.Panel();
            this.menuStrip1.SuspendLayout();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            this.sessionSplitContainer.Panel1.SuspendLayout();
            this.sessionSplitContainer.Panel2.SuspendLayout();
            this.sessionSplitContainer.SuspendLayout();
            this.splitContainer2.Panel1.SuspendLayout();
            this.splitContainer2.Panel2.SuspendLayout();
            this.splitContainer2.SuspendLayout();
            this.SuspendLayout();
            // 
            // menuStrip1
            // 
            this.menuStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.fileToolStripMenuItem,
            this.editToolStripMenuItem,
            this.viewToolStripMenuItem,
            this.profilesToolStripMenuItem});
            this.menuStrip1.Location = new System.Drawing.Point(0, 0);
            this.menuStrip1.Name = "menuStrip1";
            this.menuStrip1.Size = new System.Drawing.Size(1607, 24);
            this.menuStrip1.TabIndex = 0;
            this.menuStrip1.Text = "menuStrip1";
            // 
            // fileToolStripMenuItem
            // 
            this.fileToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.openToolStripMenuItem1,
            this.saveLogToolStripMenuItem,
            this.toolStripMenuItem1,
            this.toolStripSeparator2,
            this.exitToolStripMenuItem1});
            this.fileToolStripMenuItem.Name = "fileToolStripMenuItem";
            this.fileToolStripMenuItem.Size = new System.Drawing.Size(37, 20);
            this.fileToolStripMenuItem.Text = "&File";
            // 
            // openToolStripMenuItem1
            // 
            this.openToolStripMenuItem1.Image = ((System.Drawing.Image)(resources.GetObject("openToolStripMenuItem1.Image")));
            this.openToolStripMenuItem1.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.openToolStripMenuItem1.Name = "openToolStripMenuItem1";
            this.openToolStripMenuItem1.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.O)));
            this.openToolStripMenuItem1.Size = new System.Drawing.Size(169, 22);
            this.openToolStripMenuItem1.Text = "&Open Log";
            this.openToolStripMenuItem1.Click += new System.EventHandler(this.openToolStripMenuItem1_Click);
            // 
            // saveLogToolStripMenuItem
            // 
            this.saveLogToolStripMenuItem.Image = ((System.Drawing.Image)(resources.GetObject("saveLogToolStripMenuItem.Image")));
            this.saveLogToolStripMenuItem.Name = "saveLogToolStripMenuItem";
            this.saveLogToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.S)));
            this.saveLogToolStripMenuItem.Size = new System.Drawing.Size(169, 22);
            this.saveLogToolStripMenuItem.Text = "&Save Log";
            this.saveLogToolStripMenuItem.Click += new System.EventHandler(this.saveLogToolStripMenuItem_Click);
            // 
            // toolStripMenuItem1
            // 
            this.toolStripMenuItem1.Image = ((System.Drawing.Image)(resources.GetObject("toolStripMenuItem1.Image")));
            this.toolStripMenuItem1.Name = "toolStripMenuItem1";
            this.toolStripMenuItem1.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.N)));
            this.toolStripMenuItem1.Size = new System.Drawing.Size(169, 22);
            this.toolStripMenuItem1.Text = "Co&nnect";
            this.toolStripMenuItem1.Click += new System.EventHandler(this.toolStripMenuItem1_Click);
            // 
            // toolStripSeparator2
            // 
            this.toolStripSeparator2.Name = "toolStripSeparator2";
            this.toolStripSeparator2.Size = new System.Drawing.Size(166, 6);
            // 
            // exitToolStripMenuItem1
            // 
            this.exitToolStripMenuItem1.Name = "exitToolStripMenuItem1";
            this.exitToolStripMenuItem1.Size = new System.Drawing.Size(169, 22);
            this.exitToolStripMenuItem1.Text = "E&xit";
            this.exitToolStripMenuItem1.Click += new System.EventHandler(this.exitToolStripMenuItem1_Click);
            // 
            // editToolStripMenuItem
            // 
            this.editToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.resetToolStripMenuItem,
            this.cropLogRangeFromViewToolStripMenuItem});
            this.editToolStripMenuItem.Name = "editToolStripMenuItem";
            this.editToolStripMenuItem.Size = new System.Drawing.Size(80, 20);
            this.editToolStripMenuItem.Text = "Log Ranges";
            // 
            // resetToolStripMenuItem
            // 
            this.resetToolStripMenuItem.Name = "resetToolStripMenuItem";
            this.resetToolStripMenuItem.Size = new System.Drawing.Size(145, 22);
            this.resetToolStripMenuItem.Text = "Reset";
            this.resetToolStripMenuItem.Click += new System.EventHandler(this.resetToolStripMenuItem_Click);
            // 
            // cropLogRangeFromViewToolStripMenuItem
            // 
            this.cropLogRangeFromViewToolStripMenuItem.Name = "cropLogRangeFromViewToolStripMenuItem";
            this.cropLogRangeFromViewToolStripMenuItem.Size = new System.Drawing.Size(145, 22);
            this.cropLogRangeFromViewToolStripMenuItem.Text = "Crop To View";
            this.cropLogRangeFromViewToolStripMenuItem.Click += new System.EventHandler(this.cropLogRangeFromViewToolStripMenuItem_Click);
            // 
            // viewToolStripMenuItem
            // 
            this.viewToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.graphToolStripMenuItem,
            this.fitToFrameRecordsToolStripMenuItem,
            this.screenshotGraphToolStripMenuItem});
            this.viewToolStripMenuItem.Name = "viewToolStripMenuItem";
            this.viewToolStripMenuItem.Size = new System.Drawing.Size(44, 20);
            this.viewToolStripMenuItem.Text = "&View";
            // 
            // graphToolStripMenuItem
            // 
            this.graphToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.scaleAboutCentreToolStripMenuItem});
            this.graphToolStripMenuItem.Name = "graphToolStripMenuItem";
            this.graphToolStripMenuItem.Size = new System.Drawing.Size(185, 22);
            this.graphToolStripMenuItem.Text = "&Graph";
            // 
            // scaleAboutCentreToolStripMenuItem
            // 
            this.scaleAboutCentreToolStripMenuItem.CheckOnClick = true;
            this.scaleAboutCentreToolStripMenuItem.Name = "scaleAboutCentreToolStripMenuItem";
            this.scaleAboutCentreToolStripMenuItem.Size = new System.Drawing.Size(171, 22);
            this.scaleAboutCentreToolStripMenuItem.Text = "&Scale about centre";
            this.scaleAboutCentreToolStripMenuItem.CheckedChanged += new System.EventHandler(this.scaleAboutCentreToolStripMenuItem_CheckedChanged);
            // 
            // fitToFrameRecordsToolStripMenuItem
            // 
            this.fitToFrameRecordsToolStripMenuItem.Name = "fitToFrameRecordsToolStripMenuItem";
            this.fitToFrameRecordsToolStripMenuItem.Size = new System.Drawing.Size(185, 22);
            this.fitToFrameRecordsToolStripMenuItem.Text = "Fit To Frame Records";
            this.fitToFrameRecordsToolStripMenuItem.Click += new System.EventHandler(this.fitToFrameRecordsToolStripMenuItem_Click);
            // 
            // screenshotGraphToolStripMenuItem
            // 
            this.screenshotGraphToolStripMenuItem.Image = ((System.Drawing.Image)(resources.GetObject("screenshotGraphToolStripMenuItem.Image")));
            this.screenshotGraphToolStripMenuItem.Name = "screenshotGraphToolStripMenuItem";
            this.screenshotGraphToolStripMenuItem.Size = new System.Drawing.Size(185, 22);
            this.screenshotGraphToolStripMenuItem.Text = "Screenshot Graph";
            this.screenshotGraphToolStripMenuItem.Click += new System.EventHandler(this.screenshotGraphToolStripMenuItem_Click);
            // 
            // profilesToolStripMenuItem
            // 
            this.profilesToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.openToolStripMenuItem,
            this.saveToolStripMenuItem,
            this.toolStripSeparator1});
            this.profilesToolStripMenuItem.Name = "profilesToolStripMenuItem";
            this.profilesToolStripMenuItem.Size = new System.Drawing.Size(53, 20);
            this.profilesToolStripMenuItem.Text = "Profile";
            // 
            // openToolStripMenuItem
            // 
            this.openToolStripMenuItem.Image = ((System.Drawing.Image)(resources.GetObject("openToolStripMenuItem.Image")));
            this.openToolStripMenuItem.Name = "openToolStripMenuItem";
            this.openToolStripMenuItem.Size = new System.Drawing.Size(103, 22);
            this.openToolStripMenuItem.Text = "Open";
            this.openToolStripMenuItem.Click += new System.EventHandler(this.openToolStripMenuItem_Click);
            // 
            // saveToolStripMenuItem
            // 
            this.saveToolStripMenuItem.Image = ((System.Drawing.Image)(resources.GetObject("saveToolStripMenuItem.Image")));
            this.saveToolStripMenuItem.Name = "saveToolStripMenuItem";
            this.saveToolStripMenuItem.Size = new System.Drawing.Size(103, 22);
            this.saveToolStripMenuItem.Text = "Save";
            this.saveToolStripMenuItem.Click += new System.EventHandler(this.saveToolStripMenuItem_Click);
            // 
            // toolStripSeparator1
            // 
            this.toolStripSeparator1.Name = "toolStripSeparator1";
            this.toolStripSeparator1.Size = new System.Drawing.Size(100, 6);
            // 
            // openFileDialog
            // 
            this.openFileDialog.Filter = "Log files|*.log;*.bin|All files|*.*";
            // 
            // tabControl1
            // 
            this.tabControl1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tabControl1.Location = new System.Drawing.Point(0, 0);
            this.tabControl1.Name = "tabControl1";
            this.tabControl1.SelectedIndex = 0;
            this.tabControl1.Size = new System.Drawing.Size(1434, 778);
            this.tabControl1.TabIndex = 3;
            this.tabControl1.Selected += new System.Windows.Forms.TabControlEventHandler(this.tabControl1_Selected);
            this.tabControl1.MouseClick += new System.Windows.Forms.MouseEventHandler(this.tabControl1_MouseClick);
            // 
            // splitContainer1
            // 
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.Location = new System.Drawing.Point(0, 24);
            this.splitContainer1.Name = "splitContainer1";
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this.sessionSplitContainer);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.tabControl1);
            this.splitContainer1.Size = new System.Drawing.Size(1607, 778);
            this.splitContainer1.SplitterDistance = 169;
            this.splitContainer1.TabIndex = 4;
            // 
            // sessionSplitContainer
            // 
            this.sessionSplitContainer.Dock = System.Windows.Forms.DockStyle.Fill;
            this.sessionSplitContainer.Location = new System.Drawing.Point(0, 0);
            this.sessionSplitContainer.Name = "sessionSplitContainer";
            this.sessionSplitContainer.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // sessionSplitContainer.Panel1
            // 
            this.sessionSplitContainer.Panel1.Controls.Add(this.sessionTreeView);
            // 
            // sessionSplitContainer.Panel2
            // 
            this.sessionSplitContainer.Panel2.Controls.Add(this.splitContainer2);
            this.sessionSplitContainer.Size = new System.Drawing.Size(169, 778);
            this.sessionSplitContainer.SplitterDistance = 526;
            this.sessionSplitContainer.TabIndex = 1;
            // 
            // sessionTreeView
            // 
            this.sessionTreeView.Dock = System.Windows.Forms.DockStyle.Fill;
            this.sessionTreeView.Location = new System.Drawing.Point(0, 0);
            this.sessionTreeView.Name = "sessionTreeView";
            treeNode1.Name = "Tabs";
            treeNode1.Text = "Tabs";
            treeNode2.Name = "Logs";
            treeNode2.Text = "Logs";
            this.sessionTreeView.Nodes.AddRange(new System.Windows.Forms.TreeNode[] {
            treeNode1,
            treeNode2});
            this.sessionTreeView.Size = new System.Drawing.Size(169, 526);
            this.sessionTreeView.TabIndex = 0;
            this.sessionTreeView.BeforeCollapse += new System.Windows.Forms.TreeViewCancelEventHandler(this.sessionTreeView_BeforeCollapse);
            this.sessionTreeView.BeforeExpand += new System.Windows.Forms.TreeViewCancelEventHandler(this.sessionTreeView_BeforeExpand);
            this.sessionTreeView.ItemDrag += new System.Windows.Forms.ItemDragEventHandler(this.sessionTreeView_ItemDrag);
            this.sessionTreeView.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.sessionTreeView_AfterSelect);
            this.sessionTreeView.NodeMouseDoubleClick += new System.Windows.Forms.TreeNodeMouseClickEventHandler(this.sessionTreeView_NodeMouseDoubleClick);
            this.sessionTreeView.MouseDown += new System.Windows.Forms.MouseEventHandler(this.sessionTreeView_MouseDown);
            // 
            // splitContainer2
            // 
            this.splitContainer2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer2.FixedPanel = System.Windows.Forms.FixedPanel.Panel1;
            this.splitContainer2.IsSplitterFixed = true;
            this.splitContainer2.Location = new System.Drawing.Point(0, 0);
            this.splitContainer2.Name = "splitContainer2";
            this.splitContainer2.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // splitContainer2.Panel1
            // 
            this.splitContainer2.Panel1.Controls.Add(this.sessionInfoComboBox);
            // 
            // splitContainer2.Panel2
            // 
            this.splitContainer2.Panel2.Controls.Add(this.sessionInfoPanel);
            this.splitContainer2.Size = new System.Drawing.Size(169, 248);
            this.splitContainer2.SplitterDistance = 25;
            this.splitContainer2.TabIndex = 1;
            // 
            // sessionInfoComboBox
            // 
            this.sessionInfoComboBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.sessionInfoComboBox.FormattingEnabled = true;
            this.sessionInfoComboBox.Location = new System.Drawing.Point(0, 0);
            this.sessionInfoComboBox.Name = "sessionInfoComboBox";
            this.sessionInfoComboBox.Size = new System.Drawing.Size(169, 21);
            this.sessionInfoComboBox.TabIndex = 0;
            this.sessionInfoComboBox.SelectedIndexChanged += new System.EventHandler(this.sessionInfoComboBox_SelectedIndexChanged);
            // 
            // sessionInfoPanel
            // 
            this.sessionInfoPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.sessionInfoPanel.Location = new System.Drawing.Point(0, 0);
            this.sessionInfoPanel.Name = "sessionInfoPanel";
            this.sessionInfoPanel.Size = new System.Drawing.Size(169, 219);
            this.sessionInfoPanel.TabIndex = 0;
            // 
            // StatoscopeForm
            // 
            this.AllowDrop = true;
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1607, 802);
            this.Controls.Add(this.splitContainer1);
            this.Controls.Add(this.menuStrip1);
            this.MainMenuStrip = this.menuStrip1;
            this.Name = "StatoscopeForm";
            this.Text = "Statoscope";
            this.DragDrop += new System.Windows.Forms.DragEventHandler(this.StatoscopeForm_DragDrop);
            this.DragEnter += new System.Windows.Forms.DragEventHandler(this.StatoscopeForm_DragEnter);
            this.menuStrip1.ResumeLayout(false);
            this.menuStrip1.PerformLayout();
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            this.splitContainer1.ResumeLayout(false);
            this.sessionSplitContainer.Panel1.ResumeLayout(false);
            this.sessionSplitContainer.Panel2.ResumeLayout(false);
            this.sessionSplitContainer.ResumeLayout(false);
            this.splitContainer2.Panel1.ResumeLayout(false);
            this.splitContainer2.Panel2.ResumeLayout(false);
            this.splitContainer2.ResumeLayout(false);
            this.ResumeLayout(false);
            this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.MenuStrip menuStrip1;
		private System.Windows.Forms.ToolStripMenuItem fileToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem exitToolStripMenuItem1;
		private System.Windows.Forms.OpenFileDialog openFileDialog;
		private System.Windows.Forms.TabControl tabControl1;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
		private System.Windows.Forms.ToolStripMenuItem toolStripMenuItem1;
		private System.Windows.Forms.SplitContainer splitContainer1;
		private System.Windows.Forms.TreeView sessionTreeView;
    private System.Windows.Forms.ToolStripMenuItem viewToolStripMenuItem;
    private System.Windows.Forms.ToolStripMenuItem graphToolStripMenuItem;
    private System.Windows.Forms.ToolStripMenuItem scaleAboutCentreToolStripMenuItem;
		private System.Windows.Forms.SplitContainer sessionSplitContainer;
		private System.Windows.Forms.Panel sessionInfoPanel;
		private System.Windows.Forms.SplitContainer splitContainer2;
		private System.Windows.Forms.ComboBox sessionInfoComboBox;
		private System.Windows.Forms.ToolStripMenuItem screenshotGraphToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem profilesToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem openToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem saveToolStripMenuItem;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
		private System.Windows.Forms.ToolStripMenuItem editToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem cropLogRangeFromViewToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem resetToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem fitToFrameRecordsToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem openToolStripMenuItem1;
        private System.Windows.Forms.ToolStripMenuItem saveLogToolStripMenuItem;
	}
}


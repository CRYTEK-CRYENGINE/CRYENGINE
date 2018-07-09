// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace LuaRemoteDebugger
{
	partial class MainForm
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
			this.components = new System.ComponentModel.Container();
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MainForm));
			this.treeColumnKey = new Aga.Controls.Tree.TreeColumn();
			this.treeColumnValue = new Aga.Controls.Tree.TreeColumn();
			this.treeColumnKeyType = new Aga.Controls.Tree.TreeColumn();
			this.treeColumnValueType = new Aga.Controls.Tree.TreeColumn();
			this.imageListTreeView = new System.Windows.Forms.ImageList(this.components);
			this.watchKey = new Aga.Controls.Tree.NodeControls.NodeTextBox();
			this.watchValue = new Aga.Controls.Tree.NodeControls.NodeTextBox();
			this.watchKeyType = new Aga.Controls.Tree.NodeControls.NodeTextBox();
			this.watchValueType = new Aga.Controls.Tree.NodeControls.NodeTextBox();
			this.toolStripContainer = new System.Windows.Forms.ToolStripContainer();
			this.splitContainerHorizontal = new System.Windows.Forms.SplitContainer();
			this.splitContainerVertical = new System.Windows.Forms.SplitContainer();
			this.treeViewFiles = new System.Windows.Forms.TreeView();
			this.tabControlSourceFiles = new System.Windows.Forms.TabControl();
			this.splitContainer1 = new System.Windows.Forms.SplitContainer();
			this.tabControlBottom = new System.Windows.Forms.TabControl();
			this.tabPageCallstack = new System.Windows.Forms.TabPage();
			this.tabPageBreakpoints = new System.Windows.Forms.TabPage();
			this.tabPageLog = new System.Windows.Forms.TabPage();
			this.richTextBoxLog = new System.Windows.Forms.RichTextBox();
			this.tabControlVars = new System.Windows.Forms.TabControl();
			this.tabPageLocals = new System.Windows.Forms.TabPage();
			this.treeViewAdvLocals = new Aga.Controls.Tree.TreeViewAdv();
			this.localsKey = new Aga.Controls.Tree.NodeControls.NodeTextBox();
			this.localsValue = new Aga.Controls.Tree.NodeControls.NodeTextBox();
			this.localsKeyType = new Aga.Controls.Tree.NodeControls.NodeTextBox();
			this.localsValueType = new Aga.Controls.Tree.NodeControls.NodeTextBox();
			this.tabPageWatch = new System.Windows.Forms.TabPage();
			this.treeViewAdvWatch = new Aga.Controls.Tree.TreeViewAdv();
			this.toolStripDebug = new System.Windows.Forms.ToolStrip();
			this.toolStripButtonContinue = new System.Windows.Forms.ToolStripButton();
			this.toolStripButtonBreak = new System.Windows.Forms.ToolStripButton();
			this.toolStripButtonStepInto = new System.Windows.Forms.ToolStripButton();
			this.toolStripButtonStepOver = new System.Windows.Forms.ToolStripButton();
			this.toolStripButtonStepOut = new System.Windows.Forms.ToolStripButton();
			this.toolStripButtonBreakpoint = new System.Windows.Forms.ToolStripButton();
			this.menuStrip = new System.Windows.Forms.MenuStrip();
			this.toolStripMenuFile = new System.Windows.Forms.ToolStripMenuItem();
			this.setScriptsFolderToolStripMenu = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
			this.connectToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.disconnectToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator3 = new System.Windows.Forms.ToolStripSeparator();
			this.findFileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
			this.closeToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator4 = new System.Windows.Forms.ToolStripSeparator();
			this.exitToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.editToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.findToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.goToToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.debugToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.continueToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.breakToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator5 = new System.Windows.Forms.ToolStripSeparator();
			this.stepIntoToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.stepOverToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.stepOutToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator6 = new System.Windows.Forms.ToolStripSeparator();
			this.toggleBreakpointToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator7 = new System.Windows.Forms.ToolStripSeparator();
			this.enableCCallstackToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.disabledToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.condensedToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.fullToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.helpToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.aboutLuaRemoteDebuggerToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.listViewCallStack = new LuaRemoteDebuggerControls.DoubleClickListView();
			this.columnHeaderDescription = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeaderLine = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeaderSource = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.listViewBreakpoints = new LuaRemoteDebuggerControls.DoubleClickListView();
			this.columnHeaderName = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.toolStripContainer.ContentPanel.SuspendLayout();
			this.toolStripContainer.TopToolStripPanel.SuspendLayout();
			this.toolStripContainer.SuspendLayout();
			this.splitContainerHorizontal.Panel1.SuspendLayout();
			this.splitContainerHorizontal.Panel2.SuspendLayout();
			this.splitContainerHorizontal.SuspendLayout();
			this.splitContainerVertical.Panel1.SuspendLayout();
			this.splitContainerVertical.Panel2.SuspendLayout();
			this.splitContainerVertical.SuspendLayout();
			this.splitContainer1.Panel1.SuspendLayout();
			this.splitContainer1.Panel2.SuspendLayout();
			this.splitContainer1.SuspendLayout();
			this.tabControlBottom.SuspendLayout();
			this.tabPageCallstack.SuspendLayout();
			this.tabPageBreakpoints.SuspendLayout();
			this.tabPageLog.SuspendLayout();
			this.tabControlVars.SuspendLayout();
			this.tabPageLocals.SuspendLayout();
			this.tabPageWatch.SuspendLayout();
			this.toolStripDebug.SuspendLayout();
			this.menuStrip.SuspendLayout();
			this.SuspendLayout();
			// 
			// treeColumnKey
			// 
			this.treeColumnKey.Header = "Key";
			this.treeColumnKey.SortOrder = System.Windows.Forms.SortOrder.None;
			this.treeColumnKey.TooltipText = null;
			this.treeColumnKey.Width = 200;
			// 
			// treeColumnValue
			// 
			this.treeColumnValue.Header = "Value";
			this.treeColumnValue.SortOrder = System.Windows.Forms.SortOrder.None;
			this.treeColumnValue.TooltipText = null;
			this.treeColumnValue.Width = 200;
			// 
			// treeColumnKeyType
			// 
			this.treeColumnKeyType.Header = "Key Type";
			this.treeColumnKeyType.SortOrder = System.Windows.Forms.SortOrder.None;
			this.treeColumnKeyType.TooltipText = null;
			this.treeColumnKeyType.Width = 100;
			// 
			// treeColumnValueType
			// 
			this.treeColumnValueType.Header = "Value Type";
			this.treeColumnValueType.SortOrder = System.Windows.Forms.SortOrder.None;
			this.treeColumnValueType.TooltipText = null;
			this.treeColumnValueType.Width = 100;
			// 
			// imageListTreeView
			// 
			this.imageListTreeView.ImageStream = ((System.Windows.Forms.ImageListStreamer)(resources.GetObject("imageListTreeView.ImageStream")));
			this.imageListTreeView.TransparentColor = System.Drawing.Color.Transparent;
			this.imageListTreeView.Images.SetKeyName(0, "folder-open_16.png");
			this.imageListTreeView.Images.SetKeyName(1, "document-edit.png");
			// 
			// watchKey
			// 
			this.watchKey.DataPropertyName = "Key";
			this.watchKey.EditEnabled = true;
			this.watchKey.IncrementalSearchEnabled = true;
			this.watchKey.LeftMargin = 3;
			this.watchKey.ParentColumn = this.treeColumnKey;
			// 
			// watchValue
			// 
			this.watchValue.DataPropertyName = "Value";
			this.watchValue.IncrementalSearchEnabled = true;
			this.watchValue.LeftMargin = 3;
			this.watchValue.ParentColumn = this.treeColumnValue;
			// 
			// watchKeyType
			// 
			this.watchKeyType.DataPropertyName = "KeyType";
			this.watchKeyType.IncrementalSearchEnabled = true;
			this.watchKeyType.LeftMargin = 3;
			this.watchKeyType.ParentColumn = this.treeColumnKeyType;
			// 
			// watchValueType
			// 
			this.watchValueType.DataPropertyName = "ValueType";
			this.watchValueType.IncrementalSearchEnabled = true;
			this.watchValueType.LeftMargin = 3;
			this.watchValueType.ParentColumn = this.treeColumnValueType;
			// 
			// toolStripContainer
			// 
			// 
			// toolStripContainer.ContentPanel
			// 
			this.toolStripContainer.ContentPanel.AutoScroll = true;
			this.toolStripContainer.ContentPanel.Controls.Add(this.splitContainerHorizontal);
			this.toolStripContainer.ContentPanel.Size = new System.Drawing.Size(1276, 675);
			this.toolStripContainer.Dock = System.Windows.Forms.DockStyle.Fill;
			this.toolStripContainer.Location = new System.Drawing.Point(0, 24);
			this.toolStripContainer.Name = "toolStripContainer";
			this.toolStripContainer.Size = new System.Drawing.Size(1276, 700);
			this.toolStripContainer.TabIndex = 14;
			this.toolStripContainer.Text = "toolStripContainer1";
			// 
			// toolStripContainer.TopToolStripPanel
			// 
			this.toolStripContainer.TopToolStripPanel.Controls.Add(this.toolStripDebug);
			// 
			// splitContainerHorizontal
			// 
			this.splitContainerHorizontal.Dock = System.Windows.Forms.DockStyle.Fill;
			this.splitContainerHorizontal.Location = new System.Drawing.Point(0, 0);
			this.splitContainerHorizontal.Name = "splitContainerHorizontal";
			this.splitContainerHorizontal.Orientation = System.Windows.Forms.Orientation.Horizontal;
			// 
			// splitContainerHorizontal.Panel1
			// 
			this.splitContainerHorizontal.Panel1.Controls.Add(this.splitContainerVertical);
			// 
			// splitContainerHorizontal.Panel2
			// 
			this.splitContainerHorizontal.Panel2.Controls.Add(this.splitContainer1);
			this.splitContainerHorizontal.Size = new System.Drawing.Size(1276, 675);
			this.splitContainerHorizontal.SplitterDistance = 446;
			this.splitContainerHorizontal.TabIndex = 6;
			// 
			// splitContainerVertical
			// 
			this.splitContainerVertical.Dock = System.Windows.Forms.DockStyle.Fill;
			this.splitContainerVertical.Location = new System.Drawing.Point(0, 0);
			this.splitContainerVertical.Name = "splitContainerVertical";
			// 
			// splitContainerVertical.Panel1
			// 
			this.splitContainerVertical.Panel1.Controls.Add(this.treeViewFiles);
			// 
			// splitContainerVertical.Panel2
			// 
			this.splitContainerVertical.Panel2.Controls.Add(this.tabControlSourceFiles);
			this.splitContainerVertical.Size = new System.Drawing.Size(1276, 446);
			this.splitContainerVertical.SplitterDistance = 305;
			this.splitContainerVertical.TabIndex = 0;
			// 
			// treeViewFiles
			// 
			this.treeViewFiles.Dock = System.Windows.Forms.DockStyle.Fill;
			this.treeViewFiles.ImageIndex = 0;
			this.treeViewFiles.ImageList = this.imageListTreeView;
			this.treeViewFiles.Location = new System.Drawing.Point(0, 0);
			this.treeViewFiles.Name = "treeViewFiles";
			this.treeViewFiles.SelectedImageIndex = 0;
			this.treeViewFiles.Size = new System.Drawing.Size(305, 446);
			this.treeViewFiles.TabIndex = 0;
			this.treeViewFiles.NodeMouseDoubleClick += new System.Windows.Forms.TreeNodeMouseClickEventHandler(this.treeViewFiles_NodeMouseDoubleClick);
			// 
			// tabControlSourceFiles
			// 
			this.tabControlSourceFiles.Dock = System.Windows.Forms.DockStyle.Fill;
			this.tabControlSourceFiles.Location = new System.Drawing.Point(0, 0);
			this.tabControlSourceFiles.Name = "tabControlSourceFiles";
			this.tabControlSourceFiles.SelectedIndex = 0;
			this.tabControlSourceFiles.ShowToolTips = true;
			this.tabControlSourceFiles.Size = new System.Drawing.Size(967, 446);
			this.tabControlSourceFiles.TabIndex = 0;
			// 
			// splitContainer1
			// 
			this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
			this.splitContainer1.Location = new System.Drawing.Point(0, 0);
			this.splitContainer1.Name = "splitContainer1";
			// 
			// splitContainer1.Panel1
			// 
			this.splitContainer1.Panel1.Controls.Add(this.tabControlBottom);
			// 
			// splitContainer1.Panel2
			// 
			this.splitContainer1.Panel2.Controls.Add(this.tabControlVars);
			this.splitContainer1.Size = new System.Drawing.Size(1276, 225);
			this.splitContainer1.SplitterDistance = 627;
			this.splitContainer1.TabIndex = 4;
			// 
			// tabControlBottom
			// 
			this.tabControlBottom.Controls.Add(this.tabPageCallstack);
			this.tabControlBottom.Controls.Add(this.tabPageBreakpoints);
			this.tabControlBottom.Controls.Add(this.tabPageLog);
			this.tabControlBottom.Dock = System.Windows.Forms.DockStyle.Fill;
			this.tabControlBottom.Location = new System.Drawing.Point(0, 0);
			this.tabControlBottom.Name = "tabControlBottom";
			this.tabControlBottom.SelectedIndex = 0;
			this.tabControlBottom.Size = new System.Drawing.Size(627, 225);
			this.tabControlBottom.TabIndex = 3;
			// 
			// tabPageCallstack
			// 
			this.tabPageCallstack.Controls.Add(this.listViewCallStack);
			this.tabPageCallstack.Location = new System.Drawing.Point(4, 22);
			this.tabPageCallstack.Name = "tabPageCallstack";
			this.tabPageCallstack.Padding = new System.Windows.Forms.Padding(3);
			this.tabPageCallstack.Size = new System.Drawing.Size(619, 199);
			this.tabPageCallstack.TabIndex = 1;
			this.tabPageCallstack.Text = "Call Stack";
			this.tabPageCallstack.UseVisualStyleBackColor = true;
			// 
			// tabPageBreakpoints
			// 
			this.tabPageBreakpoints.Controls.Add(this.listViewBreakpoints);
			this.tabPageBreakpoints.Location = new System.Drawing.Point(4, 22);
			this.tabPageBreakpoints.Name = "tabPageBreakpoints";
			this.tabPageBreakpoints.Padding = new System.Windows.Forms.Padding(3);
			this.tabPageBreakpoints.Size = new System.Drawing.Size(619, 199);
			this.tabPageBreakpoints.TabIndex = 3;
			this.tabPageBreakpoints.Text = "Breakpoints";
			this.tabPageBreakpoints.UseVisualStyleBackColor = true;
			// 
			// tabPageLog
			// 
			this.tabPageLog.Controls.Add(this.richTextBoxLog);
			this.tabPageLog.Location = new System.Drawing.Point(4, 22);
			this.tabPageLog.Name = "tabPageLog";
			this.tabPageLog.Padding = new System.Windows.Forms.Padding(3);
			this.tabPageLog.Size = new System.Drawing.Size(619, 199);
			this.tabPageLog.TabIndex = 0;
			this.tabPageLog.Text = "Log";
			this.tabPageLog.UseVisualStyleBackColor = true;
			// 
			// richTextBoxLog
			// 
			this.richTextBoxLog.Dock = System.Windows.Forms.DockStyle.Fill;
			this.richTextBoxLog.Location = new System.Drawing.Point(3, 3);
			this.richTextBoxLog.Name = "richTextBoxLog";
			this.richTextBoxLog.Size = new System.Drawing.Size(613, 193);
			this.richTextBoxLog.TabIndex = 0;
			this.richTextBoxLog.Text = "";
			// 
			// tabControlVars
			// 
			this.tabControlVars.Controls.Add(this.tabPageLocals);
			this.tabControlVars.Controls.Add(this.tabPageWatch);
			this.tabControlVars.Dock = System.Windows.Forms.DockStyle.Fill;
			this.tabControlVars.Location = new System.Drawing.Point(0, 0);
			this.tabControlVars.Name = "tabControlVars";
			this.tabControlVars.SelectedIndex = 0;
			this.tabControlVars.Size = new System.Drawing.Size(645, 225);
			this.tabControlVars.TabIndex = 0;
			// 
			// tabPageLocals
			// 
			this.tabPageLocals.Controls.Add(this.treeViewAdvLocals);
			this.tabPageLocals.Location = new System.Drawing.Point(4, 22);
			this.tabPageLocals.Name = "tabPageLocals";
			this.tabPageLocals.Size = new System.Drawing.Size(637, 199);
			this.tabPageLocals.TabIndex = 2;
			this.tabPageLocals.Text = "Locals";
			this.tabPageLocals.UseVisualStyleBackColor = true;
			// 
			// treeViewAdvLocals
			// 
			this.treeViewAdvLocals.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
			| System.Windows.Forms.AnchorStyles.Left) 
			| System.Windows.Forms.AnchorStyles.Right)));
			this.treeViewAdvLocals.BackColor = System.Drawing.SystemColors.Window;
			this.treeViewAdvLocals.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.treeViewAdvLocals.Columns.Add(this.treeColumnKey);
			this.treeViewAdvLocals.Columns.Add(this.treeColumnValue);
			this.treeViewAdvLocals.Columns.Add(this.treeColumnKeyType);
			this.treeViewAdvLocals.Columns.Add(this.treeColumnValueType);
			this.treeViewAdvLocals.Cursor = System.Windows.Forms.Cursors.Default;
			this.treeViewAdvLocals.DefaultToolTipProvider = null;
			this.treeViewAdvLocals.DragDropMarkColor = System.Drawing.Color.Black;
			this.treeViewAdvLocals.LineColor = System.Drawing.SystemColors.ControlDark;
			this.treeViewAdvLocals.Location = new System.Drawing.Point(3, 3);
			this.treeViewAdvLocals.Model = null;
			this.treeViewAdvLocals.Name = "treeViewAdvLocals";
			this.treeViewAdvLocals.NodeControls.Add(this.localsKey);
			this.treeViewAdvLocals.NodeControls.Add(this.localsValue);
			this.treeViewAdvLocals.NodeControls.Add(this.localsKeyType);
			this.treeViewAdvLocals.NodeControls.Add(this.localsValueType);
			this.treeViewAdvLocals.SelectedNode = null;
			this.treeViewAdvLocals.Size = new System.Drawing.Size(631, 193);
			this.treeViewAdvLocals.TabIndex = 0;
			this.treeViewAdvLocals.Text = "treeViewAdvLocals";
			this.treeViewAdvLocals.UseColumns = true;
			// 
			// localsKey
			// 
			this.localsKey.DataPropertyName = "Key";
			this.localsKey.IncrementalSearchEnabled = true;
			this.localsKey.LeftMargin = 3;
			this.localsKey.ParentColumn = this.treeColumnKey;
			// 
			// localsValue
			// 
			this.localsValue.DataPropertyName = "Value";
			this.localsValue.IncrementalSearchEnabled = true;
			this.localsValue.LeftMargin = 3;
			this.localsValue.ParentColumn = this.treeColumnValue;
			// 
			// localsKeyType
			// 
			this.localsKeyType.DataPropertyName = "KeyType";
			this.localsKeyType.IncrementalSearchEnabled = true;
			this.localsKeyType.LeftMargin = 3;
			this.localsKeyType.ParentColumn = this.treeColumnKeyType;
			// 
			// localsValueType
			// 
			this.localsValueType.DataPropertyName = "ValueType";
			this.localsValueType.IncrementalSearchEnabled = true;
			this.localsValueType.LeftMargin = 3;
			this.localsValueType.ParentColumn = this.treeColumnValueType;
			// 
			// tabPageWatch
			// 
			this.tabPageWatch.Controls.Add(this.treeViewAdvWatch);
			this.tabPageWatch.Location = new System.Drawing.Point(4, 22);
			this.tabPageWatch.Name = "tabPageWatch";
			this.tabPageWatch.Padding = new System.Windows.Forms.Padding(3);
			this.tabPageWatch.Size = new System.Drawing.Size(637, 199);
			this.tabPageWatch.TabIndex = 4;
			this.tabPageWatch.Text = "Watch";
			this.tabPageWatch.UseVisualStyleBackColor = true;
			// 
			// treeViewAdvWatch
			// 
			this.treeViewAdvWatch.AllowDrop = true;
			this.treeViewAdvWatch.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
			| System.Windows.Forms.AnchorStyles.Left) 
			| System.Windows.Forms.AnchorStyles.Right)));
			this.treeViewAdvWatch.BackColor = System.Drawing.SystemColors.Window;
			this.treeViewAdvWatch.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.treeViewAdvWatch.Columns.Add(this.treeColumnKey);
			this.treeViewAdvWatch.Columns.Add(this.treeColumnValue);
			this.treeViewAdvWatch.Columns.Add(this.treeColumnKeyType);
			this.treeViewAdvWatch.Columns.Add(this.treeColumnValueType);
			this.treeViewAdvWatch.Cursor = System.Windows.Forms.Cursors.Default;
			this.treeViewAdvWatch.DefaultToolTipProvider = null;
			this.treeViewAdvWatch.DisplayDraggingNodes = true;
			this.treeViewAdvWatch.DragDropMarkColor = System.Drawing.Color.Black;
			this.treeViewAdvWatch.HighlightDropPosition = false;
			this.treeViewAdvWatch.LineColor = System.Drawing.SystemColors.ControlDark;
			this.treeViewAdvWatch.Location = new System.Drawing.Point(3, 3);
			this.treeViewAdvWatch.Model = null;
			this.treeViewAdvWatch.Name = "treeViewAdvWatch";
			this.treeViewAdvWatch.NodeControls.Add(this.watchKey);
			this.treeViewAdvWatch.NodeControls.Add(this.watchValue);
			this.treeViewAdvWatch.NodeControls.Add(this.watchKeyType);
			this.treeViewAdvWatch.NodeControls.Add(this.watchValueType);
			this.treeViewAdvWatch.SelectedNode = null;
			this.treeViewAdvWatch.Size = new System.Drawing.Size(631, 193);
			this.treeViewAdvWatch.TabIndex = 1;
			this.treeViewAdvWatch.Text = "treeViewAdvWatch";
			this.treeViewAdvWatch.UseColumns = true;
			this.treeViewAdvWatch.ItemDrag += new System.Windows.Forms.ItemDragEventHandler(this.treeViewAdvWatch_ItemDrag);
			this.treeViewAdvWatch.DragDrop += new System.Windows.Forms.DragEventHandler(this.treeViewAdvWatch_DragDrop);
			this.treeViewAdvWatch.DragEnter += new System.Windows.Forms.DragEventHandler(this.treeViewAdvWatch_DragEnter);
			this.treeViewAdvWatch.KeyDown += new System.Windows.Forms.KeyEventHandler(this.treeViewAdvWatch_KeyDown);
			this.treeViewAdvWatch.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.treeViewAdvWatch_MouseDoubleClick);
			// 
			// toolStripDebug
			// 
			this.toolStripDebug.Dock = System.Windows.Forms.DockStyle.None;
			this.toolStripDebug.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.toolStripButtonContinue,
			this.toolStripButtonBreak,
			this.toolStripButtonStepInto,
			this.toolStripButtonStepOver,
			this.toolStripButtonStepOut,
			this.toolStripButtonBreakpoint});
			this.toolStripDebug.Location = new System.Drawing.Point(3, 0);
			this.toolStripDebug.Name = "toolStripDebug";
			this.toolStripDebug.Size = new System.Drawing.Size(150, 25);
			this.toolStripDebug.TabIndex = 0;
			// 
			// toolStripButtonContinue
			// 
			this.toolStripButtonContinue.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.toolStripButtonContinue.Image = global::LuaRemoteDebugger.Properties.Resources._continue;
			this.toolStripButtonContinue.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.toolStripButtonContinue.Name = "toolStripButtonContinue";
			this.toolStripButtonContinue.Size = new System.Drawing.Size(23, 22);
			this.toolStripButtonContinue.Text = "Continue";
			this.toolStripButtonContinue.Click += new System.EventHandler(this.toolStripButtonContinue_Click);
			// 
			// toolStripButtonBreak
			// 
			this.toolStripButtonBreak.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.toolStripButtonBreak.Image = global::LuaRemoteDebugger.Properties.Resources._break;
			this.toolStripButtonBreak.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.toolStripButtonBreak.Name = "toolStripButtonBreak";
			this.toolStripButtonBreak.Size = new System.Drawing.Size(23, 22);
			this.toolStripButtonBreak.Text = "Break";
			this.toolStripButtonBreak.Click += new System.EventHandler(this.toolStripButtonBreak_Click);
			// 
			// toolStripButtonStepInto
			// 
			this.toolStripButtonStepInto.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.toolStripButtonStepInto.Image = global::LuaRemoteDebugger.Properties.Resources.step_into;
			this.toolStripButtonStepInto.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.toolStripButtonStepInto.Name = "toolStripButtonStepInto";
			this.toolStripButtonStepInto.Size = new System.Drawing.Size(23, 22);
			this.toolStripButtonStepInto.Text = "toolStripButton1";
			this.toolStripButtonStepInto.ToolTipText = "Step Into";
			this.toolStripButtonStepInto.Click += new System.EventHandler(this.toolStripButtonStepInto_Click);
			// 
			// toolStripButtonStepOver
			// 
			this.toolStripButtonStepOver.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.toolStripButtonStepOver.Image = global::LuaRemoteDebugger.Properties.Resources.step_over;
			this.toolStripButtonStepOver.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.toolStripButtonStepOver.Name = "toolStripButtonStepOver";
			this.toolStripButtonStepOver.Size = new System.Drawing.Size(23, 22);
			this.toolStripButtonStepOver.Text = "toolStripButton1";
			this.toolStripButtonStepOver.ToolTipText = "Step Over";
			this.toolStripButtonStepOver.Click += new System.EventHandler(this.toolStripButtonStepOver_Click);
			// 
			// toolStripButtonStepOut
			// 
			this.toolStripButtonStepOut.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.toolStripButtonStepOut.Image = global::LuaRemoteDebugger.Properties.Resources.step_out;
			this.toolStripButtonStepOut.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.toolStripButtonStepOut.Name = "toolStripButtonStepOut";
			this.toolStripButtonStepOut.Size = new System.Drawing.Size(23, 22);
			this.toolStripButtonStepOut.Text = "toolStripButton1";
			this.toolStripButtonStepOut.ToolTipText = "Step Out";
			this.toolStripButtonStepOut.Click += new System.EventHandler(this.toolStripButtonStepOut_Click);
			// 
			// toolStripButtonBreakpoint
			// 
			this.toolStripButtonBreakpoint.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.toolStripButtonBreakpoint.Image = global::LuaRemoteDebugger.Properties.Resources.breakpoint;
			this.toolStripButtonBreakpoint.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.toolStripButtonBreakpoint.Name = "toolStripButtonBreakpoint";
			this.toolStripButtonBreakpoint.Size = new System.Drawing.Size(23, 22);
			this.toolStripButtonBreakpoint.Text = "Toggle Breakpoint";
			this.toolStripButtonBreakpoint.Click += new System.EventHandler(this.toolStripButtonBreakpoint_Click);
			// 
			// menuStrip
			// 
			this.menuStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.toolStripMenuFile,
			this.editToolStripMenuItem,
			this.debugToolStripMenuItem,
			this.helpToolStripMenuItem});
			this.menuStrip.Location = new System.Drawing.Point(0, 0);
			this.menuStrip.Name = "menuStrip";
			this.menuStrip.Size = new System.Drawing.Size(1276, 24);
			this.menuStrip.TabIndex = 15;
			this.menuStrip.Text = "Menu";
			// 
			// toolStripMenuFile
			// 
			this.toolStripMenuFile.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.setScriptsFolderToolStripMenu,
			this.toolStripSeparator2,
			this.connectToolStripMenuItem,
			this.disconnectToolStripMenuItem,
			this.toolStripSeparator3,
			this.findFileToolStripMenuItem,
			this.toolStripSeparator1,
			this.closeToolStripMenuItem,
			this.toolStripSeparator4,
			this.exitToolStripMenuItem});
			this.toolStripMenuFile.Name = "toolStripMenuFile";
			this.toolStripMenuFile.Size = new System.Drawing.Size(37, 20);
			this.toolStripMenuFile.Text = "File";
			// 
			// setScriptsFolderToolStripMenu
			// 
			this.setScriptsFolderToolStripMenu.Name = "setScriptsFolderToolStripMenu";
			this.setScriptsFolderToolStripMenu.Size = new System.Drawing.Size(173, 22);
			this.setScriptsFolderToolStripMenu.Text = "Set Scripts Folder...";
			this.setScriptsFolderToolStripMenu.Click += new System.EventHandler(this.setScriptsFolderToolStripMenu_Click);
			// 
			// toolStripSeparator2
			// 
			this.toolStripSeparator2.Name = "toolStripSeparator2";
			this.toolStripSeparator2.Size = new System.Drawing.Size(170, 6);
			// 
			// connectToolStripMenuItem
			// 
			this.connectToolStripMenuItem.Name = "connectToolStripMenuItem";
			this.connectToolStripMenuItem.Size = new System.Drawing.Size(173, 22);
			this.connectToolStripMenuItem.Text = "Connect...";
			this.connectToolStripMenuItem.Click += new System.EventHandler(this.connectToolStripMenuItem_Click);
			// 
			// disconnectToolStripMenuItem
			// 
			this.disconnectToolStripMenuItem.Name = "disconnectToolStripMenuItem";
			this.disconnectToolStripMenuItem.Size = new System.Drawing.Size(173, 22);
			this.disconnectToolStripMenuItem.Text = "Disconnect";
			this.disconnectToolStripMenuItem.Click += new System.EventHandler(this.disconnectToolStripMenuItem_Click);
			// 
			// toolStripSeparator3
			// 
			this.toolStripSeparator3.Name = "toolStripSeparator3";
			this.toolStripSeparator3.Size = new System.Drawing.Size(170, 6);
			// 
			// findFileToolStripMenuItem
			// 
			this.findFileToolStripMenuItem.Name = "findFileToolStripMenuItem";
			this.findFileToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.O)));
			this.findFileToolStripMenuItem.Size = new System.Drawing.Size(173, 22);
			this.findFileToolStripMenuItem.Text = "Find File...";
			this.findFileToolStripMenuItem.Click += new System.EventHandler(this.findFileToolStripMenuItem_Click);
			// 
			// toolStripSeparator1
			// 
			this.toolStripSeparator1.Name = "toolStripSeparator1";
			this.toolStripSeparator1.Size = new System.Drawing.Size(170, 6);
			// 
			// closeToolStripMenuItem
			// 
			this.closeToolStripMenuItem.Name = "closeToolStripMenuItem";
			this.closeToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.W)));
			this.closeToolStripMenuItem.Size = new System.Drawing.Size(173, 22);
			this.closeToolStripMenuItem.Text = "Close";
			this.closeToolStripMenuItem.Click += new System.EventHandler(this.closeToolStripMenuItem_Click);
			// 
			// toolStripSeparator4
			// 
			this.toolStripSeparator4.Name = "toolStripSeparator4";
			this.toolStripSeparator4.Size = new System.Drawing.Size(170, 6);
			// 
			// exitToolStripMenuItem
			// 
			this.exitToolStripMenuItem.Name = "exitToolStripMenuItem";
			this.exitToolStripMenuItem.Size = new System.Drawing.Size(173, 22);
			this.exitToolStripMenuItem.Text = "Exit";
			this.exitToolStripMenuItem.Click += new System.EventHandler(this.exitToolStripMenuItem_Click);
			// 
			// editToolStripMenuItem
			// 
			this.editToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.findToolStripMenuItem,
			this.goToToolStripMenuItem});
			this.editToolStripMenuItem.Name = "editToolStripMenuItem";
			this.editToolStripMenuItem.Size = new System.Drawing.Size(39, 20);
			this.editToolStripMenuItem.Text = "Edit";
			// 
			// findToolStripMenuItem
			// 
			this.findToolStripMenuItem.Name = "findToolStripMenuItem";
			this.findToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.F)));
			this.findToolStripMenuItem.Size = new System.Drawing.Size(157, 22);
			this.findToolStripMenuItem.Text = "Find...";
			this.findToolStripMenuItem.Click += new System.EventHandler(this.findToolStripMenuItem_Click);
			// 
			// goToToolStripMenuItem
			// 
			this.goToToolStripMenuItem.Name = "goToToolStripMenuItem";
			this.goToToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.G)));
			this.goToToolStripMenuItem.Size = new System.Drawing.Size(157, 22);
			this.goToToolStripMenuItem.Text = "Go To...";
			this.goToToolStripMenuItem.Click += new System.EventHandler(this.goToToolStripMenuItem_Click);
			// 
			// debugToolStripMenuItem
			// 
			this.debugToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.continueToolStripMenuItem,
			this.breakToolStripMenuItem,
			this.toolStripSeparator5,
			this.stepIntoToolStripMenuItem,
			this.stepOverToolStripMenuItem,
			this.stepOutToolStripMenuItem,
			this.toolStripSeparator6,
			this.toggleBreakpointToolStripMenuItem,
			this.toolStripSeparator7,
			this.enableCCallstackToolStripMenuItem});
			this.debugToolStripMenuItem.Name = "debugToolStripMenuItem";
			this.debugToolStripMenuItem.Size = new System.Drawing.Size(54, 20);
			this.debugToolStripMenuItem.Text = "Debug";
			// 
			// continueToolStripMenuItem
			// 
			this.continueToolStripMenuItem.Name = "continueToolStripMenuItem";
			this.continueToolStripMenuItem.ShortcutKeys = System.Windows.Forms.Keys.F5;
			this.continueToolStripMenuItem.Size = new System.Drawing.Size(191, 22);
			this.continueToolStripMenuItem.Text = "Continue";
			this.continueToolStripMenuItem.Click += new System.EventHandler(this.continueToolStripMenuItem_Click);
			// 
			// breakToolStripMenuItem
			// 
			this.breakToolStripMenuItem.Name = "breakToolStripMenuItem";
			this.breakToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)(((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.Alt) 
			| System.Windows.Forms.Keys.Pause)));
			this.breakToolStripMenuItem.Size = new System.Drawing.Size(191, 22);
			this.breakToolStripMenuItem.Text = "Break";
			this.breakToolStripMenuItem.Click += new System.EventHandler(this.breakToolStripMenuItem_Click);
			// 
			// toolStripSeparator5
			// 
			this.toolStripSeparator5.Name = "toolStripSeparator5";
			this.toolStripSeparator5.Size = new System.Drawing.Size(188, 6);
			// 
			// stepIntoToolStripMenuItem
			// 
			this.stepIntoToolStripMenuItem.Name = "stepIntoToolStripMenuItem";
			this.stepIntoToolStripMenuItem.ShortcutKeys = System.Windows.Forms.Keys.F11;
			this.stepIntoToolStripMenuItem.Size = new System.Drawing.Size(191, 22);
			this.stepIntoToolStripMenuItem.Text = "Step Into";
			this.stepIntoToolStripMenuItem.Click += new System.EventHandler(this.stepIntoToolStripMenuItem_Click);
			// 
			// stepOverToolStripMenuItem
			// 
			this.stepOverToolStripMenuItem.Name = "stepOverToolStripMenuItem";
			this.stepOverToolStripMenuItem.ShortcutKeys = System.Windows.Forms.Keys.F10;
			this.stepOverToolStripMenuItem.Size = new System.Drawing.Size(191, 22);
			this.stepOverToolStripMenuItem.Text = "Step Over";
			this.stepOverToolStripMenuItem.Click += new System.EventHandler(this.stepOverToolStripMenuItem_Click);
			// 
			// stepOutToolStripMenuItem
			// 
			this.stepOutToolStripMenuItem.Name = "stepOutToolStripMenuItem";
			this.stepOutToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Shift | System.Windows.Forms.Keys.F11)));
			this.stepOutToolStripMenuItem.Size = new System.Drawing.Size(191, 22);
			this.stepOutToolStripMenuItem.Text = "Step Out";
			this.stepOutToolStripMenuItem.Click += new System.EventHandler(this.stepOutToolStripMenuItem_Click);
			// 
			// toolStripSeparator6
			// 
			this.toolStripSeparator6.Name = "toolStripSeparator6";
			this.toolStripSeparator6.Size = new System.Drawing.Size(188, 6);
			// 
			// toggleBreakpointToolStripMenuItem
			// 
			this.toggleBreakpointToolStripMenuItem.Name = "toggleBreakpointToolStripMenuItem";
			this.toggleBreakpointToolStripMenuItem.ShortcutKeys = System.Windows.Forms.Keys.F9;
			this.toggleBreakpointToolStripMenuItem.Size = new System.Drawing.Size(191, 22);
			this.toggleBreakpointToolStripMenuItem.Text = "Toggle Breakpoint";
			this.toggleBreakpointToolStripMenuItem.Click += new System.EventHandler(this.toggleBreakpointToolStripMenuItem_Click);
			// 
			// toolStripSeparator7
			// 
			this.toolStripSeparator7.Name = "toolStripSeparator7";
			this.toolStripSeparator7.Size = new System.Drawing.Size(188, 6);
			// 
			// enableCCallstackToolStripMenuItem
			// 
			this.enableCCallstackToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.disabledToolStripMenuItem,
			this.condensedToolStripMenuItem,
			this.fullToolStripMenuItem});
			this.enableCCallstackToolStripMenuItem.Name = "enableCCallstackToolStripMenuItem";
			this.enableCCallstackToolStripMenuItem.Size = new System.Drawing.Size(191, 22);
			this.enableCCallstackToolStripMenuItem.Text = "C++ Callstack";
			// 
			// disabledToolStripMenuItem
			// 
			this.disabledToolStripMenuItem.Name = "disabledToolStripMenuItem";
			this.disabledToolStripMenuItem.Size = new System.Drawing.Size(134, 22);
			this.disabledToolStripMenuItem.Text = "Disabled";
			this.disabledToolStripMenuItem.Click += new System.EventHandler(this.disabledToolStripMenuItem_Click);
			// 
			// condensedToolStripMenuItem
			// 
			this.condensedToolStripMenuItem.Name = "condensedToolStripMenuItem";
			this.condensedToolStripMenuItem.Size = new System.Drawing.Size(134, 22);
			this.condensedToolStripMenuItem.Text = "Condensed";
			this.condensedToolStripMenuItem.Click += new System.EventHandler(this.condensedToolStripMenuItem_Click);
			// 
			// fullToolStripMenuItem
			// 
			this.fullToolStripMenuItem.Name = "fullToolStripMenuItem";
			this.fullToolStripMenuItem.Size = new System.Drawing.Size(134, 22);
			this.fullToolStripMenuItem.Text = "Full";
			this.fullToolStripMenuItem.Click += new System.EventHandler(this.fullToolStripMenuItem_Click);
			// 
			// helpToolStripMenuItem
			// 
			this.helpToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.aboutLuaRemoteDebuggerToolStripMenuItem});
			this.helpToolStripMenuItem.Name = "helpToolStripMenuItem";
			this.helpToolStripMenuItem.Size = new System.Drawing.Size(44, 20);
			this.helpToolStripMenuItem.Text = "Help";
			// 
			// aboutLuaRemoteDebuggerToolStripMenuItem
			// 
			this.aboutLuaRemoteDebuggerToolStripMenuItem.Name = "aboutLuaRemoteDebuggerToolStripMenuItem";
			this.aboutLuaRemoteDebuggerToolStripMenuItem.Size = new System.Drawing.Size(237, 22);
			this.aboutLuaRemoteDebuggerToolStripMenuItem.Text = "About Lua Remote Debugger...";
			this.aboutLuaRemoteDebuggerToolStripMenuItem.Click += new System.EventHandler(this.aboutLuaRemoteDebuggerToolStripMenuItem_Click);
			// 
			// listViewCallStack
			// 
			this.listViewCallStack.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
			this.columnHeaderDescription,
			this.columnHeaderLine,
			this.columnHeaderSource});
			this.listViewCallStack.Dock = System.Windows.Forms.DockStyle.Fill;
			this.listViewCallStack.FullRowSelect = true;
			this.listViewCallStack.HeaderStyle = System.Windows.Forms.ColumnHeaderStyle.Nonclickable;
			this.listViewCallStack.LabelWrap = false;
			this.listViewCallStack.Location = new System.Drawing.Point(3, 3);
			this.listViewCallStack.Name = "listViewCallStack";
			this.listViewCallStack.Size = new System.Drawing.Size(613, 193);
			this.listViewCallStack.TabIndex = 0;
			this.listViewCallStack.UseCompatibleStateImageBehavior = false;
			this.listViewCallStack.View = System.Windows.Forms.View.Details;
			this.listViewCallStack.ItemDoubleClicked += new System.EventHandler<LuaRemoteDebuggerControls.ItemDoubleClickEventArgs>(this.listViewCallStack_ItemDoubleClicked);
			// 
			// columnHeaderDescription
			// 
			this.columnHeaderDescription.Text = "Function";
			this.columnHeaderDescription.Width = 287;
			// 
			// columnHeaderLine
			// 
			this.columnHeaderLine.Text = "Line";
			this.columnHeaderLine.Width = 48;
			// 
			// columnHeaderSource
			// 
			this.columnHeaderSource.Text = "Source";
			this.columnHeaderSource.Width = 273;
			// 
			// listViewBreakpoints
			// 
			this.listViewBreakpoints.CheckBoxes = true;
			this.listViewBreakpoints.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
			this.columnHeaderName});
			this.listViewBreakpoints.Dock = System.Windows.Forms.DockStyle.Fill;
			this.listViewBreakpoints.HeaderStyle = System.Windows.Forms.ColumnHeaderStyle.Nonclickable;
			this.listViewBreakpoints.LabelWrap = false;
			this.listViewBreakpoints.Location = new System.Drawing.Point(3, 3);
			this.listViewBreakpoints.Name = "listViewBreakpoints";
			this.listViewBreakpoints.Size = new System.Drawing.Size(613, 193);
			this.listViewBreakpoints.TabIndex = 0;
			this.listViewBreakpoints.UseCompatibleStateImageBehavior = false;
			this.listViewBreakpoints.View = System.Windows.Forms.View.Details;
			this.listViewBreakpoints.ItemDoubleClicked += new System.EventHandler<LuaRemoteDebuggerControls.ItemDoubleClickEventArgs>(this.listViewBreakpoints_ItemDoubleClicked);
			this.listViewBreakpoints.ItemChecked += new System.Windows.Forms.ItemCheckedEventHandler(this.listViewBreakpoints_ItemChecked);
			this.listViewBreakpoints.KeyDown += new System.Windows.Forms.KeyEventHandler(this.listViewBreakpoints_KeyDown);
			// 
			// columnHeaderName
			// 
			this.columnHeaderName.Text = "Name";
			this.columnHeaderName.Width = 800;
			// 
			// MainForm
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(1276, 724);
			this.Controls.Add(this.toolStripContainer);
			this.Controls.Add(this.menuStrip);
			this.MainMenuStrip = this.menuStrip;
			this.Name = "MainForm";
			this.Text = "Lua Remote Debugger";
			this.toolStripContainer.ContentPanel.ResumeLayout(false);
			this.toolStripContainer.TopToolStripPanel.ResumeLayout(false);
			this.toolStripContainer.TopToolStripPanel.PerformLayout();
			this.toolStripContainer.ResumeLayout(false);
			this.toolStripContainer.PerformLayout();
			this.splitContainerHorizontal.Panel1.ResumeLayout(false);
			this.splitContainerHorizontal.Panel2.ResumeLayout(false);
			this.splitContainerHorizontal.ResumeLayout(false);
			this.splitContainerVertical.Panel1.ResumeLayout(false);
			this.splitContainerVertical.Panel2.ResumeLayout(false);
			this.splitContainerVertical.ResumeLayout(false);
			this.splitContainer1.Panel1.ResumeLayout(false);
			this.splitContainer1.Panel2.ResumeLayout(false);
			this.splitContainer1.ResumeLayout(false);
			this.tabControlBottom.ResumeLayout(false);
			this.tabPageCallstack.ResumeLayout(false);
			this.tabPageBreakpoints.ResumeLayout(false);
			this.tabPageLog.ResumeLayout(false);
			this.tabControlVars.ResumeLayout(false);
			this.tabPageLocals.ResumeLayout(false);
			this.tabPageWatch.ResumeLayout(false);
			this.toolStripDebug.ResumeLayout(false);
			this.toolStripDebug.PerformLayout();
			this.menuStrip.ResumeLayout(false);
			this.menuStrip.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.RichTextBox richTextBoxLog;
		private System.Windows.Forms.TreeView treeViewFiles;
		private System.Windows.Forms.ImageList imageListTreeView;
		private System.Windows.Forms.SplitContainer splitContainerHorizontal;
		private System.Windows.Forms.SplitContainer splitContainerVertical;
		private System.Windows.Forms.TabControl tabControlBottom;
		private System.Windows.Forms.TabPage tabPageLog;
		private System.Windows.Forms.TabPage tabPageCallstack;
		private System.Windows.Forms.TabPage tabPageLocals;
		private LuaRemoteDebuggerControls.DoubleClickListView listViewCallStack;
		private System.Windows.Forms.ColumnHeader columnHeaderSource;
		private System.Windows.Forms.ColumnHeader columnHeaderLine;
		private System.Windows.Forms.ColumnHeader columnHeaderDescription;
		private System.Windows.Forms.ToolStripContainer toolStripContainer;
		private System.Windows.Forms.ToolStrip toolStripDebug;
		private System.Windows.Forms.ToolStripButton toolStripButtonBreakpoint;
		private System.Windows.Forms.ToolStripButton toolStripButtonContinue;
		private System.Windows.Forms.ToolStripButton toolStripButtonBreak;
		private System.Windows.Forms.ToolStripButton toolStripButtonStepInto;
		private System.Windows.Forms.ToolStripButton toolStripButtonStepOver;
		private System.Windows.Forms.ToolStripButton toolStripButtonStepOut;
		private System.Windows.Forms.MenuStrip menuStrip;
		private System.Windows.Forms.ToolStripMenuItem toolStripMenuFile;
		private System.Windows.Forms.ToolStripMenuItem setScriptsFolderToolStripMenu;
		private System.Windows.Forms.ToolStripMenuItem connectToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem disconnectToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem exitToolStripMenuItem;
		private System.Windows.Forms.TabControl tabControlSourceFiles;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator3;
		private System.Windows.Forms.ToolStripMenuItem closeToolStripMenuItem;
		private System.Windows.Forms.TabPage tabPageBreakpoints;
		private LuaRemoteDebuggerControls.DoubleClickListView listViewBreakpoints;
		private System.Windows.Forms.ColumnHeader columnHeaderName;
		private System.Windows.Forms.ToolStripMenuItem findFileToolStripMenuItem;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator4;
		private System.Windows.Forms.ToolStripMenuItem editToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem goToToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem debugToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem continueToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem breakToolStripMenuItem;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator5;
		private System.Windows.Forms.ToolStripMenuItem stepIntoToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem stepOverToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem stepOutToolStripMenuItem;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator6;
		private System.Windows.Forms.ToolStripMenuItem toggleBreakpointToolStripMenuItem;
		private Aga.Controls.Tree.TreeViewAdv treeViewAdvLocals;
		private Aga.Controls.Tree.NodeControls.NodeTextBox watchKey;
		private Aga.Controls.Tree.NodeControls.NodeTextBox watchValue;
		private Aga.Controls.Tree.NodeControls.NodeTextBox watchKeyType;
		private Aga.Controls.Tree.NodeControls.NodeTextBox watchValueType;
		private Aga.Controls.Tree.TreeColumn treeColumnKey;
		private Aga.Controls.Tree.TreeColumn treeColumnValue;
		private Aga.Controls.Tree.TreeColumn treeColumnKeyType;
		private Aga.Controls.Tree.TreeColumn treeColumnValueType;
		private System.Windows.Forms.ToolStripMenuItem helpToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem aboutLuaRemoteDebuggerToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem findToolStripMenuItem;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator7;
		private System.Windows.Forms.TabPage tabPageWatch;
		private Aga.Controls.Tree.TreeViewAdv treeViewAdvWatch;
		private Aga.Controls.Tree.NodeControls.NodeTextBox localsKey;
		private Aga.Controls.Tree.NodeControls.NodeTextBox localsValue;
		private Aga.Controls.Tree.NodeControls.NodeTextBox localsKeyType;
		private Aga.Controls.Tree.NodeControls.NodeTextBox localsValueType;
		private System.Windows.Forms.ToolStripMenuItem enableCCallstackToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem fullToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem condensedToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem disabledToolStripMenuItem;
		private System.Windows.Forms.SplitContainer splitContainer1;
		private System.Windows.Forms.TabControl tabControlVars;
	}
}


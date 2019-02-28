namespace ProfVis
{
    partial class ThreadAnalyzer
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
            this.folderBrowserDialog1 = new System.Windows.Forms.FolderBrowserDialog();
            this.pickFolder = new System.Windows.Forms.Button();
            this.treeView1 = new System.Windows.Forms.TreeView();
            this.progressBar1 = new System.Windows.Forms.ProgressBar();
            this.stopDepth = new System.Windows.Forms.NumericUpDown();
            this.openInAnotherApp = new System.Windows.Forms.Button();
            this.stopBreath = new System.Windows.Forms.NumericUpDown();
            this.folderPath = new System.Windows.Forms.TextBox();
            this.start = new System.Windows.Forms.Button();
            this.treeView2 = new System.Windows.Forms.TreeView();
            this.region = new System.Windows.Forms.TextBox();
            this.pickerMode = new System.Windows.Forms.CheckBox();
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.loadSession = new System.Windows.Forms.Button();
            this.sumpSession = new System.Windows.Forms.Button();
            this.threadIdx = new System.Windows.Forms.NumericUpDown();
            this.label4 = new System.Windows.Forms.Label();
            this.filterLabel = new System.Windows.Forms.Label();
            this.inverseFilter = new System.Windows.Forms.CheckBox();
            this.label3 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.label1 = new System.Windows.Forms.Label();
            this.button1 = new System.Windows.Forms.Button();
            this.clearPickMode = new System.Windows.Forms.Button();
            this.splitContainer2 = new System.Windows.Forms.SplitContainer();
            this.openFileDialog1 = new System.Windows.Forms.OpenFileDialog();
            ((System.ComponentModel.ISupportInitialize)(this.stopDepth)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.stopBreath)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.threadIdx)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer2)).BeginInit();
            this.splitContainer2.Panel1.SuspendLayout();
            this.splitContainer2.Panel2.SuspendLayout();
            this.splitContainer2.SuspendLayout();
            this.SuspendLayout();
            // 
            // pickFolder
            // 
            this.pickFolder.Location = new System.Drawing.Point(12, 4);
            this.pickFolder.Name = "pickFolder";
            this.pickFolder.Size = new System.Drawing.Size(89, 23);
            this.pickFolder.TabIndex = 0;
            this.pickFolder.Text = "Pick Folder";
            this.pickFolder.UseVisualStyleBackColor = true;
            this.pickFolder.Click += new System.EventHandler(this.PickFolder_Click);
            // 
            // treeView1
            // 
            this.treeView1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.treeView1.HideSelection = false;
            this.treeView1.Location = new System.Drawing.Point(0, 0);
            this.treeView1.Name = "treeView1";
            this.treeView1.Size = new System.Drawing.Size(696, 469);
            this.treeView1.TabIndex = 1;
            this.treeView1.MouseClick += new System.Windows.Forms.MouseEventHandler(this.treeView1_MouseClick);
            // 
            // progressBar1
            // 
            this.progressBar1.Dock = System.Windows.Forms.DockStyle.Bottom;
            this.progressBar1.Location = new System.Drawing.Point(0, 570);
            this.progressBar1.Name = "progressBar1";
            this.progressBar1.Size = new System.Drawing.Size(1004, 23);
            this.progressBar1.TabIndex = 2;
            // 
            // stopDepth
            // 
            this.stopDepth.Location = new System.Drawing.Point(284, 36);
            this.stopDepth.Minimum = new decimal(new int[] {
            1,
            0,
            0,
            0});
            this.stopDepth.Name = "stopDepth";
            this.stopDepth.Size = new System.Drawing.Size(45, 20);
            this.stopDepth.TabIndex = 3;
            this.stopDepth.Value = new decimal(new int[] {
            5,
            0,
            0,
            0});
            // 
            // openInAnotherApp
            // 
            this.openInAnotherApp.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.openInAnotherApp.Location = new System.Drawing.Point(720, 32);
            this.openInAnotherApp.Name = "openInAnotherApp";
            this.openInAnotherApp.Size = new System.Drawing.Size(262, 23);
            this.openInAnotherApp.TabIndex = 4;
            this.openInAnotherApp.Text = "Open Selection In New App";
            this.openInAnotherApp.UseVisualStyleBackColor = true;
            this.openInAnotherApp.Click += new System.EventHandler(this.OpenInNewApp_Click);
            // 
            // stopBreath
            // 
            this.stopBreath.Location = new System.Drawing.Point(414, 36);
            this.stopBreath.Minimum = new decimal(new int[] {
            1,
            0,
            0,
            0});
            this.stopBreath.Name = "stopBreath";
            this.stopBreath.Size = new System.Drawing.Size(45, 20);
            this.stopBreath.TabIndex = 5;
            this.stopBreath.Value = new decimal(new int[] {
            1,
            0,
            0,
            0});
            // 
            // folderPath
            // 
            this.folderPath.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.folderPath.Location = new System.Drawing.Point(107, 6);
            this.folderPath.Name = "folderPath";
            this.folderPath.Size = new System.Drawing.Size(375, 20);
            this.folderPath.TabIndex = 6;
            // 
            // start
            // 
            this.start.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.start.Location = new System.Drawing.Point(488, 4);
            this.start.Name = "start";
            this.start.Size = new System.Drawing.Size(112, 23);
            this.start.TabIndex = 7;
            this.start.Text = "Generate DB";
            this.start.UseVisualStyleBackColor = true;
            this.start.Click += new System.EventHandler(this.Start_Click);
            // 
            // treeView2
            // 
            this.treeView2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.treeView2.Location = new System.Drawing.Point(0, 0);
            this.treeView2.Name = "treeView2";
            this.treeView2.Size = new System.Drawing.Size(304, 469);
            this.treeView2.TabIndex = 8;
            // 
            // region
            // 
            this.region.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.region.Location = new System.Drawing.Point(107, 64);
            this.region.Name = "region";
            this.region.ReadOnly = true;
            this.region.Size = new System.Drawing.Size(583, 20);
            this.region.TabIndex = 9;
            // 
            // pickerMode
            // 
            this.pickerMode.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.pickerMode.Appearance = System.Windows.Forms.Appearance.Button;
            this.pickerMode.AutoSize = true;
            this.pickerMode.Location = new System.Drawing.Point(720, 62);
            this.pickerMode.Name = "pickerMode";
            this.pickerMode.Size = new System.Drawing.Size(82, 23);
            this.pickerMode.TabIndex = 10;
            this.pickerMode.Text = "Pick Function";
            this.pickerMode.UseVisualStyleBackColor = true;
            this.pickerMode.CheckedChanged += new System.EventHandler(this.Picker_CheckedChanged);
            // 
            // splitContainer1
            // 
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.FixedPanel = System.Windows.Forms.FixedPanel.Panel1;
            this.splitContainer1.Location = new System.Drawing.Point(0, 0);
            this.splitContainer1.Name = "splitContainer1";
            this.splitContainer1.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this.loadSession);
            this.splitContainer1.Panel1.Controls.Add(this.sumpSession);
            this.splitContainer1.Panel1.Controls.Add(this.threadIdx);
            this.splitContainer1.Panel1.Controls.Add(this.label4);
            this.splitContainer1.Panel1.Controls.Add(this.filterLabel);
            this.splitContainer1.Panel1.Controls.Add(this.inverseFilter);
            this.splitContainer1.Panel1.Controls.Add(this.label3);
            this.splitContainer1.Panel1.Controls.Add(this.label2);
            this.splitContainer1.Panel1.Controls.Add(this.label1);
            this.splitContainer1.Panel1.Controls.Add(this.button1);
            this.splitContainer1.Panel1.Controls.Add(this.clearPickMode);
            this.splitContainer1.Panel1.Controls.Add(this.start);
            this.splitContainer1.Panel1.Controls.Add(this.pickerMode);
            this.splitContainer1.Panel1.Controls.Add(this.pickFolder);
            this.splitContainer1.Panel1.Controls.Add(this.region);
            this.splitContainer1.Panel1.Controls.Add(this.stopDepth);
            this.splitContainer1.Panel1.Controls.Add(this.openInAnotherApp);
            this.splitContainer1.Panel1.Controls.Add(this.folderPath);
            this.splitContainer1.Panel1.Controls.Add(this.stopBreath);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.splitContainer2);
            this.splitContainer1.Size = new System.Drawing.Size(1004, 570);
            this.splitContainer1.SplitterDistance = 97;
            this.splitContainer1.TabIndex = 11;
            // 
            // loadSession
            // 
            this.loadSession.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.loadSession.Location = new System.Drawing.Point(861, 4);
            this.loadSession.Name = "loadSession";
            this.loadSession.Size = new System.Drawing.Size(121, 23);
            this.loadSession.TabIndex = 21;
            this.loadSession.Text = "Load Session";
            this.loadSession.UseVisualStyleBackColor = true;
            this.loadSession.Click += new System.EventHandler(this.LoadSession_Click);
            // 
            // sumpSession
            // 
            this.sumpSession.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.sumpSession.Location = new System.Drawing.Point(720, 4);
            this.sumpSession.Name = "sumpSession";
            this.sumpSession.Size = new System.Drawing.Size(137, 23);
            this.sumpSession.TabIndex = 20;
            this.sumpSession.Text = "Dump Session";
            this.sumpSession.UseVisualStyleBackColor = true;
            this.sumpSession.Click += new System.EventHandler(this.DumpSession_Click);
            // 
            // threadIdx
            // 
            this.threadIdx.Location = new System.Drawing.Point(146, 36);
            this.threadIdx.Name = "threadIdx";
            this.threadIdx.Size = new System.Drawing.Size(45, 20);
            this.threadIdx.TabIndex = 19;
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(12, 38);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(127, 13);
            this.label4.TabIndex = 18;
            this.label4.Text = "Analyze thread with index";
            // 
            // filterLabel
            // 
            this.filterLabel.Location = new System.Drawing.Point(9, 67);
            this.filterLabel.Name = "filterLabel";
            this.filterLabel.Size = new System.Drawing.Size(98, 13);
            this.filterLabel.TabIndex = 17;
            this.filterLabel.Text = "Filter: Include only";
            this.filterLabel.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // inverseFilter
            // 
            this.inverseFilter.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.inverseFilter.AutoSize = true;
            this.inverseFilter.Location = new System.Drawing.Point(894, 67);
            this.inverseFilter.Name = "inverseFilter";
            this.inverseFilter.Size = new System.Drawing.Size(86, 17);
            this.inverseFilter.TabIndex = 16;
            this.inverseFilter.Text = "Inverse Filter";
            this.inverseFilter.UseVisualStyleBackColor = true;
            this.inverseFilter.CheckedChanged += new System.EventHandler(this.inverseFilter_CheckedChanged);
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(465, 38);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(91, 13);
            this.label3.TabIndex = 15;
            this.label3.Text = "slowest functions!";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(335, 38);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(73, 13);
            this.label2.TabIndex = 14;
            this.label2.Text = ". Concider top";
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(197, 38);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(81, 13);
            this.label1.TabIndex = 13;
            this.label1.Text = "until depth level";
            // 
            // button1
            // 
            this.button1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.button1.Location = new System.Drawing.Point(606, 4);
            this.button1.Name = "button1";
            this.button1.Size = new System.Drawing.Size(84, 23);
            this.button1.TabIndex = 12;
            this.button1.Text = "Analyze";
            this.button1.UseVisualStyleBackColor = true;
            this.button1.Click += new System.EventHandler(this.AnalyzeLoadedFiles_Click);
            // 
            // clearPickMode
            // 
            this.clearPickMode.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.clearPickMode.Location = new System.Drawing.Point(808, 62);
            this.clearPickMode.Name = "clearPickMode";
            this.clearPickMode.Size = new System.Drawing.Size(82, 23);
            this.clearPickMode.TabIndex = 11;
            this.clearPickMode.Text = "Clear Function";
            this.clearPickMode.UseVisualStyleBackColor = true;
            this.clearPickMode.Click += new System.EventHandler(this.ClearPickMode_Click);
            // 
            // splitContainer2
            // 
            this.splitContainer2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer2.Location = new System.Drawing.Point(0, 0);
            this.splitContainer2.Name = "splitContainer2";
            // 
            // splitContainer2.Panel1
            // 
            this.splitContainer2.Panel1.Controls.Add(this.treeView1);
            // 
            // splitContainer2.Panel2
            // 
            this.splitContainer2.Panel2.Controls.Add(this.treeView2);
            this.splitContainer2.Size = new System.Drawing.Size(1004, 469);
            this.splitContainer2.SplitterDistance = 696;
            this.splitContainer2.TabIndex = 0;
            // 
            // openFileDialog1
            // 
            this.openFileDialog1.FileName = "openFileDialog1";
            this.openFileDialog1.Filter = "xml|*.xml";
            // 
            // ThreadAnalyzer
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1004, 593);
            this.Controls.Add(this.splitContainer1);
            this.Controls.Add(this.progressBar1);
            this.Name = "ThreadAnalyzer";
            this.Text = "ThreadAnalyzer";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.ScanTool_FormClosed);
            ((System.ComponentModel.ISupportInitialize)(this.stopDepth)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.stopBreath)).EndInit();
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel1.PerformLayout();
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.threadIdx)).EndInit();
            this.splitContainer2.Panel1.ResumeLayout(false);
            this.splitContainer2.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer2)).EndInit();
            this.splitContainer2.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion
        private System.Windows.Forms.FolderBrowserDialog folderBrowserDialog1;
        private System.Windows.Forms.Button pickFolder;
        private System.Windows.Forms.TreeView treeView1;
        private System.Windows.Forms.ProgressBar progressBar1;
        private System.Windows.Forms.NumericUpDown stopDepth;
        private System.Windows.Forms.Button openInAnotherApp;
        private System.Windows.Forms.NumericUpDown stopBreath;
        private System.Windows.Forms.TextBox folderPath;
        private System.Windows.Forms.Button start;
        private System.Windows.Forms.TreeView treeView2;
        private System.Windows.Forms.TextBox region;
        private System.Windows.Forms.CheckBox pickerMode;
        private System.Windows.Forms.SplitContainer splitContainer1;
        private System.Windows.Forms.SplitContainer splitContainer2;
        private System.Windows.Forms.Button clearPickMode;
        private System.Windows.Forms.Button button1;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.CheckBox inverseFilter;
        private System.Windows.Forms.Label filterLabel;
        private System.Windows.Forms.NumericUpDown threadIdx;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Button loadSession;
        private System.Windows.Forms.Button sumpSession;
        private System.Windows.Forms.OpenFileDialog openFileDialog1;
    }
}
// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace LuaRemoteDebugger
{
	partial class FindFileDialog
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
			this.listViewFiles = new System.Windows.Forms.ListView();
			this.columnHeaderFile = new System.Windows.Forms.ColumnHeader();
			this.columnHeaderPath = new System.Windows.Forms.ColumnHeader();
			this.textBoxFilter = new System.Windows.Forms.TextBox();
			this.buttonOK = new System.Windows.Forms.Button();
			this.buttonCancel = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// listViewFiles
			// 
			this.listViewFiles.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
									| System.Windows.Forms.AnchorStyles.Left)
									| System.Windows.Forms.AnchorStyles.Right)));
			this.listViewFiles.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeaderFile,
            this.columnHeaderPath});
			this.listViewFiles.FullRowSelect = true;
			this.listViewFiles.HideSelection = false;
			this.listViewFiles.Location = new System.Drawing.Point(12, 12);
			this.listViewFiles.MultiSelect = false;
			this.listViewFiles.Name = "listViewFiles";
			this.listViewFiles.Size = new System.Drawing.Size(1112, 222);
			this.listViewFiles.TabIndex = 3;
			this.listViewFiles.UseCompatibleStateImageBehavior = false;
			this.listViewFiles.View = System.Windows.Forms.View.Details;
			this.listViewFiles.DoubleClick += new System.EventHandler(this.listViewFiles_DoubleClick);
			// 
			// columnHeaderFile
			// 
			this.columnHeaderFile.Text = "File";
			this.columnHeaderFile.Width = 200;
			// 
			// columnHeaderPath
			// 
			this.columnHeaderPath.Text = "Path";
			this.columnHeaderPath.Width = 880;
			// 
			// textBoxFilter
			// 
			this.textBoxFilter.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)
									| System.Windows.Forms.AnchorStyles.Right)));
			this.textBoxFilter.Location = new System.Drawing.Point(12, 240);
			this.textBoxFilter.Name = "textBoxFilter";
			this.textBoxFilter.Size = new System.Drawing.Size(950, 20);
			this.textBoxFilter.TabIndex = 0;
			this.textBoxFilter.TextChanged += new System.EventHandler(this.textBoxFilter_TextChanged);
			this.textBoxFilter.KeyDown += new System.Windows.Forms.KeyEventHandler(this.textBoxFilter_KeyDown);
			// 
			// buttonOK
			// 
			this.buttonOK.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.buttonOK.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.buttonOK.Location = new System.Drawing.Point(968, 238);
			this.buttonOK.Name = "buttonOK";
			this.buttonOK.Size = new System.Drawing.Size(75, 23);
			this.buttonOK.TabIndex = 1;
			this.buttonOK.Text = "OK";
			this.buttonOK.UseVisualStyleBackColor = true;
			// 
			// buttonCancel
			// 
			this.buttonCancel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.buttonCancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.buttonCancel.Location = new System.Drawing.Point(1049, 238);
			this.buttonCancel.Name = "buttonCancel";
			this.buttonCancel.Size = new System.Drawing.Size(75, 23);
			this.buttonCancel.TabIndex = 2;
			this.buttonCancel.Text = "Cancel";
			this.buttonCancel.UseVisualStyleBackColor = true;
			// 
			// FindFileDialog
			// 
			this.AcceptButton = this.buttonOK;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.buttonCancel;
			this.ClientSize = new System.Drawing.Size(1136, 272);
			this.Controls.Add(this.buttonCancel);
			this.Controls.Add(this.buttonOK);
			this.Controls.Add(this.textBoxFilter);
			this.Controls.Add(this.listViewFiles);
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "FindFileDialog";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Find File";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.ListView listViewFiles;
		private System.Windows.Forms.TextBox textBoxFilter;
		private System.Windows.Forms.Button buttonOK;
		private System.Windows.Forms.Button buttonCancel;
		private System.Windows.Forms.ColumnHeader columnHeaderFile;
		private System.Windows.Forms.ColumnHeader columnHeaderPath;
	}
}
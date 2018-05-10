// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace LuaRemoteDebugger
{
	partial class FindTextDialog
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
			this.labelFind = new System.Windows.Forms.Label();
			this.textBoxFind = new System.Windows.Forms.TextBox();
			this.buttonFindNext = new System.Windows.Forms.Button();
			this.buttonCancel = new System.Windows.Forms.Button();
			this.checkBoxMatchCase = new System.Windows.Forms.CheckBox();
			this.checkBoxWholeWord = new System.Windows.Forms.CheckBox();
			this.checkBoxSearchUp = new System.Windows.Forms.CheckBox();
			this.SuspendLayout();
			// 
			// labelFind
			// 
			this.labelFind.AutoSize = true;
			this.labelFind.Location = new System.Drawing.Point(12, 9);
			this.labelFind.Name = "labelFind";
			this.labelFind.Size = new System.Drawing.Size(56, 13);
			this.labelFind.TabIndex = 0;
			this.labelFind.Text = "Find what:";
			// 
			// textBoxFind
			// 
			this.textBoxFind.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
									| System.Windows.Forms.AnchorStyles.Right)));
			this.textBoxFind.Location = new System.Drawing.Point(15, 25);
			this.textBoxFind.Name = "textBoxFind";
			this.textBoxFind.Size = new System.Drawing.Size(257, 20);
			this.textBoxFind.TabIndex = 1;
			// 
			// buttonFindNext
			// 
			this.buttonFindNext.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.buttonFindNext.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.buttonFindNext.Location = new System.Drawing.Point(116, 81);
			this.buttonFindNext.Name = "buttonFindNext";
			this.buttonFindNext.Size = new System.Drawing.Size(75, 23);
			this.buttonFindNext.TabIndex = 2;
			this.buttonFindNext.Text = "Find Next";
			this.buttonFindNext.UseVisualStyleBackColor = true;
			this.buttonFindNext.Click += new System.EventHandler(this.buttonFindNext_Click);
			// 
			// buttonCancel
			// 
			this.buttonCancel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.buttonCancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.buttonCancel.Location = new System.Drawing.Point(197, 81);
			this.buttonCancel.Name = "buttonCancel";
			this.buttonCancel.Size = new System.Drawing.Size(75, 23);
			this.buttonCancel.TabIndex = 3;
			this.buttonCancel.Text = "Cancel";
			this.buttonCancel.UseVisualStyleBackColor = true;
			this.buttonCancel.Click += new System.EventHandler(this.buttonCancel_Click);
			// 
			// checkBoxMatchCase
			// 
			this.checkBoxMatchCase.AutoSize = true;
			this.checkBoxMatchCase.Location = new System.Drawing.Point(15, 51);
			this.checkBoxMatchCase.Name = "checkBoxMatchCase";
			this.checkBoxMatchCase.Size = new System.Drawing.Size(82, 17);
			this.checkBoxMatchCase.TabIndex = 4;
			this.checkBoxMatchCase.Text = "Match case";
			this.checkBoxMatchCase.UseVisualStyleBackColor = true;
			// 
			// checkBoxWholeWord
			// 
			this.checkBoxWholeWord.AutoSize = true;
			this.checkBoxWholeWord.Location = new System.Drawing.Point(103, 51);
			this.checkBoxWholeWord.Name = "checkBoxWholeWord";
			this.checkBoxWholeWord.Size = new System.Drawing.Size(83, 17);
			this.checkBoxWholeWord.TabIndex = 5;
			this.checkBoxWholeWord.Text = "Whole word";
			this.checkBoxWholeWord.UseVisualStyleBackColor = true;
			// 
			// checkBoxSearchUp
			// 
			this.checkBoxSearchUp.AutoSize = true;
			this.checkBoxSearchUp.Location = new System.Drawing.Point(192, 51);
			this.checkBoxSearchUp.Name = "checkBoxSearchUp";
			this.checkBoxSearchUp.Size = new System.Drawing.Size(75, 17);
			this.checkBoxSearchUp.TabIndex = 6;
			this.checkBoxSearchUp.Text = "Search up";
			this.checkBoxSearchUp.UseVisualStyleBackColor = true;
			// 
			// FindTextDialog
			// 
			this.AcceptButton = this.buttonFindNext;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.buttonCancel;
			this.ClientSize = new System.Drawing.Size(284, 116);
			this.Controls.Add(this.checkBoxSearchUp);
			this.Controls.Add(this.checkBoxWholeWord);
			this.Controls.Add(this.checkBoxMatchCase);
			this.Controls.Add(this.buttonCancel);
			this.Controls.Add(this.buttonFindNext);
			this.Controls.Add(this.textBoxFind);
			this.Controls.Add(this.labelFind);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Name = "FindTextDialog";
			this.StartPosition = System.Windows.Forms.FormStartPosition.Manual;
			this.Text = "Find";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Label labelFind;
		private System.Windows.Forms.TextBox textBoxFind;
		private System.Windows.Forms.Button buttonFindNext;
		private System.Windows.Forms.Button buttonCancel;
		private System.Windows.Forms.CheckBox checkBoxMatchCase;
		private System.Windows.Forms.CheckBox checkBoxWholeWord;
		private System.Windows.Forms.CheckBox checkBoxSearchUp;
	}
}
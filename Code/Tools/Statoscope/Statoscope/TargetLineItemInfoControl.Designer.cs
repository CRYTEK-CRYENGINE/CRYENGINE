// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace Statoscope
{
	partial class TargetLineItemInfoControl
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
			this.itemColourGroupBox = new System.Windows.Forms.GroupBox();
			this.itemColourButton = new System.Windows.Forms.Button();
			this.itemColourRandomButton = new System.Windows.Forms.Button();
			this.itemPathLabel = new System.Windows.Forms.Label();
			this.itemPathGroupBox = new System.Windows.Forms.GroupBox();
			this.labelLocationGroupBox = new System.Windows.Forms.GroupBox();
			this.rightLabelSideRadioButton = new System.Windows.Forms.RadioButton();
			this.leftLabelSideRadioButton = new System.Windows.Forms.RadioButton();
			this.nameTextBox = new System.Windows.Forms.TextBox();
			this.nameGroupBox = new System.Windows.Forms.GroupBox();
			this.nameFromValueCheckBox = new System.Windows.Forms.CheckBox();
			this.valueGroupBox = new System.Windows.Forms.GroupBox();
			this.valueTextBox = new System.Windows.Forms.TextBox();
			this.fpsValuesCheckBox = new System.Windows.Forms.CheckBox();
			this.itemColourGroupBox.SuspendLayout();
			this.itemPathGroupBox.SuspendLayout();
			this.labelLocationGroupBox.SuspendLayout();
			this.nameGroupBox.SuspendLayout();
			this.valueGroupBox.SuspendLayout();
			this.SuspendLayout();
			// 
			// itemColourGroupBox
			// 
			this.itemColourGroupBox.Controls.Add(this.itemColourButton);
			this.itemColourGroupBox.Controls.Add(this.itemColourRandomButton);
			this.itemColourGroupBox.Location = new System.Drawing.Point(240, 3);
			this.itemColourGroupBox.Name = "itemColourGroupBox";
			this.itemColourGroupBox.Size = new System.Drawing.Size(86, 52);
			this.itemColourGroupBox.TabIndex = 8;
			this.itemColourGroupBox.TabStop = false;
			this.itemColourGroupBox.Text = "Colour";
			// 
			// itemColourButton
			// 
			this.itemColourButton.BackColor = System.Drawing.Color.White;
			this.itemColourButton.Location = new System.Drawing.Point(7, 19);
			this.itemColourButton.Name = "itemColourButton";
			this.itemColourButton.Size = new System.Drawing.Size(23, 23);
			this.itemColourButton.TabIndex = 0;
			this.itemColourButton.UseVisualStyleBackColor = false;
			this.itemColourButton.Click += new System.EventHandler(this.itemColourButton_Click);
			// 
			// itemColourRandomButton
			// 
			this.itemColourRandomButton.Location = new System.Drawing.Point(36, 19);
			this.itemColourRandomButton.Name = "itemColourRandomButton";
			this.itemColourRandomButton.Size = new System.Drawing.Size(40, 23);
			this.itemColourRandomButton.TabIndex = 2;
			this.itemColourRandomButton.Text = "Rnd";
			this.itemColourRandomButton.UseVisualStyleBackColor = true;
			this.itemColourRandomButton.Click += new System.EventHandler(this.itemColourRandomButton_Click);
			// 
			// itemPathLabel
			// 
			this.itemPathLabel.AutoSize = true;
			this.itemPathLabel.Location = new System.Drawing.Point(9, 24);
			this.itemPathLabel.Name = "itemPathLabel";
			this.itemPathLabel.Size = new System.Drawing.Size(57, 13);
			this.itemPathLabel.TabIndex = 1;
			this.itemPathLabel.Text = "/blah/blah";
			// 
			// itemPathGroupBox
			// 
			this.itemPathGroupBox.Controls.Add(this.itemPathLabel);
			this.itemPathGroupBox.Location = new System.Drawing.Point(3, 3);
			this.itemPathGroupBox.Name = "itemPathGroupBox";
			this.itemPathGroupBox.Size = new System.Drawing.Size(231, 52);
			this.itemPathGroupBox.TabIndex = 7;
			this.itemPathGroupBox.TabStop = false;
			this.itemPathGroupBox.Text = "Path";
			// 
			// labelLocationGroupBox
			// 
			this.labelLocationGroupBox.Controls.Add(this.rightLabelSideRadioButton);
			this.labelLocationGroupBox.Controls.Add(this.leftLabelSideRadioButton);
			this.labelLocationGroupBox.Location = new System.Drawing.Point(211, 61);
			this.labelLocationGroupBox.Name = "labelLocationGroupBox";
			this.labelLocationGroupBox.Size = new System.Drawing.Size(115, 52);
			this.labelLocationGroupBox.TabIndex = 9;
			this.labelLocationGroupBox.TabStop = false;
			this.labelLocationGroupBox.Text = "Label";
			// 
			// rightLabelSideRadioButton
			// 
			this.rightLabelSideRadioButton.AutoSize = true;
			this.rightLabelSideRadioButton.Location = new System.Drawing.Point(59, 22);
			this.rightLabelSideRadioButton.Name = "rightLabelSideRadioButton";
			this.rightLabelSideRadioButton.Size = new System.Drawing.Size(50, 17);
			this.rightLabelSideRadioButton.TabIndex = 1;
			this.rightLabelSideRadioButton.TabStop = true;
			this.rightLabelSideRadioButton.Text = "Right";
			this.rightLabelSideRadioButton.UseVisualStyleBackColor = true;
			this.rightLabelSideRadioButton.CheckedChanged += new System.EventHandler(this.rightLabelSideRadioButton_CheckedChanged);
			// 
			// leftLabelSideRadioButton
			// 
			this.leftLabelSideRadioButton.AutoSize = true;
			this.leftLabelSideRadioButton.Location = new System.Drawing.Point(10, 22);
			this.leftLabelSideRadioButton.Name = "leftLabelSideRadioButton";
			this.leftLabelSideRadioButton.Size = new System.Drawing.Size(43, 17);
			this.leftLabelSideRadioButton.TabIndex = 0;
			this.leftLabelSideRadioButton.TabStop = true;
			this.leftLabelSideRadioButton.Text = "Left";
			this.leftLabelSideRadioButton.UseVisualStyleBackColor = true;
			this.leftLabelSideRadioButton.CheckedChanged += new System.EventHandler(this.leftLabelSideRadioButton_CheckedChanged);
			// 
			// nameTextBox
			// 
			this.nameTextBox.Location = new System.Drawing.Point(12, 21);
			this.nameTextBox.Name = "nameTextBox";
			this.nameTextBox.Size = new System.Drawing.Size(100, 20);
			this.nameTextBox.TabIndex = 2;
			this.nameTextBox.TextChanged += new System.EventHandler(this.nameTextBox_TextChanged);
			this.nameTextBox.Leave += new System.EventHandler(this.nameTextBox_Leave);
			// 
			// nameGroupBox
			// 
			this.nameGroupBox.Controls.Add(this.nameFromValueCheckBox);
			this.nameGroupBox.Controls.Add(this.nameTextBox);
			this.nameGroupBox.Location = new System.Drawing.Point(3, 61);
			this.nameGroupBox.Name = "nameGroupBox";
			this.nameGroupBox.Size = new System.Drawing.Size(202, 52);
			this.nameGroupBox.TabIndex = 10;
			this.nameGroupBox.TabStop = false;
			this.nameGroupBox.Text = "Name";
			// 
			// nameFromValueCheckBox
			// 
			this.nameFromValueCheckBox.AutoSize = true;
			this.nameFromValueCheckBox.CheckAlign = System.Drawing.ContentAlignment.MiddleRight;
			this.nameFromValueCheckBox.Location = new System.Drawing.Point(118, 22);
			this.nameFromValueCheckBox.Name = "nameFromValueCheckBox";
			this.nameFromValueCheckBox.Size = new System.Drawing.Size(78, 17);
			this.nameFromValueCheckBox.TabIndex = 4;
			this.nameFromValueCheckBox.Text = "From value";
			this.nameFromValueCheckBox.UseVisualStyleBackColor = true;
			this.nameFromValueCheckBox.CheckedChanged += new System.EventHandler(this.nameFromValueCheckBox_CheckedChanged);
			// 
			// valueGroupBox
			// 
			this.valueGroupBox.Controls.Add(this.valueTextBox);
			this.valueGroupBox.Controls.Add(this.fpsValuesCheckBox);
			this.valueGroupBox.Location = new System.Drawing.Point(4, 120);
			this.valueGroupBox.Name = "valueGroupBox";
			this.valueGroupBox.Size = new System.Drawing.Size(201, 50);
			this.valueGroupBox.TabIndex = 11;
			this.valueGroupBox.TabStop = false;
			this.valueGroupBox.Text = "Value";
			// 
			// valueTextBox
			// 
			this.valueTextBox.Location = new System.Drawing.Point(11, 19);
			this.valueTextBox.Name = "valueTextBox";
			this.valueTextBox.Size = new System.Drawing.Size(132, 20);
			this.valueTextBox.TabIndex = 12;
			this.valueTextBox.TextChanged += new System.EventHandler(this.valueTextBox_TextChanged);
			// 
			// fpsValuesCheckBox
			// 
			this.fpsValuesCheckBox.AutoSize = true;
			this.fpsValuesCheckBox.CheckAlign = System.Drawing.ContentAlignment.MiddleRight;
			this.fpsValuesCheckBox.Location = new System.Drawing.Point(149, 21);
			this.fpsValuesCheckBox.Name = "fpsValuesCheckBox";
			this.fpsValuesCheckBox.Size = new System.Drawing.Size(46, 17);
			this.fpsValuesCheckBox.TabIndex = 11;
			this.fpsValuesCheckBox.Text = "FPS";
			this.fpsValuesCheckBox.UseVisualStyleBackColor = true;
			this.fpsValuesCheckBox.CheckedChanged += new System.EventHandler(this.fpsValuesCheckBox_CheckedChanged);
			// 
			// TargetLineItemInfoControl
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.Controls.Add(this.valueGroupBox);
			this.Controls.Add(this.nameGroupBox);
			this.Controls.Add(this.labelLocationGroupBox);
			this.Controls.Add(this.itemColourGroupBox);
			this.Controls.Add(this.itemPathGroupBox);
			this.Name = "TargetLineItemInfoControl";
			this.Size = new System.Drawing.Size(329, 310);
			this.itemColourGroupBox.ResumeLayout(false);
			this.itemPathGroupBox.ResumeLayout(false);
			this.itemPathGroupBox.PerformLayout();
			this.labelLocationGroupBox.ResumeLayout(false);
			this.labelLocationGroupBox.PerformLayout();
			this.nameGroupBox.ResumeLayout(false);
			this.nameGroupBox.PerformLayout();
			this.valueGroupBox.ResumeLayout(false);
			this.valueGroupBox.PerformLayout();
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.GroupBox itemColourGroupBox;
		private System.Windows.Forms.Button itemColourButton;
		private System.Windows.Forms.Button itemColourRandomButton;
		private System.Windows.Forms.Label itemPathLabel;
		private System.Windows.Forms.GroupBox itemPathGroupBox;
		private System.Windows.Forms.GroupBox labelLocationGroupBox;
		private System.Windows.Forms.RadioButton rightLabelSideRadioButton;
		private System.Windows.Forms.RadioButton leftLabelSideRadioButton;
		private System.Windows.Forms.TextBox nameTextBox;
		private System.Windows.Forms.GroupBox nameGroupBox;
		private System.Windows.Forms.CheckBox nameFromValueCheckBox;
		private System.Windows.Forms.GroupBox valueGroupBox;
		private System.Windows.Forms.CheckBox fpsValuesCheckBox;
		private System.Windows.Forms.TextBox valueTextBox;

	}
}

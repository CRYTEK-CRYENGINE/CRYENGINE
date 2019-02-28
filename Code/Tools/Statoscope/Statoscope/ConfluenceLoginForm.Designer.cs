// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace Statoscope
{
	partial class ConfluenceLoginForm
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
			this.UsernameTextBox = new System.Windows.Forms.TextBox();
			this.UsernameLabel = new System.Windows.Forms.Label();
			this.PasswordLabel = new System.Windows.Forms.Label();
			this.PasswordTextBox = new System.Windows.Forms.TextBox();
			this.LoginButton = new System.Windows.Forms.Button();
			this.LoginCancelButton = new System.Windows.Forms.Button();
			this.LoginInfoLabel = new System.Windows.Forms.Label();
			this.SuspendLayout();
			// 
			// UsernameTextBox
			// 
			this.UsernameTextBox.Location = new System.Drawing.Point(73, 12);
			this.UsernameTextBox.Name = "UsernameTextBox";
			this.UsernameTextBox.Size = new System.Drawing.Size(100, 20);
			this.UsernameTextBox.TabIndex = 0;
			// 
			// UsernameLabel
			// 
			this.UsernameLabel.AutoSize = true;
			this.UsernameLabel.Location = new System.Drawing.Point(12, 15);
			this.UsernameLabel.Name = "UsernameLabel";
			this.UsernameLabel.Size = new System.Drawing.Size(55, 13);
			this.UsernameLabel.TabIndex = 1;
			this.UsernameLabel.Text = "Username";
			// 
			// PasswordLabel
			// 
			this.PasswordLabel.AutoSize = true;
			this.PasswordLabel.Location = new System.Drawing.Point(12, 41);
			this.PasswordLabel.Name = "PasswordLabel";
			this.PasswordLabel.Size = new System.Drawing.Size(53, 13);
			this.PasswordLabel.TabIndex = 2;
			this.PasswordLabel.Text = "Password";
			// 
			// PasswordTextBox
			// 
			this.PasswordTextBox.Location = new System.Drawing.Point(73, 38);
			this.PasswordTextBox.Name = "PasswordTextBox";
			this.PasswordTextBox.PasswordChar = '*';
			this.PasswordTextBox.Size = new System.Drawing.Size(100, 20);
			this.PasswordTextBox.TabIndex = 3;
			// 
			// LoginButton
			// 
			this.LoginButton.Location = new System.Drawing.Point(24, 89);
			this.LoginButton.Name = "LoginButton";
			this.LoginButton.Size = new System.Drawing.Size(75, 23);
			this.LoginButton.TabIndex = 4;
			this.LoginButton.Text = "Login";
			this.LoginButton.UseVisualStyleBackColor = true;
			this.LoginButton.Click += new System.EventHandler(this.LoginButton_Click);
			// 
			// LoginCancelButton
			// 
			this.LoginCancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.LoginCancelButton.Location = new System.Drawing.Point(107, 89);
			this.LoginCancelButton.Name = "LoginCancelButton";
			this.LoginCancelButton.Size = new System.Drawing.Size(75, 23);
			this.LoginCancelButton.TabIndex = 5;
			this.LoginCancelButton.Text = "Cancel";
			this.LoginCancelButton.UseVisualStyleBackColor = true;
			// 
			// LoginInfoLabel
			// 
			this.LoginInfoLabel.AutoSize = true;
			this.LoginInfoLabel.Location = new System.Drawing.Point(7, 66);
			this.LoginInfoLabel.Name = "LoginInfoLabel";
			this.LoginInfoLabel.Size = new System.Drawing.Size(191, 13);
			this.LoginInfoLabel.TabIndex = 6;
			this.LoginInfoLabel.Text = "Login can take 15s - please be patient!";
			this.LoginInfoLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// ConfluenceLoginForm
			// 
			this.AcceptButton = this.LoginButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.LoginCancelButton;
			this.ClientSize = new System.Drawing.Size(205, 124);
			this.ControlBox = false;
			this.Controls.Add(this.LoginInfoLabel);
			this.Controls.Add(this.LoginCancelButton);
			this.Controls.Add(this.LoginButton);
			this.Controls.Add(this.PasswordTextBox);
			this.Controls.Add(this.PasswordLabel);
			this.Controls.Add(this.UsernameLabel);
			this.Controls.Add(this.UsernameTextBox);
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "ConfluenceLoginForm";
			this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
			this.Text = "Confluence Login";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Label UsernameLabel;
		private System.Windows.Forms.Label PasswordLabel;
		private System.Windows.Forms.Button LoginButton;
		private System.Windows.Forms.Button LoginCancelButton;
		private System.Windows.Forms.TextBox UsernameTextBox;
		private System.Windows.Forms.TextBox PasswordTextBox;
		private System.Windows.Forms.Label LoginInfoLabel;
	}
}
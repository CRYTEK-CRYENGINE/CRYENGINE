// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace Statoscope
{
	partial class ConfluenceLoginForm : Form
	{
		ConfluenceService m_confluenceService;

		public ConfluenceLoginForm(ConfluenceService confluenceService, string initialUsername, string initialPassword)
		{
			m_confluenceService = confluenceService;

			FormBorderStyle = FormBorderStyle.FixedDialog;
			InitializeComponent();

			UsernameTextBox.Text = initialUsername;
			PasswordTextBox.Text = initialPassword;
		}

		private void LoginButton_Click(object sender, EventArgs e)
		{
			LoginInfoLabel.Text = "Logging in...";
			LoginInfoLabel.Refresh();

			if (m_confluenceService.Login(UsernameTextBox.Text, PasswordTextBox.Text))
			{
				Close();
			}
			else
			{
				LoginInfoLabel.Text = "Please check username and password";
				MessageBox.Show("Login failed :(", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
			}
		}
	}
}

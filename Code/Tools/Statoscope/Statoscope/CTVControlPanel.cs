// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace Statoscope
{
	partial class CTVControlPanel : UserControl
	{
		CheckboxTreeView m_ctv;
		public CheckboxTreeView CTV
		{
			get
			{
				return m_ctv;
			}

			set
			{
				m_ctv = value;

				if (CTV != null)
				{
					CTV.HistoryStateListener += HistoryStateChanged;
					UpdateButtons();
				}
			}
		}

		public CTVControlPanel()
		{
			InitializeComponent();
		}

		public CTVControlPanel(CheckboxTreeView ctv)
		{
			InitializeComponent();
			CTV = ctv;
		}

		private void undoButton_Click(object sender, EventArgs e)
		{
			CTV.Undo();
			UpdateButtons();
		}

		private void redoButton_Click(object sender, EventArgs e)
		{
			CTV.Redo();
			UpdateButtons();
		}

		private void filterText_TextChanged(object sender, EventArgs e)
		{
			CTV.FilterTextChanged(filterText.Text.ToUpper());
		}

		void UpdateButtons()
		{
			undoButton.Enabled = CTV.NumUndoStates > 0;
			redoButton.Enabled = CTV.NumRedoStates > 0;
		}

		public void HistoryStateChanged(object sender, EventArgs e)
		{
			UpdateButtons();
		}
	}
}

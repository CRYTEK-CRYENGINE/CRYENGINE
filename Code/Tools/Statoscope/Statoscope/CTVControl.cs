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
	// checkbox treeview control
	partial class CTVControl : UserControl
	{
		public CTVControl(CheckboxTreeView ctv)
			: this(ctv, new CTVControlPanel(ctv))
		{
		}

		public CTVControl(CheckboxTreeView ctv, Control controlPanel)
		{
			InitializeComponent();
			Dock = DockStyle.Fill;
			tableLayoutPanel.Controls.Add(controlPanel);
			tableLayoutPanel.Controls.Add(ctv, 0, 1);
		}
	}
}

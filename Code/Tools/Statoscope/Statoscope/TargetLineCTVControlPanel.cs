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
	partial class TargetLineCTVControlPanel : UserControl
	{
		TRDICheckboxTreeView TCTV { get { return (TRDICheckboxTreeView)ctvControlPanel.CTV; } }

		public TargetLineCTVControlPanel(TRDICheckboxTreeView ctv)
		{
			InitializeComponent();
			ctvControlPanel.CTV = ctv;
			TCTV.AfterSelect += new TreeViewEventHandler(TCTV_AfterSelect);
			addTargetLineButton.Enabled = false;
			addNodeButton.Enabled = false;
		}

		void TCTV_AfterSelect(object sender, TreeViewEventArgs e)
		{
			TargetLineRDI selectedTRDI = TCTV.SelectedRDINode;
			addTargetLineButton.Enabled = selectedTRDI.CanHaveChildren;
			addNodeButton.Enabled = selectedTRDI.CanHaveChildren;
		}

		private void addTargetLineButton_Click(object sender, EventArgs e)
		{
			TargetLineRDI selectedTRDI = TCTV.SelectedRDINode;
			string basePath = selectedTRDI.Path;
			string name = selectedTRDI.GetNewChildName("newTargetLine");
			TargetLineRDI trdi = new TargetLineRDI(basePath, name, 0, RGB.RandomHueRGB(), TargetLineRDI.ELabelLocation.ELL_Right);
			TCTV.AddRDIToTree(trdi);
			TCTV.SelectedRDINode = trdi;
		}

		private void addNodeButton_Click(object sender, EventArgs e)
		{
			TCTV.AddChildNodeToSelectedNode();
		}

		private void deleteButton_Click(object sender, EventArgs e)
		{
			TCTV.RemoveSelectedNode();
		}
	}
}

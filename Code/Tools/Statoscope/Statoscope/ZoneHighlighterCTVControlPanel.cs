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
	partial class ZoneHighlighterCTVControlPanel : UserControl
	{
		ZRDICheckboxTreeView ZCTV { get { return (ZRDICheckboxTreeView)ctvControlPanel.CTV; } }

		public ZoneHighlighterCTVControlPanel(ZRDICheckboxTreeView ctv)
		{
			InitializeComponent();

			ctvControlPanel.CTV = ctv;
			ZCTV.AfterSelect += new TreeViewEventHandler(TCTV_AfterSelect);
			addZoneHighlighterButton.Enabled = false;
			addNodeButton.Enabled = false;
		}

		void TCTV_AfterSelect(object sender, TreeViewEventArgs e)
		{
			ZoneHighlighterRDI selectedZRDI = ZCTV.SelectedRDINode;
			addZoneHighlighterButton.Enabled = selectedZRDI.CanHaveChildren;
			addNodeButton.Enabled = selectedZRDI.CanHaveChildren;
		}

		private void addZoneHighlighterButton_Click(object sender, EventArgs e)
		{
			ZoneHighlighterRDI selectedZRDI = ZCTV.SelectedRDINode;
			string basePath = selectedZRDI.Path;
			string name = selectedZRDI.GetNewChildName("newZoneHighlighter");
			ZoneHighlighterRDI zrdi = new StdThresholdZRDI(basePath, name);
			ZCTV.AddRDIToTree(zrdi);
			ZCTV.SelectedRDINode = zrdi;
		}

		private void addNodeButton_Click(object sender, EventArgs e)
		{
			ZCTV.AddChildNodeToSelectedNode();
		}

		private void deleteButton_Click(object sender, EventArgs e)
		{
			ZCTV.RemoveSelectedNode();
		}
	}
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Windows.Forms;

namespace Statoscope
{
	public class ORDICheckboxTreeView : RDICheckboxTreeView<OverviewRDI>
	{
		public ORDICheckboxTreeView(LogControl logControl, OverviewRDI ordiTree)
			: base(logControl, ordiTree)
		{
		}

		public override void SerializeInRDIs(OverviewRDI[] ordis)
		{
			if (ordis == null)
				return;

			base.SerializeInRDIs(ordis);

			foreach (TreeNode node in TreeNodeHierarchy)
			{
				OverviewRDI ordi = (OverviewRDI)node.Tag;
				node.Checked = ordi.Displayed;
			}
		}
	}
}

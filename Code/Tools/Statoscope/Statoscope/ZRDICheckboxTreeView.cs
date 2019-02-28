// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Windows.Forms;

namespace Statoscope
{
	public class ZRDICheckboxTreeView : EditableRDICheckboxTreeView<ZoneHighlighterRDI>
	{
		public ZRDICheckboxTreeView(LogControl logControl, ZoneHighlighterRDI zrdiTree)
			: base(logControl, zrdiTree)
		{
		}

		public override void AddRDIToTree(ZoneHighlighterRDI zrdi)
		{
			base.AddRDIToTree(zrdi);

			if (!zrdi.CanHaveChildren)
				LogControl.AddZoneHighlighter(zrdi);
		}

		public override void RemoveRDIFromTree(ZoneHighlighterRDI zrdi)
		{
			base.RemoveRDIFromTree(zrdi);

			if (!zrdi.CanHaveChildren)
				LogControl.RemoveZoneHighlighter(zrdi);
		}

		public override void RenameNode(TreeNode node, string newName)
		{
			ZoneHighlighterRDI zrdi = (ZoneHighlighterRDI)node.Tag;
			zrdi.NameFromPath = false;

			base.RenameNode(node, newName);
		}
	}
}

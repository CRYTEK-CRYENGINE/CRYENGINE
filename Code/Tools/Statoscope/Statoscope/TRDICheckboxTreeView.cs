// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Windows.Forms;

namespace Statoscope
{
	public class TRDICheckboxTreeView : EditableRDICheckboxTreeView<TargetLineRDI>
	{
		public TRDICheckboxTreeView(LogControl logControl, TargetLineRDI trdiTree)
			: base(logControl, trdiTree)
		{
		}

		public override void RenameNode(TreeNode node, string newName)
		{
			TargetLineRDI trdi = (TargetLineRDI)node.Tag;
			trdi.NameFromValue = false;

			base.RenameNode(node, newName);
		}
	}
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
#if __MonoCS__
// Define aliases to avoid ambiguous references with System.*
using gtkalias = Gtk;
using gdkalias = Gdk;
#endif

namespace Statoscope
{
	class RDIInfoControl<T> : ItemInfoControl where T : RecordDisplayInfo<T>, new()
	{
		T rdi;
		Label itemPathLabel;

		protected readonly T RDITree;
		protected readonly RDICheckboxTreeView<T> CTV;
		protected readonly ToolTip ItemPathLabelToolTip = new ToolTip();
		protected readonly ContextMenuStrip ItemPathCMS = new ContextMenuStrip();

		protected Label ItemPathLabel
		{
			get { return itemPathLabel; }

			set
			{
				itemPathLabel = value;
				itemPathLabel.ContextMenuStrip = ItemPathCMS;
			}
		}

		protected virtual TextBox NameTextBox
		{
			get { return null; }
		}

		protected virtual T RDI
		{
			get { return rdi; }
			set { rdi = value; }
		}

		protected RDIInfoControl(LogControl logControl, T rdiTree, RDICheckboxTreeView<T> ctv)
		: base(logControl)
		{
			RDITree = rdiTree;
			CTV = ctv;
			ItemPathCMS.Items.Add("Copy path", null, itemPathCMS_CopyPathClicked);
		}

		void itemPathCMS_CopyPathClicked(object sender, EventArgs e)
		{
#if __MonoCS__
			gtkalias.Clipboard clipboard = gtkalias.Clipboard.Get(gdkalias.Atom.Intern("CLIPBOARD", false));
			clipboard.Text = itemPathLabel.Text;
#else
			System.Windows.Clipboard.SetText(ItemPathLabel.Text);
#endif
		}

		public override void SetItem(object item)
		{
			RDI = item as T;
		}

		public override void RefreshItem()
		{
			RDI = RDI;
		}

		protected virtual void SetItemColour(RGB colour)
		{
			RDI.Colour = colour;
			LogControl.InvalidateGraphControl();
		}

		protected void ItemColourButton_Click()
		{
			ColorDialog cd = new ColorDialog();
			cd.FullOpen = true;
			cd.Color = new RGB(0.5f, 0.5f, 0.5f).ConvertToSysDrawCol();

			if (cd.ShowDialog() == DialogResult.OK)
			{
				SetItemColour(new RGB(cd.Color));
			}
		}

		protected void ItemColourRandomButton_Click()
		{
			SetItemColour(RGB.RandomHueRGB());
		}

		protected virtual void SetPathText()
		{
			ItemPathLabel.Text = RDI.Path;
			ItemPathLabelToolTip.SetToolTip(ItemPathLabel, RDI.Path);
		}

		protected virtual void SetNameText()
		{
		}

		protected bool SetNameTextBoxBackColor()
		{
			string path = RDI.BasePath + "/" + NameTextBox.Text;
			T pathRDI = RDITree[path];

			if (((pathRDI != null) && (pathRDI != RDI)) || NameTextBox.Text.Contains("/"))
			{
				NameTextBox.BackColor = Color.Red;
				return false;
			}

			NameTextBox.BackColor = SystemColors.Control;
			return true;
		}

		protected void NameTextBox_TextChanged()
		{
			string newName = NameTextBox.Text;

			if (!SetNameTextBoxBackColor() || RDI.Name == newName)
			{
				return;
			}

			TreeNode node = CTV.RDITreeNodeHierarchy[RDI];
			node.Name = newName;
			node.Text = newName;
			CTV.Sort();

			RDI.Name = newName;
			RDI.DisplayName = newName;
			SetPathText();

			LogControl.InvalidateGraphControl();
		}

		protected void NameTextBox_Leave()
		{
			SetNameText();
		}
	}
}

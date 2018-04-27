// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Windows.Forms;

namespace Statoscope
{
	public class URDICheckboxTreeView : RDICheckboxTreeView<UserMarkerRDI>
	{
		public URDICheckboxTreeView(LogControl logControl, UserMarkerRDI urdiTree)
			: base(logControl, urdiTree)
		{
			ContextMenuStrip userMarkerCMS = new ContextMenuStrip();
			userMarkerCMS.Opening += new CancelEventHandler(userMarkerCMS_Opening);
			userMarkerCMS.Items.Add("Use as start frame", null, userMarkerCMS_StartFrameClicked);
			userMarkerCMS.Items.Add("Use as end frame", null, userMarkerCMS_EndFrameClicked);
			userMarkerCMS.ShowImageMargin = false;

			ContextMenuStrip = userMarkerCMS;

			m_toolTip.SetToolTip(this,
				"Right-click: toggle subtree" + Environment.NewLine +
				"Right-click (leaf node): select log range" + Environment.NewLine +
				"Ctrl-left-click: isolate subtree");
		}

		public override void AddRDIs(IEnumerable<UserMarkerRDI> newToAdd)
		{
			base.AddRDIs(newToAdd);
			SetNodeColours(newToAdd);
		}

		public override void SerializeInRDIs(UserMarkerRDI[] urdis)
		{
			if (urdis == null)
				return;

			foreach (UserMarkerRDI urdi in RDITree.GetEnumerator(ERDIEnumeration.Full))
			{
				urdi.Displayed = false;
			}

			foreach (UserMarkerRDI urdi in urdis)
			{
				bool bIsLeaf = (urdi.DisplayName != urdi.Name);
				UserMarkerRDI treeURDI = RDITree[bIsLeaf ? urdi.BasePath : urdi.Path];

				if (treeURDI == null)
					continue;

				if (!bIsLeaf)
				{
					// internal nodes
					CopySerializedMembers(treeURDI, urdi);
				}
				else
				{
					// leaf nodes (actual markers)
					foreach (UserMarkerRDI childURDI in treeURDI.Children)
					{
						if (childURDI.DisplayName == urdi.DisplayName)
						{
							CopySerializedMembers(childURDI, urdi);
						}
					}
				}
			}

			foreach (TreeNode node in TreeNodeHierarchy)
			{
				UserMarkerRDI urdi = (UserMarkerRDI)node.Tag;
				node.Checked = urdi.Displayed;
			}
		}

		public void SetNodeColours(IEnumerable<UserMarkerRDI> UMRDIs)
		{
			foreach (UserMarkerRDI umrdi in UMRDIs)
			{
				LogView nodeLogView = umrdi.m_logView;
				if (nodeLogView != null && nodeLogView.m_singleOrdiColour != null)
				{
					TreeNode node = RDITreeNodeHierarchy[umrdi];
					node.ForeColor = nodeLogView.m_singleOrdiColour.ConvertToSysDrawCol();
				}
			}
		}

		protected override void  OnNodeMouseClick(TreeNodeMouseClickEventArgs e)
		{
			base.OnNodeMouseClick(e);
			// right-clicking on leaf nodes opens up a menu, so don't toggle the check state
			if ((e.Button == MouseButtons.Right) && (e.Node.Nodes.Count == 0))
				e.Node.Checked = !e.Node.Checked;
		}

    protected override void OnNodeMouseDoubleClick(TreeNodeMouseClickEventArgs e)
		{
			base.OnNodeMouseDoubleClick(e);

			string path = "/" + e.Node.FullPath;
			float x = float.MinValue;

			foreach (UserMarkerRDI umrdi in RDITree.GetEnumerator(ERDIEnumeration.OnlyLeafNodes))
			{
				if (umrdi.Path == path)
				{
					// only set x to a valid value if it's the only path that matches
					if (x == float.MinValue)
						x = umrdi.m_uml.m_fr.GetDisplayXValue(umrdi.m_logView.m_logData, LogControl.m_OGLGraphingControl.m_againstFrameTime);
					else
						x = float.MaxValue;

					umrdi.Highlight();
				}
				else
				{
					umrdi.Unhighlight();
				}
			}

			if (x != float.MinValue && x != float.MaxValue)
				LogControl.m_OGLGraphingControl.ZoomViewToXValue(x);

			LogControl.m_OGLGraphingControl.Invalidate();
		}

		void userMarkerCMS_Opening(object sender, CancelEventArgs e)
		{
			e.Cancel = true;

			if (SelectedNode != null)
			{
				UserMarkerRDI umrdi = (UserMarkerRDI)SelectedNode.Tag;
				if (umrdi.m_uml != null)
					e.Cancel = false; // only show the pop-up menu for user marker leaf nodes
			}
		}

		void userMarkerCMS_StartFrameClicked(object sender, EventArgs e)
		{
			if (SelectedNode != null)
			{
				UserMarkerRDI umrdi = (UserMarkerRDI)SelectedNode.Tag;
				int umIdx = umrdi.m_uml.m_fr.Index;
				umrdi.m_logView.SetFrameRecordRange(umIdx, Math.Max(umIdx, umrdi.m_logView.GetFrameRecordRange().EndIdx));
				umrdi.m_logView.m_startUM = umrdi.DisplayUserMarker;
			}
		}

		void userMarkerCMS_EndFrameClicked(object sender, EventArgs e)
		{
			if (SelectedNode != null)
			{
				UserMarkerRDI umrdi = (UserMarkerRDI)SelectedNode.Tag;
				int umIdx = umrdi.m_uml.m_fr.Index;
				umrdi.m_logView.SetFrameRecordRange(Math.Min(umIdx, umrdi.m_logView.GetFrameRecordRange().StartIdx), umIdx);
				umrdi.m_logView.m_endUM = umrdi.DisplayUserMarker;
			}
		}

		/*
    private void TargetLineKeyDown(System.Windows.Forms.KeyEventArgs e)
    {
      m_nonNumberEntered = false;
      // Determine whether the keystroke is a number from the top of the keyboard.
      if (e.KeyCode < Keys.D0 || e.KeyCode > Keys.D9)
      {
        // Determine whether the keystroke is a number from the keypad.
        if (e.KeyCode < Keys.NumPad0 || e.KeyCode > Keys.NumPad9)
        {
          // Determine whether the keystroke is a backspace or a decimal point
          if (e.KeyCode != Keys.Back && e.KeyCode != Keys.Decimal && e.KeyValue != 190) // main keyboard .
          {
            // A non-numerical keystroke was pressed.
            // Set the flag to true and evaluate in KeyPress event.
            m_nonNumberEntered = true;
          }
        }
      }
    }

    private void userDefinedFPSTargetLine_KeyDown(object sender, System.Windows.Forms.KeyEventArgs e)
		{
			TargetLineKeyDown(e);
		}

		private void userDefinedFPSTargetLine_KeyPress(object sender, KeyPressEventArgs e)
		{
			if (m_nonNumberEntered == true)
			{
				e.Handled = true;
			}
		}

		private void userDefinedFPSTargetLine_TextChanged(object sender, EventArgs e)
		{
			//Update the user line
			userDefinedFPSTargetLine.BackColor = SystemColors.Window;

			if (userDefinedFPSTargetLine.Text.Length > 0)
			{
				try
				{
					float value = Convert.ToSingle(userDefinedFPSTargetLine.Text);
					m_OGLGraphingControl.m_targetLines[Convert.ToInt16(EReservedTargetLines.UserFPS)].m_displayLevel = value == 0.0f ? 0.0f : (1.0f / value) * 1000.0f;
					m_OGLGraphingControl.m_targetLines[Convert.ToInt16(EReservedTargetLines.UserFPS)].m_label.Text = string.Format("*{0}", value);
				}
				catch (FormatException)
				{
					userDefinedFPSTargetLine.BackColor = Color.Red;
				}
			}
			else
			{
				m_OGLGraphingControl.m_targetLines[Convert.ToInt16(EReservedTargetLines.UserFPS)].m_displayLevel = 0.0f;
				m_OGLGraphingControl.m_targetLines[Convert.ToInt16(EReservedTargetLines.UserFPS)].m_label.Text = "";
			}

			InvalidateGraphControl();
		}

		private void userDefinedNumTargetLine_KeyDown(object sender, KeyEventArgs e)
		{
			TargetLineKeyDown(e);
		}

		private void userDefinedNumTargetLine_KeyPress(object sender, KeyPressEventArgs e)
		{
			if (m_nonNumberEntered == true)
			{
				e.Handled = true;
			}
		}

		private void userDefinedNumTargetLine_TextChanged(object sender, EventArgs e)
		{
			//Update the user line
			userDefinedNumTargetLine.BackColor = SystemColors.Window;

			if (userDefinedNumTargetLine.Text.Length > 0)
			{
				try
				{
					m_OGLGraphingControl.m_targetLines[Convert.ToInt16(EReservedTargetLines.UserNum)].m_displayLevel = Convert.ToSingle(userDefinedNumTargetLine.Text);
					m_OGLGraphingControl.m_targetLines[Convert.ToInt16(EReservedTargetLines.UserNum)].m_label.Text = string.Format("*{0}", m_OGLGraphingControl.m_targetLines[Convert.ToInt16(EReservedTargetLines.UserNum)].m_displayLevel);
				}
				catch (FormatException)
				{
					userDefinedNumTargetLine.BackColor = Color.Red;
				}
			}
			else
			{
				m_OGLGraphingControl.m_targetLines[Convert.ToInt16(EReservedTargetLines.UserNum)].m_displayLevel = 0.0f;
				m_OGLGraphingControl.m_targetLines[Convert.ToInt16(EReservedTargetLines.UserNum)].m_label.Text = "";
			}

			InvalidateGraphControl();
		}
		*/

	}
}
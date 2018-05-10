// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Windows.Forms;

namespace Statoscope
{
	public class CheckboxTreeView : TreeView
	{
		Stack<Dictionary<TreeNode, CNodeCheckHistoryState>> m_nodeCheckStateUndoStack = new Stack<Dictionary<TreeNode, CNodeCheckHistoryState>>();
		Stack<Dictionary<TreeNode, CNodeCheckHistoryState>> m_nodeCheckStateRedoStack = new Stack<Dictionary<TreeNode, CNodeCheckHistoryState>>();

		protected ToolTip m_toolTip;
		protected bool m_refreshOnTreeNodeChecked = true;
		bool m_doingUndoNodeCheckReplay = false;
		bool m_addToCurrentNodeCheckUndoStates = false;

		public HierarchyTreeNodeCollection TreeNodeHierarchy;

    public EventHandler HistoryStateListener;

		public CheckboxTreeView()
		{
			CheckBoxes = true;
			PathSeparator = "/";
			Dock = DockStyle.Fill;

			TreeNodeHierarchy = new HierarchyTreeNodeCollection(Nodes);
			m_toolTip = new ToolTip();
			m_toolTip.InitialDelay = 2000;
			m_toolTip.ShowAlways = true;
			m_toolTip.SetToolTip(this,
				"Right-click: toggle subtree" + Environment.NewLine +
				"Ctrl-left-click: isolate subtree");
		}

		private const int WM_LBUTTONDBLCLK = 0x0203;

		// a hack to get round a bug in the TreeView implementation
		protected override void WndProc(ref Message m)
		{
			if (m.Msg == WM_LBUTTONDBLCLK)
			{
				TreeViewHitTestInfo tvhti = HitTest(new System.Drawing.Point((int)m.LParam));
				if (tvhti != null && tvhti.Location == TreeViewHitTestLocations.StateImage)
				{
					m.Result = IntPtr.Zero;
					return;
				}
			}
			base.WndProc(ref m);
		}

		class CNodeCheckHistoryState
		{
			public bool m_beforeChecked;
			public bool m_afterChecked;

			public CNodeCheckHistoryState(bool beforeChecked)
			{
				m_beforeChecked = beforeChecked;
			}
		}

		void StartCheckStateBlock()
		{
			m_nodeCheckStateUndoStack.Push(new Dictionary<TreeNode, CNodeCheckHistoryState>());
			m_addToCurrentNodeCheckUndoStates = true;
			m_refreshOnTreeNodeChecked = false;
			UpdateHistoryStateListeners();
		}

		void EndCheckStateBlock()
		{
			m_refreshOnTreeNodeChecked = true;
			m_addToCurrentNodeCheckUndoStates = false;
		}

		public class CheckStateScope : IDisposable
		{
			private readonly CheckboxTreeView CTV;

			public CheckStateScope(CheckboxTreeView ctv)
			{
				CTV = ctv;
				CTV.StartCheckStateBlock();
			}

			public void Dispose()
			{
				CTV.EndCheckStateBlock();
			}
		}

		public CheckStateScope CheckStateBlock()
		{
			return new CheckStateScope(this);
		}

		void SetUndoNodeBeforeCheckState(TreeNode node, bool beforeCheckedState)
		{
			if (!m_doingUndoNodeCheckReplay)
			{
				m_nodeCheckStateRedoStack.Clear();

				Dictionary<TreeNode, CNodeCheckHistoryState> currentUndoNodeCheckStates;

				if (m_addToCurrentNodeCheckUndoStates)
				{
					currentUndoNodeCheckStates = m_nodeCheckStateUndoStack.Peek();
				}
				else
				{
					currentUndoNodeCheckStates = new Dictionary<TreeNode, CNodeCheckHistoryState>();
					m_nodeCheckStateUndoStack.Push(currentUndoNodeCheckStates);
				}

				if (!currentUndoNodeCheckStates.ContainsKey(node))
				{
					currentUndoNodeCheckStates.Add(node, new CNodeCheckHistoryState(beforeCheckedState));
				}

				UpdateHistoryStateListeners();
			}
		}

		void SetUndoNodeAfterCheckState(TreeNode node, bool afterCheckedState)
		{
			if (!m_doingUndoNodeCheckReplay)
			{
				Dictionary<TreeNode, CNodeCheckHistoryState> currentUndoNodeCheckStates = m_nodeCheckStateUndoStack.Peek();
				currentUndoNodeCheckStates[node].m_afterChecked = afterCheckedState;
				UpdateHistoryStateListeners();
			}
		}

		void UpdateHistoryStateListeners()
		{
			if (HistoryStateListener != null)
				HistoryStateListener(this, EventArgs.Empty);
		}

		public virtual void Undo()
		{
			m_doingUndoNodeCheckReplay = true;
			m_refreshOnTreeNodeChecked = false;

			if (m_nodeCheckStateUndoStack.Count > 0)
			{
				Dictionary<TreeNode, CNodeCheckHistoryState> topUndoNodeCheckStates = m_nodeCheckStateUndoStack.Pop();

				foreach (KeyValuePair<TreeNode, CNodeCheckHistoryState> undoNodeCheckStatePair in topUndoNodeCheckStates)
				{
					TreeNode node = undoNodeCheckStatePair.Key;
					node.Checked = undoNodeCheckStatePair.Value.m_beforeChecked;
				}

				m_nodeCheckStateRedoStack.Push(topUndoNodeCheckStates);
				UpdateHistoryStateListeners();
			}

			m_refreshOnTreeNodeChecked = true;
			m_doingUndoNodeCheckReplay = false;
		}

		public virtual void Redo()
		{
			m_doingUndoNodeCheckReplay = true;
			m_refreshOnTreeNodeChecked = false;

			if (m_nodeCheckStateRedoStack.Count > 0)
			{
				Dictionary<TreeNode, CNodeCheckHistoryState> topRedoNodeCheckStates = m_nodeCheckStateRedoStack.Pop();

				foreach (KeyValuePair<TreeNode, CNodeCheckHistoryState> redoNodeCheckStatePair in topRedoNodeCheckStates)
				{
          TreeNode node = redoNodeCheckStatePair.Key;
					node.Checked = redoNodeCheckStatePair.Value.m_afterChecked;
				}

				m_nodeCheckStateUndoStack.Push(topRedoNodeCheckStates);
				UpdateHistoryStateListeners();
			}

			m_refreshOnTreeNodeChecked = true;
			m_doingUndoNodeCheckReplay = false;
		}

		public virtual void FilterTextChanged(string filterText)
		{
			m_refreshOnTreeNodeChecked = false;

			using (CheckStateBlock())
			{
				if (filterText == "")
				{
					// check everything
					foreach (TreeNode node in GetTreeNodeEnumerator(Nodes))
					{
						node.Checked = true;
					}
				}
				else
				{
					foreach (TreeNode node in GetTreeNodeEnumerator(Nodes))
					{
						if (node.Text.ToUpper().Contains(filterText))
						{
							node.Checked = true;

							// check all parent nodes
							for (TreeNode tn = node; tn != null; tn = tn.Parent)
							{
								tn.Checked = true;
							}
						}
						else
						{
							node.Checked = false;
						}
					}
				}
			}

			m_refreshOnTreeNodeChecked = true;
		}

		public int NumUndoStates
		{
			get { return m_nodeCheckStateUndoStack.Count; }
		}

		public int NumRedoStates
		{
			get { return m_nodeCheckStateRedoStack.Count; }
		}

		// select treeNode in treeView, toggle its check state and set the entire subtree's check state to the new state
		public void ToggleSubTree(TreeNode treeNode)
		{
			using (CheckStateBlock())
			{
				SelectedNode = treeNode;

				foreach (TreeNode node in GetTreeNodeEnumerator(treeNode.Nodes))
					node.Checked = !treeNode.Checked;

				treeNode.Checked = !treeNode.Checked;
			}
		}

		public void IsolateSubTree(TreeNode treeNode)
		{
			using (CheckStateBlock())
			{
				// uncheck everything
				foreach (TreeNode subNode in GetTreeNodeEnumerator(Nodes))
					subNode.Checked = false;

				// check all sub nodes
				foreach (TreeNode subNode in GetTreeNodeEnumerator(treeNode.Nodes))
					subNode.Checked = true;

				// check all parent nodes
				for (TreeNode tn = treeNode; tn != null; tn = tn.Parent)
					tn.Checked = true;
			}
		}

		static IEnumerable<TreeNode> GetTreeNodeEnumerator(TreeNodeCollection nodes)
		{
			foreach (TreeNode node in nodes)
			{
				yield return node;

				foreach (TreeNode subNode in GetTreeNodeEnumerator(node.Nodes))
				{
					yield return subNode;
				}
			}
		}

		protected void SelectNode(string[] pathLocations)
		{
			SelectedNode = TreeNodeHierarchy[pathLocations];
			Focus();
			this.PaintNow();
		}

		protected void SelectNextClosestNode()
		{
			if (SelectedNode.NextNode != null)
				SelectedNode = SelectedNode.NextNode;
			else if (SelectedNode.PrevNode != null)
				SelectedNode = SelectedNode.PrevNode;
			else if (SelectedNode.Parent != null)
				SelectedNode = SelectedNode.Parent;
		}

		protected override void OnBeforeCheck(TreeViewCancelEventArgs e)
		{
			base.OnBeforeCheck(e);
			SetUndoNodeBeforeCheckState(e.Node, e.Node.Checked);
		}

		protected override void OnAfterCheck(TreeViewEventArgs e)
		{
			base.OnAfterCheck(e);
			SetUndoNodeAfterCheckState(e.Node, e.Node.Checked);
		}

		protected override void OnNodeMouseClick(TreeNodeMouseClickEventArgs e)
		{
			base.OnNodeMouseClick(e);

			if (e.Button == MouseButtons.Left)
			{
				if (StatoscopeForm.m_ctrlIsBeingPressed)
				{
					IsolateSubTree(e.Node);
				}
			}
			else if (e.Button == MouseButtons.Right)
			{
				ToggleSubTree(e.Node);
			}
		}

		protected override void OnItemDrag(ItemDragEventArgs e)
		{
			base.OnItemDrag(e);

			TreeNode treeNode = (TreeNode)e.Item;
			DoDragDrop("/" + treeNode.FullPath, DragDropEffects.Copy);	
		}

		// a convenience for accessing all TreeNodeHierarchy in a hierarchy - a TreeNodeCollection is just the current level
		public class HierarchyTreeNodeCollection : IEnumerable
		{
			TreeNodeCollection Nodes;

			public HierarchyTreeNodeCollection(TreeNodeCollection nodes)
			{
				Nodes = nodes;
			}

			public TreeNode this[string path]
			{
				get
				{
					return this[path.PathLocations()];
				}
			}

			public TreeNode this[IEnumerable<string> pathLocations]
			{
				get
				{
					TreeNode node = null;
					TreeNodeCollection nodes = Nodes;

					foreach (string location in pathLocations)
					{
						if (!nodes.ContainsKey(location))
						{
							return null;
						}

						node = nodes[location];
						nodes = node.Nodes;
					}

					return node;
				}
			}

			public bool IsChecked(string path)
			{
				TreeNodeCollection nodes = Nodes;

				foreach (string location in path.PathLocations())
				{
					TreeNode node = nodes[location];
					if (node == null || !node.Checked)
						return false;
					nodes = node.Nodes;
				}

				return true;
			}

			public IEnumerator GetEnumerator()
			{
				foreach (TreeNode node in Nodes)
				{
					yield return node;

					foreach (TreeNode subNode in GetTreeNodeEnumerator(node.Nodes))
					{
						yield return subNode;
					}
				}
			}
		}
	}
}
// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Windows.Forms;
using System.Xml.Serialization;

namespace Statoscope
{
	public class RDICheckboxTreeView<T> : CheckboxTreeView where T : RecordDisplayInfo<T>, new()
	{
		protected readonly LogControl LogControl;
		protected readonly T RDITree;

		public RDIHierarchyTreeNodeCollection RDITreeNodeHierarchy;

		protected RDICheckboxTreeView(LogControl logControl, T rdiTree)
		{
			LogControl = logControl;
			RDITree = rdiTree;
			RDITreeNodeHierarchy = new RDIHierarchyTreeNodeCollection(Nodes);
		}

		public virtual void AddRDIs(IEnumerable<T> newToAdd)
		{
			foreach (T rdi in newToAdd)
			{
				TreeNodeCollection nodes = Nodes;
				string[] vanillaLocations = rdi.GetPathLocations(ERDIPathType.Vanilla);
				string[] displayLocations = rdi.GetPathLocations(ERDIPathType.Displayed);

				for (int i = 0; i < displayLocations.Length; i++)
				{
					TreeNode node;
					// nodes are named with the unique vanillaLocation and have the text of the non-unique displayLocation
					string vanillaLocation = vanillaLocations[i];
					string displayLocation = displayLocations[i];

					if (nodes.ContainsKey(vanillaLocation))
					{
						node = nodes[vanillaLocation];
					}
					else
					{
						node = new TreeNode(displayLocation);
						node.Name = vanillaLocation;
						node.Checked = rdi.Displayed;
						if (i == displayLocations.Length - 1)
							node.Tag = rdi;
						nodes.Add(node);
					}

					nodes = node.Nodes;
				}
			}

			EnsureANodeIsSelected();
		}

		protected void RemoveRDIs(IEnumerable<T> rdisToRemove)
		{
			SelectNextClosestNode();

			foreach (T rdi in rdisToRemove)
			{
				RDITreeNodeHierarchy[rdi].Remove();
			}
		}

		public virtual void AddRDIToTree(T rdi)
		{
			List<T> newRDIs = RDITree.Add(rdi);
			AddRDIs(newRDIs);
			LogControl.InvalidateGraphControl();
		}

		public virtual void RemoveRDIFromTree(T rdi)
		{
			List<T> removedRDIs = RDITree.Remove(rdi);
			RemoveRDIs(removedRDIs);
			LogControl.InvalidateGraphControl();
		}

		public void AddChildNodeToSelectedNode()
		{
			string basePath = SelectedRDINode.Path;
			string name = SelectedRDINode.GetNewChildName("newNode");
			T rdi = RecordDisplayInfo<T>.CreateNewRDI(basePath, name);
			AddRDIToTree(rdi);
			SelectedRDINode = rdi;
		}

		public void RemoveSelectedNode()
		{
			if ((SelectedNode != null) && (SelectedNode.Parent != null))
				RemoveRDIFromTree(SelectedRDINode);
		}

		public virtual void SerializeInRDIs(T[] rdis)
		{
			foreach (T rdi in RDITree.GetEnumerator(ERDIEnumeration.Full))
				rdi.Displayed = false;

			foreach (T rdi in rdis)
			{
				T treeNode = RDITree[rdi.Path];
				if (treeNode != null)
					CopySerializedMembers(treeNode, rdi);
			}
		}

		public T[] SerializeOutRDIs()
		{
			return RDITree.GetSerializeableArray();
		}

		protected static void CopySerializedMembers(object target, object source)
		{
			Func<object, bool> isDontCopyAttribute = a => (a.GetType() == typeof(XmlIgnoreAttribute)) || (a.GetType() == typeof(SerializeDontCopy));
			Func<MemberInfo, bool> hasXmlIgnoreAttribute = mi => mi.GetCustomAttributes(false).Where(isDontCopyAttribute).Count() != 0;

			foreach (FieldInfo fi in source.GetType().GetFields(BindingFlags.Instance | BindingFlags.Public))
				if (!hasXmlIgnoreAttribute(fi) && !fi.IsInitOnly)
					fi.SetValue(target, fi.GetValue(source));

			foreach (PropertyInfo pi in source.GetType().GetProperties(BindingFlags.Instance | BindingFlags.Public))
				if (!hasXmlIgnoreAttribute(pi) && pi.CanWrite)
					pi.SetValue(target, pi.GetValue(source, null), null);
		}

		public override void Undo()
		{
			base.Undo();
			LogControl.InvalidateGraphControl();
		}

		public override void Redo()
		{
			base.Redo();
			LogControl.InvalidateGraphControl();
		}

		public override void FilterTextChanged(string filterText)
		{
			base.FilterTextChanged(filterText);
			LogControl.InvalidateGraphControl();
		}

		public T SelectedRDINode
		{
			get { return SelectedNode.Tag as T; }
			set { SelectNode(value.GetPathLocations(ERDIPathType.Vanilla)); }
		}

		void EnsureANodeIsSelected()
		{
			if ((SelectedNode == null) && (Nodes.Count > 0))
				SelectedRDINode = Nodes[0].Tag as T;
		}

		protected override void OnAfterCheck(TreeViewEventArgs e)
		{
			base.OnAfterCheck(e);

			RecordDisplayInfo<T> rdi = (RecordDisplayInfo<T>)e.Node.Tag;
			rdi.Displayed = e.Node.Checked;

			if (m_refreshOnTreeNodeChecked)
				LogControl.InvalidateGraphControl();
    }

		protected override void OnAfterSelect(TreeViewEventArgs e)
		{
			base.OnAfterSelect(e);
			LogControl.SelectItemInfo(e.Node.Tag);
		}

		protected override void OnNodeMouseClick(TreeNodeMouseClickEventArgs e)
		{
			base.OnNodeMouseClick(e);

			if (e.Button == MouseButtons.Left)
			{
				if (StatoscopeForm.m_ctrlIsBeingPressed)
				{
					LogControl.InvalidateGraphControl();
				}
			}
			else if (e.Button == MouseButtons.Right)
			{
				LogControl.InvalidateGraphControl();
			}

			// don't SelectItemInfo if the click is on the check box or +/-
			TreeViewHitTestInfo tvhti = HitTest(e.Location);
			if (tvhti == null || (tvhti.Location != TreeViewHitTestLocations.StateImage && tvhti.Location != TreeViewHitTestLocations.PlusMinus))
			{
				LogControl.SelectItemInfo(e.Node.Tag);
			}
		}

		public class RDIHierarchyTreeNodeCollection : HierarchyTreeNodeCollection
		{
			public RDIHierarchyTreeNodeCollection(TreeNodeCollection nodes)
				: base(nodes)
			{
			}

			public TreeNode this[RecordDisplayInfo<T> rdi]
			{
				get
				{
					return this[rdi.GetPathLocations(ERDIPathType.Vanilla)];
				}
			}
		}
	}
}
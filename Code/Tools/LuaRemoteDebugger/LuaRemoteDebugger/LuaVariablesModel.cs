// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Aga.Controls.Tree;

namespace LuaRemoteDebugger
{
	// Class used to populate the Advance TreeView control: http://sourceforge.net/projects/treeviewadv/
	public class LuaVariableItem
	{
		public LuaVariableItem(ILuaAnyValue _key, ILuaAnyValue _value)
		{
			key = _key;
			value = _value;
		}

		private ILuaAnyValue key;
		private ILuaAnyValue value;

		public ILuaAnyValue LuaKey { get { return key; } }
		public ILuaAnyValue LuaValue { get { return value; } }

		public string Key { get { return key.ToString(); } }
		public string Value { get { return value.ToString(); } }
		public string KeyType { get { return key.Type.ToString(); } }
		public string ValueType { get { return value.Type.ToString(); } }
	}

	class ExpandedNodesDictionary : Dictionary<ILuaAnyValue, ExpandedNodesDictionary>
	{
		public ExpandedNodesDictionary(bool isExpanded)
		{
			this.isExpanded = isExpanded;
		}
		private bool isExpanded;
		public bool IsExpanded { get { return isExpanded; } }
	}

	class LuaVariablesModel : ITreeModel
	{
		LuaTable rootLuaTable = new LuaTable();
		LuaTable filteredLuaTable;
		int filter = -1;

		public LuaTable RootLuaTable { get { return rootLuaTable; } }
		public LuaTable FilteredLuaTable { get { return filteredLuaTable; } }

		public void SetFilter(int filter, bool force)
		{
			if (this.filter != filter || force)
			{
				this.filter = filter;
				filteredLuaTable = rootLuaTable.Value.FirstOrDefault(kvp => kvp.Key.ToString() == filter.ToString()).Value as LuaTable;
				OnStructureChanged(new TreePathEventArgs(TreePath.Empty));
			}
		}

		public virtual System.Collections.IEnumerable GetChildren(TreePath treePath)
		{
			List<LuaVariableItem> children = new List<LuaVariableItem>();
			LuaTable currentTable = null;
			if (treePath.IsEmpty())
			{
				currentTable = filteredLuaTable != null ? filteredLuaTable : filter < 0 ? rootLuaTable : null;
			}
			else
			{
				LuaVariableItem item = treePath.LastNode as LuaVariableItem;
				if (item != null)
				{
					currentTable = item.LuaValue as LuaTable;
				}
				else
				{
					LuaWatchVariable watchVariable = treePath.LastNode as LuaWatchVariable;
					if (watchVariable != null)
					{
						currentTable = watchVariable.LuaValue as LuaTable;
					}
				}
			}
			if (currentTable != null)
			{
				children.Capacity = currentTable.Value.Count;
				foreach (var kvp in currentTable.Value)
				{
					LuaVariableItem item = new LuaVariableItem(kvp.Key, kvp.Value);
					children.Add(item);
				}
			}
			// Sort items alphabetically by Key name
			children.Sort((x, y) => x.Key.CompareTo(y.Key));
			return children;
		}

		public virtual bool IsLeaf(TreePath treePath)
		{
			LuaVariableItem item = treePath.LastNode as LuaVariableItem;
			if (item != null)
			{
				if (item.LuaValue is LuaTable)
				{
					return false;
				}
			}
			return true;
		}

		public void SetLuaTable(LuaTable table, TreeNodeAdv rootNode)
		{
			ExpandedNodesDictionary expanded = SaveExpandedNodes(rootNode);
			rootLuaTable = table;
			SetFilter(this.filter, true);
			RestoreExpandedNodes(rootNode, expanded);
		}

		protected ExpandedNodesDictionary SaveExpandedNodes(TreeNodeAdv rootNode)
		{
			ExpandedNodesDictionary expandedObjects = new ExpandedNodesDictionary(rootNode.IsExpanded);
			foreach (TreeNodeAdv child in rootNode.Children)
			{
				LuaVariableItem item = child.Tag as LuaVariableItem;
				if (item != null && item.LuaValue.Type == LuaVariableType.Table)
				{
					expandedObjects[item.LuaKey] = SaveExpandedNodes(child);
				}
			}
			return expandedObjects;
		}

		protected void RestoreExpandedNodes(TreeNodeAdv rootNode, ExpandedNodesDictionary expanded)
		{
			rootNode.IsExpanded = expanded.IsExpanded;
			foreach (TreeNodeAdv child in rootNode.Children)
			{
				LuaVariableItem item = child.Tag as LuaVariableItem;
				if (item != null)
				{
					ExpandedNodesDictionary childExpanded;
					if (expanded.TryGetValue(item.LuaKey, out childExpanded))
					{
						RestoreExpandedNodes(child, childExpanded);
					}
				}
			}
		}

		protected void OnNodesChanged(TreeModelEventArgs e)
		{
			if (NodesChanged != null)
			{
				NodesChanged(this, e);
			}
		}

		protected void OnNodesInserted(TreeModelEventArgs e)
		{
			if (NodesInserted != null)
			{
				NodesInserted(this, e);
			}
		}

		protected void OnNodesRemoved(TreeModelEventArgs e)
		{
			if (NodesRemoved != null)
			{
				NodesRemoved(this, e);
			}
		}

		protected void OnStructureChanged(TreePathEventArgs e)
		{
			if (StructureChanged != null)
			{
				StructureChanged(this, e);
			}
		}

		public event EventHandler<TreeModelEventArgs> NodesChanged;
		public event EventHandler<TreeModelEventArgs> NodesInserted;
		public event EventHandler<TreeModelEventArgs> NodesRemoved;
		public event EventHandler<TreePathEventArgs> StructureChanged;
	}

	public class LuaWatchVariable
	{
		public LuaWatchVariable(string _name, ILuaAnyValue _value)
		{
			name = _name;
			value = _value;
		}

		private string name;
		private ILuaAnyValue value;

		public string Name { get { return name; } set { name = value; } }
		public ILuaAnyValue LuaValue { get { return value; } set { this.value = value; } }

		public string Key { get { return name; } }
		public string Value { get { return value.ToString(); } }
		public string KeyType { get { return "Watch"; } }
		public string ValueType { get { return value.Type.ToString(); } }
	}

	class LuaWatchModel : LuaVariablesModel
	{
		List<LuaWatchVariable> watchVariables = new List<LuaWatchVariable>();
		LuaWatchVariable newWatchVariable = new LuaWatchVariable("", LuaNil.Instance);

		public LuaWatchVariable NewWatchVariable { get { return newWatchVariable; } }
		public List<LuaWatchVariable> WatchVariables { get { return watchVariables; } }

		public override System.Collections.IEnumerable GetChildren(TreePath treePath)
		{
			if (treePath.IsEmpty())
			{
				List<LuaWatchVariable> variables = new List<LuaWatchVariable>(watchVariables);
				variables.Add(newWatchVariable);
				return variables;
			}
			else
			{
				return base.GetChildren(treePath);
			}
		}

		public override bool IsLeaf(TreePath treePath)
		{
			LuaWatchVariable item = treePath.LastNode as LuaWatchVariable;
			if (item != null)
			{
				if (item.LuaValue is LuaTable)
				{
					return false;
				}
			}
			return base.IsLeaf(treePath);
		}

		public void AddWatchVariable(LuaWatchVariable variable)
		{
			watchVariables.Add(variable);
			OnNodesInserted(new TreeModelEventArgs(TreePath.Empty, new int[] { watchVariables.Count - 1 }, new object[] { variable }));
		}

		public void WatchVariableChanged(LuaWatchVariable variable)
		{
			//OnNodesChanged(new TreeModelEventArgs(TreePath.Empty, new object[] { variable }));
			OnNodesRemoved(new TreeModelEventArgs(TreePath.Empty, new object[] { variable }));
			OnNodesInserted(new TreeModelEventArgs(TreePath.Empty, new int[] { watchVariables.IndexOf(variable) }, new object[] { variable }));
		}

		public void DeleteWatchVariable(LuaWatchVariable variable)
		{
			if (watchVariables.Remove(variable))
			{
				OnNodesRemoved(new TreeModelEventArgs(TreePath.Empty, new object[] { variable }));
			}
		}

		public void RefreshStructure(TreeNodeAdv rootNode)
		{
			var expandedWatches = new List<ExpandedNodesDictionary>();
			foreach (TreeNodeAdv child in rootNode.Children)
			{
				expandedWatches.Add(SaveExpandedNodes(child));
			}
			OnStructureChanged(new TreePathEventArgs(TreePath.Empty));
			for (int i=0; i<expandedWatches.Count && i<rootNode.Children.Count; ++i)
			{
				RestoreExpandedNodes(rootNode.Children[i], expandedWatches[i]);
			}
		}
	}
}

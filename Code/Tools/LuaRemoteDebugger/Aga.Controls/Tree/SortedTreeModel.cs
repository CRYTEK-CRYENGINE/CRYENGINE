// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Collections;

namespace Aga.Controls.Tree
{
	public class SortedTreeModel: TreeModelBase
	{
		private ITreeModel _innerModel;
		public ITreeModel InnerModel
		{
			get { return _innerModel; }
		}

		private IComparer _comparer;
		public IComparer Comparer
		{
			get { return _comparer; }
			set 
			{ 
				_comparer = value;
				OnStructureChanged(new TreePathEventArgs(TreePath.Empty));
			}
		}

		public SortedTreeModel(ITreeModel innerModel)
		{
			_innerModel = innerModel;
			_innerModel.NodesChanged += new EventHandler<TreeModelEventArgs>(_innerModel_NodesChanged);
			_innerModel.NodesInserted += new EventHandler<TreeModelEventArgs>(_innerModel_NodesInserted);
			_innerModel.NodesRemoved += new EventHandler<TreeModelEventArgs>(_innerModel_NodesRemoved);
			_innerModel.StructureChanged += new EventHandler<TreePathEventArgs>(_innerModel_StructureChanged);
		}

		void _innerModel_StructureChanged(object sender, TreePathEventArgs e)
		{
			OnStructureChanged(e);
		}

		void _innerModel_NodesRemoved(object sender, TreeModelEventArgs e)
		{
			OnStructureChanged(new TreePathEventArgs(e.Path));
		}

		void _innerModel_NodesInserted(object sender, TreeModelEventArgs e)
		{
			OnStructureChanged(new TreePathEventArgs(e.Path));
		}

		void _innerModel_NodesChanged(object sender, TreeModelEventArgs e)
		{
			OnStructureChanged(new TreePathEventArgs(e.Path));
		}

		public override IEnumerable GetChildren(TreePath treePath)
		{
			if (Comparer != null)
			{
				ArrayList list = new ArrayList();
				IEnumerable res = InnerModel.GetChildren(treePath);
				if (res != null)
				{
					foreach (object obj in res)
						list.Add(obj);
					list.Sort(Comparer);
					return list;
				}
				else
					return null;
			}
			else
				return InnerModel.GetChildren(treePath);
		}

		public override bool IsLeaf(TreePath treePath)
		{
			return InnerModel.IsLeaf(treePath);
		}
	}
}

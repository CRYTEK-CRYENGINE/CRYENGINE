// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Text;
using System.Collections.ObjectModel;

namespace Aga.Controls.Tree
{
	public class TreePath
	{
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Security", "CA2104:DoNotDeclareReadOnlyMutableReferenceTypes")]
		public static readonly TreePath Empty = new TreePath();

		private object[] _path;
		public object[] FullPath
		{
			get { return _path; }
		}

		public object LastNode
		{
			get
			{
				if (_path.Length > 0)
					return _path[_path.Length - 1];
				else
					return null;
			}
		}

		public object FirstNode
		{
			get
			{
				if (_path.Length > 0)
					return _path[0];
				else
					return null;
			}
		}

		public TreePath()
		{
			_path = new object[0];
		}

		public TreePath(object node)
		{
			_path = new object[] { node };
		}

		public TreePath(object[] path)
		{
			_path = path;
		}

		public TreePath(TreePath parent, object node)
		{
			_path = new object[parent.FullPath.Length + 1];
			for (int i = 0; i < _path.Length - 1; i++)
				_path[i] = parent.FullPath[i];
			_path[_path.Length - 1] = node;
		}

		public bool IsEmpty()
		{
			return (_path.Length == 0);
		}
	}
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.ComponentModel;

namespace Aga.Controls.Tree.NodeControls
{
	public abstract class InteractiveControl : BindableControl
	{
		private bool _editEnabled = false;
		[DefaultValue(false)]
		public bool EditEnabled
		{
			get { return _editEnabled; }
			set { _editEnabled = value; }
		}

		protected bool IsEditEnabled(TreeNodeAdv node)
		{
			if (EditEnabled)
			{
				NodeControlValueEventArgs args = new NodeControlValueEventArgs(node);
				args.Value = true;
				OnIsEditEnabledValueNeeded(args);
				return Convert.ToBoolean(args.Value);
			}
			else
				return false;
		}

		public event EventHandler<NodeControlValueEventArgs> IsEditEnabledValueNeeded;
		private void OnIsEditEnabledValueNeeded(NodeControlValueEventArgs args)
		{
			if (IsEditEnabledValueNeeded != null)
				IsEditEnabledValueNeeded(this, args);
		}
	}
}

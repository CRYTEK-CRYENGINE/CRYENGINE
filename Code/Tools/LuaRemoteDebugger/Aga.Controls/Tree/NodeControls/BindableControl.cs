// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Reflection;
using System.ComponentModel;

namespace Aga.Controls.Tree.NodeControls
{
	public abstract class BindableControl : NodeControl
	{
		private struct MemberAdapter
		{
			private object _obj;
			private PropertyInfo _pi;
			private FieldInfo _fi;

			public static readonly MemberAdapter Empty = new MemberAdapter();

			public Type MemberType
			{
				get
				{
					if (_pi != null)
						return _pi.PropertyType;
					else if (_fi != null)
						return _fi.FieldType;
					else
						return null;
				}
			}

			public object Value
			{
				get
				{
					if (_pi != null && _pi.CanRead)
						return _pi.GetValue(_obj, null);
					else if (_fi != null)
						return _fi.GetValue(_obj);
					else
						return null;
				}
				set
				{
					if (_pi != null && _pi.CanWrite)
						_pi.SetValue(_obj, value, null);
					else if (_fi != null)
						_fi.SetValue(_obj, value);
				}
			}

			public MemberAdapter(object obj, PropertyInfo pi)
			{
				_obj = obj;
				_pi = pi;
				_fi = null;
			}

			public MemberAdapter(object obj, FieldInfo fi)
			{
				_obj = obj;
				_fi = fi;
				_pi = null;
			}
		}

		#region Properties

		private bool _virtualMode = false;
		[DefaultValue(false), Category("Data")]
		public bool VirtualMode
		{
			get { return _virtualMode; }
			set { _virtualMode = value; }
		}

		private string _propertyName = "";
		[DefaultValue(""), Category("Data")]
		public string DataPropertyName
		{
			get { return _propertyName; }
			set 
			{
				if (_propertyName == null)
					_propertyName = string.Empty;
				_propertyName = value; 
			}
		}

		private bool _incrementalSearchEnabled = false;
		[DefaultValue(false)]
		public bool IncrementalSearchEnabled
		{
			get { return _incrementalSearchEnabled; }
			set { _incrementalSearchEnabled = value; }
		}

		#endregion

		public virtual object GetValue(TreeNodeAdv node)
		{
			if (VirtualMode)
			{
				NodeControlValueEventArgs args = new NodeControlValueEventArgs(node);
				OnValueNeeded(args);
				return args.Value;
			}
			else
			{
				try
				{
					return GetMemberAdapter(node).Value;
				}
				catch (TargetInvocationException ex)
				{
					if (ex.InnerException != null)
						throw new ArgumentException(ex.InnerException.Message, ex.InnerException);
					else
						throw new ArgumentException(ex.Message);
				}
			}
		}

		public virtual void SetValue(TreeNodeAdv node, object value)
		{
			if (VirtualMode)
			{
				NodeControlValueEventArgs args = new NodeControlValueEventArgs(node);
				args.Value = value;
				OnValuePushed(args);
			}
			else
			{
				try
				{
					MemberAdapter ma = GetMemberAdapter(node);
					ma.Value = value;
				}
				catch (TargetInvocationException ex)
				{
					if (ex.InnerException != null)
						throw new ArgumentException(ex.InnerException.Message, ex.InnerException);
					else
						throw new ArgumentException(ex.Message);
				}
			}
		}

		public Type GetPropertyType(TreeNodeAdv node)
		{
			return GetMemberAdapter(node).MemberType;
		}

		private MemberAdapter GetMemberAdapter(TreeNodeAdv node)
		{
			if (node.Tag != null && !string.IsNullOrEmpty(DataPropertyName))
			{
				Type type = node.Tag.GetType();
				PropertyInfo pi = type.GetProperty(DataPropertyName);
				if (pi != null)
					return new MemberAdapter(node.Tag, pi);
				else
				{
					FieldInfo fi = type.GetField(DataPropertyName, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
					if (fi != null)
						return new MemberAdapter(node.Tag, fi);
				}
			}
			return MemberAdapter.Empty;
		}

		public override string ToString()
		{
			if (string.IsNullOrEmpty(DataPropertyName))
				return GetType().Name;
			else
				return string.Format("{0} ({1})", GetType().Name, DataPropertyName);
		}

		public event EventHandler<NodeControlValueEventArgs> ValueNeeded;
		private void OnValueNeeded(NodeControlValueEventArgs args)
		{
			if (ValueNeeded != null)
				ValueNeeded(this, args);
		}

		public event EventHandler<NodeControlValueEventArgs> ValuePushed;
		private void OnValuePushed(NodeControlValueEventArgs args)
		{
			if (ValuePushed != null)
				ValuePushed(this, args);
		}
	}
}

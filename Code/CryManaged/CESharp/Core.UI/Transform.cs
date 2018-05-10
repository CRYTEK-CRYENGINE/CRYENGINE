// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;

namespace CryEngine.UI
{
	/// <summary>
	/// Represents location of a SceneObject in space. Holds a SceneObject's hierarchical children.
	/// </summary>
	public class Transform : UIComponent, IEnumerator<Transform>
	{
		/// <summary>
		/// Raised if parent SceneObject was changed.
		/// </summary>
		public event Action ParentChanged;

		Transform _parent = null;
		int _childrenPosition = -1;
		Transform _currentChild = null;

		/// <summary>
		/// Moves this Transform from one SceneObject to another.
		/// </summary>
		/// <value>The parent.</value>
		public Transform Parent
		{
			get
			{
				return _parent;
			}
			set
			{
				_parent?.Children.Remove(this);
				_parent = value;
				_parent?.Children.Add(this);
				if(ParentChanged != null)
				{
					ParentChanged();
				}
			}
		}

		/// <summary>
		/// Decending SceneObjects.
		/// </summary>
		public List<Transform> Children = new List<Transform>();

		[DataMember(Name = "Children")]
		List<SceneObject> _childOwners
		{
			get
			{
				return Children.Select(x => x.Owner).ToList();
			}
			set
			{
				Children = value.Select(x => x.GetComponent<Transform>()).ToList();
				foreach(var ch in value)
				{
					ch.Transform._parent = this;
				}
			}
		}

		/// <summary>
		/// IEnumerator specific.
		/// </summary>
		public IEnumerator<Transform> GetEnumerator()
		{
			Reset();
			return this;
		}

		/// <summary>
		/// IEnumerator specific.
		/// </summary>
		public bool MoveNext()
		{
			if(++_childrenPosition >= Children.Count)
			{
				return false;
			}

			_currentChild = Children[_childrenPosition];
			return true;
		}

		/// <summary>
		/// IEnumerator specific.
		/// </summary>
		public void Reset()
		{
			_childrenPosition = -1;
		}

		/// <summary>
		/// IEnumerator specific.
		/// </summary>
		void IDisposable.Dispose() 
		{ 
			
		}

		/// <summary>
		/// IEnumerator specific.
		/// </summary>
		public Transform Current { get { return _currentChild; } }
		object IEnumerator.Current { get { return _currentChild; } }

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnDestroy()
		{
			Children.Clear();
		}
	}
}

// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Reflection;
using System.Diagnostics;
using System.Runtime.Serialization;
using System.IO;
using CryEngine.Components;

namespace CryEngine
{
	/// <summary>
	/// Represents any single entity or a logical object in a scene. Allows for hierarchical representation of SceneObjects to form a scene tree. Handles a list of Components for own modification and control.
	/// </summary>
	[DebuggerDisplay("SceneObject({Name})"), DataContract]
	public class SceneObject : IUpdateReceiver
	{
		public event EventHandler<SceneObject> ActiveChanged; ///< Called if Active property was changed

		public Transform Transform { get; private set; } ///< Defines position in space for this SceneObject. Points to Child SceneObjects
		[DataMember]
		public string Name; ///< Name of the SceneObject
		[DataMember]
		public List<Component> Components { get { return _components; } } ///< Modifier Components for this SceneObject
		public SceneObject Root { get { return Parent == null ? this : Parent.Root; } } ///< First SceneObject in this scene hierarchy
		public SceneObject Parent { get { return Transform.Parent == null ? null : Transform.Parent.Owner; } } ///< Direct Parent object

		static int _updateOrder;
		List<Component> _components = new List<Component>();
		[DataMember(Name="ACT")]
		protected bool _isActive = true;
		[DataMember(Name="ABH")]
		protected bool _isActiveByHierarchy = true;
		MethodInfo _OnAwake = null;
		MethodInfo _OnUpdate = null;
		MethodInfo _OnDestroy = null;

		public bool Active
		{
			set 
			{
				foreach (var c in Components)
					c.ActiveByHierarchy = _isActiveByHierarchy && value;
				foreach (var t in Transform)
					t.Owner.ActiveByHierarchy = _isActiveByHierarchy && value;
				
				if (_isActive != value) 
				{
					_isActive = value;
					if (ActiveChanged != null)
						ActiveChanged (this);
				}
			}
			get 
			{
				return _isActive;
			}
		} ///< Defines whether this object and its children and components are updated or not 

		public bool ActiveByHierarchy
		{
			set 
			{
				foreach (var c in Components)
					c.ActiveByHierarchy = value && Active;
				foreach (var t in Transform)
					t.Owner.ActiveByHierarchy = value && Active;
				
				_isActiveByHierarchy = value;
			}
			get 
			{
				return _isActiveByHierarchy && Active;
			}
		} ///< Propagates hierarchical activity flag to children and components. Do not set directly.

		void InitMethods(Type t)
		{
			_OnAwake = t.GetMethod ("OnAwake");
			_OnUpdate = t.GetMethod ("OnUpdate");
			_OnDestroy = t.GetMethod ("OnDestroy");
		}

		/// <summary>
		/// Creates an instance of a SceneObject and wires it into scene hierarchy.
		/// </summary>
		/// <param name="parent">Intended parent Object.</param>
		/// <param name="name">Optional Object Name. Will use class name if null.</param>
		public static SceneObject Instantiate(SceneObject parent, string name = null)
		{
			return Instantiate<SceneObject> (parent, name);
		}

		/// <summary>
		/// Creates an instance of a T and wires it into scene hierarchy.
		/// </summary>
		/// <param name="parent">Intended Parent.</param>
		/// <param name="name">Optional Object Name. Will use class name if null.</param>
		/// <typeparam name="T">Type of the instance.</typeparam>
		public static T Instantiate<T>(SceneObject parent, string name = null) where T : SceneObject
		{
			T instance = (T)Activator.CreateInstance (typeof(T));
			instance.Name = name ?? typeof(T).Name;
			instance.Transform = instance.AddComponent<Transform> ();
			instance.Transform.Parent = parent == null ? null : parent.Transform;
			instance.InitMethods (typeof(T));

			if (instance._OnAwake != null)
				instance._OnAwake.InvokeSecure (instance);

			if (instance._OnUpdate != null)
				SceneManager.InvalidateSceneOrder (instance.Root);
			return instance;
		}

		/// <summary>
		/// Refreshs update order in hierarchical layer of this SceneObject and its decentants.
		/// </summary>
		public void RefreshUpdateOrder()
		{
			if (this == Root)
				_updateOrder = 0;

			if (_OnUpdate != null)
				SceneManager.RegisterUpdateReceiver (this, ++_updateOrder);

			Components.ForEach(x => x.TryRegisterUpdateReceiver(++_updateOrder));
			Transform.Children.ForEach(x => x.Owner.RefreshUpdateOrder());
		}

		/// <summary>
		/// Gets the first component of type T.
		/// </summary>
		public T GetComponent<T>() where T : Component
		{
			return Components.FirstOrDefault(x => x is T) as T;
		}

		/// <summary>
		/// Adds a component of type T.
		/// </summary>
		public T AddComponent<T>() where T : Component
		{			
			var c = Component.Instantiate<T>(this);
			c.ActiveByHierarchy = ActiveByHierarchy;
			Components.Add (c);
			if (c.IsUpdateable)
				SceneManager.InvalidateSceneOrder (Root);
			return c;
		}

		/// <summary>
		/// Calls 'fkt' on each component in hierarchy. Optionally does not process inactive components.
		/// </summary>
		public bool ForEachComponent(Func<Component, bool> fkt, bool testForActivity = true)
		{
			if (testForActivity && !ActiveByHierarchy)
				return false;
			foreach (var t in Transform.Children) 
			{
				if (t.Owner.ForEachComponent (fkt, testForActivity))
					return true;
			}
			foreach (var c in Components)
				if ((!testForActivity || c.ActiveByHierarchy) && fkt (c))
					return true;
			return false;
		}

		/// <summary>
		/// Same as ForEachComponent, but calls 'fkt' in inverse component order.
		/// </summary>
		public bool ForEachComponentReverse(Func<Component, bool> fkt, bool testForActivity = true)
		{
			if (testForActivity && !ActiveByHierarchy)
				return false;
			foreach (var c in Components.Reverse<Component>())
				if ((!testForActivity || c.ActiveByHierarchy) && fkt (c))
					return true;
			foreach (var t in Transform.Children.Reverse<Transform>()) 
			{
				if (t.Owner.ForEachComponent (fkt, testForActivity))
					return true;
			}
			return false;
		}

		public void ForEach<T>(Action<T> a) where T : SceneObject
		{
			foreach (var t in Transform.Children) 
			{
				if (t.Owner is T)
					a (t.Owner as T);
				t.Owner.ForEach<T> (a);
			}
		}

		public void ForEachComponent<C>(Action<C> a) where C : Component
		{
			foreach (var c in Components)
				if (c is C)
					a (c as C);
				
			foreach (var t in Transform.Children) 
				t.Owner.ForEachComponent<C> (a);
		}

		/// <summary>
		/// Makes this object the first to be updated in its hierarchical layer.
		/// </summary>
		public void SetAsFirstSibling()
		{
			Parent.Transform.Children.Remove(Transform);
			Parent.Transform.Children.Insert(0, Transform);
			SceneManager.InvalidateSceneOrder(Root);
		}

		/// <summary>
		/// Makes this object the last to be updated in its hierarchical layer.
		/// </summary>
		public void SetAsLastSibling()
		{
			Parent.Transform.Children.Remove(Transform);
			Parent.Transform.Children.Add(Transform);
			SceneManager.InvalidateSceneOrder(Root);
		}

		/// <summary>
		/// Finds and returns the first SceneObject with 'name' in self and children.
		/// </summary>
		/// <returns>The First Object wich matches in Name.</returns>
		/// <param name="name">Name to be searched for.</param>
		public SceneObject FindFirst(string name)
		{
			if (Name == name)
				return this;
			foreach (var t in Transform)
				if (t.Owner.Name == name)
					return t.Owner;
			return null;
		}

		/// <summary>
		/// Removes a specific component from this SceneObject.
		/// </summary>
		public void RemoveComponent(Component c)
		{
			Components.Remove (c);
			c.Destroy ();
		}

		/// <summary>
		/// Called by Framweork internally. Not to be called actively.
		/// </summary>
		public void Update()
		{
			if (ActiveByHierarchy)
				_OnUpdate.Invoke (this, null);
		}

		/// <summary>
		/// Outputs current scene hierarchy to CryEngine's standard log.
		/// </summary>
		public void PrintScene(int depth = 0)
		{
			string spaces = "";
			for (int i = 0; i < depth; i++)
				spaces += "  ";
			
			Log.Info (spaces + Name + " {");
			foreach (var c in Components)
				Log.Info (spaces + "  [" + c.GetType().Name + "]");

			foreach (var t in Transform)
				t.Owner.PrintScene (depth+1);
			Log.Info (spaces + "}");
		}

		/// <summary>
		/// Deinitialized this SceneObject and all its hierarchical decendants.
		/// </summary>
		public void Destroy ()
		{
			if (_OnUpdate != null)
				SceneManager.RemoveUpdateReceiver(this);

			if (_OnDestroy != null)
				_OnDestroy.Invoke (this, null);

			var children = new List<Transform> (Transform.Children);
			foreach (var t in children)
				t.Owner.Destroy ();

			var components = new List<Component> (Components);
			components.ForEach (x => x.Destroy ());

			if (Parent != null)
				Parent.Transform.Children.Remove (Transform);
		}

		/// <summary>
		/// Serializes this object and all its hierarchical decendants to JSON.
		/// </summary>
		public string ToJSON()
		{
			return Tools.ToJSON (this);
		}

		/// <summary>
		/// Creates a human readable JSON serialization and stores it on file.
		/// </summary>
		/// <param name="name">File name to be used. Stores under SceneObject's Name if null.</param>
		public void Store(string name = null)
		{
			if (!Directory.Exists (Application.DataPath + "/scenes/"))
				Directory.CreateDirectory (Application.DataPath + "/scenes/");

			// Make output human readable
			var str = ToJSON ();
			var fmtStr = str;
			var fmtStrIdx = 0;
			var tabs = "";
			for (int i = 0; i < str.Length; i++) 
			{
				var fmtSubStr = fmtStr.Substring(fmtStrIdx);
				if (fmtSubStr.StartsWith ("[{"))
					tabs += "\t";
				if (fmtSubStr.StartsWith ("{\"__type\"")) 
				{
					fmtStr = fmtStr.Insert (fmtStrIdx, "\r\n" + tabs);
					fmtStrIdx += ("\r\n" + tabs).Length;
				}
				if (fmtSubStr.StartsWith ("}]")) 
				{
					tabs = tabs.Remove(0, 1);
					fmtStr = fmtStr.Insert (fmtStrIdx, "\r\n" + tabs);
					fmtStrIdx += ("\r\n" + tabs).Length;
				}
				fmtStrIdx++;
			}

			var f = System.IO.File.CreateText (Application.DataPath + "/scenes/" + (name ?? Name) + ".scn");
			f.Write (fmtStr);
			f.Close ();
		}
	}
}

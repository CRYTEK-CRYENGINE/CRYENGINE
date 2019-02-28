// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace CryEngine.UI
{
	/// <summary>
	/// Represents any single entity or a logical object in a scene. Allows for hierarchical representation of SceneObjects to form a scene tree. Handles a list of Components for own modification and control.
	/// </summary>
	[DebuggerDisplay("SceneObject({Name})")]
	public class SceneObject
	{
		private static int _updateOrder;

		private List<UIComponent> _components = new List<UIComponent>();
		private bool _isActive = true;
		private bool _isActiveByHierarchy = true;

		internal Action AwakeAction{ get; private set; }
		internal Action UpdateAction{ get; private set; }
		internal Action RenderAction{ get; private set; }
		internal Action DestroyAction{ get; private set; }

		/// <summary>
		/// Called if Active property was changed
		/// </summary>
		public event Action<SceneObject> ActiveChanged;

		/// <summary>
		/// Defines position in space for this SceneObject. Points to Child SceneObjects
		/// </summary>
		/// <value>The transform.</value>
		public Transform Transform { get; private set; }

		/// <summary>
		/// Name of the SceneObject
		/// </summary>
		public string Name;

		/// <summary>
		/// Modifier Components for this SceneObject
		/// </summary>
		/// <value>The components.</value>
		public List<UIComponent> Components { get { return _components; } }

		/// <summary>
		/// First SceneObject in this scene hierarchy
		/// </summary>
		/// <value>The root.</value>
		public SceneObject Root { get { return Parent == null ? this : Parent.Root; } }

		/// <summary>
		/// Direct Parent object
		/// </summary>
		/// <value>The parent.</value>
		public SceneObject Parent { get { return Transform.Parent == null ? null : Transform.Parent.Owner; } }

		/// <summary>
		/// Indicates whether this object can be updated.
		/// </summary>
		/// <value><c>true</c> if is updateable; otherwise, <c>false</c>.</value>
		public bool IsUpdateable { get; private set; } = false;

		/// <summary>
		/// Called when this SceneObject is instantiated.
		/// </summary>
		protected virtual void OnAwake() { }

		/// <summary>
		/// Called once every frame.
		/// </summary>
		protected virtual void OnUpdate() { }

		/// <summary>
		/// Called every frame before the frame is rendered and after the normal update.
		/// </summary>
		protected virtual void OnRender() { }

		/// <summary>
		/// Called when this SceneObject is destroyed.
		/// </summary>
		protected virtual void OnDestroy() { }

		/// <summary>
		/// Defines whether this object and its children and components are updated or not 
		/// </summary>
		/// <value><c>true</c> if active; otherwise, <c>false</c>.</value>
		public bool Active
		{
			set
			{
				foreach(var c in Components)
				{
					c.ActiveByHierarchy = _isActiveByHierarchy && value;
				}

				foreach(var t in Transform)
				{
					t.Owner.ActiveByHierarchy = _isActiveByHierarchy && value;
				}

				if(_isActive != value)
				{
					_isActive = value;

					ActiveChanged?.Invoke(this);
				}
			}
			get
			{
				return _isActive;
			}
		}

		/// <summary>
		/// Propagates hierarchical activity flag to children and components. Do not set directly.
		/// </summary>
		/// <value><c>true</c> if active by hierarchy; otherwise, <c>false</c>.</value>
		public bool ActiveByHierarchy
		{
			set
			{
				foreach(var c in Components)
				{
					c.ActiveByHierarchy = value && Active;
				}

				foreach(var t in Transform)
				{
					t.Owner.ActiveByHierarchy = value && Active;
				}

				_isActiveByHierarchy = value;
			}
			get
			{
				return _isActiveByHierarchy && Active;
			}
		}

		private void InspectOverrides(Type t)
		{
			var baseType = typeof(SceneObject);
			var flags = System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic;
			var awake = t.GetMethod(nameof(OnAwake), flags);
			var update = t.GetMethod(nameof(OnUpdate), flags);
			var render = t.GetMethod(nameof(OnRender), flags);
			var destroy = t.GetMethod(nameof(OnDestroy), flags);

			AwakeAction = awake.DeclaringType == baseType ? null : (Action)Delegate.CreateDelegate(typeof(Action), this, awake);
			UpdateAction = update.DeclaringType == baseType ? null : (Action)Delegate.CreateDelegate(typeof(Action), this, update);
			RenderAction = render.DeclaringType == baseType ? null : (Action)Delegate.CreateDelegate(typeof(Action), this, render);
			DestroyAction = destroy.DeclaringType == baseType ? null : (Action)Delegate.CreateDelegate(typeof(Action), this, destroy);

			IsUpdateable = UpdateAction != null || RenderAction != null;
		}

		/// <summary>
		/// Creates an instance of a SceneObject and wires it into scene hierarchy.
		/// </summary>
		/// <param name="parent">Intended parent Object.</param>
		/// <param name="name">Optional Object Name. Will use class name if null.</param>
		public static SceneObject Instantiate(SceneObject parent, string name = null)
		{
			return Instantiate<SceneObject>(parent, name);
		}

		/// <summary>
		/// Creates an instance of a T and wires it into scene hierarchy.
		/// </summary>
		/// <param name="parent">Intended Parent.</param>
		/// <param name="name">Optional Object Name. Will use class name if null.</param>
		/// <typeparam name="T">Type of the instance.</typeparam>
		public static T Instantiate<T>(SceneObject parent, string name = null) where T : SceneObject
		{
			var instance = (T)Activator.CreateInstance(typeof(T));
			instance.Name = name ?? typeof(T).Name;
			instance.Transform = instance.AddComponent<Transform>();
			instance.Transform.Parent = parent == null ? null : parent.Transform;
			instance.InspectOverrides(typeof(T));
			instance.AwakeAction?.Invoke();
			if(instance.IsUpdateable)
			{
				SceneManager.InvalidateSceneOrder(instance.Root);
			}
			return instance;
		}

		/// <summary>
		/// Refreshs update order in hierarchical layer of this SceneObject and its decentants.
		/// </summary>
		public void RefreshUpdateOrder()
		{
			if(this == Root)
			{
				_updateOrder = 0;
			}

			if(IsUpdateable)
			{
				SceneManager.RegisterUpdateReceiver(Update, Render, Root, ++_updateOrder);
			}

			Components.ForEach(x => x.TryRegisterUpdateReceiver(++_updateOrder));
			Transform.Children.ForEach(x => x.Owner.RefreshUpdateOrder());
		}

		/// <summary>
		/// Gets the first component of type T.
		/// </summary>
		public T GetComponent<T>() where T : UIComponent
		{
			return Components.FirstOrDefault(x => x is T) as T;
		}

		/// <summary>
		/// Returns all components of type T on this SceneObject. Returns an empty list if no components are found.
		/// </summary>
		/// <returns>The components.</returns>
		/// <typeparam name="T">The type of the components to return.</typeparam>
		public List<T> GetComponents<T>() where T : UIComponent
		{
			List<T> list = new List<T>(Components.Count);
			foreach(var component in Components)
			{
				T tComponent = component as T;
				if(tComponent != null)
				{
					list.Add(tComponent);
				}
			}
			return list;
		}

		/// <summary>
		/// Get the first parent SceneObject of type T. Returns null of no parent is of type T.
		/// </summary>
		/// <returns>The parent with type T, or null of none is found.</returns>
		/// <param name="includeSelf">If set to <c>true</c> includes itself while searching for the type.</param>
		/// <typeparam name="T">The type of the parent.</typeparam>
		public T GetParentWithType<T>(bool includeSelf = true) where T : SceneObject
		{
			SceneObject element = includeSelf ? this : Parent;
			while(element != null)
			{
				var foundType = element as T;
				if(foundType != null)
				{
					return foundType;
				}
				element = element.Parent;
			}
			return null;
		}

		/// <summary>
		/// Adds a component of type T.
		/// </summary>
		public T AddComponent<T>() where T : UIComponent
		{
			var c = UIComponent.Instantiate<T>(this);
			c.ActiveByHierarchy = ActiveByHierarchy;
			Components.Add(c);
			if(c.IsUpdateable)
			{
				SceneManager.InvalidateSceneOrder(Root);
			}
			return c;
		}

		/// <summary>
		/// Calls 'fkt' on each component in hierarchy. Optionally does not process inactive components.
		/// </summary>
		public bool ForEachComponent(Func<UIComponent, bool> fkt, bool testForActivity = true)
		{
			if(testForActivity && !ActiveByHierarchy)
			{
				return false;
			}

			foreach(var t in Transform.Children)
			{
				if(t.Owner.ForEachComponent(fkt, testForActivity))
				{
					return true;
				}
			}

			foreach(var c in Components)
			{
				if((!testForActivity || c.ActiveByHierarchy) && fkt(c))
				{
					return true;
				}
			}

			return false;
		}

		/// <summary>
		/// Same as ForEachComponent, but calls 'fkt' in inverse component order.
		/// </summary>
		public bool ForEachComponentReverse(Func<UIComponent, bool> fkt, bool testForActivity = true)
		{
			if(testForActivity && !ActiveByHierarchy)
			{
				return false;
			}

			foreach(var c in Components.Reverse<UIComponent>())
			{
				if((!testForActivity || c.ActiveByHierarchy) && fkt(c))
				{
					return true;
				}
			}

			foreach(var t in Transform.Children.Reverse<Transform>())
			{
				if(t.Owner.ForEachComponent(fkt, testForActivity))
				{
					return true;
				}
			}

			return false;
		}

		/// <summary>
		/// Execute an action on all child objects of Type <typeparamref name="T"/> on this SceneObject.
		/// </summary>
		/// <param name="action">The action that will that will be run.</param>
		/// <typeparam name="T">The type of SceneObjects it will run on.</typeparam>
		public void ForEach<T>(Action<T> action) where T : SceneObject
		{
			foreach(var t in Transform.Children)
			{
				if(t.Owner is T)
				{
					action(t.Owner as T);
				}
				t.Owner.ForEach(action);
			}
		}

		/// <summary>
		/// Execute an action on all components of Type <typeparamref name="C"/> on this SceneObject, and run it also on all child SceneObjects.
		/// </summary>
		/// <param name="action">The action that will be run.</param>
		/// <typeparam name="C">The type of components it will be run on.</typeparam>
		public void ForEachComponent<C>(Action<C> action) where C : UIComponent
		{
			foreach(var component in Components)
			{
				C castComponent = component as C;
				if(castComponent != null)
				{
					action(castComponent);
				}
			}

			foreach(var t in Transform.Children)
			{
				t.Owner.ForEachComponent(action);
			}
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
			if(Name == name)
			{
				return this;
			}

			foreach(var t in Transform)
			{
				if(t.Owner.Name == name)
				{
					return t.Owner;
				}
			}

			return null;
		}

		/// <summary>
		/// Removes a specific component from this SceneObject.
		/// </summary>
		public void RemoveComponent(UIComponent c)
		{
			Components.Remove(c);
			c.Destroy();
		}

		/// <summary>
		/// Called by Framweork internally. Not to be called actively.
		/// </summary>
		private void Update()
		{
			if(ActiveByHierarchy)
			{
				UpdateAction?.Invoke();
			}
		}

		private void Render()
		{
			if(ActiveByHierarchy)
			{
				RenderAction?.Invoke();
			}
		}

		/// <summary>
		/// Outputs current scene hierarchy to CryEngine's standard log.
		/// </summary>
		public void PrintScene(int depth = 0)
		{
			string spaces = "";
			for(int i = 0; i < depth; i++)
			{
				spaces += "  ";
			}

			Log.Info(spaces + Name + " {");

			foreach(var c in Components)
			{
				Log.Info(spaces + "  [" + c.GetType().Name + "]");
			}

			foreach(var t in Transform)
			{
				t.Owner.PrintScene(depth + 1);
			}

			Log.Info(spaces + "}");
		}

		/// <summary>
		/// Deinitialized this SceneObject and all its hierarchical decendants.
		/// </summary>
		public void Destroy()
		{
			if(IsUpdateable)
			{
				SceneManager.RemoveUpdateReceiver(Update, Render, Root);
			}

			DestroyAction?.Invoke();

			var children = new List<Transform>(Transform.Children);
			foreach(var t in children)
			{
				t.Owner.Destroy();
			}

			var components = new List<UIComponent>(Components);
			components.ForEach(x => x.Destroy());

			if(Parent != null)
			{
				Parent.Transform.Children.Remove(Transform);
			}
		}

		/// <summary>
		/// Serializes this object and all its hierarchical decendants to JSON.
		/// </summary>
		public string ToJSON()
		{
			return Tools.ToJSON(this);
		}

		/// <summary>
		/// Creates a human readable JSON serialization and stores it on file.
		/// </summary>
		/// <param name="name">File name to be used. Stores under SceneObject's Name if null.</param>
		public void Store(string name = null)
		{
			if(!Directory.Exists(Path.Combine(Engine.DataDirectory, "/scenes/")))
			{
				Directory.CreateDirectory(Path.Combine(Engine.DataDirectory, "/scenes/"));
			}

			// Make output human readable
			var str = ToJSON();
			var fmtStr = str;
			var fmtStrIdx = 0;
			var tabs = "";
			for(int i = 0; i < str.Length; i++)
			{
				var fmtSubStr = fmtStr.Substring(fmtStrIdx);
				if(fmtSubStr.StartsWith("[{", StringComparison.InvariantCulture))
				{
					tabs += "\t";
				}

				if(fmtSubStr.StartsWith("{\"__type\"", StringComparison.InvariantCulture))
				{
					fmtStr = fmtStr.Insert(fmtStrIdx, "\r\n" + tabs);
					fmtStrIdx += ("\r\n" + tabs).Length;
				}

				if(fmtSubStr.StartsWith("}]", StringComparison.InvariantCulture))
				{
					tabs = tabs.Remove(0, 1);
					fmtStr = fmtStr.Insert(fmtStrIdx, "\r\n" + tabs);
					fmtStrIdx += ("\r\n" + tabs).Length;
				}
				fmtStrIdx++;
			}

			var f = File.CreateText(Path.Combine(Engine.DataDirectory, "/scenes/", (name ?? Name), ".scn"));
			f.Write(fmtStr);
			f.Close();
		}
	}
}

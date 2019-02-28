// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using CryEngine.Common;

namespace CryEngine.UI
{
	/// <summary>
	/// Handles prioritized and ordered updating of SceneObjects and Components. 
	/// Receives update call itself from GameFramework. 
	/// Will update scenes by priority in ascending order. Will update receivers in ascending order.
	/// </summary>
	public class SceneManager
	{
		private class GameFrameworkListener :IGameFrameworkListener
		{
			public event Action PostUpdate;
			public event Action PreRender;

			public GameFrameworkListener()
			{
				RegisterListener();
			}

			public void RegisterListener()
			{
				Engine.GameFramework.RegisterListener(this, "MonoGameFrameworkUI", EFRAMEWORKLISTENERPRIORITY.FRAMEWORKLISTENERPRIORITY_HUD);
			}

			public void UnregisterListener()
			{
				Engine.GameFramework?.UnregisterListener(this);
			}

			public override void OnPostUpdate(float fDeltaTime)
			{
				if(Engine.GameFramework.IsInLevelLoad())
				{
					return;
				}

				PostUpdate?.Invoke();
			}
			public override void OnPreRender()
			{
				if(Engine.GameFramework.IsInLevelLoad())
				{
					return;
				}

				PreRender?.Invoke();
			}

			// All functions need to be overridden otherwise it will result in pure virtual calls.
			//public override void OnPreRender(){}
			public override void OnActionEvent(SActionEvent arg0){}
			public override void OnForceLoadingWithFlash(){}
			public override void OnLevelEnd(string nextLevel){}
			public override void OnLoadGame(ILoadGame pLoadGame){}
			public override void OnSaveGame(ISaveGame pSaveGame){}
			public override void OnSavegameFileLoadedInMemory(string pLevelName){}
		}

		private class SceneDescription
		{
			public SceneObject Root;
			public bool IsValid = true;
			public int Priority = 0;
			public Dictionary<Action, int> UpdateReceiverOrder = new Dictionary<Action, int>();
			public Dictionary<Action, int> PreRenderReceiverOrder = new Dictionary<Action, int>();
		}

		private static SceneManager _instance = new SceneManager();
		private GameFrameworkListener _listener;
		private readonly List<SceneDescription> _scenes = new List<SceneDescription>();
		private readonly List<Canvas> _canvasses = new List<Canvas>();
		private SceneObject _rootObject;



		/// <summary>
		/// The root SceneObject created by the SceneManager.
		/// </summary>
		/// <value>The root object.</value>
		public static SceneObject RootObject 
		{ 
			get
			{
				return _instance._rootObject;
			}
		}

		internal SceneManager()
		{
			Engine.EngineUnloading += OnEngineUnload;
			Engine.EngineReloaded += OnEngineReload;
			_rootObject = SceneObject.Instantiate(null, "Root");
			_listener = new GameFrameworkListener();
			_listener.PostUpdate += Update;
			_listener.PreRender += PreRender;
		}

		/// <summary>
		/// Releases unmanaged resources and performs other cleanup operations before the
		/// <see cref="T:CryEngine.UI.SceneManager"/> is reclaimed by garbage collection.
		/// </summary>
		~SceneManager()
		{
			_listener.UnregisterListener();
		}

		private void OnEngineUnload()
		{
			_listener.UnregisterListener();
		}

		private void OnEngineReload()
		{
			_listener.RegisterListener();
		}

		/// <summary>
		/// Will trigger refreshing of update receiver order inside update loop.
		/// </summary>
		/// <param name="root">Root object of referenced scene.</param>
		internal static void InvalidateSceneOrder(SceneObject root)
		{
			var scenes = _instance._scenes;
			var scene = scenes.SingleOrDefault(x => x.Root == root);
			if(scene != null)
			{
				scene.IsValid = false;
			}
			else
			{
				scenes.Add(new SceneDescription { Root = root, IsValid = false });
			}
		}

		/// <summary>
		/// Defines scene update order.
		/// </summary>
		/// <param name="root">Root object of referenced scene.</param>
		/// <param name="priority">Priority to be assigned.</param>
		public static void SetScenePriority(SceneObject root, int priority)
		{
			var scenes = _instance._scenes;
			var scene = scenes.SingleOrDefault(x => x.Root == root);
			if(scene != null)
			{
				scene.Priority = priority;
			}
		}

		/// <summary>
		/// Registers an update receiver by sorting it into its scene root element.
		/// </summary>
		/// <param name="updateAction">Update action to call.</param>
		/// <param name="preRenderAction">Render action to call</param>
		/// <param name="root">Root scene of the update receiver.</param>
		/// <param name="order">Call order.</param>
		public static void RegisterUpdateReceiver(Action updateAction, Action preRenderAction, SceneObject root,  int order = 0)
		{
			var scenes = _instance._scenes;
			var scene = scenes.SingleOrDefault(x => x.Root == root);
			if(scene == null)
			{
				scene = new SceneDescription { Root = root };
				scenes.Add(scene);
			}
			if(updateAction != null)
			{
				scene.UpdateReceiverOrder[updateAction] = order;
			}

			if(preRenderAction != null)
			{
				scene.PreRenderReceiverOrder[preRenderAction] = order;
			}
		}

		/// <summary>
		/// Removes an update receiver.
		/// </summary>
		/// <param name="updateAction">Update action to be removed.</param>
		/// <param name="renderAction">Render action to be removed.</param>
		/// <param name="root">Root scene of the update receiver.</param>
		public static void RemoveUpdateReceiver(Action updateAction, Action renderAction, SceneObject root)
		{
			var scenes = _instance._scenes;
			var scene = scenes.SingleOrDefault(x => x.Root == root);
			if(scene != null)
			{
				if(updateAction != null)
				{
					scene.UpdateReceiverOrder.Remove(updateAction);
				}

				if(renderAction != null)
				{
					scene.PreRenderReceiverOrder.Remove(renderAction);
				}
			}
		}

		internal static void RegisterCanvas(Canvas canvas)
		{
			_instance._canvasses.Add(canvas);
		}

		internal static void UnregisterCanvas(Canvas canvas)
		{
			_instance._canvasses.Remove(canvas);
		}

		private void PreRender()
		{
			var canvasses = _canvasses.ToList();
			foreach(var canvas in canvasses)
			{
				canvas?.ClearRenderTarget();
			}

			var scenes = new List<SceneDescription>(_scenes);
			foreach(var scene in scenes.Where(x => !x.IsValid))
			{
				scene.Root.RefreshUpdateOrder();
				scene.IsValid = true;
			}

			foreach(var scene in scenes.OrderBy(x => x.Priority))
			{
				var sorted = scene.PreRenderReceiverOrder.OrderBy(x => x.Value).ToList();
				sorted.ForEach(x => 
				{ 
					if(scene.PreRenderReceiverOrder.ContainsKey(x.Key))
					{
						x.Key?.Invoke(); 
					}
				});
			}
		}

		private void Update()
		{
			var scenes = new List<SceneDescription>(_scenes);
			foreach(var scene in scenes.Where(x => !x.IsValid))
			{
				scene.Root.RefreshUpdateOrder();
				scene.IsValid = true;
			}

			foreach(var scene in scenes.OrderBy(x => x.Priority))
			{
				var sorted = scene.UpdateReceiverOrder.OrderBy(x => x.Value).ToList();
				sorted.ForEach(x => 
				{ 
					if(scene.UpdateReceiverOrder.ContainsKey(x.Key))
					{
						x.Key?.Invoke();
					}
				});
			}
		}
	}
}

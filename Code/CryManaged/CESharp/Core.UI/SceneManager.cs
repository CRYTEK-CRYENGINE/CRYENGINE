using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace CryEngine.UI
{
	/// <summary>
	/// Used by SceneManager to receive hierarchically ordered update calls.
	/// </summary>
	public interface IUpdateReceiver
	{
		SceneObject Root { get; } ///< Contains the Objects scene root
		void Update(); ///< Called by SceneManager
	}

	/// <summary>
	/// Handles prioritized and ordered updating of SceneObjects and Components. Receives update call itself from GameFramework. Will update scenes by priority in ascending order. Will update receivers in ascending order.
	/// @see GameFramework
	/// @see SceneObject
	/// @see Components.UIComponent
	/// </summary>
	public class SceneManager : IGameUpdateReceiver
	{
		static SceneManager()
		{
			RootObject = SceneObject.Instantiate(null, "Root");
		}

		class SceneDescription
		{
			public SceneObject Root;
			public bool IsValid = true;
			public int Priority = 0;
			public Dictionary<IUpdateReceiver, int> UpdateReceiverOrder = new Dictionary<IUpdateReceiver, int>();
		}

        internal static SceneManager Instance { get; set; } = new SceneManager();

		public static SceneObject RootObject { get; private set; }

		static List<SceneDescription> _scenes = new List<SceneDescription>();

		/// <summary>
		/// Will trigger refreshing of update receiver order inside update loop.
		/// </summary>
		/// <param name="root">Root object of referenced scene.</param>
		public static void InvalidateSceneOrder(SceneObject root)
		{
			var scene = _scenes.SingleOrDefault(x => x.Root == root);
			if (scene != null)
				scene.IsValid = false;
			else
				_scenes.Add(new SceneDescription() { Root = root, IsValid = false });
		}

		/// <summary>
		/// Defines scene update order.
		/// </summary>
		/// <param name="root">Root object of referenced scene.</param>
		/// <param name="priority">Priority to be assigned.</param>
		public static void SetScenePriority(SceneObject root, int priority)
		{
			var scene = _scenes.SingleOrDefault(x => x.Root == root);
			if (scene != null)
				scene.Priority = priority;
		}

		/// <summary>
		/// Registers an update receiver by sorting it into its scene root element.
		/// </summary>
		/// <param name="ur">Object to be updated.</param>
		/// <param name="order">Call order.</param>
		public static void RegisterUpdateReceiver(IUpdateReceiver ur, int order = 0)
		{
			var root = ur.Root;
			var scene = _scenes.SingleOrDefault(x => x.Root == root);
			if (scene == null)
			{
				scene = new SceneDescription() { Root = root };
				_scenes.Add(scene);
			}
			scene.UpdateReceiverOrder[ur] = order;
		}

		/// <summary>
		/// Removes an update receiver.
		/// </summary>
		/// <param name="ur">Object to be removed.</param>
		public static void RemoveUpdateReceiver(IUpdateReceiver ur)
		{
			var root = ur.Root;
			var scene = _scenes.SingleOrDefault(x => x.Root == root);
			if (scene != null)
				scene.UpdateReceiverOrder.Remove(ur);
		}
		
		internal SceneManager()
		{
			GameFramework.RegisterForUpdate(this);
		}

		~SceneManager()
		{
			GameFramework.UnregisterFromUpdate(this);
		}

		/// <summary>
		/// Refreshes update receiver order if scene was invalidated. Calls all update functions for update receivers in correct scene priority and receiver order.
		/// </summary>
		public void OnUpdate()
		{
			var scenes = new List<SceneDescription>(_scenes);
			foreach (var scene in scenes.Where(x => !x.IsValid))
			{
				scene.Root.RefreshUpdateOrder();
				scene.IsValid = true;
			}
			foreach (var scene in scenes.OrderBy(x => x.Priority))
			{
				var sorted = scene.UpdateReceiverOrder.OrderBy(x => x.Value).ToList();
				sorted.ForEach(x => { if (scene.UpdateReceiverOrder.ContainsKey(x.Key)) x.Key.Update(); });
			}
		}
	}
}

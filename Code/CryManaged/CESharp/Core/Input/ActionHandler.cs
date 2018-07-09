// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using System;
using System.Collections.Generic;
using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// ActionHandler is the C# wrapper for the ActionMaps. 
	/// Using this you can have a config-file that defines your input instead of all the input being hard coded.
	/// </summary>
	public class ActionHandler : IActionListener
	{
		[SerializeValue]
		private readonly Dictionary<string, Action<string, InputState, float>> _handlers = new Dictionary<string, Action<string, InputState, float>>();
		[SerializeValue]
		private readonly string _actionMapName;

		/// <summary>
		/// Create a new ActionHandler that can receive the input as described in the config-file at actionMapPath.
		/// This ActionHandler will use the ActionMap specified by actionMapName.
		/// </summary>
		/// <param name="actionMapPath"></param>
		/// <param name="actionMapName"></param>
		public ActionHandler(string actionMapPath, string actionMapName)
		{
			_actionMapName = actionMapName;
			var actionMapManager = Global.gEnv.pGameFramework.GetIActionMapManager();
			if(!actionMapManager.InitActionMaps(actionMapPath))
			{
				throw new ArgumentException(string.Format("Unable to create ActionHandler for action-map {0}", actionMapPath), nameof(actionMapPath));
			}

			actionMapManager.Enable(true);
			actionMapManager.EnableActionMap(actionMapName, true);

			if(!actionMapManager.AddExtraActionListener(this, actionMapName))
			{
				throw new ArgumentException(string.Format("Unable to get ActionMap {0} from {1}", actionMapName, actionMapPath), nameof(actionMapName));
			}
		}

		//TODO The AfterAction and OnAction are now exposed to the user. It would make more sense if these were private or internal.
		/// <summary>
		/// Called after an action has been invoked.
		/// </summary>
		public override void AfterAction() { }

		/// <summary>
		/// Called when an action is invoked.
		/// </summary>
		/// <param name="action"></param>
		/// <param name="activationMode"></param>
		/// <param name="value"></param>
		public override void OnAction(CCryName action, int activationMode, float value)
		{
			var actionName = action.c_str();
			var state = (InputState)activationMode;
			if(_handlers.ContainsKey(actionName))
			{
				_handlers[actionName](actionName, state, value);
			}
		}

		/// <summary>
		/// Clear the handlers and dispose of this instance.
		/// </summary>
		public override void Dispose()
		{
			_handlers.Clear();
			var actionMapManager = Global.gEnv?.pGameFramework?.GetIActionMapManager();
			actionMapManager?.RemoveExtraActionListener(this, _actionMapName);
			base.Dispose();
		}

		/// <summary>
		/// Add a handler for an action to this ActionHandler.
		/// </summary>
		/// <param name="action"></param>
		/// <param name="handler"></param>
		public void AddHandler(string action, Action<string, InputState, float> handler)
		{
			_handlers.Add(action, handler);
		}

		/// <summary>
		/// Remove a handler from this ActionHandler.
		/// </summary>
		/// <param name="action"></param>
		public void RemoveHandler(string action)
		{
			_handlers.Remove(action);
		}
	}
}
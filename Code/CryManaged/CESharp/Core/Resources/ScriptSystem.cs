// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Linq;
using CryEngine.Resources;
using CryEngine.Components;
using CryEngine.FlowSystem;

namespace CryEngine.Resources
{
	public class InterDomainHandler : MarshalByRefObject
	{
		#region Fields
		private Dictionary<uint, Tuple<string, Dictionary<string, string>>> _entityCache = new Dictionary<uint, Tuple<string, Dictionary<string, string>>>();
		#endregion

		#region Properties
		public Dictionary<uint, Tuple<string, Dictionary<string, string>>> EntityCache { get { return _entityCache; } }
		#endregion

		#region Events
		public event EventHandler RequestQuit;
		#endregion

		#region Methods
		public void RaiseRequestQuit()
		{
			if (RequestQuit != null)
				RequestQuit ();
		}
		#endregion
	}

	/// <summary>
	/// Interface which must be implemented to allow for an assembly to be automatically instanciated by the framework on runtime. If an assembly holds a class which implements this interface, ScriptSystem will watch the assembly for modifications. If the file is changed, the plugin will be newly instanciated.
	/// </summary>
	public interface ICryEngineAddIn
	{		
		/// <summary>
		/// Supposed to initialize the assembly (e.g. creating managers as well as type registration)
		/// </summary>
		void Initialize(InterDomainHandler handler);

		/// <summary>
		/// Raises the run scene function.
		/// </summary>
		/// <param name="scene">Scene object which originated the event.</param>
		void OnFlowNodeSignal(FlowNode node, PropertyInfo signal);

		/// <summary>
		/// Create game relevant objects and initialize game logic.
		/// </summary>
		void StartGame();

		/// <summary>
		/// End game logic and clean up game revelant objects.
		/// </summary>
		void EndGame();

		/// <summary>
		/// Supposed to cleanup all resources used by the assembly.
		/// </summary>
		void Shutdown();
	}
}

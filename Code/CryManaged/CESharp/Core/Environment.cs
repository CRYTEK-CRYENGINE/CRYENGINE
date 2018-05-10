using System;
using CryEngine.Common;

using CryEngine.Rendering;
using CryEngine.EntitySystem;

using System.Reflection;

namespace CryEngine
{
	/// <summary>
	/// Wraps CryEngine's gEnv variable for global access to main modules. Initializes all C# sided handler and wrapper classes.
	/// Interfaces should be removed from here once a managed wrapper (such as Input) exists.
	/// </summary>
	public class Engine
	{
		public static IConsole Console { get { return Global.gEnv.pConsole; } }
		public static ICryFont Font { get { return Global.gEnv.pCryFont; } }
		public static ISystem System { get { return Global.gEnv.pSystem; } }
		public static IFlowSystem FlowSystem { get { return Global.gEnv.pFlowSystem; } }
		public static IParticleManager ParticleManager { get { return Global.gEnv.pParticleManager; } }
		public static I3DEngine Engine3D { get { return Global.gEnv.p3DEngine; } }
		public static IGameFramework GameFramework { get { return Global.gEnv.pGameFramework; } }
		public static ITimer Timer { get { return Global.gEnv.pTimer; } }
		public static Common.CryAudio.IAudioSystem AudioSystem { get { return Global.gEnv.pAudioSystem; } }
		public static IRenderAuxGeom AuxRenderer { get { return Global.gEnv.pRenderer.GetIRenderAuxGeom(); } }
		public static IPhysicalWorld PhysicalWorld { get { return Global.gEnv.pPhysicalWorld; } }
		public static bool IsSandbox { get { return Global.gEnv.IsEditor(); } } ///< Indicates whether CryEngine is run in Editor mode.
			
		public static event EventHandler StartReload;
		public static event EventHandler EndReload;

		/// <summary>
		/// Called by C++ runtime. Do not call directly.
		/// </summary>
		internal static void OnEngineStart()
		{
			SystemEventHandler.Instance = new SystemEventHandler();
			Input.Instance = new Input();
			Renderer.Instance = new Renderer();
			Mouse.Instance = new Mouse();
			CryEngine.GameFramework.Instance = new CryEngine.GameFramework();
			LevelSystem.Instance = new LevelSystem();
		}

		/// <summary>
		/// Called by C++ runtime. Do not call directly.
		/// </summary>
		internal static void OnEngineShutdown()
		{
			// Make sure we unify shutdown behavior with unload
			OnUnloadStart();
		}

		/// <summary>
		/// Called by the C++ runtime just before unloading the domain this assembly belongs to
		/// </summary>
		internal static void OnUnloadStart()
		{
			if (StartReload != null)
			{
				StartReload();
			}
		}

		/// <summary>
		/// Called by the C++ runtime just after reloading the domain this assembly belongs to
		/// Is *not* called on the initial load, see OnEngineStart instead!
		/// </summary>
		internal static void OnReloadDone()
		{
			if (EndReload != null)
			{
				EndReload();
			}
		}

		internal static void ScanAssembly(Assembly assembly)
		{
			foreach (Type t in assembly.GetTypes())
			{
				EntityComponent.TryRegister(t);
				BehaviorTreeNodeFactory.TryRegister(t);
			}
		}
	}
}

// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Net;
using System.IO;
using System.Linq;
using System.Net.Sockets;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using CryEngine.Common;
using CryEngine.Rendering;

namespace CryEngine
{
	/// <summary>
	/// Wraps CryEngine's gEnv variable for global access to main modules. Initializes all C# sided handler and wrapper classes.
	/// Interfaces should be removed from here once a managed wrapper (such as Input) exists.
	/// </summary>
	public static class Engine
	{
		public static IConsole Console { get { return Global.gEnv.pConsole; } }
		public static ICryFont Font { get { return Global.gEnv.pCryFont; } }
		public static ISystem System { get { return Global.gEnv.pSystem; } }
		public static ParticleManager ParticleManager { get { return ParticleManager.Instance; } }
		public static I3DEngine Engine3D { get { return Global.gEnv.p3DEngine; } }
		public static IGameFramework GameFramework { get { return Global.gEnv.pGameFramework; } }
		public static ITimer Timer { get { return Global.gEnv.pTimer; } }
		public static Common.CryAudio.IAudioSystem AudioSystem { get { return Global.gEnv.pAudioSystem; } }
		public static IRenderAuxGeom AuxRenderer { get { return Global.gEnv.pRenderer.GetIRenderAuxGeom(); } }
		public static IPhysicalWorld PhysicalWorld { get { return Global.gEnv.pPhysicalWorld; } }

		/// <summary>
		/// Indicates whether this <see cref="T:CryEngine.Engine"/> is running in the Sandbox.
		/// </summary>
		/// <value><c>true</c> if this is running in the Sandbox; otherwise, <c>false</c>.</value>
		public static bool IsSandbox { get { return Global.gEnv.IsEditor(); } }

		/// <summary>
		/// True if the application is currently running in the sandbox and GameMode has been started.
		/// </summary>
		/// <value><c>true</c> if in GameMode; otherwise, <c>false</c>.</value>
		public static bool IsSandboxGameMode { get { return Global.gEnv.IsEditorGameMode(); } }

		/// <summary>
		/// True if the <see cref="T:CryEngine.Engine"/> is currently running as a dedicated server.
		/// This means that certain systems like the <see cref="T:CryEngine.Renderer"/> and <see cref="T:CryEngine.Input"/> and input are not initialized.
		/// </summary>
		/// <value><c>true</c> if is dedicated server; otherwise, <c>false</c>.</value>
		public static bool IsDedicatedServer { get { return Global.gEnv.IsDedicated(); } }

		/// <summary>
		/// Root directory of the engine.
		/// </summary>
		/// <value>The engine root directory.</value>
		public static string EngineRootDirectory => Global.GetEnginePath().c_str();

		/// <summary>
		/// Path where application data should be stored.
		/// </summary>
		/// <value>The data directory.</value>
		public static string DataDirectory => Global.GetGameFolder().c_str() + "/";

		internal static string MonoDirectory => Path.Combine(EngineRootDirectory, "bin", "common", "Mono");

		internal static string GlobalAssemblyCacheDirectory => Path.Combine(MonoDirectory, "lib", "mono", "gac");

		internal static event Action StartReload;
		internal static event Action EndReload;

		/// <summary>
		/// Called by C++ runtime. Do not call directly.
		/// </summary>
		internal static void OnEngineStart()
		{
			SystemEventHandler.Instance = new SystemEventHandler();

			if(!IsDedicatedServer)
			{
				Input.Initialize();
				Renderer.Instance = new Renderer();
				Mouse.Instance = new Mouse();
			}

			CryEngine.GameFramework.Instance = new GameFramework();
			LevelSystem.Instance = new LevelSystem();
		}

		/// <summary>
		/// Called by C++ runtime. Do not call directly.
		/// </summary>
		internal static void OnEngineShutdown()
		{
			// Make sure we unify shutdown behavior with unload
			OnUnloadStart();

			CryEngine.GameFramework.Instance?.Dispose();

			GC.Collect();
			GC.WaitForPendingFinalizers();
		}

		/// <summary>
		/// Called by the C++ runtime just before unloading the domain this assembly belongs to
		/// </summary>
		internal static void OnUnloadStart()
		{
			StartReload?.Invoke();
		}

		/// <summary>
		/// Called by the C++ runtime just after reloading the domain this assembly belongs to
		/// Is *not* called on the initial load, see OnEngineStart instead!
		/// </summary>
		internal static void OnReloadDone()
		{
			EndReload?.Invoke();
		}

		internal static void ScanAssembly(Assembly assembly)
		{
			var registeredTypes = new List<Type>();
			foreach(Type t in assembly.GetTypes())
			{
				if(typeof(EntityComponent).IsAssignableFrom(t) && t != typeof(object))
				{
					if(registeredTypes.Contains(t))
					{
						continue;
					}

					RegisterComponent(t, ref registeredTypes);
				}

				if(typeof(BehaviorTreeNodeBase).IsAssignableFrom(t) && !t.IsAbstract)
				{
					BehaviorTreeNodeFactory.TryRegister(t);
				}
			}
		}

		private static void RegisterComponent(Type component, ref List<Type> registeredTypes)
		{
			// Get the base class so those can be registered first.
			var baseType = component.BaseType;
			if(baseType != null && baseType != typeof(object))
			{
				var registerQueue = new List<Type>();
				registerQueue.Add(baseType);

				while(baseType.BaseType != null && baseType != typeof(EntityComponent))
				{
					baseType = baseType.BaseType;
					registerQueue.Add(baseType);
				}

				for(int i = registerQueue.Count - 1; i > -1; i--)
				{
					var type = registerQueue[i];
					if(registeredTypes.Contains(type))
					{
						continue;
					}

					registeredTypes.Add(type);
					EntityComponent.TryRegister(type);
				}
			}

			registeredTypes.Add(component);
			EntityComponent.TryRegister(component);
		}

		/// <summary>
		/// Shuts the engine down, or exits game-mode while in the Sandbox.
		/// </summary>
		public static void Shutdown()
		{
			if(!IsSandbox)
			{
				Console.ExecuteString("quit", false, true);
			}
			else
			{
				Console.ExecuteString("ed_disable_game_mode", false, true);
				OnUnloadStart();
			}
		}

		internal static string TypeToHash(Type type)
		{
			string result = string.Empty;
			string input = type.FullName;
			using(SHA384 hashGenerator = SHA384.Create())
			{
				var hash = hashGenerator.ComputeHash(Encoding.Default.GetBytes(input));
				var shortHash = new byte[16];
				for(int i = 0, j = 0; i < hash.Length; ++i, ++j)
				{
					if(j >= shortHash.Length)
					{
						j = 0;
					}
					unchecked
					{
						shortHash[j] += hash[i];
					}
				}
				result = BitConverter.ToString(shortHash);
				result = result.Replace("-", string.Empty);
			}
			return result;
		}

		internal static string GetPluginGuid(Type type)
		{
			var guidAttribute = (GuidAttribute)type.GetCustomAttributes(typeof(GuidAttribute), false).FirstOrDefault();
			if(guidAttribute != null)
			{
				return guidAttribute.Value;
			}

			// Fall back to generating GUID based on type
			return (new Guid(TypeToHash(type))).ToString();
		}
	}
}

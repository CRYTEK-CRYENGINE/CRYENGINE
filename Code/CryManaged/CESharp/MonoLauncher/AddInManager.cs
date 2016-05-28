// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using CryEngine.Common;
using CryEngine.Components;
using CryEngine.FlowSystem;
using CryEngine.Resources;

namespace CryEngine.Launcher
{	
	[DebuggerDisplay("CryEngineAddin({Url})")]
	internal class AddIn : MarshalByRefObject 
	{
		private Assembly _assembly;
		private ICryEngineAddIn _addIn;
		private Type _addinType;
		private bool _isInitialized = false;
		private bool _isRunning = false;
		private string _url;

		public bool IsInitialized { get { return _isInitialized; } }
		public bool IsRunning { get { return _isRunning; } }
		public string Url { get { return _url; } }

		public static AddIn CreateInDomain(AppDomain domain)
		{
			return (AddIn) domain.CreateInstanceAndUnwrap( typeof(AddIn).Assembly.FullName, typeof(AddIn).FullName );
		}

		public override object InitializeLifetimeService() 
		{
			return null;
		}

		public bool LoadAssembly(string url) 
		{
			Log.Info<Launcher> ("LoadAssembly: {0}", url);
			_url = url;
			
			byte[] symbolStore = File.Exists (url + ".mdb") ? File.ReadAllBytes (url + ".mdb") : null;
			_assembly = AppDomain.CurrentDomain.Load(File.ReadAllBytes(url), symbolStore);
			_addIn = null;

			_addinType = _assembly.GetTypes().FirstOrDefault(x => x.GetInterfaces().Contains(typeof(ICryEngineAddIn)));
			if(_addinType != null)
				_addIn = (ICryEngineAddIn)Activator.CreateInstance (_addinType);
			return _addIn != null;
		}

		public void Initialize(InterDomainHandler handler)
		{
			if (_isInitialized)
				return;
			
			Log.Info<Launcher> ("Init Assembly: {0}", _url);

			_addIn.Initialize (handler);
			_isInitialized = true;
		}

		public void OnFlowNodeSignal(FlowNode node, PropertyInfo signal)
		{
			_addIn.OnFlowNodeSignal (node, signal);
		}

		public void StartGame()
		{
			if (_isRunning)
				return;

			_addIn.StartGame ();
			_isRunning = true;
		}

		public void EndGame()
		{
			if (!_isRunning)
				return;

			_addIn.EndGame ();
			_isRunning = false;
		}

		public void Shutdown ()
		{
			if (!_isInitialized)
				return;
			
			Log.Info<Launcher> ("Shutdown Assembly: {0}", _url);

			_addIn.Shutdown ();
			_isInitialized = false;
		}
	}

	internal class AddInManager : IMonoListener
	{
		public static event EventHandler AppUnloaded;

		public static InterDomainHandler InterDomainHandler { get; private set; }

		private FileSystemWatcher _assemblyWatcher;
		private AppDomain _hostDomain;
		private Dictionary<string, AddIn> _addInByName;
		private List<KeyValuePair<FlowNode, PropertyInfo>> _nodesToRun = new List<KeyValuePair<FlowNode, PropertyInfo>>();
		private bool _addInsLoaded = false;
		private bool _addinsRunning = false;
		private bool _notifyUnload = false;
		private bool _notifyUnloaded = false;

		public DateTime BootTime = DateTime.MaxValue;

		public AddInManager()
		{
			InterDomainHandler = new InterDomainHandler ();
			InterDomainHandler.RequestQuit += () => 
			{
				_notifyUnload = true;
			};

			_addInByName = new Dictionary<string, AddIn> ();
			AppDomain.CurrentDomain.AssemblyResolve += LoadFromExecutingFolder;

			var files = Directory.GetFiles(Application.ExecutionPath + "/mono/AddIns", "*.dll");
			foreach(var f in files)
				AddAssembly (new FileInfo(f).FullName);

			_assemblyWatcher = new FileSystemWatcher();
			_assemblyWatcher.Path = Application.ExecutionPath + "/mono/AddIns";
			_assemblyWatcher.NotifyFilter = NotifyFilters.LastWrite;
			_assemblyWatcher.Filter = "*.dll";
			_assemblyWatcher.Created += OnAssemblyChanged;
			_assemblyWatcher.Changed += OnAssemblyChanged;
			_assemblyWatcher.EnableRaisingEvents = true;

			LoadAddins ();
			Global.gEnv.pMonoRuntime.RegisterListener(this);
			FlowNode.OnSignal += OnFlowNodeSignal;
		}

		public void OnFlowNodeSignal(FlowNode node, PropertyInfo signal)
		{
			_nodesToRun.Add (new KeyValuePair<FlowNode, PropertyInfo>(node, signal));
		}

		public void Deinitialize()
		{			
			UnloadApplication ();
			_assemblyWatcher.Created -= OnAssemblyChanged;
			_assemblyWatcher.Changed -= OnAssemblyChanged;
		}

		List<AppDomain> _oldDomains = new List<AppDomain>();

		public void UnloadApplication()
		{
			StopAddins ();
			Log.Info<Launcher> ("Unload Application");

			if (_hostDomain != null)
			{
				InterDomainHandler.EntityCache.Clear ();
				var assemblyUrls = _addInByName.Keys.ToList ();

				// Shutdown Core last.
				var core = assemblyUrls.SingleOrDefault(x => x.Contains("CryEngine.Core"));
				assemblyUrls.Remove(core);
				assemblyUrls.Add (core);

				foreach (var url in assemblyUrls) 
				{
					if (_addInByName [url] != null) {
						try{
						_addInByName [url].Shutdown ();
						}
						catch(Exception ex) {
							Log.Exception (ex);
						}
					}
					_addInByName [url] = null;
				}

				//AppDomain.Unload (_hostDomain); // Leads to crash on reload. Need to investigate...
				_oldDomains.Add(_hostDomain);
				_addInsLoaded = false;

				_hostDomain = null;
				_notifyUnloaded = true;
			}
		}

		public void StopApplication()
		{
			StopAddins ();
		}

		private void LoadAddins()
		{
			Log.Info<Launcher> ("Load addins");

			string searchDir = Global.gEnv.pMonoRuntime.GetProjectDllDir ();
			if (String.IsNullOrEmpty (searchDir)) {
				StringBuilder sb = new StringBuilder (256);
				Global.CryGetExecutableFolder (sb, 256);
				searchDir = sb.ToString ();
			} else {
				searchDir = Path.Combine (Environment.CurrentDirectory, searchDir);
			}
			_hostDomain = AppDomain.CreateDomain ("CryEngineMonoApp", AppDomain.CurrentDomain.Evidence, Environment.CurrentDirectory, searchDir, false);
			_hostDomain.AssemblyResolve += LoadFromExecutingFolder;

			var assemblyUrls = _addInByName.Keys.ToList ();

			// Initialize Core first.
			var core = assemblyUrls.SingleOrDefault(x => x.Contains("CryEngine.Core"));
			assemblyUrls.Remove(core);
			assemblyUrls.Insert (0, core);

			foreach (var url in assemblyUrls) 
			{
				if (!File.Exists(url))
				{
					_addInByName.Remove(url);
					continue;
				}
				var addIn = AddIn.CreateInDomain (_hostDomain);
				if (addIn.LoadAssembly (url)) 
					_addInByName [url] = addIn;
			}
			foreach (AddIn addin in _addInByName.Values) 
			{
				if (addin != null) 
				{
					try
					{
						if(!addin.IsInitialized)
							addin.Initialize(InterDomainHandler);
					}
					catch(Exception ex)
					{
						Log.Exception<Launcher> (ex, "Could not initialize Addin: {0}", addin.Url);
					}
				}
			}
			_addInsLoaded = true;
		}

		private void StartAddins()
		{
			if (!_addInsLoaded)
				return;

			Global.gEnv.pLog.Log ("[Launcher] Start addins");
			
			foreach (AddIn addin in _addInByName.Values) {
				if (addin != null) {
					try
					{
						if(!addin.IsRunning)
							addin.StartGame ();
					}
					catch(Exception ex)
					{
						Log.Exception<Launcher> (ex, "Could not start Addin: {0}", addin.Url);
					}
				}
			}
			_addinsRunning = true;
			BootTime = DateTime.MaxValue;
		}

		private void StopAddins()
		{
			if (!_addInsLoaded || !_addinsRunning)
				return;

			Global.gEnv.pLog.Log ("[Launcher] Stop addins");

			foreach (AddIn addin in _addInByName.Values) {
				if (addin != null) {
					try
					{
						if(addin.IsRunning)
							addin.EndGame ();
					}
					catch(Exception ex)
					{
						Log.Exception<Launcher> (ex, "Could not stop Addin: {0}", addin.Url);
					}
				}
			}
			_addinsRunning = false;
		}

		private void AddAssembly(string url)
		{
			UnloadApplication ();
			_addInByName [url] = null;

			// In case we are in Sandbox, we should leave loading to GameStarted event.
			if (!Env.IsSandbox)
				BootTime = DateTime.Now.AddSeconds (0.1f);
		}

		public override void OnUpdate (int updateFlags, int nPauseMode)
		{
			if (!_addInsLoaded)
				LoadAddins ();
			
			// Load AddIns if there are.
			if (BootTime < DateTime.Now && !_addinsRunning)
				StartAddins ();

			// Process Run Scene nodes if there are.
			if (_addInsLoaded && _nodesToRun.Count > 0)  
			{
				var assemblyUrls = _addInByName.Keys.ToList ();

				foreach(var pair in _nodesToRun)
				{
					foreach (var url in assemblyUrls) 
					{
						if (_addInByName [url] != null)
							_addInByName [url].OnFlowNodeSignal (pair.Key, pair.Value);
					}
				}
				_nodesToRun.Clear ();
			}

			if (_notifyUnload) 
			{
				_notifyUnload = false;
				UnloadApplication ();
			}

			if (_notifyUnloaded) 
			{
				_notifyUnloaded = false;
				if (AppUnloaded != null)
					AppUnloaded ();
			}
		}

		public IEnumerable<AppDomain> GetDomains()
		{
			yield return _hostDomain;
		}

		void OnAssemblyChanged(object source, FileSystemEventArgs e)
		{
			AddAssembly (e.FullPath);
		}

		static Assembly LoadFromExecutingFolder(object sender, ResolveEventArgs args)
		{
			string folderPath = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
			string assemblyPath = Path.Combine(folderPath, new AssemblyName(args.Name).Name + ".dll");
			return File.Exists(assemblyPath) ? Assembly.LoadFrom(assemblyPath) : null;
		}
	}
}

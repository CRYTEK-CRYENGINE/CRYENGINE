// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;

namespace CryEngine
{
	public sealed class ParticleManager
	{
		private static ParticleManager _instance;
		private static object _singletonLock = new object();

		internal static ParticleManager Instance
		{
			get
			{
				if(_instance == null)
				{
					lock(_singletonLock)
					{
						if(_instance == null)
						{
							_instance = new ParticleManager();
						}
					}
				}
				return _instance;
			}
		}

		[SerializeValue]
		internal IParticleManager NativeHandle { get; private set; }

		private ParticleManager()
		{
			NativeHandle = Global.gEnv.pParticleManager;
		}

		public void AddEventListener(ParticleEffectListener listener)
		{
			if(listener == null)
			{
				throw new ArgumentNullException(nameof(listener));
			}

			var nativeListener = listener.NativeHandle;
			if(nativeListener == null)
			{
				Log.Error<ParticleManager>("Unable to add the event listener because of a missing reference to native handle for {0}!", nameof(listener));
				return;
			}

			NativeHandle.AddEventListener(nativeListener);
			
		}

		public void AddVertexIndexPoolUsageEntry(uint nVertexMemory, uint nIndexMemory, string pContainerName)
		{
			NativeHandle.AddVertexIndexPoolUsageEntry(nVertexMemory, nIndexMemory, pContainerName);
		}

		public void ClearDeferredReleaseResources()
		{
			NativeHandle.ClearDeferredReleaseResources();
		}

		public void ClearRenderResources(bool bForceClear)
		{
			NativeHandle.ClearRenderResources(bForceClear);
		}

		public ParticleEffect CreateEffect()
		{
			var nativeEffect = NativeHandle.CreateEffect();
			return nativeEffect == null ? null : new ParticleEffect(nativeEffect);
		}

		public void CreatePerfHUDWidget()
		{
			NativeHandle.CreatePerfHUDWidget();
		}

		public void DeleteEffect(ParticleEffect effect)
		{
			if(effect == null)
			{
				throw new ArgumentNullException(nameof(effect));
			}

			NativeHandle.DeleteEffect(effect.NativeHandle);
		}

		public void DeleteEmitter(ParticleEmitter emitter)
		{
			if(emitter == null)
			{
				throw new ArgumentNullException(nameof(emitter));
			}

			NativeHandle.DeleteEmitter(emitter.NativeHandle);
		}

		public ParticleEffect FindEffect(string effectName)
		{
			var nativeEffect = NativeHandle.FindEffect(effectName);
			return nativeEffect == null ? null : new ParticleEffect(nativeEffect);
		}

		public ParticleEffect FindEffect(string sEffectName, string sSource)
		{
			var nativeEffect = NativeHandle.FindEffect(sEffectName, sSource);
			return nativeEffect == null ? null : new ParticleEffect(nativeEffect);
		}

		public ParticleEffect FindEffect(string sEffectName, string sSource, bool bLoadResources)
		{
			var nativeEffect = NativeHandle.FindEffect(sEffectName, sSource, bLoadResources);
			return nativeEffect == null ? null : new ParticleEffect(nativeEffect);
		}

		public ParticleEffect GetDefaultEffect()
		{
			var nativeEffect = NativeHandle.GetDefaultEffect();
			return nativeEffect == null ? null : new ParticleEffect(nativeEffect);
		}


		public ParticleEffect LoadEffect(string sEffectName, XmlNodeRef effectNode, bool bLoadResources)
		{
			var nativeEffect = NativeHandle.LoadEffect(sEffectName, effectNode, bLoadResources);
			return nativeEffect == null ? null : new ParticleEffect(nativeEffect);
		}

		public ParticleEffect LoadEffect(string sEffectName, XmlNodeRef effectNode, bool bLoadResources, string sSource)
		{
			var nativeEffect = NativeHandle.LoadEffect(sEffectName, effectNode, bLoadResources, sSource);
			return nativeEffect == null ? null : new ParticleEffect(nativeEffect);
		}

		public bool LoadLibrary(string particlesLibrary)
		{
			return NativeHandle.LoadLibrary(particlesLibrary);
		}

		public bool LoadLibrary(string particlesLibrary, string particlesLibraryFile)
		{
			return NativeHandle.LoadLibrary(particlesLibrary, particlesLibraryFile);
		}

		public bool LoadLibrary(string particlesLibrary, string particlesLibraryFile, bool loadResources)
		{
			return NativeHandle.LoadLibrary(particlesLibrary, particlesLibraryFile, loadResources);
		}

		public void MarkAsOutOfMemory()
		{
			NativeHandle.MarkAsOutOfMemory();
		}

		public void RemoveEventListener(ParticleEffectListener listener)
		{
			if(listener == null)
			{
				throw new ArgumentNullException(nameof(listener));
			}
			NativeHandle.RemoveEventListener(listener.NativeHandle);
		}

		public void RenderDebugInfo()
		{
			NativeHandle.RenderDebugInfo();
		}

		public void Reset()
		{
			NativeHandle.Reset();
		}

		public void SetDefaultEffect(ParticleEffect effect)
		{
			if(effect == null)
			{
				throw new ArgumentNullException(nameof(effect));
			}
			NativeHandle.SetDefaultEffect(effect.NativeHandle);
		}
	}
}

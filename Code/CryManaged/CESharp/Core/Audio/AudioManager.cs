// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using CryEngine.Common;
using CryEngine.Common.CryAudio;

namespace CryEngine
{
	using NativeAudioSystem = CryEngine.NativeInternals.IAudioSystem;
	/// <summary>
	/// Internal class used to manage native code
	/// </summary>
	internal sealed class AudioManager : IDisposable
	{
		private static Dictionary<string, uint> _triggerByName = new Dictionary<string, uint>();
		private static Dictionary<uint, string> _indexTriggerIdToName = new Dictionary<uint, string>();

		private static IDictionary<string, Audio.ManagedAudioStateListenerDelegate> _requestListenersDelegates = new Dictionary<string, Audio.ManagedAudioStateListenerDelegate>();
		private static Audio.RequestListenerDelegate _requestListener;

		private static AudioRequestInfo _info;
		private static bool _isDisposed;

		internal static void Initialize()
		{
			AddListener();
			Engine.StartReload += RemoveListener;
			Engine.EndReload += AddListener;
		}

		~AudioManager()
		{
			Dispose(false);
		}

		private static void AddListener()
		{
			// Bind listener to c++
			_requestListener = OnAudioEvent;
			IntPtr fnPtr = System.Runtime.InteropServices.Marshal.GetFunctionPointerForDelegate(_requestListener);
			NativeAudioSystem.AddAudioRequestListener(fnPtr);
		}

		private static void RemoveListener()
		{
			if(_requestListener == null)
			{
				return;
			}

			// Remove listener from c++
			IntPtr fnPtr = System.Runtime.InteropServices.Marshal.GetFunctionPointerForDelegate(_requestListener);
			NativeAudioSystem.RemoveAudioRequestListener(fnPtr);
			_requestListener = null;
		}

		private static void OnAudioEvent(IntPtr requestInfo)
		{
			_info = null;
			_info = new AudioRequestInfo(requestInfo, true); // requestInfo created natively , C++ will handle destruction so only a copy is needed
			uint ctrlId = _info.ControlId;
			var requestResult = _info.RequestResult;
			var enumFlagsType = _info.SystemEvents;

			AudioStateType audioState = AudioStateType.Unknown;
			switch((ESystemEvents)enumFlagsType)
			{
				case ESystemEvents.TriggerExecuted:
					{
						audioState = AudioStateType.TriggerStarted;
						break;
					}
				case ESystemEvents.TriggerFinished:
					{
						audioState = AudioStateType.TriggerEnded;
						break;
					}
			}
			string triggerName = GetTriggerName(ctrlId);
			if (triggerName != null)
			{
				Audio.ManagedAudioStateListenerDelegate audioStateListenerDelegate = AudioManager.GetAudioStateListener(triggerName);
				audioStateListenerDelegate?.Invoke(audioState, triggerName);
			}
		}

		private void Dispose(bool isDisposing)
		{
			if(_isDisposed) return;

			if(isDisposing)
			{
				_info.Dispose();
				_info = null;
				_requestListenersDelegates.Clear();
				_requestListenersDelegates = null;
				_indexTriggerIdToName.Clear();
				_indexTriggerIdToName = null;
				_triggerByName.Clear();
				_triggerByName = null;
			}

			RemoveListener();

			_isDisposed = true;
		}

		/// <summary>
		/// Add the specified trigger name and retrieves the trigger id. If trigger name is already existing, trigger id will be returned. Returns true if the trigger name is added
		/// </summary>
		/// <param name="triggerName"></param>
		/// <param name="triggerId"></param>
		/// <returns></returns>
		public static bool AddOrGetTrigger(string triggerName, out uint triggerId)
		{
			bool ret = false;
			triggerId = Audio.InvalidControlId;
			if(!_triggerByName.TryGetValue(triggerName, out triggerId))
			{
				triggerId = NativeAudioSystem.GetAudioTriggerId(triggerName);
				if(ret = (triggerId != Audio.InvalidControlId))
				{
					_triggerByName[triggerName] = triggerId;
					_indexTriggerIdToName[triggerId] = triggerName;
				}
				ret = true;
			}
			else
			{
				triggerId = _triggerByName[triggerName];
			}
			return ret;
		}

		/// <summary>
		/// Removes the specified trigger with the specified name. Returns true if the trigger is removed
		/// </summary>
		/// <param name="triggerName"></param>
		/// <returns></returns>
		public static bool RemoveTrigger(string triggerName)
		{
			bool ret = false;
			uint triggerId = Audio.InvalidControlId;
			if(_triggerByName.TryGetValue(triggerName, out triggerId))
			{
				NativeAudioSystem.StopTrigger(triggerId, false);
				ret = (triggerId != Audio.InvalidControlId);

				//update entries
				_triggerByName.Remove(triggerName);
				_indexTriggerIdToName.Remove(triggerId);

				//listeners should be removed manually by user
				ret = true;
			}
			return ret;
		}

		/// <summary>
		/// Gets the trigger name with the specified id. Note: This only works if the trigger is added by AddOrGetTrigger
		/// </summary>
		/// <param name="triggerId"></param>
		/// <returns></returns>
		public static string GetTriggerName(uint triggerId)
		{
			string ret = null;
			if(_indexTriggerIdToName.ContainsKey(triggerId))
			{
				ret = _indexTriggerIdToName[triggerId];
			}

			return ret;
		}

		/// <summary>
		/// Gets the trigger id with the specified name. Note. this only works if the trigger is added by AddOrGetTrigger
		/// </summary>
		/// <param name="triggerName"></param>
		/// <returns></returns>
		public static uint GetTriggerId(string triggerName)
		{
			uint triggerId = Audio.InvalidControlId;
			if(_triggerByName.ContainsKey(triggerName))
			{
				triggerId = _triggerByName[triggerName];
			}
			return triggerId;
		}

		/// <summary>
		/// Add an audio trigger state listener for the specified trigger name. Returns true if the trigger is existing and the no listener has been bound to the trigger yet. 
		/// Unlike the C++ audio listener, the listener will be notified based on trigger name (instead of based on all audio triggers)
		/// </summary>
		/// <param name="triggerName"></param>
		/// <param name="requestListener"></param>
		/// <returns></returns>
		public static bool AddAudioStateListener(string triggerName, Audio.ManagedAudioStateListenerDelegate requestListener)
		{
			bool ret = false;
			if(!_requestListenersDelegates.ContainsKey(triggerName))
			{
				_requestListenersDelegates.Add(triggerName, requestListener);
				ret = true;
			}
			return ret;
		}

		/// <summary>
		/// Remove an audio trigger state listener for the specified trigger name. Returns true if the trigger is existing and listener has been bound. 
		/// </summary>
		/// <param name="triggerName"></param>
		/// <returns></returns>
		public static bool RemoveAudioStateListener(string triggerName)
		{
			bool ret = false;
			if(_requestListenersDelegates.ContainsKey(triggerName))
			{
				_requestListenersDelegates.Remove(triggerName);
				ret = true;
			}
			return ret;
		}

		/// <summary>
		/// Retrieves the audio trigger state listener with the specified trigger name. Returns null if not found
		/// </summary>
		/// <param name="triggerName"></param>
		/// <returns></returns>
		public static Audio.ManagedAudioStateListenerDelegate GetAudioStateListener(string triggerName)
		{
			if(_requestListenersDelegates.ContainsKey(triggerName))
			{
				return _requestListenersDelegates[triggerName];
			}
			return null;
		}

		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}
	}
}

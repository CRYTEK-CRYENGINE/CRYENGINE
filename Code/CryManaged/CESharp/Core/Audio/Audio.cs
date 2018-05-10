// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;

namespace CryEngine
{
	using NativeAudioSystem = NativeInternals.IAudioSystem;

	/// <summary>
	/// State of an audio trigger.
	/// </summary>
	public enum AudioStateType
	{
		/// <summary>
		/// Indicates the trigger has started.
		/// </summary>
		TriggerStarted,
		/// <summary>
		/// Indicates the trigger has ended.
		/// </summary>
		TriggerEnded,
		/// <summary>
		/// Indicates the current state is unknown.
		/// </summary>
		Unknown
	}

	/// <summary>
	/// ID of an audio trigger.
	/// </summary>
	public struct AudioTriggerId
	{
		internal uint _id;
		internal AudioTriggerId(uint id)
		{
			_id = id;
		}

		/// <summary>
		/// Returns the a string in the format of "AudioTriggerId: {id}".
		/// </summary>
		/// <returns></returns>
		public override string ToString()
		{
			return string.Format("AudioTriggerId: {0}", _id);
		}

		/// <summary>
		/// Returns whether this ID is equal to the <see cref="Audio.InvalidControlId"/>.
		/// </summary>
		public bool IsValid
		{
			get
			{
				return _id != Audio.InvalidControlId;
			}
		}
	}

	/// <summary>
	/// Maps trigger identifier names to id's of internal audio project. Audio projects can be setup via SDL, WWISE or FMOD. 
	/// </summary>
	public sealed class Audio
	{
		internal static readonly uint InvalidControlId;
		internal static readonly uint InvalidSwitchStateId;
		internal static readonly uint InvalidEnvironmentId;

		/// <summary>
		/// internal listener for audio request listener in C++
		/// </summary>
		/// <param name="requestInfo"></param>
		internal delegate void RequestListenerDelegate(IntPtr requestInfo);

		/// <summary>
		/// Public listener for audio request listener in C#
		/// </summary>
		/// <param name="audioState"></param>
		/// <param name="triggerName"></param>
		public delegate void ManagedAudioStateListenerDelegate(AudioStateType audioState, string triggerName);

		static Audio()
		{
			// get IDs during initialization
			InvalidControlId = Global.InvalidControlId;
			InvalidSwitchStateId = Global.InvalidSwitchStateId;
			InvalidEnvironmentId = Global.InvalidEnvironmentId;
		}
		
		/// <summary>
		/// Retrieves and executes the specified audio trigger by name
		/// </summary>
		/// <param name="triggerName"></param>
		/// <returns>Returns true if the audio trigger is executed. False otherwise</returns>
		public static bool Play(string triggerName)
		{
			uint triggerId = InvalidControlId;
			AudioManager.AddOrGetTrigger(triggerName, out triggerId);
			if (triggerId == InvalidControlId)
			{
				return false;
			}
			NativeAudioSystem.ExecuteTrigger(triggerId);
			return true;
		}

		/// <summary>
		/// Returns the trigger id of the specified audio trigger 
		/// </summary>
		/// <param name="triggerName"></param>
		/// <returns></returns>
		public static AudioTriggerId GetTriggerId(string triggerName)
		{
			return new AudioTriggerId(AudioManager.GetTriggerId(triggerName));
		}

		/// <summary>
		/// Plays the audio specified by the trigger id. Returns true if the trigger id is valid
		/// </summary>
		/// <param name="triggerId"></param>
		/// <returns></returns>
		public static bool Play(AudioTriggerId triggerId)
		{
			if(triggerId.IsValid)
			{
				NativeAudioSystem.ExecuteTrigger(triggerId._id);
				return true;
			}
			return false;
		}

		/// <summary>
		/// Stops the audio trigger by the specified name. Only audio triggers that have been "played" can be stopped.
		/// </summary>
		/// <param name="triggerName"></param>
		/// <returns>True if the audio trigger is stopped</returns>
		public static bool Stop(string triggerName)
		{
			uint triggerId = AudioManager.GetTriggerId(triggerName);
			if (triggerId != Audio.InvalidControlId)
			{
				NativeAudioSystem.StopTrigger(triggerId, false);
			}
			return triggerId != InvalidControlId;
		}

		/// <summary>
		/// Stops the specified audio trigger. Only audio triggers that have been "played" can be stopped.
		/// </summary>
		/// <param name="triggerId"></param>
		/// <returns>True if the audio trigger is stopped</returns>
		public static bool Stop(AudioTriggerId triggerId)
		{
			if(triggerId.IsValid)
			{
				NativeAudioSystem.StopTrigger(triggerId._id);
				return true;
			}
			return false;
		}

		/// <summary>
		/// Stops all active audio triggers. 
		/// </summary>
		/// <returns></returns>
		public static void StopAllTriggers()
		{
			NativeAudioSystem.StopTrigger(InvalidControlId, false);
		}

		/// <summary>
		/// Stops all playing audio
		/// </summary>
		public static void StopAllSounds()
		{
			NativeAudioSystem.StopAllSounds();
		}

		/// <summary>
		/// Mute or unmute all sounds 
		/// </summary>
		/// <param name="enabled"></param>
		public static void EnableAllSound(bool enabled)
		{
			NativeAudioSystem.EnableAllSound(enabled);
		}

		/// <summary>
		/// Returns the path in which audio configuration data is stored 
		/// </summary>
		/// <returns></returns>
		public static string GetConfigurationPath()
		{
			return NativeAudioSystem.GetConfigPath();
		}

		/// <summary>
		/// Sets the specified audio parameter with the specified name to the specified value
		/// </summary>
		/// <param name="parameterName"></param>
		/// <param name="parameterVal"></param>
		public static void SetParameter(string parameterName, float parameterVal)
		{
			uint audioParameterId = NativeAudioSystem.GetAudioParameterId(parameterName);
			if (audioParameterId != Audio.InvalidControlId)
			{
				NativeAudioSystem.SetAudioParameter(audioParameterId, parameterVal);
			}
		}

		/// <summary>
		/// Sets the Audio switch with the specified name to the specified state name
		/// </summary>
		/// <param name="switchName"></param>
		/// <param name="switchStateName"></param>
		public static void SetSwitchState(string switchName, string switchStateName)
		{
			uint switchId = NativeAudioSystem.GetAudioSwitchId(switchName);
			if (switchId != Audio.InvalidControlId)
			{
				uint switchStateId = NativeAudioSystem.GetAudioSwitchStateId(switchId, switchStateName);
				if (switchStateId != Audio.InvalidSwitchStateId)
				{
					NativeAudioSystem.SetAudioSwitchState(switchId, switchStateId);
				}
			}
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="triggerName"></param>
		/// <param name="requestListener"></param>
		/// <returns></returns>
		public static bool AddAudioStateListener(string triggerName, ManagedAudioStateListenerDelegate requestListener)
		{
			bool ret = AudioManager.AddAudioStateListener(triggerName, requestListener);
			return ret;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="triggerName"></param>
		/// <returns></returns>
		public static bool RemoveAudioStateListener(string triggerName)
		{
			bool ret = AudioManager.RemoveAudioStateListener(triggerName);
			return ret;
		}
	}
}

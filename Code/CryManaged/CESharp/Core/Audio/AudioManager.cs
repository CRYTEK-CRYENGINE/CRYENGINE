using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace CryEngine
{
	/// <summary>
	/// Maps trigger identifier names to id's of internal audio project. Audio projects can be setup via SDL, WWISE or FMOD. 
	/// </summary>
	public class AudioManager
	{
		private static Dictionary<string, uint> _triggerByName = new Dictionary<string, uint>();

		/// <summary>
		/// Gets trigger ID by trigger name. Plays back trigger with respect to currently loaded audio lib.
		/// </summary>
		/// <returns><c>true</c>, if trigger loaded successfully.</returns>
		/// <param name="triggerName">Trigger name.</param>
		public static bool PlayTrigger(string triggerName)
		{
			uint triggerId;
			if (!_triggerByName.TryGetValue(triggerName, out triggerId))
				triggerId = _triggerByName[triggerName] = Engine.AudioSystem.GetAudioTriggerId(triggerName);

			if (triggerId != 0)
				Engine.AudioSystem.PlayAudioTriggerId(triggerId);
			return triggerId != 0;
		}
	}
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;

namespace CryEngine.Debugger.Mono
{
	public class CryEngineConsoleListener : IRemoteConsoleClientListener
	{
		public void OnAutoCompleteDone(List<string> autoCompleteList)
		{
			// Autocomplete is of no interest to the VS Extension.
		}

		public void OnConnected()
		{
			OutputHandler.LogMessage("Remote console connected.");
		}

		public void OnDisconnected()
		{
			OutputHandler.LogMessage("Remote console Disconnected.");
		}

		public void OnLogMessage(LogMessage message)
		{
			var cleanMessage = RemoveColorCodes(message.Message);
			OutputHandler.LogMessage(cleanMessage);
		}

		private string RemoveColorCodes(string message)
		{
			string cleanMessage = "";
			char[] messageChars = message.ToCharArray();
			for (int i = 0; i < messageChars.Length; ++i)
			{
				char c = messageChars[i];
				if (i == messageChars.Length - 1 || c != '$' || !Char.IsNumber(messageChars[i + 1]))
				{
					cleanMessage += c;
					continue;
				}

				// Skip adding this and the next character
				i++;
			}

			return cleanMessage;
		}
	}
}

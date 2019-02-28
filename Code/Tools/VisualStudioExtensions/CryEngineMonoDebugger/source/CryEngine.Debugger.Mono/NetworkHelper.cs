// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Linq;
using System.Net.NetworkInformation;

namespace CryEngine.Debugger.Mono
{
	public static class NetworkHelper
	{
		public static int GetAvailablePort(int startPort, int stopPort)
		{
			var random = new Random(DateTime.Now.Millisecond);

			IPGlobalProperties ipGlobalProperties = IPGlobalProperties.GetIPGlobalProperties();
			TcpConnectionInformation[] tcpConnInfoArray = ipGlobalProperties.GetActiveTcpConnections();

			var busyPorts = tcpConnInfoArray.Select(t => t.LocalEndPoint.Port).Where(v => v >= startPort && v <= stopPort).ToArray();

			var firstAvailableRandomPort = Enumerable.Range(startPort, stopPort - startPort).OrderBy(v => random.Next()).FirstOrDefault(p => !busyPorts.Contains(p));

			return firstAvailableRandomPort;
		}
	}
}

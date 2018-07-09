// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Net;

namespace Statoscope
{
	class LogDownload
	{
		public static void DownloadFile(string sourceLocation, string destFile)
		{
			WebClient webClient = new WebClient();
			webClient.DownloadFile(sourceLocation, destFile);
		}
	}
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Statoscope
{
	class FileLogProcessor
	{
		FileSessionInfo SessionInfo;

		public FileLogProcessor(string logFilename) : this(new FileSessionInfo(logFilename))
		{
		}

		public FileLogProcessor(FileSessionInfo sessionInfo)
		{
			SessionInfo = sessionInfo;
		}

		public LogData ProcessLog()
		{
			string logFilename = SessionInfo.LocalLogFile;
			Console.WriteLine("Reading log file {0}", logFilename);

			LogData logData = new LogData(SessionInfo);

			if (!logFilename.EndsWith(".log"))
			{
				using (FileLogBinaryDataStream logDataStream = new FileLogBinaryDataStream(logFilename))
				{
					BinaryLogParser logParser = new BinaryLogParser(logDataStream, logData, logFilename);
					logParser.ProcessDataStream();
				}
			}
			else
			{
				using (FileLogDataStream logDataStream = new FileLogDataStream(logFilename))
				{
					LogParser logParser = new LogParser(logDataStream, logData);
					logParser.ProcessDataStream();
				}
			}

			logData.ProcessRecords();

			return logData;
		}
	}
}

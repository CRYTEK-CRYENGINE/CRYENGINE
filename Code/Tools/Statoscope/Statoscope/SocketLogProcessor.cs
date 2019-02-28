// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net.Sockets;
using System.Windows.Forms;
using System.Threading;

namespace Statoscope
{
	class SocketLogProcessor
	{
		public LogData ProcessLog(StatoscopeForm ssForm)
		{
			TcpClient client = null;
			bool bShouldLogToFile = false;
			string connectionName = "";

			using (ConnectMessageBox connectMsgBox = new ConnectMessageBox())
			{
				bool successful = false;
				while (!successful)
				{
					if (connectMsgBox.ShowDialog() == DialogResult.OK)
					{
						try
						{
							client = new TcpClient(connectMsgBox.IpAddress, connectMsgBox.Port);
							bShouldLogToFile = connectMsgBox.logToFileCheckBox.Checked;
							connectionName = connectMsgBox.ConnectionName;
							successful = true;
						}
						catch (Exception ex)
						{
							ex.ToString();
							ConnectionErrorDialog ced = new ConnectionErrorDialog();
							ced.errorLabel.Text += connectMsgBox.IpAddress + ":" + connectMsgBox.Port;
							ced.ShowDialog();
						}
					}
					else
					{
						return null;
					}
				}
			}

			string logWriteFilename = "";

			if (bShouldLogToFile)
			{
				SaveFileDialog fd = new SaveFileDialog();
				fd.CheckFileExists = false;
				fd.FileName = "log.bin";
				fd.Filter = "Binary log files (*.bin)|*.bin|All files (*.*)|*.*";
				fd.FilterIndex = 0;

				if (fd.ShowDialog() == DialogResult.OK)
				{
					logWriteFilename = fd.FileName;
				}
			}

			SocketSessionInfo sessionInfo = new SocketSessionInfo(connectionName, logWriteFilename);
			SocketLogData logData = new SocketLogData(sessionInfo);
			logData.ProcessRecords();

			List<LogRange> logRanges = new List<LogRange>();
			LogRange logRange = new LogRange(logData, null);

			logRanges.Add(logRange);
			ssForm.AddToLogRangeList(logRange);

			ssForm.CreateLogControlTabPage(logRanges, connectionName, null);

			ThreadPool.QueueUserWorkItem(new WaitCallback(ProcessSocketDataStreamCB), new SSocketParseArgs(logData, logWriteFilename, client));

			return logData;
		}

		struct SSocketParseArgs
		{
			public SocketLogData m_logData;
			public TcpClient m_client;

			public SSocketParseArgs(SocketLogData logData, string logWriteFilename, TcpClient client)
			{
				m_logData = logData;
				m_logData.m_logWriteFilename = logWriteFilename;
				m_client = client;
			}
		}

		static void ProcessSocketDataStreamCB(object state)
		{
			SSocketParseArgs args = (SSocketParseArgs)state;
			NetworkStream netStream = args.m_client.GetStream();

			if (args.m_logData.m_logWriteFilename == "")
			{
				args.m_logData.m_logWriteFilename = Path.GetTempFileName();
				args.m_logData.m_bTempFile = true;
			}

			using (SocketLogBinaryDataStream logDataStream = new SocketLogBinaryDataStream(netStream, args.m_logData.m_logWriteFilename))
			{
				try
				{
					args.m_logData.DataStream = logDataStream;
					BinaryLogParser logParser = new BinaryLogParser(logDataStream, args.m_logData, "");
					logParser.ProcessDataStream();
				}
				catch (Exception)
				{
				}
			}

			netStream.Close();
			args.m_client.Close();
		}
	}
}

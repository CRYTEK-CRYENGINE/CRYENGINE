// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Net.Sockets;
using System.Threading;

namespace CryEngine.Debugger.Mono
{
	static class Constants
	{
		public const int DefaultPort = 4600;
		public const int DefaultBuffer = 4096;
		public const int ReconnectionTime = 3000;
		public const int ConnectionAttempts = 25;
	}

	public enum MessageType
	{
		Message = 0,
		Warning,
		Error,
	}

	public class LogMessage
	{
		public MessageType Type { get; set; }
		public string Message { get; set; }

		public LogMessage(MessageType type, string message)
		{
			Type = type;
			Message = message;
		}
	}

	interface IRemoteConsoleClientListener
	{
		void OnLogMessage(LogMessage message);
		void OnAutoCompleteDone(List<string> autoCompleteList);
		void OnConnected();
		void OnDisconnected();
	}

	interface IRemoteConsoleClient
	{
		void Start();
		void Stop();
		void SetServer(string ip);
		void ExecuteConsoleCommand(string command);
		void ExecuteGameplayCommand(string command);
		void SetListener(IRemoteConsoleClientListener listener);
		void PumpEvents();
	}

	class RemoteConsoleClientListenerState
	{
		public IRemoteConsoleClientListener Listener { get; set; } = null;
		public bool Connected { get; set; } = false;
		public bool AutoCompleteSent { get; set; } = false;

		public void SetListener(IRemoteConsoleClientListener listener)
		{
			Listener = listener;
			Reset();
		}

		public void Reset()
		{
			AutoCompleteSent = false;
		}
	}

	class RemoteConsole : IRemoteConsoleClient, IDisposable
	{
		private enum ConsoleEventType
		{
			Noop = 0,
			Req,
			LogMessage,
			LogWarning,
			LogError,
			ConsoleCommand,
			AutoCompleteList,
			AutoCompleteListDone,

			Strobo_GetThreads,
			Strobo_ThreadAdd,
			Strobo_ThreadDone,

			Strobo_GetResult,
			Strobo_ResultStart,
			Strobo_ResultDone,

			Strobo_StatStart,
			Strobo_StatAdd,
			Strobo_IPStart,
			Strobo_IPAdd,
			Strobo_SymStart,
			Strobo_SymAdd,
			Strobo_CallstackStart,
			Strobo_CallstackAdd,

			GameplayEvent,
		}

		private class CommandEvent
		{
			public ConsoleEventType Type { get; set; }
			public string Command { get; set; }

			public CommandEvent(ConsoleEventType type, string command)
			{
				Type = type;
				Command = command;
			}
		}

		private Thread _consoleThread = null;
		private TcpClient _clientSocket = null;
		private byte[] _inStream = new byte[Constants.DefaultBuffer];

		private string _server = "";
		private Queue<CommandEvent> _commands = new Queue<CommandEvent>();
		private Queue<string> _autoComplete = new Queue<string>();
		private Queue<LogMessage> _messages = new Queue<LogMessage>();

		private RemoteConsoleClientListenerState _listener = new RemoteConsoleClientListenerState();

		private object _locker = new object();
		private object _clientLocker = new object();

		private volatile bool _running = false;
		private volatile bool _stopping = false;
		private volatile bool _isConnected = false;
		private volatile bool _autoCompleteIsDone = false;
		private volatile bool _resetConnection = false;
		private int _ticks = 0;
		private int _connectionAttempts = 0;

		public int Port { get; private set; }

		public RemoteConsole()
		{
			_clientSocket = null;
			Port = Constants.DefaultPort;
		}

		public RemoteConsole(int port)
		{
			_clientSocket = null;
			Port = port;
		}

		public bool SetPort(string portText)
		{
			int port = -1;
			if((!_isConnected || _resetConnection) && int.TryParse(portText, out port))
			{
				Port = port;
				return true;
			}
			return false;
		}

		public void Start()
		{
			if (!_running)
			{
				_running = true;
				_consoleThread = new Thread(RemoteConsoleLoop);
				_consoleThread.Name = nameof(RemoteConsoleLoop);
				_consoleThread.Start();
			}
		}

		public void Stop()
		{
			if (_running)
			{
				_running = false;
				_stopping = true;
				lock (_clientLocker)
				{
					if (_clientSocket != null)
					{
						_clientSocket.Close();
					}
				}
				while (_stopping && _consoleThread.IsAlive)
				{
					Thread.Sleep(10);
				}
				_consoleThread = null;
			}
		}

		public void SetServer(string ip)
		{
			if (_listener.Listener != null)
			{
				_listener.Listener.OnDisconnected();
				_listener.Connected = false;
			}
			lock (_locker)
			{
				_server = ip;
				_ticks = 0;
				_autoCompleteIsDone = false;
				_listener.Reset();
				_resetConnection = true;
				_connectionAttempts = 0;
			}
		}

		public void ExecuteConsoleCommand(string command)
		{
			lock (_locker)
			{
				_commands.Enqueue(new CommandEvent(ConsoleEventType.ConsoleCommand, command));
			}
		}

		public void ExecuteGameplayCommand(string command)
		{
			lock (_locker)
			{
				_commands.Enqueue(new CommandEvent(ConsoleEventType.GameplayEvent, command));
			}
		}

		public void SetListener(IRemoteConsoleClientListener listener)
		{
			lock (_locker)
			{
				_listener.SetListener(listener);
			}
		}

		public void PumpEvents()
		{
			List<LogMessage> messages = null;
			List<string> autoComplete = null;
			bool sendConn = false;
			bool isConn = false;
			IRemoteConsoleClientListener l = null;
			lock (_locker)
			{
				l = _listener.Listener;
				messages = new List<LogMessage>(_messages);
				_messages.Clear();
				if (_autoCompleteIsDone && !_listener.AutoCompleteSent)
				{
					autoComplete = new List<string>(_autoComplete);
					_autoComplete.Clear();
					_listener.AutoCompleteSent = true;
				}
				if (_isConnected != _listener.Connected && !_resetConnection)
				{
					_listener.Connected = _isConnected;
					sendConn = true;
					isConn = _isConnected;
				}
			}
			if (l != null)
			{
				if (sendConn)
				{
					if (isConn)
					{
						l.OnConnected();
					}
					else
					{
						l.OnDisconnected();
					}
				}
				if (messages != null)
				{
					foreach (var message in messages)
					{
						l.OnLogMessage(message);
					}
				}
				if (autoComplete != null)
				{
					l.OnAutoCompleteDone(autoComplete);
				}
			}
		}

		private void RemoteConsoleLoop()
		{
			while (_running)
			{
				if(!_resetConnection && !_isConnected && _connectionAttempts >= Constants.ConnectionAttempts)
				{
					AddLogMessage(MessageType.Message, string.Format("Unable to connect after {0} connection attemtps.", _connectionAttempts));
					break;
				}

				if (!_isConnected || _resetConnection)
				{
					ClearCommands();
					if (_ticks++ % Constants.ReconnectionTime == 0)
					{
						_connectionAttempts++;
						lock (_clientLocker)
						{
							if (_clientSocket != null && _clientSocket.Connected)
							{
								_clientSocket.GetStream().Close();
								_clientSocket.Close();
								_clientSocket.Dispose();
							}
							_clientSocket = new TcpClient();
						}
						try
						{
							_clientSocket.Connect(_server, Port);
							NetworkStream stream = _clientSocket.GetStream();
							stream.ReadTimeout = 3000;
							stream.WriteTimeout = 3000;
							_ticks = 0;
						}
#if DEBUG
						catch (Exception ex)
						{
							// Prevent log-spam if connecting failed but there are still connection attempts left.
							if(!(ex is SocketException) && _connectionAttempts < Constants.ConnectionAttempts)
							{
								AddLogMessage(MessageType.Message, ex.Message);
							}
						}
#else
						catch (System.Exception){ }
#endif
					}

					_isConnected = _clientSocket != null ? _clientSocket.Connected : false;
					if (!_isConnected)
					{
						_autoCompleteIsDone = false;
					}
					else
					{
						// Reset the connection attempts if reconnecting was succesful.
						_connectionAttempts = 0;
					}

					if (_resetConnection)
					{
						_resetConnection = false;
					}
				}
				else
				{
					try
					{
						if(_clientSocket.Available > 0)
						{
							_isConnected = ProcessClient();
						}

						// Pump the events immediately since they will be logged thread-safe later on.
						PumpEvents();
					}
					catch(Exception ex)
					{
						string message = string.Format("Disconnecting the remote console because it ran into the following exception: {0}{1}{2}", ex.Message, Environment.NewLine, ex.StackTrace);
						AddLogMessage(MessageType.Error, message);
						break;
					}
					
				}
			}
			_stopping = false;
		}

		private bool ReadData(ref ConsoleEventType id, ref string data)
		{
			try
			{
				NetworkStream stream = _clientSocket.GetStream();
				int ret = 0;
				while (true)
				{
					ret += stream.Read(_inStream, ret, Constants.DefaultBuffer - ret);
					if(ret == 0)
					{
						return false;
					}

					if (_inStream[ret - 1] == '\0')
					{
						break;
					}
				}
				string returndata = System.Text.Encoding.ASCII.GetString(_inStream);
				id = (ConsoleEventType)(returndata[0] - '0');
				int index = returndata.IndexOf('\0');
				data = returndata.Substring(1, index - 1);
			}
			catch (Exception)
			{
				return false;
			}
			return true;
		}

		private bool SendData(ConsoleEventType id, string data = "")
		{
			char cid = (char)((char)id + '0');
			string msg = "";
			msg += cid;
			msg += data;
			msg += "\0";
			try
			{
				byte[] outStream = System.Text.Encoding.ASCII.GetBytes(msg);
				NetworkStream stream = _clientSocket.GetStream();
				stream.Write(outStream, 0, outStream.Length);
				stream.Flush();
			}
			catch (Exception)
			{
				return false;
			}
			return true;
		}

		private void AddLogMessage(MessageType type, string message)
		{
			lock (_locker)
			{
				_messages.Enqueue(new LogMessage(type, message));
			}
		}

		private void AddAutoCompleteItem(string item)
		{
			lock (_locker)
			{
				_autoComplete.Enqueue(item);
			}
		}

		private bool GetCommand(ref CommandEvent command)
		{
			bool res = false;
			lock (_locker)
			{
				if (_commands.Count > 0)
				{
					command = _commands.Dequeue();
					res = true;
				}
			}
			return res;
		}

		private void AutoCompleteDone()
		{
			lock (_locker)
			{
				_autoCompleteIsDone = true;
			}
		}

		private void ClearCommands()
		{
			lock (_locker)
			{
				_commands.Clear();
			}
		}

		private bool ProcessClient()
		{
			ConsoleEventType eventType = ConsoleEventType.Noop;
			string data = "";
			if (!ReadData(ref eventType, ref data))
				return false;

			switch (eventType)
			{
				case ConsoleEventType.LogMessage:
					AddLogMessage(MessageType.Message, data);
					return SendData(ConsoleEventType.Noop);
				case ConsoleEventType.LogWarning:
					AddLogMessage(MessageType.Warning, data);
					return SendData(ConsoleEventType.Noop);
				case ConsoleEventType.LogError:
					AddLogMessage(MessageType.Error, data);
					return SendData(ConsoleEventType.Noop);
				case ConsoleEventType.AutoCompleteList:
					AddAutoCompleteItem(data);
					return SendData(ConsoleEventType.Noop);
				case ConsoleEventType.AutoCompleteListDone:
					AutoCompleteDone();
					return SendData(ConsoleEventType.Noop);
				case ConsoleEventType.Req:
					CommandEvent command = null;
					if (GetCommand(ref command))
						return SendData(command.Type, command.Command);
					else
						return SendData(ConsoleEventType.Noop);
				default:
					return SendData(ConsoleEventType.Noop);
			}
		}

		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		public void Dispose(bool disposing)
		{
			if(disposing)
			{
				_clientSocket.Dispose();
				_clientSocket = null;
			}
		}
	}
}

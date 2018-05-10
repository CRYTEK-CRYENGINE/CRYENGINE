// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using MiscUtil.IO;
using System.IO;
using MiscUtil.Conversion;
using System.Diagnostics;
using System.Text.RegularExpressions;
using System.Globalization;

namespace LuaRemoteDebugger
{
	enum PacketType : byte
	{
		Version,
		LogMessage,
		Break,
		Continue,
		StepOver,
		StepInto,
		StepOut,
		SetBreakpoints,
		ExecutionLocation,
		LuaVariables,
		BinaryFileDetected,
		GameFolder,
		FileMD5,
		FileContents,
		EnableCppCallstack,
		ModulesInformation,
		ScriptError,
	}

	public enum Platform : byte
	{
		Unknown,
		Win32,
		Win64,
		Xbox,
		PS3,
	};

	public enum LuaVariableType : byte
	{
		Any = 0,
		Nil,
		Boolean,
		Handle,
		Number,
		String,
		Table,
		Function,
		UserData,
		Vector,
	}

	enum CppCallStackMode
	{
		Disabled,
		Condensed,
		Full,
	}

	struct Breakpoint
	{
		public string FileName;
		public int Line;
	}

	public interface ILuaAnyValue
	{
		LuaVariableType Type { get; }
	}

	public class LuaValueBase<T> : ILuaAnyValue
	{
		protected LuaVariableType type;
		protected T value;
		public LuaVariableType Type { get { return type; } }
		public T Value { get { return value; } }
		public override string ToString()
		{
			return string.Format("{{{0}}}", Type);
		}
		public override bool Equals(object obj)
		{
			LuaValueBase<T> other = obj as LuaValueBase<T>;
			if (other != null)
			{
				return value.Equals(other.value);
			}
			return false;
		}
		public override int GetHashCode()
		{
			return value.GetHashCode();
		}
	}

	public class LuaNil : LuaValueBase<LuaNil.Empty>
	{
		public static LuaNil Instance = new LuaNil();
		public struct Empty { }
		private LuaNil() { type = LuaVariableType.Nil; }
	}

	public class LuaBool : LuaValueBase<bool>
	{
		public LuaBool(bool value) { type = LuaVariableType.Boolean; this.value = value; }
		public override string ToString()
		{
			return string.Format("{0}", value);
		}
	}

	public class LuaHandle32 : LuaValueBase<UInt32>
	{
		public LuaHandle32(UInt32 value) { type = LuaVariableType.Handle; this.value = value; }
		public override string ToString()
		{
			return string.Format("0x{0:x8}", value);
		}
	}

	public class LuaHandle64 : LuaValueBase<UInt64>
	{
		public LuaHandle64(UInt64 value) { type = LuaVariableType.Handle; this.value = value; }
		public override string ToString()
		{
			return string.Format("0x{0:x16}", value);
		}
	}

	public class LuaNumber : LuaValueBase<float>
	{
		public LuaNumber(float value) { type = LuaVariableType.Number; this.value = value; }
		public override string ToString()
		{
			return string.Format("{0}", value);
		}
	}

	public class LuaString : LuaValueBase<string>
	{
		public LuaString(string value) { type = LuaVariableType.String; this.value = value; }
		public override string ToString()
		{
			return string.Format("\"{0}\"", value);
		}
	}

	public class LuaTable : LuaValueBase<Dictionary<ILuaAnyValue, ILuaAnyValue>>
	{
		public LuaTable() { type = LuaVariableType.Table; this.value = new Dictionary<ILuaAnyValue, ILuaAnyValue>(); }
	}

	public class LuaFunction32 : LuaValueBase<UInt32>
	{
		public LuaFunction32(UInt32 value) { type = LuaVariableType.Function; this.value = value; }
		public override string ToString()
		{
			return string.Format("0x{0:x8}", value);
		}
	}

	public class LuaFunction64 : LuaValueBase<UInt64>
	{
		public LuaFunction64(UInt64 value) { type = LuaVariableType.Function; this.value = value; }
		public override string ToString()
		{
			return string.Format("0x{0:x16}", value);
		}
	}

	public class LuaUserData32 : LuaValueBase<LuaUserData32.DataStruct>
	{
		public struct DataStruct
		{
			public UInt32 Pointer;
			public Int32 Reference;
		}
		public LuaUserData32(UInt32 pointer, Int32 reference) { type = LuaVariableType.UserData; this.value = new DataStruct { Pointer = pointer, Reference = reference }; }
		public override string ToString()
		{
			return string.Format("(0x{0:x8}, {1})", value.Pointer, value.Reference);
		}
	}

	public class LuaUserData64 : LuaValueBase<LuaUserData64.DataStruct>
	{
		public struct DataStruct
		{
			public UInt64 Pointer;
			public Int32 Reference;
		}
		public LuaUserData64(UInt64 pointer, Int32 reference) { type = LuaVariableType.UserData; this.value = new DataStruct { Pointer = pointer, Reference = reference }; }
		public override string ToString()
		{
			return string.Format("(0x{0:x16}, {1})", value.Pointer, value.Reference);
		}
	}

	public class LuaVector : LuaValueBase<LuaVector.DataStruct>
	{
		public struct DataStruct
		{
			public float X, Y, Z;
		}
		public LuaVector(float x, float y, float z) { type = LuaVariableType.Vector; this.value = new DataStruct { X = x, Y = y, Z = z }; }
		public override string ToString()
		{
			return string.Format("({0}, {1}, {2})", value.X, value.Y, value.Z);
		}
	}

	public struct CallStackItem
	{
		public string Source;
		public int Line;
		public string Description;
		public int FrameIndex;
	}

	public delegate void EmptyFunctionHandler();
	public delegate void LogMessageHandler(string message);
	public delegate void ExecutionLocationChangedHandler(string source, int line, List<CallStackItem> callstack);
	public delegate void LuaVariablesChangedHandler(LuaTable table);
	public delegate void VersionsInformationHandler(Platform hostPlatform, int hostVersion, int clientVersion);
	public delegate void GameFolderHandler(string gameFolder);
	public delegate void FileMD5Handler(string fileName, byte[] md5);
	public delegate void FileContentsHandler(string fileName, string contents);
	public delegate void ScriptErrorHandler(string error);

	class LuaRemoteDebugger
	{
		CryNotificationNetworkClient networkClient = new CryNotificationNetworkClient();
		MemoryStream memStreamSendBuffer = new MemoryStream();
		EndianBinaryWriter sendBufferWriter;
		string connectedIpAddress = null;

		private const string ChannelName = "LuaDebug";
		public const int ClientVersion = 7;
		private int hostVersion = 0;
		private Platform hostPlatform = Platform.Unknown;
		private SymbolsManager symbolsManager;
		private CppCallStackMode cppCallStackMode = CppCallStackMode.Disabled;

		public int HostVersion { get { return hostVersion; } }
		public string ConnectedIpAddress { get { return connectedIpAddress; } }

		public event LogMessageHandler MessageLogged;
		public event ExecutionLocationChangedHandler ExecutionLocationChanged;
		public event LuaVariablesChangedHandler LuaVariablesChanged;
		public event EmptyFunctionHandler Connected;
		public event EmptyFunctionHandler Disconnected;
		public event EmptyFunctionHandler BinaryFileDetected;
		public event VersionsInformationHandler VersionInformationReceived;
		public event GameFolderHandler GameFolderReceived;
		public event FileMD5Handler FileMD5Received;
		public event FileContentsHandler FileContentsReceived;
		public event ScriptErrorHandler ScriptError;

		public LuaRemoteDebugger()
		{
			networkClient.NotificationNetworkReceived += OnNotificationNetworkReceived;
			sendBufferWriter = new EndianBinaryWriter(new BigEndianBitConverter(), memStreamSendBuffer);
		}

		private void Reset()
		{
			hostVersion = 0;
			hostPlatform = Platform.Unknown;
			symbolsManager = null;
			cppCallStackMode = CppCallStackMode.Disabled;
		}

		public void SetSymbolsManager(SymbolsManager manager)
		{
			symbolsManager = manager;
		}

		public bool Connect(string ipAddress, int port, out string error)
		{
			if (connectedIpAddress != null)
			{
				OnDisconnected();
			}
			try
			{
				networkClient.Connect(ipAddress, port);
			}
			catch (System.Net.Sockets.SocketException ex)
			{
				error = ex.Message;
				return false;
			}
			if (networkClient.IsConnected)
			{
				LogMessage("Connected to {0}!", ipAddress);
				networkClient.RegisterChannel(ChannelName);
				SendVersion();
				connectedIpAddress = ipAddress;
				if (Connected != null)
				{
					Connected();
				}
			}
			error = string.Empty;
			return true;
		}

		public void Disconnect()
		{
			networkClient.Disconnect();
		}

		private void OnDisconnected()
		{
			LogMessage("Disconnected!");
			connectedIpAddress = null;
			Reset();
			if (Disconnected != null)
			{
				Disconnected();
			}
		}

		public void Update()
		{
			networkClient.Update();
			if (connectedIpAddress != null && !networkClient.IsConnected)
			{
				OnDisconnected();
			}
		}
		
		private void SendVersion()
		{
			sendBufferWriter.Write((byte)PacketType.Version);
			sendBufferWriter.Write(ClientVersion);
			SendPacket();
		}

		public void Break()
		{
			sendBufferWriter.Write((byte)PacketType.Break);
			SendPacket();
		}

		public void Continue()
		{
			sendBufferWriter.Write((byte)PacketType.Continue);
			SendPacket();
		}

		public void StepOver()
		{
			sendBufferWriter.Write((byte)PacketType.StepOver);
			SendPacket();
		}

		public void StepInto()
		{
			sendBufferWriter.Write((byte)PacketType.StepInto);
			SendPacket();
		}

		public void StepOut()
		{
			sendBufferWriter.Write((byte)PacketType.StepOut);
			SendPacket();
		}

		public void SetBreakpoints(List<Breakpoint> breakpoints)
		{
			sendBufferWriter.Write((byte)PacketType.SetBreakpoints);
			sendBufferWriter.Write(breakpoints.Count);
			foreach (Breakpoint breakpoint in breakpoints)
			{
				WriteString(sendBufferWriter, breakpoint.FileName);
				sendBufferWriter.Write(breakpoint.Line);
			}
			SendPacket();
		}

		public void RequestFileMD5(string fileName)
		{
			// MD5 checking is only supported in version 2 and above
			if (hostVersion >= 2)
			{
				sendBufferWriter.Write((byte)PacketType.FileMD5);
				WriteString(sendBufferWriter, fileName);
				SendPacket();
			}
		}

		public void RequestFileContents(string fileName)
		{
			// File contents is only supported in version 2 and above
			if (hostVersion >= 2)
			{
				sendBufferWriter.Write((byte)PacketType.FileContents);
				WriteString(sendBufferWriter, fileName);
				SendPacket();
			}
		}

		public void EnableCppCallstack(CppCallStackMode mode)
		{
			if (hostVersion >= 4)
			{
				// Request the module information if necessary
				if (mode != CppCallStackMode.Disabled && hostPlatform == Platform.Xbox && symbolsManager == null)
				{
					sendBufferWriter.Write((byte)PacketType.ModulesInformation);
					SendPacket();
				}
			}
			if (hostVersion >= 3)
			{
				cppCallStackMode = mode;
				bool enable = (mode != CppCallStackMode.Disabled);
				sendBufferWriter.Write((byte)PacketType.EnableCppCallstack);
				sendBufferWriter.Write(enable);
				SendPacket();
			}
		}

		private void SendPacket()
		{
			networkClient.Send(ChannelName, memStreamSendBuffer.ToArray());
			memStreamSendBuffer.SetLength(0);
		}

		private void ReceiveVersion(EndianBinaryReader reader)
		{
			hostVersion = reader.ReadInt32();
			hostPlatform = Platform.Unknown;
			if (hostVersion >= 4)
			{
				hostPlatform = (Platform)reader.ReadByte();
			}
			LogMessage("Platform: {1}, Host version: {0}, Client version: {2}", hostVersion, hostPlatform, ClientVersion);
			if (VersionInformationReceived != null)
			{
				VersionInformationReceived(hostPlatform, hostVersion, ClientVersion);
			}
		}

		private void ReceiveLogMessage(EndianBinaryReader reader)
		{
			string message = ReadString(reader);
			LogMessage(message);
		}

		private void ReceiveExecutionLocation(EndianBinaryReader reader)
		{
			string source = ReadString(reader);
			int line = reader.ReadInt32();

			// Lua callstack
			int luaCallstackLength = reader.ReadInt32();
			List<CallStackItem> callstack = new List<CallStackItem>(luaCallstackLength);
			for (int i = 0; i < luaCallstackLength; ++i)
			{
				CallStackItem item;
				item.FrameIndex = i;
				item.Source = ReadString(reader);
				item.Line = reader.ReadInt32();
				item.Description = ReadString(reader);
				if (hostVersion < 6)
				{
					ReadString(reader);
				}
				callstack.Add(item);
			}

			if (hostVersion >= 3)
			{
				// C++ callstack
				List<string> cppCallstack = new List<string>();
				int cppCallstackLength = reader.ReadInt32();
				for (int i = 0; i < cppCallstackLength; ++i)
				{
					string cppItem = ReadString(reader);
					cppCallstack.Add(cppItem.Trim());
				}

				// For PC parse the callstack (symbols are already resolved)
				if (hostPlatform == Platform.Win32 || hostPlatform == Platform.Win64 || hostPlatform == Platform.Unknown)
				{
					int frameIndex = luaCallstackLength;
					foreach (string cppItem in cppCallstack)
					{
						CallStackItem item = new CallStackItem();
						Match match = Regex.Match(cppItem, @"(.*)  \[(.*):(\d+)]");
						if (match.Success && match.Groups.Count == 4)
						{
							item.Description = match.Groups[1].Value;
							item.Source = match.Groups[2].Value;
							item.Line = Int32.Parse(match.Groups[3].Value);
						}
						else
						{
							item.Description = cppItem;
						}
						item.FrameIndex = ++frameIndex;
						callstack.Add(item);
					}
				}
				/* These platforms are deprecated
				else if (hostPlatform == Platform.Xbox || hostPlatform == Platform.PS3)
				{
					// For consoles we need to resolve the symbols using the symbols manager
					if (symbolsManager != null)
					{
						List<UInt32> addresses = new List<UInt32>();
						foreach (string cppItem in cppCallstack)
						{
							UInt32 address;
							if (UInt32.TryParse(cppItem, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out address))
							{
								addresses.Add(address);
							}
						}
						callstack.AddRange(symbolsManager.GetSymbolNamesFromAddresses(addresses));
					}
					else
					{
						foreach (string cppItem in cppCallstack)
						{
							CallStackItem item = new CallStackItem();
							item.Description = "<symbol 0x" + cppItem + " unresolved>";
							callstack.Add(item);
						}
					}
				}
				*/
				if (cppCallStackMode == CppCallStackMode.Condensed)
				{
					// Trim out the part of the C++ callstack down to where the LUA function is initially called
					for (int i = luaCallstackLength; i < callstack.Count; ++i)
					{
						if (callstack[i].Description.Contains("lua_pcall"))
						{
							do 
							{
								++i;
							} while (i < callstack.Count && callstack[i].Description.StartsWith("CScriptSystem::EndCall", StringComparison.OrdinalIgnoreCase));
							callstack.RemoveRange(luaCallstackLength, i - luaCallstackLength);
							break;
						}
					}
				}
			}

			if (ExecutionLocationChanged != null)
			{
				ExecutionLocationChanged(source, line, callstack);
			}
		}

		private ILuaAnyValue ReadLuaAnyValue(EndianBinaryReader reader, int pointerSize)
		{
			LuaVariableType type = (LuaVariableType)reader.ReadByte();
			switch (type)
			{
				case LuaVariableType.Nil:
					return LuaNil.Instance;
				case LuaVariableType.Boolean:
					return new LuaBool(reader.ReadBoolean());
				case LuaVariableType.Handle:
					if (pointerSize == 4)
						return new LuaHandle32(reader.ReadUInt32());
					else
						return new LuaHandle64(reader.ReadUInt64());
				case LuaVariableType.Number:
					return new LuaNumber(reader.ReadSingle());
				case LuaVariableType.String:
					return new LuaString(ReadString(reader));
				case LuaVariableType.Table:
					return ReadLuaTable(reader, pointerSize);
				case LuaVariableType.Function:
					if (pointerSize == 4)
						return new LuaFunction32(reader.ReadUInt32());
					else
						return new LuaFunction64(reader.ReadUInt64());
				case LuaVariableType.UserData:
					if (pointerSize == 4)
					{
						UInt32 pointer = reader.ReadUInt32();
						int reference = reader.ReadInt32();
						return new LuaUserData32(pointer, reference);
					}
					else
					{
						UInt64 pointer = reader.ReadUInt64();
						int reference = reader.ReadInt32();
						return new LuaUserData64(pointer, reference);
					}
				case LuaVariableType.Vector:
					float x = reader.ReadSingle();
					float y = reader.ReadSingle();
					float z = reader.ReadSingle();
					return new LuaVector(x, y, z);
			}
			throw new Exception("Unrecognised lua value type");
		}

		private LuaTable ReadLuaTable(EndianBinaryReader reader, int pointerSize)
		{
			LuaTable table = new LuaTable();
			int count = reader.ReadUInt16();
			for (int i = 0; i < count; ++i)
			{
				ILuaAnyValue key = ReadLuaAnyValue(reader, pointerSize);
				ILuaAnyValue value = ReadLuaAnyValue(reader, pointerSize);
                if (!table.Value.ContainsKey(key))
                {
                    table.Value[key] = value;
                }
            }
			return table;
		}

		private void ReceiveLuaVariables(EndianBinaryReader reader)
		{
			int pointerSize = reader.ReadByte();
			Debug.Assert(pointerSize == 4 || pointerSize == 8);
			LuaTable table = ReadLuaTable(reader, pointerSize);
			if (LuaVariablesChanged != null)
			{
				LuaVariablesChanged(table);
			}
		}

		private void ReceiveBinaryFileDetected(EndianBinaryReader reader)
		{
			LogMessage("Binary Lua file detected! These files cannot be debugged.");
			if (BinaryFileDetected != null)
			{
				BinaryFileDetected();
			}
		}

		private void ReceiveGameFolder(EndianBinaryReader reader)
		{
			string gameFolder = ReadString(reader);
			if (GameFolderReceived != null)
			{
				GameFolderReceived(gameFolder);
			}
		}

		private void ReceiveFileMD5(EndianBinaryReader reader)
		{
			string fileName = ReadString(reader);
			byte[] md5 = new byte[16];
			for (int i=0; i<16; ++i)
			{
				md5[i] = reader.ReadByte();
			}
			if (FileMD5Received != null)
			{
				FileMD5Received(fileName, md5);
			}
		}

		private void ReceiveFileContents(EndianBinaryReader reader)
		{
			string fileName = ReadString(reader);
			UInt32 length = reader.ReadUInt32();
			byte[] buffer = new byte[length];
			reader.Read(buffer, 0, (int)length);
			string contents = System.Text.Encoding.ASCII.GetString(buffer);
			if (FileContentsReceived != null)
			{
				FileContentsReceived(fileName, contents);
			}
		}

		private void ReceiveModulesInformation(EndianBinaryReader reader)
		{
			switch (hostPlatform)
			{
				case Platform.Xbox:
					XboxSymbolsManager xboxSymbolsManager = new XboxSymbolsManager();
					int numModules = reader.ReadInt32();
					for (int i = 0; i < numModules; ++i)
					{
						string path = ReadString(reader);
						UInt32 baseAddress = reader.ReadUInt32();
						UInt32 size = reader.ReadUInt32();
						Int32 a = reader.ReadInt32();
						Int16 b = reader.ReadInt16();
						Int16 c = reader.ReadInt16();
						Byte[] d = reader.ReadBytes(8);
						Guid guid = new Guid(a, b, c, d);
						UInt32 age = reader.ReadUInt32();
						try
						{
							xboxSymbolsManager.AddModuleInformation(path, baseAddress, size, guid, age);
						}
						catch (System.IO.FileNotFoundException)
						{
							// This can happen if Dia2Lib.dll cannot be found
						}
					}
					symbolsManager = xboxSymbolsManager;
					break;
				default:
					LogMessage("Received module information for platform {0} but this is not interpreted yet", hostPlatform);
					break;
			}
		}

		private void ReceiveScriptError(EndianBinaryReader reader)
		{
			string error = ReadString(reader);
			LogMessage("Script Error: " + error);
			if (ScriptError != null)
			{
				ScriptError(error);
			}
		}

		private void OnNotificationNetworkReceived(string channelName, byte[] data)
		{
			using (MemoryStream memStream = new MemoryStream(data))
			{
				using (EndianBinaryReader reader = new EndianBinaryReader(new BigEndianBitConverter(), memStream))
				{
					PacketType packetType = (PacketType)reader.ReadByte();
					//LogMessage("Received packet of type: {0}", packetType);
					switch (packetType)
					{
						case PacketType.Version:
							ReceiveVersion(reader);
							break;
						case PacketType.LogMessage:
							ReceiveLogMessage(reader);
							break;
						case PacketType.ExecutionLocation:
							ReceiveExecutionLocation(reader);
							break;
						case PacketType.LuaVariables:
							ReceiveLuaVariables(reader);
							break;
						case PacketType.BinaryFileDetected:
							ReceiveBinaryFileDetected(reader);
							break;
						case PacketType.GameFolder:
							ReceiveGameFolder(reader);
							break;
						case PacketType.FileMD5:
							ReceiveFileMD5(reader);
							break;
						case PacketType.FileContents:
							ReceiveFileContents(reader);
							break;
						case PacketType.ModulesInformation:
							ReceiveModulesInformation(reader);
							break;
						case PacketType.ScriptError:
							ReceiveScriptError(reader);
							break;
						default:
							throw new Exception("Unrecognised packet type");
					}
				}
			}
		}

		private static string ReadString(EndianBinaryReader reader)
		{
			int length;
			length = reader.ReadInt32();
			byte[] data = reader.ReadBytes(length);
			string result = System.Text.Encoding.ASCII.GetString(data);
			if (reader.ReadByte() != 0)	// Check NULL terminator
				throw new Exception("Invalid string");
			return result;
		}

		private static void WriteString(EndianBinaryWriter writer, string str)
		{
			int length = str.Length;
			writer.Write(length);
			byte[] data = System.Text.Encoding.ASCII.GetBytes(str);
			writer.Write(data);
			writer.Write((byte)0);	// NULL terminator
		}

		private void LogMessage(string format, params object[] args)
		{
			LogMessage(string.Format(format, args));
		}

		private void LogMessage(string message)
		{
			if (MessageLogged != null)
			{
				MessageLogged(message);
			}
		}
	}
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net.Sockets;
using System.Net;
using System.IO;
using MiscUtil.IO;
using MiscUtil.Conversion;
using System.Threading;

namespace LuaRemoteDebugger
{
	public delegate void NotificationNetworkReceiveHandler(string channelName, byte[] data);

	class CryNotificationNetworkClient
	{
		TcpClient tcpClient;

		public const Int32 NN_PORT = 9432;
		public const Int32 NN_PORT_EDITOR = 9433;

		const Int32 NN_CHANNEL_NAME_LENGTH_MAX = 16;

		const Int32 NN_PACKET_HEADER_LENGTH = 2 * sizeof(UInt32) + NN_CHANNEL_NAME_LENGTH_MAX;
		const UInt32 NN_PACKET_HEADER_ID = 0xbada2217;
		const Int32 NN_PACKET_HEADER_OFFSET_ID = 0;
		const Int32 NN_PACKET_HEADER_OFFSET_DATA_LENGTH = sizeof(UInt32);
		const Int32 NN_PACKET_HEADER_OFFSET_CHANNEL = sizeof(UInt32) + sizeof(UInt32);

		const UInt32 NN_PACKET_CHANNEL_REGISTER_ID = 0xab4eda30;
		const UInt32 NN_PACKET_CHANNEL_UNREGISTER_ID = 0xfa4e3423;

		public event NotificationNetworkReceiveHandler NotificationNetworkReceived;

		public bool IsConnected { get { return tcpClient != null && tcpClient.Connected; } }

		public void Connect(string hostname)
		{
			Connect(hostname, NN_PORT);
		}

		public void Connect(string hostname, int port)
		{
			Disconnect();

			tcpClient = new TcpClient(hostname, port);
		}

		public void Disconnect()
		{
			if (tcpClient != null)
			{
				tcpClient.Close();
				tcpClient = null;
			}
		}

		public void Send(string channelName, byte[] buffer)
		{
			SendInternal(channelName, buffer, NN_PACKET_HEADER_ID);
		}

		public void RegisterChannel(string channelName)
		{
			SendInternal(channelName, new byte[0], NN_PACKET_CHANNEL_REGISTER_ID);
		}

		public void UnregisterChannel(string channelName)
		{
			SendInternal(channelName, new byte[0], NN_PACKET_CHANNEL_UNREGISTER_ID);
		}

		private void SendInternal(string channelName, byte[] buffer, UInt32 packetHeaderType)
		{
			if (channelName.Length > NN_CHANNEL_NAME_LENGTH_MAX)
				throw new ArgumentException("Channel name too long", "channelName");

			if (tcpClient != null && tcpClient.Connected)
			{
				try
				{
					EndianBinaryWriter writer = new EndianBinaryWriter(new BigEndianBitConverter(), tcpClient.GetStream());
					writer.Write(packetHeaderType);
					writer.Write(buffer.Length);

					byte[] byChannelName = new byte[NN_CHANNEL_NAME_LENGTH_MAX];
					System.Text.Encoding.ASCII.GetBytes(channelName, 0, channelName.Length, byChannelName, 0);

					writer.Write(byChannelName);
					writer.Write(buffer);
				}
				catch (System.IO.IOException)
				{
					// Connection may have been closed
				}
			}
		}

		public void Update()
		{
			if (tcpClient != null && tcpClient.Connected)
			{
				NetworkStream networkStream = tcpClient.GetStream();
				if (networkStream.DataAvailable)
				{
					EndianBinaryReader reader = new EndianBinaryReader(new BigEndianBitConverter(), networkStream);
					UInt32 header = reader.ReadUInt32();
					if (header != NN_PACKET_HEADER_ID)
					{
						// Wrong header ID, bail!
						Disconnect();
					}
					Int32 length = reader.ReadInt32();
					byte[] byChannelName = reader.ReadBytes(NN_CHANNEL_NAME_LENGTH_MAX);
					string channelName = System.Text.Encoding.ASCII.GetString(byChannelName);
					channelName = channelName.TrimEnd('\0');
					byte[] data = reader.ReadBytes(length);
					if (NotificationNetworkReceived != null)
					{
						NotificationNetworkReceived(channelName, data);
					}
				}
			}
		}
	}
}

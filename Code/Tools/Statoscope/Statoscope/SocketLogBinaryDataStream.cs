// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.IO;
using System.Net.Sockets;
using System;

namespace Statoscope
{
	class SocketLogBinaryDataStream : BinaryLogDataStream
	{
		BinaryWriter m_binaryWriter;

		protected override void Dispose(bool disposing)
		{
			if (m_binaryWriter != null)
			{
				m_binaryWriter.Close();
				m_binaryWriter = null;
			}

			base.Dispose(disposing);
		}

		public bool FileWriterNeedsClosing { get { return m_binaryWriter != null; } }

		public SocketLogBinaryDataStream(NetworkStream netStream, string logWriteFilename)
		{
			m_binaryReader = new BinaryReader(netStream);
			m_binaryWriter = null;

			if (logWriteFilename != "")
			{
				m_binaryWriter = new BinaryWriter(File.Open(logWriteFilename, FileMode.Create, FileAccess.Write, FileShare.Read));
			}
		}

		public void CloseNetStream()
		{
			if (m_binaryReader.BaseStream != null)
			{
				m_binaryReader.BaseStream.Close();
			}
		}

		public override byte[] ReadBytes(int nBytes)
		{
			byte[] bytes = base.ReadBytes(nBytes);

			if (m_binaryWriter != null)
			{
				m_binaryWriter.Write(bytes);
			}

			return bytes;
		}

		public override byte ReadByte()
		{
			byte b = m_binaryReader.ReadByte();

			if (m_binaryWriter != null)
			{
				m_binaryWriter.Write(b);
			}

			return b;
		}

		public override bool ReadBool()
		{
			bool b = m_binaryReader.ReadBoolean();

			if (m_binaryWriter != null)
			{
				m_binaryWriter.Write(b);
			}

			return b;
		}

		public override short ReadInt16()
		{
			Int16 i = m_binaryReader.ReadInt16();

			if (m_binaryWriter != null)
			{
				m_binaryWriter.Write(i);
			}

			unsafe
			{
				if (m_bSwapEndian)
				{
					EndianBitConverter.SwapEndian2((ushort*)&i);
				}
			}
			return i;
		}

		public override ushort ReadUInt16()
		{
			UInt16 i = m_binaryReader.ReadUInt16();

			if (m_binaryWriter != null)
			{
				m_binaryWriter.Write(i);
			}

			unsafe
			{
				if (m_bSwapEndian)
				{
					EndianBitConverter.SwapEndian2(&i);
				}
			}
			return i;
		}

		public override int ReadInt32()
		{
			int i = m_binaryReader.ReadInt32();

			if (m_binaryWriter != null)
			{
				m_binaryWriter.Write(i);
			}

			unsafe
			{
				if (m_bSwapEndian)
				{
					EndianBitConverter.SwapEndian4((uint*)&i);
				}
			}

			return i;
		}

		public override uint ReadUInt32()
		{
			uint i = m_binaryReader.ReadUInt32();

			if (m_binaryWriter != null)
			{
				m_binaryWriter.Write(i);
			}

			unsafe
			{
				if (m_bSwapEndian)
				{
					EndianBitConverter.SwapEndian4(&i);
				}
			}

			return i;
		}

		public override long ReadInt64()
		{
			long l = m_binaryReader.ReadInt64();

			if (m_binaryWriter != null)
			{
				m_binaryWriter.Write(l);
			}

			unsafe
			{
				if (m_bSwapEndian)
				{
					EndianBitConverter.SwapEndian8((ulong*)&l);
				}
			}
			return l;
		}

		public override ulong ReadUInt64()
		{
			ulong l = m_binaryReader.ReadUInt64();

			if (m_binaryWriter != null)
			{
				m_binaryWriter.Write(l);
			}

			unsafe
			{
				if (m_bSwapEndian)
				{
					EndianBitConverter.SwapEndian8(&l);
				}
			}
			return l;
		}

		public override float ReadFloat()
		{
			UInt32 p = m_binaryReader.ReadUInt32();

			if (m_binaryWriter != null)
			{
				m_binaryWriter.Write(p);
			}

			unsafe
			{
				if (m_bSwapEndian)
				{
					EndianBitConverter.SwapEndian4((uint*)&p);
				}

				return *(float*)&p;
			}
		}

		public override double ReadDouble()
		{
			UInt64 p = m_binaryReader.ReadUInt64();

			if (m_binaryWriter != null)
			{
				m_binaryWriter.Write(p);
			}

			unsafe
			{
				if (m_bSwapEndian)
				{
					EndianBitConverter.SwapEndian8(&p);
				}

				return *(double*)&p;
			}
		}

		public override bool IsEndOfStream
		{
			get
			{
				// if the socket closes, the Read operation will thrown an exception
				return false;
			}
		}

		public override void FlushWriteStream()
		{
			if (m_binaryWriter != null)
			{
				m_binaryWriter.Flush();
			}
		}

		public override void CloseWriteStream()
		{
			if (m_binaryWriter != null)
			{
				m_binaryWriter.Close();
			}
		}
	}
}

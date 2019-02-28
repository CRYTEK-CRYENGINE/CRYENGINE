// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;

namespace Statoscope
{
  interface IBinaryLogDataStream : IDisposable
  {
    int Peek();
    byte ReadByte();
    byte[] ReadBytes(int nBytes);
		bool ReadBool();
    Int64 ReadInt64();
    UInt64 ReadUInt64();
    int ReadInt32();
    uint ReadUInt32();
    Int16 ReadInt16();
    UInt16 ReadUInt16();
    float ReadFloat();
    double ReadDouble();
    string ReadString();
    bool IsEndOfStream { get; }
    EEndian Endianness { get; }
    void SetEndian(EEndian endian);
		// used for when writing out the stream that's being processed (e.g. when network logging)
		void FlushWriteStream();
		void CloseWriteStream();
  }
}

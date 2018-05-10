// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.IO;

namespace Statoscope
{
  class FileLogBinaryDataStream : BinaryLogDataStream
  {
    public FileLogBinaryDataStream(string fileName)
    {
      m_binaryReader = new BinaryReader(File.Open(fileName, FileMode.Open, FileAccess.Read, FileShare.ReadWrite));
    }
  }
}

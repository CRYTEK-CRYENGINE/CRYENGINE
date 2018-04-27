// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace Statoscope
{
	class FileLogDataStream : ILogDataStream
	{
    int m_lineNumber; // For debugging
		StreamReader m_streamReader;

		public FileLogDataStream(string fileName)
		{
			m_streamReader = new StreamReader(fileName);
      m_lineNumber = 0;
		}

		public void Dispose()
		{
			m_streamReader.Dispose();
			GC.SuppressFinalize(this);
		}

		public int Peek()
		{
			return m_streamReader.Peek();
		}

		public string ReadLine()
		{
      string line = m_streamReader.ReadLine();
      if (line != null)
      {
        ++m_lineNumber;
      }
			return line;
		}

		public bool IsEndOfStream
		{
			get
			{
				return m_streamReader.EndOfStream;
			}
		}
	}
}

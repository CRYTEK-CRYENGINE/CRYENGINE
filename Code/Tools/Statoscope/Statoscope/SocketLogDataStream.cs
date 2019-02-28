// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Net.Sockets;

namespace Statoscope
{
	class SocketLogDataStream : ILogDataStream
	{
		NetworkStream m_netStream;
		Queue<string> m_lines = new Queue<string>();
		string m_partLine = "";
		Byte[] m_buffer = new Byte[16 * 1024];

		public SocketLogDataStream(NetworkStream netStream)
		{
			m_netStream = netStream;
		}

		public void Dispose()
		{
		}

		public int Peek()
		{
			while (m_lines.Count == 0)
			{
				GetMoreLines();
			}

			string nextLine = m_lines.Peek();

            //System.Console.WriteLine(nextLine);

			return nextLine != "" ? nextLine[0] : 0;
		}

		public string ReadLine()
		{
			while (m_lines.Count == 0)
			{
				GetMoreLines();
			}

			return m_lines.Dequeue();
		}

		public bool IsEndOfStream
		{
			get
			{
				if (m_lines.Count == 0)
				{
					GetMoreLines();
				}

				return (m_lines.Count == 0);
			}
		}

		void GetMoreLines()
    {
      while (m_lines.Count == 0)
      {
        while (!m_netStream.DataAvailable)
        {
        }

        //we may need a few read attempts to get a whole line (eg for screen shots)
        int numBytesRead = m_netStream.Read(m_buffer, 0, m_buffer.Length);

        if (numBytesRead <= 0)
        {
          throw new Exception("NetworkStream Read failed");
        }

        string str = System.Text.Encoding.ASCII.GetString(m_buffer, 0, numBytesRead);
        string[] lines = str.Split(new char[] { '\n' });
        int numLines = lines.Length;
        bool strEndsWithNewLine = str.EndsWith("\n");

        if (strEndsWithNewLine)
        {
          // Split will have put an empty string on the end of lines
          numLines--;
        }

        m_partLine += lines[0];

        if (numLines > 1 || strEndsWithNewLine)
        {
          // m_partLine must now be complete
          m_lines.Enqueue(m_partLine);
          m_partLine = "";
        }

        if (numLines > 1 && !strEndsWithNewLine)
        {
          // the last line is incomplete so keep it in m_partLine for later completion
          // note: the previous m_partLine must have already been added to m_lines above
          numLines--;
          m_partLine = lines[numLines];
        }

        // don't take the first one as this will have been taken already to complete the last part line
        for (int i = 1; i < numLines; i++)
        {
          m_lines.Enqueue(lines[i]);
        }

        //if (m_lines.Count == 0)
        //{
        //System.Console.WriteLine("Waiting for lines");
        //}
      }

      if (m_lines.Count == 0)
      {
        System.Console.WriteLine("No more lines");
      }
    }
	}
}

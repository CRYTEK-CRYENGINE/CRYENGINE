// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

namespace Statoscope
{
  class FRElementBarsDrawBuffer : IDisposable
  {
    private readonly int m_nVertsPerBuffer;

    private List<BarVertex[]> m_bufferMaps = new List<BarVertex[]>();
    private List<int> m_vertsInBuffer = new List<int>();

    private List<GLBuffer> m_buffers = new List<GLBuffer>();

    private int m_end = 0;

    public FRElementBarsDrawBuffer(int nVertsPerBuffer)
    {
      m_nVertsPerBuffer = nVertsPerBuffer;
    }

    public void Begin()
    {
      for (int i = 0, c = m_vertsInBuffer.Count; i != c; ++i)
        m_vertsInBuffer[i] = 0;

      // Reset counter
      m_end = 0;
    }

    public int End()
    {
      // Finalise the last buffer
      int nLastBuffer = m_end / m_nVertsPerBuffer;
      if (nLastBuffer < m_vertsInBuffer.Count)
        m_vertsInBuffer[nLastBuffer] = m_end - (nLastBuffer * m_nVertsPerBuffer);

      // Add GL buffers as necessary
      while (m_buffers.Count <= m_bufferMaps.Count)
        m_buffers.Add(new GLBuffer(GLBuffer.GL_ARRAY_BUFFER_ARB, GLBuffer.GL_STREAM_DRAW_ARB, System.Runtime.InteropServices.Marshal.SizeOf(typeof(BarVertex)) * m_nVertsPerBuffer));

      // Push all verts to gl buffers
      unsafe
      {
        for (int i = 0, c = m_bufferMaps.Count; i != c; ++i)
        {
          if (m_vertsInBuffer[i] != 0)
          {
            m_buffers[i].Discard();
            fixed (BarVertex* bv = m_bufferMaps[i])
            {
              m_buffers[i].Update(new IntPtr(bv), (uint)m_vertsInBuffer[i] * 16, 0);
            }
          }
        }
      }

      return nLastBuffer + 1;
    }

    public KeyValuePair<GLBuffer, int> GetBuffer(int n)
    {
      if (n < m_buffers.Count && n < m_vertsInBuffer.Count)
        return new KeyValuePair<GLBuffer,int>(m_buffers[n], m_vertsInBuffer[n]);
      return new KeyValuePair<GLBuffer,int>(null, 0);
    }

    public KeyValuePair<BarVertex[], int> BeginAllocate(int nVerts)
    {
      if (nVerts <= m_nVertsPerBuffer)
      {
        int nBuffer = m_end / m_nVertsPerBuffer;

        {
          int nBufferBase = nBuffer * m_nVertsPerBuffer;
          int nBufferRemain = m_nVertsPerBuffer - (m_end - nBufferBase);

          if (nBufferRemain < nVerts)
          {
            // Not enough room in this buffer. Finalise it and allocate from the next
            ++nBuffer;

            m_end = nBuffer * m_nVertsPerBuffer;
          }
        }

        while (m_bufferMaps.Count <= nBuffer)
        {
          m_bufferMaps.Add(new BarVertex[m_nVertsPerBuffer]);
          m_vertsInBuffer.Add(0);
        }

        int nStart = m_end;
        m_end += nVerts;

        return new KeyValuePair<BarVertex[], int>(m_bufferMaps[nBuffer], nStart - (nBuffer * m_nVertsPerBuffer));
      }

      return new KeyValuePair<BarVertex[], int>(null, 0);
    }

    public void EndAllocate(int nVertsReserved, int nVertsUsed)
    {
      int nStart = m_end - nVertsReserved;
      int nEnd = nStart + nVertsUsed;
      int nBuffer = nStart / m_nVertsPerBuffer;

      m_vertsInBuffer[nBuffer] = nEnd - (nBuffer * m_nVertsPerBuffer);
      m_end = nEnd;
    }

    public void Dispose()
    {
      foreach (GLBuffer b in m_buffers)
        b.Dispose();
      m_buffers.Clear();
      m_bufferMaps.Clear();
    }
  }
}

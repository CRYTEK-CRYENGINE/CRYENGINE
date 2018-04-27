// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Statoscope
{
  class GLBuffer : IDisposable
  {
    public const uint GL_ARRAY_BUFFER_ARB = 0x8892;
    public const uint GL_ELEMENT_ARRAY_BUFFER_ARB = 0x8893;
    public const uint GL_ARRAY_BUFFER_BINDING_ARB = 0x8894;
    public const uint GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB = 0x8895;
    public const uint GL_VERTEX_ARRAY_BUFFER_BINDING_ARB = 0x8896;
    public const uint GL_NORMAL_ARRAY_BUFFER_BINDING_ARB = 0x8897;
    public const uint GL_COLOR_ARRAY_BUFFER_BINDING_ARB = 0x8898;
    public const uint GL_INDEX_ARRAY_BUFFER_BINDING_ARB = 0x8899;
    public const uint GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB = 0x889A;
    public const uint GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB = 0x889B;
    public const uint GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB = 0x889C;
    public const uint GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING_ARB = 0x889D;
    public const uint GL_WEIGHT_ARRAY_BUFFER_BINDING_ARB = 0x889E;
    public const uint GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB = 0x889F;
    public const uint GL_STREAM_DRAW_ARB = 0x88E0;
    public const uint GL_STREAM_READ_ARB = 0x88E1;
    public const uint GL_STREAM_COPY_ARB = 0x88E2;
    public const uint GL_STATIC_DRAW_ARB = 0x88E4;
    public const uint GL_STATIC_READ_ARB = 0x88E5;
    public const uint GL_STATIC_COPY_ARB = 0x88E6;
    public const uint GL_DYNAMIC_DRAW_ARB = 0x88E8;
    public const uint GL_DYNAMIC_READ_ARB = 0x88E9;
    public const uint GL_DYNAMIC_COPY_ARB = 0x88EA;
    public const uint GL_READ_ONLY_ARB = 0x88B8;
    public const uint GL_WRITE_ONLY_ARB = 0x88B9;
    public const uint GL_READ_WRITE_ARB = 0x88BA;
    public const uint GL_BUFFER_SIZE_ARB = 0x8764;
    public const uint GL_BUFFER_USAGE_ARB = 0x8765;
    public const uint GL_BUFFER_ACCESS_ARB = 0x88BB;
    public const uint GL_BUFFER_MAPPED_ARB = 0x88BC;
    public const uint GL_BUFFER_MAP_POINTER_ARB = 0x88BD;

    private unsafe delegate int glGenBuffersARBHandler(uint n, uint* buffers);
    private unsafe delegate int glBufferDataARBHandler(uint target, int size, void* data, uint usage);
    private unsafe delegate void glDeleteBuffersARBHandler(uint n, uint* buffers);
    private unsafe delegate void glBindBufferARBHandler(uint target, uint buffer);
    private unsafe delegate void* glMapBufferARBHandler(uint target, uint access);
    private unsafe delegate int glUnmapBufferARBHandler(uint target);
    private unsafe delegate void glBufferSubDataARBHandler(uint target, uint offset, uint size, void* data);

    private static glGenBuffersARBHandler glGenBuffersARB;
    private static glBufferDataARBHandler glBufferDataARB;
    private static glDeleteBuffersARBHandler glDeleteBuffersARB;
    private static glBindBufferARBHandler glBindBufferARB;
    private static glMapBufferARBHandler glMapBufferARB;
    private static glUnmapBufferARBHandler glUnmapBufferARB;
    private static glBufferSubDataARBHandler glBufferSubDataARB;

    private uint m_buffer;
    private uint m_target;
    private uint m_usage;
    private int m_size;

    public GLBuffer(uint target, uint usage, int size)
    {
      InitFunctions();

      uint buffer;
      unsafe
      {
        glGenBuffersARB(1, &buffer);
        glBufferDataARB(target, size, IntPtr.Zero.ToPointer(), usage);
      }
      m_buffer = buffer;
      m_target = target;
      m_usage = usage;
      m_size = size;
    }

    public void Discard()
    {
      Bind();

      unsafe
      {
        glBufferDataARB(m_target, m_size, IntPtr.Zero.ToPointer(), m_usage);
      }
    }

    public void Bind()
    {
      glBindBufferARB(m_target, m_buffer);
    }

    public void Update(IntPtr src, uint size, uint dst)
    {
      Bind();

      unsafe
      {
        glBufferSubDataARB(m_target, dst, size, src.ToPointer());
      }

      //System.Diagnostics.Debugger.Log(0, "", string.Format("[of] {0}\n", size));
    }

    public IntPtr Map(uint access, bool discard)
    {
      Bind();

      unsafe
      {
        if (discard)
          glBufferDataARB(m_target, m_size, IntPtr.Zero.ToPointer(), m_usage);

        return new IntPtr(glMapBufferARB(m_target, access));
      }
    }

    public void Unmap()
    {
      Bind();

      glUnmapBufferARB(m_target);
    }

    private static void InitFunctions()
    {
      if (glGenBuffersARB == null)
        glGenBuffersARB = (glGenBuffersARBHandler)WGL.GetProc<glGenBuffersARBHandler>("glGenBuffersARB");
      if (glBufferDataARB == null)
        glBufferDataARB = (glBufferDataARBHandler)WGL.GetProc<glBufferDataARBHandler>("glBufferDataARB");
      if (glDeleteBuffersARB == null)
        glDeleteBuffersARB = (glDeleteBuffersARBHandler)WGL.GetProc<glDeleteBuffersARBHandler>("glDeleteBuffersARB");
      if (glBindBufferARB == null)
        glBindBufferARB = (glBindBufferARBHandler)WGL.GetProc<glBindBufferARBHandler>("glBindBufferARB");
      if (glMapBufferARB == null)
        glMapBufferARB = (glMapBufferARBHandler)WGL.GetProc<glMapBufferARBHandler>("glMapBufferARB");
      if (glUnmapBufferARB == null)
        glUnmapBufferARB = (glUnmapBufferARBHandler)WGL.GetProc<glUnmapBufferARBHandler>("glUnmapBufferARB");
      if (glBufferSubDataARB == null)
        glBufferSubDataARB = (glBufferSubDataARBHandler)WGL.GetProc<glBufferSubDataARBHandler>("glBufferSubDataARB");
    }

    public void Dispose()
    {
      Dispose(false);
    }

    protected void Dispose(bool bFromFinalize)
    {
      if (m_buffer > 0)
      {
        uint buffer = m_buffer;
        unsafe
        {
          glDeleteBuffersARB(1, &buffer);
        }

        m_buffer = 0;
      }

      if (!bFromFinalize)
        GC.SuppressFinalize(this);
    }

    ~GLBuffer()
    {
      //Dispose(true);
    }
  }
}

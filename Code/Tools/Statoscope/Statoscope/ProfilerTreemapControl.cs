// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Drawing;
using System.IO;
using System.Threading;
using System.Xml;

namespace Statoscope
{
  class ProfilerTreemapControl : TreemapControl
  {
    private FrameRecord m_frameRecord;
    private LogControl m_logControl;
		private List<LogView> m_logViews;
    private bool m_remakeTree = true;
    private string m_thumbSearchDirectory = string.Empty;
    private P4FileFetcher m_fileFetcher = new P4FileFetcher();

    private Dictionary<string, ThumbRequest> m_textures = new Dictionary<string, ThumbRequest>();

    public FrameRecord FrameRecord
    {
      get { return m_frameRecord; }
      set { m_frameRecord = value; }
    }

    public string ThumbSearchDirectory
    {
      get { return m_thumbSearchDirectory; }
      set { m_thumbSearchDirectory = value; }
    }

    public ProfilerTreemapControl(LogControl logControl)
    {
      m_logControl = logControl;
			m_logViews = logControl.m_logViews;

      XmlDocument doc = new XmlDocument();

      try
      {
        doc.Load("TextureP4Config.xml");

        XmlNodeList connNodes = doc.GetElementsByTagName("connection");
        foreach (XmlNode n in connNodes)
        {
          m_fileFetcher.Username = n.Attributes["username"].Value;
          m_fileFetcher.Workspace = n.Attributes["workspace"].Value;
          m_fileFetcher.Server = n.Attributes["server"].Value;
          m_fileFetcher.Port = int.Parse(n.Attributes["port"].Value);
        }

        XmlNodeList gameNodes = doc.GetElementsByTagName("game");
        foreach (XmlNode n in gameNodes)
        {
          m_thumbSearchDirectory = n.Attributes["root"].Value;
        }
      }
      catch (System.Exception)
      {
      }

      m_thumbSearchDirectory = m_thumbSearchDirectory.TrimEnd('/');
    }

    public void InvalidateTree()
    {
      m_remakeTree = true;
      Invalidate();
    }

    protected override string FormatTooltip(TreemapNode node)
    {
      RDIElementValue<ProfilerRDI> prdiEv = node.Tag as RDIElementValue<ProfilerRDI>;
      if (prdiEv != null)
      {
        float val = prdiEv.m_value;
        string vals;
        if (val > 1.0f)
          vals = string.Format("{0:F1}MB", val);
        else if (val > 1.0f/1024.0f)
          vals = string.Format("{0:F0}KB", val * 1024.0f);
        else
          vals = string.Format("{0:F0}B", val * 1024.0f * 1024.0f);

        return string.Format("{0}\n{1}", prdiEv.m_rdi.Path, vals);
      }

      return string.Empty;
    }

    protected override void glDraw()
    {
      while (true)
      {
        KeyValuePair<Image, GLTexture> req;

        lock (m_thumbnailComplete)
        {
          if (m_thumbnailComplete.Count == 0)
            break;
          req = m_thumbnailComplete[0];
          m_thumbnailComplete.RemoveAt(0);
        }

        if (req.Key != null)
        {
          req.Value.Update((Bitmap)req.Key);
          req.Key.Dispose();
        }
      }

      if (m_logViews.Count > 0)
      {
        LogView lv = m_logViews[0];
        LogData ld = lv.m_logData;

        if (m_remakeTree)
        {
          VirtualBounds = new RectD(0, 0, 1, 1);
          View = new RectD(0, 0, 1, 1);

          TreemapNode rootNode = CreateTree();

          FrameRecord fr = m_frameRecord;

          if (fr != null)
          {
            lock (ld)
            {
              List<RDIElementValue<ProfilerRDI>> children = new List<RDIElementValue<ProfilerRDI>>();

              float totalSize = 0;

              foreach (RDIElementValue<ProfilerRDI> prdiEv in m_logControl.m_prdiTree.GetValueEnumerator(fr))
              {
                float size = prdiEv.m_value;

                if (size == 0.0f)
                {
                  continue;
                }

                children.Add(prdiEv);
                totalSize += size;
              }

              rootNode.Name = String.Format("{0} MB", totalSize);

              children.Sort((a, b) => -a.m_value.CompareTo(b.m_value));

              foreach (RDIElementValue<ProfilerRDI> ev in children)
              {
                float size = ev.m_value;
                string path = ev.m_rdi.Path;
                int sep = path.LastIndexOf('/');

                GLTexture tex = null;

                if (path.StartsWith("/TexStrm/"))
                {
                  string texturepath = path.Substring("/TexStrm/".Length).Replace(".dds", ".tif");
                  tex = RequestThumbnail(texturepath, -size);
                }

                TreemapNode child = CreateChild(rootNode);
                child.Size = size / totalSize;
                child.Name = (sep >= 0) ? path.Substring(sep + 1) : path;
                child.Tag = ev;
                child.Texture = tex;
              }
            }
          }

          VirtualBounds = new RectD(0, 0, 1, 1);
          View = new RectD(0, 0, 1, 1);

          m_remakeTree = false;
        }
      }
      base.glDraw();
    }

    protected override void Dispose(bool disposing)
    {
      m_thumbnailDisposing = true;

      MakeCurrent();

      lock (m_thumbnailRequests)
      {
        m_thumbnailRequests.Clear();
      }

      if (m_thumbnailLoader != null)
        m_thumbnailLoader.Join();

      foreach (KeyValuePair<Image, GLTexture> kv in m_thumbnailComplete)
      {
        if (kv.Key != null)
          kv.Key.Dispose();
      }
      m_thumbnailComplete.Clear();

      foreach (KeyValuePair<string, ThumbRequest> kv in m_textures)
        kv.Value.Texture.Dispose();
      m_textures.Clear();

      m_thumbnailDisposing = false;

      base.Dispose(disposing);
    }

    private GLTexture RequestThumbnail(string filename, float priority)
    {
      ThumbRequest req;

      lock (m_thumbnailRequests)
      {
        if (m_textures.TryGetValue(filename, out req))
        {
          req.Priority = priority;
        }
        else
        {
          req = new ThumbRequest();

          if (m_thumbSearchDirectory.EndsWith("/"))
            req.Path = m_thumbSearchDirectory + filename;
          else
            req.Path = m_thumbSearchDirectory + "/" + filename;

          req.Priority = priority;
          req.Texture = new GLTexture();
          m_textures.Add(filename, req);

          bool startThread = m_thumbnailRequests.Count == 0;
          m_thumbnailRequests.Add(req);

          if (startThread)
          {
            m_thumbnailLoader = new Thread(new ThreadStart(ThumbnailLoaderEntry));
            m_thumbnailLoader.Start();
          }
        }
      }

      return req.Texture;
    }

    private void ThumbnailLoaderEntry()
    {
      while (true)
      {
        ThumbRequest request;

        lock (m_thumbnailRequests)
        {
          if (m_thumbnailRequests.Count == 0)
            break;

          m_thumbnailRequests.Sort((a, b) => (a.Priority.CompareTo(b.Priority)));

          request = m_thumbnailRequests[0];
          m_thumbnailRequests.RemoveAt(0);
        }

        Bitmap thumb = null;

        if (m_fileFetcher.IsConfigured)
        {
          using (MemoryStream stream = new MemoryStream())
          {
            try
            {
              if (m_fileFetcher.FetchFile(request.Path, stream))
              {
                stream.Seek(0, SeekOrigin.Begin);

                using (Bitmap fullbm = (Bitmap)Image.FromStream(stream))
                {
                  thumb = new Bitmap(fullbm, new Size(128, 128));
                }
              }
            }
            catch (System.IO.IOException)
            {
            }
            catch (System.OutOfMemoryException)
            {
            }
            catch (System.ArgumentException)
            {
            }
          }
        }

        lock (m_thumbnailComplete)
        {
          m_thumbnailComplete.Add(new KeyValuePair<Image, GLTexture>(thumb, request.Texture));
          if (!m_thumbnailDisposing)
            BeginInvoke((Action)(() => Invalidate()));
        }
      }
    }

    private class ThumbRequest
    {
      public float Priority;
      public string Path;
      public GLTexture Texture;
    }

    private Thread m_thumbnailLoader;
    private volatile bool m_thumbnailDisposing;
    private List<ThumbRequest> m_thumbnailRequests = new List<ThumbRequest>();
    private List<KeyValuePair<Image, GLTexture>> m_thumbnailComplete = new List<KeyValuePair<Image, GLTexture>>();
  }
}

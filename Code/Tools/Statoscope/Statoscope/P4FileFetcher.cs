// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace Statoscope
{
  class P4FileFetcher
  {
    private string m_username = string.Empty;
    private string m_workspace = string.Empty;
    private string m_server = string.Empty;
    private int m_port = 0;

    private DirTree m_root = new DirTree("//");
    private TextWriter m_p4Log = null;

    private class DirTree
    {
      public string P4Path;
      public Dictionary<string, DirTree> Dirs;
			public Dictionary<string, string> Files;

      public DirTree(string p4Path)
      {
        P4Path = p4Path;
      }
    }

    public string Username
    {
      get { return m_username; }
      set { m_username = value; }
    }

    public string Workspace
    {
      get { return m_workspace; }
      set { m_workspace = value; }
    }

    public string Server
    {
      get { return m_server; }
      set { m_server = value; }
    }

    public int Port
    {
      get { return m_port; }
      set { m_port = value; }
    }

    public bool IsConfigured
    {
      get
      {
        return
          !string.IsNullOrEmpty(m_username) &&
          !string.IsNullOrEmpty(m_workspace) &&
          !string.IsNullOrEmpty(m_server) &&
          m_port != 0;
      }
    }

    public P4FileFetcher()
    {
      try
      {
        m_p4Log = new StreamWriter(File.OpenWrite("p4log.log"));
      }
      catch (System.Exception)
      {
      }
    }

    public bool FetchFile(string p4Path, Stream outStream)
    {
      p4Path = p4Path.Replace('\\', '/');

      if (p4Path.StartsWith("//"))
      {
        string path = p4Path.Substring(2);

        string[] parts = path.Split('/');

        DirTree tree = m_root;
        for (int parti = 0, partc = parts.Length - 1; parti < partc; ++parti)
        {
          if (tree.Dirs == null)
          {
            tree.Dirs = new Dictionary<string, DirTree>();
            foreach (string sub in GetSubDirs(tree.P4Path))
              tree.Dirs.Add(sub.ToLower(), new DirTree(tree.P4Path + sub + "/"));
          }

          string partlwr = parts[parti].ToLower();

          DirTree childTree;
          if (tree.Dirs.TryGetValue(partlwr, out childTree))
          {
            tree = childTree;
          }
          else
          {
            // File not found
            return false;
          }
        }

				if (tree.Files == null)
				{
					tree.Files = new Dictionary<string, string>();
					foreach (string file in GetFiles(tree.P4Path))
						tree.Files.Add(file.ToLower(), tree.P4Path + file);
				}

        // Last part is the file - todo, remap this too
				string p4RemapPath;
				if (tree.Files.TryGetValue(parts[parts.Length - 1], out p4RemapPath))
				{
					try
					{
						using (Process process = new Process())
						{
							process.StartInfo.FileName = "p4";
							process.StartInfo.Arguments = string.Format("-c {0} -u {1} -p {2}:{3} print -q \"{4}\"", m_workspace, m_username, m_server, m_port, p4RemapPath);
							process.StartInfo.UseShellExecute = false;
							process.StartInfo.CreateNoWindow = true;
							process.StartInfo.RedirectStandardOutput = true;
							Log("p4 {0}", process.StartInfo.Arguments);
							if (process.Start())
							{
								byte[] buf = new byte[4096];
								while (true)
								{
									int numRead = process.StandardOutput.BaseStream.Read(buf, 0, 4096);
									if (numRead == 0)
										break;
									outStream.Write(buf, 0, numRead);
								}

								process.WaitForExit();
								Log("Exit: {0}", process.ExitCode);
								return process.ExitCode == 0;
							}
							else
							{
								Log("Failed.");
							}
						}
					}
					catch (System.Exception ex)
					{
						Log("Exception: {0}", ex.Message);
					}
				}
      }
      return false;
    }

    private void Log(string format, params object[] args)
    {
      if (m_p4Log != null)
      {
        m_p4Log.WriteLine(format, args);
				m_p4Log.Flush();
      }
    }

    private List<string> GetFiles(string p4Path)
    {
      List<string> subdirs = new List<string>();

      try
      {
        using (Process process = new Process())
        {
          process.StartInfo.FileName = "p4";
          process.StartInfo.Arguments = string.Format("-c {0} -u {1} -p {2}:{3} files \"{4}*\"", m_workspace, m_username, m_server, m_port, p4Path);
          process.StartInfo.RedirectStandardOutput = true;
          process.StartInfo.UseShellExecute = false;
          process.StartInfo.CreateNoWindow = true;

          Log("p4 {0}", process.StartInfo.Arguments);

          if (process.Start())
          {
            while (true)
            {
              string line = process.StandardOutput.ReadLine();
              if (line == null)
                break;

              Log("{0}", line);

              if (line.StartsWith(p4Path))
              {
								int revSep = line.IndexOf('#');
                string subdir = line.Substring(p4Path.Length, revSep - p4Path.Length);
                subdirs.Add(subdir);
              }
            }
            process.WaitForExit();
            Log("Exit: {0}", process.ExitCode);
          }
          else
          {
            Log("Failed");
          }
        }
      }
      catch (System.Exception ex)
      {
        Log("Exception: {0}", ex.Message);
      }

      return subdirs;
    }

    private List<string> GetSubDirs(string p4Path)
    {
      List<string> subdirs = new List<string>();

      try
      {
        using (Process process = new Process())
        {
          process.StartInfo.FileName = "p4";
          process.StartInfo.Arguments = string.Format("-c {0} -u {1} -p {2}:{3} dirs -C \"{4}*\"", m_workspace, m_username, m_server, m_port, p4Path);
          process.StartInfo.RedirectStandardOutput = true;
          process.StartInfo.UseShellExecute = false;
          process.StartInfo.CreateNoWindow = true;

          Log("p4 {0}", process.StartInfo.Arguments);

          if (process.Start())
          {
            while (true)
            {
              string line = process.StandardOutput.ReadLine();
              if (line == null)
                break;

              Log("{0}", line);

              if (line.StartsWith(p4Path))
              {
                string subdir = line.Substring(p4Path.Length);
                subdirs.Add(subdir);
              }
            }
            process.WaitForExit();
            Log("Exit: {0}", process.ExitCode);
          }
          else
          {
            Log("Failed");
          }
        }
      }
      catch (System.Exception ex)
      {
        Log("Exception: {0}", ex.Message);
      }

      return subdirs;
    }
  }
}

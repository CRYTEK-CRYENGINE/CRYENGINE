using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Xml;

namespace ProfVis
{
    public partial class ThreadAnalyzer : Form
    {
        private class FileEntry
        {
            public string filePath;
            public List<FileEntry> similarFiles;
            public CFunction graph;
            public CThreadTimeline threadSortedTimeline;

            public FileEntry(string path, CFunction funcGraph, CThreadTimeline sortedTimeline)
            {
                filePath = path;
                similarFiles = new List<FileEntry>();
                graph = funcGraph;
                threadSortedTimeline = sortedTimeline;
            }
        };

        private Dictionary<string, FileEntry> m_filesDB;
        private BackgroundWorker m_scanWorker;
        private BackgroundWorker m_sessionWorker;
        private Form1 m_mainForm;
        private bool m_includeFilter;

        public ThreadAnalyzer(Form1 mainForm)
        {
            InitializeComponent();

            m_mainForm = mainForm;
            m_includeFilter = true;
            m_filesDB = new Dictionary<string, FileEntry>();
            m_scanWorker = new BackgroundWorker();
            m_scanWorker.WorkerReportsProgress = true;

            m_scanWorker.DoWork += (object progressSender, DoWorkEventArgs doWorkArgs) =>
            {
                string rootFolder = doWorkArgs.Argument as string;

                if (rootFolder == null)
                {
                    Dictionary<string, FileEntry> newFilesDB = new Dictionary<string, FileEntry>();
                    int filesDone = 0;
                    int filesCount = GetAllFilesCount(m_filesDB);

                    List<string> callGraph = region.Tag != null ? region.Tag as List<string> : new List<string>();
                    int endDepth = Convert.ToInt32(stopDepth.Value);
                    int endBreath = Convert.ToInt32(stopBreath.Value);
                    FileEntry mainFile = null;
                    CThreadAnalyzedData result = null;
                    foreach (var fileEntry in m_filesDB)
                    {
                        foreach (var fileSubEntry in fileEntry.Value.similarFiles)
                        {
                            result = Form1.GetSortedCalltreeForThread(fileSubEntry.threadSortedTimeline, endDepth, endBreath, callGraph, m_includeFilter);

                            if (newFilesDB.TryGetValue(result.callgraphAsString, out mainFile))
                            {
                                mainFile.similarFiles.Add(new FileEntry(fileSubEntry.filePath, result.callgraph, result.threadTimelineSorted));
                            }
                            else
                            {
                                newFilesDB.Add(result.callgraphAsString, new FileEntry(fileSubEntry.filePath, result.callgraph, result.threadTimelineSorted));
                            }

                            m_scanWorker.ReportProgress((int)((filesDone / (double)filesCount) * 100.0));
                            ++filesDone;
                        }

                        result = Form1.GetSortedCalltreeForThread(fileEntry.Value.threadSortedTimeline, endDepth, endBreath, callGraph, m_includeFilter);

                        if (newFilesDB.TryGetValue(result.callgraphAsString, out mainFile))
                        {
                            mainFile.similarFiles.Add(new FileEntry(fileEntry.Value.filePath, result.callgraph, result.threadTimelineSorted));
                        }
                        else
                        {
                            newFilesDB.Add(result.callgraphAsString, new FileEntry(fileEntry.Value.filePath, result.callgraph, result.threadTimelineSorted));
                        }

                        m_scanWorker.ReportProgress((int)((filesDone / (double)filesCount) * 100.0));
                        ++filesDone;
                    }

                    m_filesDB = newFilesDB;
                }
                else
                {
                    m_filesDB.Clear();

                    // Process the list of files found in the directory.
                    string[] fileEntries = Directory.GetFiles(rootFolder, "*.xml");
                    int filesDone = 0;
                    int filesCount = fileEntries.Length;

                    List<string> callGraph = region.Tag != null ? region.Tag as List<string> : new List<string>();
                    int endDepth = Convert.ToInt32(stopDepth.Value);
                    int endBreath = Convert.ToInt32(stopBreath.Value);
                    int threadIndex = Convert.ToInt32(threadIdx.Value);
                    foreach (string fileName in fileEntries)
                    {
                        CThreadAnalyzedData result = Form1.GetSortedCalltreeForThread(fileName, threadIndex, endDepth, endBreath, callGraph, m_includeFilter);
                        FileEntry mainFile = null;

                        if (!result.callgraph.name.Equals(""))
                        {
                            if (m_filesDB.TryGetValue(result.callgraphAsString, out mainFile))
                            {
                                mainFile.similarFiles.Add(new FileEntry(fileName, result.callgraph, result.threadTimelineSorted));
                            }
                            else
                            {
                                m_filesDB.Add(result.callgraphAsString, new FileEntry(fileName, result.callgraph, result.threadTimelineSorted));
                            }
                        }

                        m_scanWorker.ReportProgress((int)((filesDone / (double)filesCount) * 100.0));
                        ++filesDone;
                    }
                }
            };

            m_scanWorker.ProgressChanged += (object progressSender, ProgressChangedEventArgs progressArgs) =>
            {
                progressBar1.Value = progressArgs.ProgressPercentage;
            };

            m_scanWorker.RunWorkerCompleted += (object progressSender, RunWorkerCompletedEventArgs progressArgs) =>
            {
                progressBar1.Value = 0;
                FillListGrid();
            };

            m_sessionWorker = new BackgroundWorker();
            m_sessionWorker.WorkerReportsProgress = true;

            m_sessionWorker.DoWork += (object progressSender, DoWorkEventArgs doWorkArgs) =>
            {
                string fileNamePath = doWorkArgs.Argument as string;

                if (fileNamePath != null)
                {
                    try
                    {
                        XmlDocument xmlDoc = new XmlDocument();

                        xmlDoc.Load(fileNamePath);

                        XmlNode sessionRootElement = xmlDoc.SelectSingleNode("SessionRoot");
                        if (sessionRootElement != null)
                        {
                            XmlNode depthNode = sessionRootElement.Attributes.GetNamedItem("StopDepth");
                            if (depthNode != null)
                            {
                                int depth = Convert.ToInt32(depthNode.Value);
                                stopDepth.Invoke((MethodInvoker)delegate
                                {
                                    stopDepth.Value = depth;
                                });
                            }

                            XmlNode breathNode = sessionRootElement.Attributes.GetNamedItem("StopBreath");
                            if (breathNode != null)
                            {
                                int breath = Convert.ToInt32(breathNode.Value);
                                stopBreath.Invoke((MethodInvoker)delegate
                                {
                                    stopBreath.Value = breath;
                                });
                            }

                            XmlNode threadIdxNode = sessionRootElement.Attributes.GetNamedItem("ThreadIdx");
                            if (threadIdxNode != null)
                            {
                                int threadIndex = Convert.ToInt32(threadIdxNode.Value);
                                threadIdx.Invoke((MethodInvoker)delegate
                                {
                                    threadIdx.Value = threadIndex;
                                });
                            }

                            XmlNode directoryPathNode = sessionRootElement.Attributes.GetNamedItem("DirectoryPath");
                            if (directoryPathNode != null)
                            {
                                folderPath.Invoke((MethodInvoker)delegate
                                {
                                    folderPath.Text = directoryPathNode.Value;
                                });
                            }

                            m_filesDB.Clear();
                            int filesRead = 0;
                            int filesCount = 1;

                            XmlNode filesDbElement = sessionRootElement.SelectSingleNode("filesDB");
                            XmlNode filesCountNode = filesDbElement.Attributes.GetNamedItem("filesCount");
                            if (filesCountNode != null)
                            {
                                filesCount = Convert.ToInt32(filesCountNode.Value);
                            }

                            foreach (XmlNode fileEntryElement in filesDbElement)
                            {
                                FileEntry fileEntry = ReadFileInfoFromXML(fileEntryElement);
                                ++filesRead;
                                m_filesDB.Add(filesRead.ToString(), fileEntry);
                                m_sessionWorker.ReportProgress((int)((filesRead / (double)filesCount) * 100.0));
                            }
                        }
                    }
                    catch (System.Exception ex)
                    {
                        return;
                    }
                }
                else
                {
                    XmlDocument xmlDoc = new XmlDocument();
                    XmlElement root = xmlDoc.CreateElement("SessionRoot");
                    xmlDoc.AppendChild(root);
                    string dump = Path.GetTempFileName();
                    
                    string filename = "SessionDump_" + DateTime.Now.ToString() + ".xml";
                    filename = filename.Replace(":", "-");


                    List<string> callGraph = region.Tag != null ? region.Tag as List<string> : new List<string>();
                    int endDepth = Convert.ToInt32(stopDepth.Value);
                    int endBreath = Convert.ToInt32(stopBreath.Value);
                    int threadIndex = Convert.ToInt32(threadIdx.Value);

                    root.SetAttribute("DirectoryPath", folderPath.Text);
                    root.SetAttribute("StopDepth", endDepth.ToString());
                    root.SetAttribute("StopBreath", endBreath.ToString());
                    root.SetAttribute("ThreadIdx", threadIndex.ToString());

                    XmlElement fileEntriesElement = xmlDoc.CreateElement("filesDB");
                    int filesCount = GetAllFilesCount(m_filesDB);
                    int filesWritten = 0;
                    fileEntriesElement.SetAttribute("filesCount", filesCount.ToString());
                    root.AppendChild(fileEntriesElement);
                    foreach (var fileEntry in m_filesDB)
                    {
                        XmlElement fileInfoElement = xmlDoc.CreateElement("file");
                        fileEntriesElement.AppendChild(fileInfoElement);

                        FileEntry fileInfo = fileEntry.Value;
                        WriteFileInfoToXML(fileInfoElement, fileEntry.Value, xmlDoc);
                        ++filesWritten;
                        m_sessionWorker.ReportProgress((int)((filesWritten / (double)filesCount) * 100.0));

                        foreach (var fileSubEntry in fileEntry.Value.similarFiles)
                        {
                            fileInfoElement = xmlDoc.CreateElement("file");
                            fileEntriesElement.AppendChild(fileInfoElement);

                            WriteFileInfoToXML(fileInfoElement, fileSubEntry, xmlDoc);
                            ++filesWritten;
                            m_sessionWorker.ReportProgress((int)((filesWritten / (double)filesCount) * 100.0));
                        }
                    }

                    xmlDoc.Save(filename);
                }
            };

            m_sessionWorker.ProgressChanged += (object progressSender, ProgressChangedEventArgs progressArgs) =>
            {
                progressBar1.Value = progressArgs.ProgressPercentage;
            };

            m_sessionWorker.RunWorkerCompleted += (object progressSender, RunWorkerCompletedEventArgs progressArgs) =>
            {
                m_scanWorker.RunWorkerAsync(m_filesDB);
            };
        }

        private int GetAllFilesCount(Dictionary<string, FileEntry> files)
        {
            int result = 0;
            foreach (KeyValuePair<string, FileEntry> entry in m_filesDB)
            {
                foreach(FileEntry similarFile in entry.Value.similarFiles)
                {
                    ++result;
                }
                ++result;
            }
            return result;
        }

        private void FillListGrid()
        {
            treeView1.Nodes.Clear();
            treeView2.Nodes.Clear();

            int filesCount = GetAllFilesCount(m_filesDB);

            TreeNode root = treeView1.Nodes.Add("Different Profiler Frames " + m_filesDB.Count().ToString() + " out of " + filesCount.ToString());

            foreach (KeyValuePair<string,FileEntry> entry in m_filesDB.OrderByDescending(i => i.Value.similarFiles.Count))
            {
                int similarFilesCount = (1 + entry.Value.similarFiles.Count);
                float percentOfAllFiles = ((float)similarFilesCount /  (float)filesCount) * 100.0f;
                TreeNode newNode = new TreeNode(Path.GetFileName(entry.Value.filePath) + string.Format(" (Time {0}ms) - {1:0.0}% of all analyzed frames ({2} Files)", entry.Value.graph.timeMS, percentOfAllFiles, similarFilesCount));
                newNode.Tag = entry.Value;
                if (entry.Value.similarFiles.Count > 0)
                {
                    foreach (FileEntry similarFile in entry.Value.similarFiles)
                    {
                        TreeNode childNode = new TreeNode(Path.GetFileName(similarFile.filePath) + string.Format(" (Time {0}ms)", similarFile.graph.timeMS));
                        childNode.Tag = similarFile;
                        newNode.Nodes.Add(childNode);
                    }
                }
                root.Nodes.Add(newNode);
            }

            root.Expand();
        }

        private void PickFolder_Click(object sender, EventArgs e)
        {
            if (folderBrowserDialog1.ShowDialog() == DialogResult.OK)
            {
                folderPath.Text = folderBrowserDialog1.SelectedPath;
            }
        }

        private void OpenInNewApp_Click(object sender, EventArgs e)
        {
            if (treeView1.SelectedNode != null && treeView1.SelectedNode.Tag != null)
            {
                FileEntry fileEntry = treeView1.SelectedNode.Tag as FileEntry;
                Process newProfVis = new Process();
                newProfVis.StartInfo.FileName = Application.ExecutablePath;
                newProfVis.StartInfo.Arguments = fileEntry.filePath;
                newProfVis.Start();
            }
        }

        public static void InsertTreeNodesRecursively(TreeNode node, CFunction func)
        {
            string time = func.timeMS > 1000 ? string.Format("{0} s", func.timeMS / 1000.0) : string.Format("{0} ms", func.timeMS);
            TreeNode parent = node.Nodes.Add(func.callCount > 1 ? string.Format("{0} ({1}) ({2} calls)", func.name, time, func.callCount) :
                string.Format("{0} ({1})", func.name, time));

            parent.Tag = func;

            for (int i = 0; i < func.subFunctions.Count; ++i)
            {
                InsertTreeNodesRecursively(parent, func.subFunctions[i]);
            }

            int count = Math.Min(1, node.Nodes.Count);
            for (int i = 0; i < count; i++)
            {
                node.Nodes[i].BackColor = Color.PaleVioletRed;
            }

            count = Math.Min(2, node.Nodes.Count);
            for (int i = 1; i < count; i++)
            {
                node.Nodes[i].BackColor = Color.LightGoldenrodYellow;
            }
        }

        public void SetRegion(List<string> callPath, int depth)
        {
            stopDepth.Minimum = depth;
            stopDepth.Value = depth;
            region.Text = callPath.First();
            region.Tag = callPath;
            m_mainForm.SetPickRegionMode(false);
            pickerMode.CheckState = CheckState.Unchecked;
        }

        public void ClearRegion()
        {
            stopDepth.Minimum = 1;
        }

        private void treeView1_MouseClick(object sender, MouseEventArgs e)
        {
            TreeViewHitTestInfo info = treeView1.HitTest(e.Location);

            if (info.Location == TreeViewHitTestLocations.Label)
            {
                TreeNode node = treeView1.GetNodeAt(e.Location);

                if (node != null && node.Tag != null)
                {
                  FileEntry entry = node.Tag as FileEntry;
                  
                  if(entry.graph != null)
                  {
                      treeView2.Nodes.Clear();

                      string time = entry.graph.timeMS > 1000 ? string.Format("{0} s", entry.graph.timeMS / 1000.0) : string.Format("{0} ms", entry.graph.timeMS);
                      TreeNode root = new TreeNode(entry.graph.callCount > 1 ? string.Format("{0} ({1}) ({2} calls)", entry.graph.name, time, entry.graph.callCount) :
                          string.Format("{0} ({1})", entry.graph.name, time));
                      treeView2.Nodes.Add(root);

                      foreach (CFunction funcEntry in entry.graph.subFunctions)
                      {
                          InsertTreeNodesRecursively(root, funcEntry);
                      }

                      root.ExpandAll();
                  }

                  if (File.Exists(entry.filePath))
                  {
                      m_mainForm.OpenSession(entry.filePath);
                  }
                }
            }
        }

        private void Start_Click(object sender, EventArgs e)
        {
            if (folderPath.Text.Length > 0 && !AreWorkersBusy())
            {
                if (Directory.Exists(folderPath.Text))
                {
                    m_scanWorker.RunWorkerAsync(folderPath.Text);
                }
                else
                {
                    MessageBox.Show(folderPath.Text + " doesn't exist");
                }
            }
        }

        private void Picker_CheckedChanged(object sender, EventArgs e)
        {
            CheckBox btn = sender as CheckBox;
            if (btn != null)
            {
                bool modeOn = btn.CheckState == CheckState.Checked;
                m_mainForm.SetPickRegionMode(modeOn);
                if(modeOn)
                {
                    btn.BackColor = Color.ForestGreen;
                }
                else
                {
                    btn.BackColor = Color.LightGray;
                }
            }
        }

        private void ScanTool_FormClosed(object sender, FormClosedEventArgs e)
        {
            m_mainForm.ScanToolClosed();
        }

        private void ClearPickMode_Click(object sender, EventArgs e)
        {
            region.Text = "";
            region.Tag = new List<string>();
            m_mainForm.SetPickRegionMode(false);
            ClearRegion();
        }

        private void AnalyzeLoadedFiles_Click(object sender, EventArgs e)
        {
            if (m_filesDB.Count > 0 && !AreWorkersBusy())
            {
                m_scanWorker.RunWorkerAsync(m_filesDB);
            }
        }

        private bool AreWorkersBusy()
        {
            return m_scanWorker.IsBusy || m_sessionWorker.IsBusy;
        }

        private void inverseFilter_CheckedChanged(object sender, EventArgs e)
        {
            CheckBox btn = sender as CheckBox;
            if (btn != null)
            {
                bool modeOn = btn.CheckState == CheckState.Checked;
                m_includeFilter = !modeOn;
                if (modeOn)
                {
                    filterLabel.Text = "Filter: Exclude";
                }
                else
                {
                    filterLabel.Text = "Filter: Include only";
                }
            }
        }

        private static CFunction ReadFunctionFromXML(XmlNode functionRootNode, CFunction parent)
        {
            // Read function info
            string name = functionRootNode.Attributes.GetNamedItem("name").Value;
            int callCount = Convert.ToInt32(functionRootNode.Attributes.GetNamedItem("callCount").Value);
            float timeMS = Single.Parse(functionRootNode.Attributes.GetNamedItem("tMS").Value);
            CFunction functionInfo = new CFunction(name, timeMS, 0, 0, "", parent);
            functionInfo.callCount = callCount;
            if (parent != null)
            {
                parent.subFunctions.Add(functionInfo);
            }

            foreach (XmlNode subFunctionNode in functionRootNode)
            {
                ReadFunctionFromXML(subFunctionNode, functionInfo);
            }

            return functionInfo;
        }

        private static CThreadTimeline ReadThreadTimelineFromXML(XmlNode threadTimelineNode)
        {
            CThreadTimeline result = new CThreadTimeline(threadTimelineNode.Attributes.GetNamedItem("name").Value);

            foreach (XmlNode functionRootNode in threadTimelineNode)
            {
                result.functions.Add(ReadFunctionFromXML(functionRootNode, null));
            }

            return result;
        }

        private static FileEntry ReadFileInfoFromXML(XmlNode fileEntryNode)
        {
            string path = fileEntryNode.Attributes.GetNamedItem("path").Value;

            XmlNode threadTimelineElement = fileEntryNode.SelectSingleNode("threadTimeline");
            CThreadTimeline threadTimeline = ReadThreadTimelineFromXML(threadTimelineElement);

            FileEntry fileEntry = new FileEntry(path, null, threadTimeline);

            return fileEntry;
        }

        private static void WriteFunctionTreeToXML(XmlDocument doc, XmlElement parentElement, CFunction function)
        {
            XmlElement functionElement = doc.CreateElement("f");
            parentElement.AppendChild(functionElement);

            // Fill function info
            functionElement.SetAttribute("name", function.name);
            functionElement.SetAttribute("callCount", function.callCount.ToString());
            functionElement.SetAttribute("tMS", function.timeMS.ToString());
            //functionElement.SetAttribute("t", function.time.ToString());
            //functionElement.SetAttribute("tStart", function.timeRange.First().Item1.ToString());
            //functionElement.SetAttribute("tEnd", function.timeRange.First().Item2.ToString());

            foreach (CFunction subFunction in function.subFunctions)
            {
                WriteFunctionTreeToXML(doc, functionElement, subFunction);
            }
        }

        private static void WriteFileInfoToXML(XmlElement fileInfoElement, FileEntry fileInfo, XmlDocument xmlDoc)
        {
            fileInfoElement.SetAttribute("path", fileInfo.filePath);

            XmlElement threadTimelineElement = xmlDoc.CreateElement("threadTimeline");
            fileInfoElement.AppendChild(threadTimelineElement);

            CThreadTimeline timelineToExport = fileInfo.threadSortedTimeline;

            threadTimelineElement.SetAttribute("name", timelineToExport.name);
            foreach (CFunction timelineFunction in timelineToExport.functions)
            {
                WriteFunctionTreeToXML(xmlDoc, threadTimelineElement, timelineFunction);
            }
        }

        private void DumpSession_Click(object sender, EventArgs e)
        {
            if (!AreWorkersBusy())
            {
                m_sessionWorker.RunWorkerAsync();
            }
        }

        private void LoadSession_Click(object sender, EventArgs e)
        {
            if (!AreWorkersBusy() && openFileDialog1.ShowDialog() == DialogResult.OK)
            {
                m_sessionWorker.RunWorkerAsync(openFileDialog1.FileName);
            }
        }
    }
}

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Xml;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;


namespace ProfVis
{
    public partial class Form1 : Form
    {
        public Form1(string[] args)
        {
            m_threadsData = new List<CThreadTag>();
            m_callTree = new List<CThreadTimeline>();
            m_pickRegionMode = false;

            InitializeComponent();

            Show();

            if (args.Length >= 1)
            {
                OpenSession(args[0]);
            }
        }

        public static Color[] s_clrBack = { Color.FromArgb(235, 235, 235), Color.White };
        public static Color[] s_clrBars = {  Color.FromArgb(140, 198, 63),
                                             Color.FromArgb(247, 148, 30),
                                             Color.FromArgb(37, 170, 225),
                                             Color.FromArgb(122, 45, 24),
                                             Color.FromArgb(239, 62, 54),
                                           };


        private ToolTip m_tooltip; //TODO: kill this and use some custom panel to prevent flickering
        public List<Brush> m_BarsBrushes;
        public Font m_barFont;

        private static Random m_random;

        // session
        private string m_sessionName;
        public List<CThreadTag> m_threadsData;
        public List<CThreadTimeline> m_callTree;

        public static string m_filter = "";
        public static string m_argFilter = "";
        public static List<Tuple<long,long>> m_filterTime;
        public static float m_timeThresholdMs = 0.0f;
        public static float m_filteredTimeMSAcc = 0.0f;
        public static List<float> m_filteredTimePerThread;
        public static List<Tuple<long, long>> m_filterTimeRanges;

        private BackgroundWorker m_loadingWorker;
        private Point m_PrevMousePos;
        private CBlockData m_hoveredData;

        public static int s_bumpDepth = 0;
        public static bool s_functionHasBeenFiltered = false;
        private bool m_pickRegionMode;
        private ThreadAnalyzer m_scanTool;

        public static void InsertTreeNodesRecursively(TreeNode node, CFunction func)
        {
            string time = func.timeMS > 1000 ? string.Format("{0} s", func.timeMS / 1000.0) : string.Format("{0} ms", func.timeMS);
            TreeNode parent = node.Nodes.Add(func.callCount > 1 ? string.Format("{0} ({1}) ({2} calls)", func.name, time, func.callCount) :
                string.Format("{0} ({1})", func.name, time));

            parent.Tag = func;

            for (int i = 0; i < func.subFunctions.Count; ++i )
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

        public static bool IsTimeInFilterRange(long start, long end)
        {
            if (m_filterTime != null)
            {
                for (int i = 0; i < m_filterTime.Count; ++i)
                {
                    if (start >= m_filterTime[i].Item1 && end <= m_filterTime[i].Item2)
                    {
                        return true;
                    }
                }

                return false;
            }

            return true;
        }

        public static void SortFunctionsByTimeRecusively(CFunction func)
        {
            for (int i = 0; i < func.subFunctions.Count; ++i )
            {
                SortFunctionsByTimeRecusively(func.subFunctions[i]);
            }

            func.subFunctions.Sort(
                delegate (CFunction funcA, CFunction funcB)
                {
                    return funcA.timeMS <= funcB.timeMS ? 1 : -1;
                }
            );
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            //DoubleBuffered = true;

            dataGridView_Prof.Dock = DockStyle.Fill;

            dataGridView_Prof.AutoGenerateColumns = false;
            dataGridView_Prof.ColumnCount = 2;
            //AdjustDataGridViewSizing();
            dataGridView_Prof.Columns[0].Name = "Thread";
            dataGridView_Prof.Columns[1].Name = "Blocks";

            dataGridView_Prof.Columns[0].Frozen = true;

            dataGridView_Prof.Columns[0].AutoSizeMode = DataGridViewAutoSizeColumnMode.DisplayedCells;
            dataGridView_Prof.Columns[1].AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill;
            //dataGridView_Prof.Columns[1].AutoSizeMode = DataGridViewAutoSizeColumnMode.None;
            //dataGridView_Prof.Columns[1].Width = 500;

            dataGridView_Prof.Columns[0].SortMode = DataGridViewColumnSortMode.NotSortable;
            dataGridView_Prof.Columns[1].SortMode = DataGridViewColumnSortMode.NotSortable;

            dataGridView_Prof.ShowCellToolTips = false;

            dataGridView_Prof.MouseWheel += new MouseEventHandler(dataGridView_Prof_MouseWheel);
            dataGridView_Prof.MouseClick += new MouseEventHandler(dataGridView_Prof_MouseClick);

            //dataGridView_Prof.AllowUserToResizeRows = true;

            callGraphTreeView.NodeMouseClick += new TreeNodeMouseClickEventHandler(onTreeViewMouseClick);
            callGraphTreeView.NodeMouseHover += new TreeNodeMouseHoverEventHandler(onTreeViewNodeHover);

            m_barFont = (Font)dataGridView_Prof.DefaultCellStyle.Font.Clone();

            m_scanTool = new ThreadAnalyzer(this);
            s_bumpDepth = 0;

            m_tooltip = new ToolTip();
            m_tooltip.ShowAlways = false;
            m_tooltip.UseAnimation = false;
            m_tooltip.UseFading = false;
            m_tooltip.AutomaticDelay = 0;
            m_tooltip.AutoPopDelay = 0;

            m_BarsBrushes = new List<Brush>();
            for(int i = 0; i < Form1.s_clrBars.Length; ++i)
            {
                m_BarsBrushes.Add(new SolidBrush(s_clrBars[i]));
            }

            m_loadingWorker = new BackgroundWorker();
            m_loadingWorker.WorkerReportsProgress = true;
            m_loadingWorker.ProgressChanged += (object progressSender, ProgressChangedEventArgs progressArgs) =>
            {
                DataGridViewRow row = progressArgs.UserState as DataGridViewRow;
                if (row != null)
                    dataGridView_Prof.Rows.Add(row);
            };

            m_loadingWorker.RunWorkerCompleted += (object progressSender, RunWorkerCompletedEventArgs progressArgs) =>
            {
                progressBar1.Visible = false;
                progressBar1.Enabled = false;
                menuStrip1.Enabled = true;
                this.Text = m_sessionName;
                RefreshFilter();
            };

            m_loadingWorker.DoWork += (object progressSender, DoWorkEventArgs doWorkArgs) =>
            {
                Graphics g = this.CreateGraphics();
                try
                {
                    m_hoveredData = null;
                    m_filterTime = null;
                    if (m_threadsData != null)
                        m_threadsData.Clear();

                    string filename = doWorkArgs.Argument as string;

                    XmlDocument xmlDoc = new XmlDocument();
                    xmlDoc.Load(filename);

                    m_sessionName = filename;

                    m_threadsData = new List<CThreadTag>();
                    XmlNode rootNode = xmlDoc.SelectSingleNode("root");
                    int threadIndex = 0;
                    int num_childs = rootNode.ChildNodes.Count;
                    foreach (XmlNode threadNode in rootNode)
                    {
                        string threadName = threadNode.Attributes.GetNamedItem("name").Value;
                        float threadTimeMS = Single.Parse(threadNode.Attributes.GetNamedItem("totalTimeMS").Value, System.Globalization.CultureInfo.InvariantCulture);
                        long threadStart = Convert.ToInt64(threadNode.Attributes.GetNamedItem("startTime").Value);
                        long threadEnd = Convert.ToInt64(threadNode.Attributes.GetNamedItem("stopTime").Value);
                        long threadTime = threadEnd - threadStart;

                        m_threadsData.Add(new CThreadTag());

                        CThreadTag threadData = m_threadsData[threadIndex];
                        threadData.m_Times = new CBlockData(threadName, threadTimeMS, threadStart, threadEnd, 0, 0, "");

                        DataGridViewRow row = new DataGridViewRow();
                        DataGridViewCell cell0 = new DataGridViewTextBoxCell();
                        cell0.Value = threadName;
                        row.Cells.Add(cell0);
                        row.Tag = threadData;
                        m_loadingWorker.ReportProgress(0, row);

                        getBlocksFromXML(threadNode, 0, threadIndex);
                        threadIndex++;

                        // create rows
                        for (int i = 0; i < threadData.m_StackEnties.Count; ++i)
                        {
                            CBlockTag rowInfo = threadData.m_StackEnties[i];

                            // used for DrawFrectangles()
                            for (int j = 0; j < rowInfo.blocks.Count; ++j)
                            {
                                CBlockData data = rowInfo.blocks[j];
                                //rowInfo.blocksByColor[data.colorIdx].Add(data);
                                data.m_labelSize = g.MeasureString(data.name, m_barFont);
                            }

                            DataGridViewRow stackedRow = new DataGridViewRow();
                            DataGridViewCell stackedCell = new DataGridViewTextBoxCell();
                            stackedCell.Value = i;
                            stackedRow.Cells.Add(stackedCell);
                            stackedRow.Tag = rowInfo;
                            m_loadingWorker.ReportProgress(0, stackedRow);
                        }
                    }// foreach (XmlNode threadNode in rootNode)

                    // Call stack tree
                    m_callTree.Clear();
                    m_callTree.Capacity = m_threadsData.Count;
                    for (int i = 0; i < m_threadsData.Count; ++i)
                    {
                        CThreadTag tag = m_threadsData[i];
                        CThreadTimeline threadCallTree = new CThreadTimeline(tag.m_Times.name);
                        m_callTree.Add(threadCallTree);
                        for (int j = 0; j < tag.m_StackEnties.Count; ++j)
                        {
                            CBlockTag blockTag = tag.m_StackEnties[j];
                            for (int k = 0; k < blockTag.blocks.Count; ++k)
                            {
                                CBlockData blockData = blockTag.blocks[k];
                                threadCallTree.AddFunction(blockData.name, blockData.timeMS, blockData.timeStart, blockData.timeEnd, blockData.args);
                            }
                        }
                    }

                    this.BeginInvoke(new MethodInvoker(delegate
                    {
                        callGraphTreeView.BeginUpdate();
                        for (int i = 0; i < m_callTree.Count; ++i)
                        {
                            CThreadTimeline threadCallStack = m_callTree[i];
                            TreeNode root = callGraphTreeView.Nodes.Add(threadCallStack.name);
                            threadCallStack.functions.Sort(
                                delegate (CFunction funcA, CFunction funcB)
                                {
                                    return funcA.timeMS <= funcB.timeMS ? 1 : -1;
                                }
                            );

                            for (int j = 0; j < threadCallStack.functions.Count; ++j)
                            {
                                CFunction func = threadCallStack.functions[j];
                                SortFunctionsByTimeRecusively(func);
                                InsertTreeNodesRecursively(root, func);
                            }

                            root.BackColor = Color.BlanchedAlmond;
                        }
                        callGraphTreeView.EndUpdate();
                    }));
                }
                catch (System.Exception ex)
                {
                    m_sessionName = string.Empty;
                    MessageBox.Show(ex.Message);
                }
                g.Dispose();
            };
        }

        public void SetPickRegionMode(bool state)
        {
            m_pickRegionMode = state;
            if (m_pickRegionMode)
            {
                Cursor = System.Windows.Forms.Cursors.Hand;
                m_scanTool.Cursor = System.Windows.Forms.Cursors.Hand;
            }
            else
            {
                Cursor = System.Windows.Forms.Cursors.Default;
                m_scanTool.Cursor = System.Windows.Forms.Cursors.Default;
            }

        }

        public void ScanToolClosed()
        {
            SetPickRegionMode(false);
            m_scanTool = null;
        }

        private List<string> GetParentsOfHoverData(long start, long end)
        {
            List<string> result = new List<string>();

            if (m_callTree.Count > 0)
            {
                Queue<CFunction> functionsToInspect = new Queue<CFunction>();
                foreach (CThreadTimeline thread in m_callTree)
                {
                    for (int i = 0; i < thread.functions.Count; i++)
                    {
                        functionsToInspect.Enqueue(thread.functions[i]);
                    }

                    bool found = false;
                    CFunction currentFunction = null;
                    while (true)
                    {
                        if (functionsToInspect.Count > 0)
                        {
                            currentFunction = functionsToInspect.Dequeue();
                        }
                        else
                        {
                            break;
                        }

                        foreach (var time in currentFunction.timeRange)
                        {
                            if (time.Item1 == start && time.Item2 == end)
                            {
                                found = true;
                                break;
                            }
                        }

                        foreach (var subFunction in currentFunction.subFunctions)
                        {
                            functionsToInspect.Enqueue(subFunction);
                        }

                        if (found)
                        {
                            result = currentFunction.ExtractCallPath();
                            break;
                        }
                    }
                }
            }

            return result;
        }

        static private void getBlocksFromXML(XmlNode node, CThreadTag threadData, int depth, int threadIndex)
        {
            if (node.SelectSingleNode("block") != null)
            {
                if (depth >= threadData.m_StackEnties.Count)
                {
                    threadData.m_StackEnties.Add(new CBlockTag()); //new stack level
                    threadData.m_StackEnties[depth].threadIndex = threadIndex;
                }

                foreach (XmlNode blockNode in node)
                {
                    string name = blockNode.Attributes.GetNamedItem("name").Value;
                    float totalTimeMS = Single.Parse(blockNode.Attributes.GetNamedItem("totalTimeMS").Value, System.Globalization.CultureInfo.InvariantCulture);
                    long startTime = Convert.ToInt64(blockNode.Attributes.GetNamedItem("startTime").Value);
                    long stopTime = Convert.ToInt64(blockNode.Attributes.GetNamedItem("stopTime").Value);
                    
                    threadData.m_StackEnties[depth].blocks.Add(new CBlockData(name, totalTimeMS, startTime, stopTime, 0, depth, ""));

                    getBlocksFromXML(blockNode, threadData, depth + 1, threadIndex);
                }
            }
        }

        private static void RestoreGraphBeforeFiltering(CFunction rootFunction)
        {
            Stack<CFunction> workSet = new Stack<CFunction>();
            CFunction currentFunction = rootFunction;

            while (currentFunction != null)
            {
                foreach (var subFunction in currentFunction.subFunctions)
                {
                    workSet.Push(subFunction);
                }

                foreach (var filteredFunction in currentFunction.filteredSubFunctions)
                {
                    CFunction parent = currentFunction;
                    while (parent != null)
                    {
                        parent.timeMS += filteredFunction.timeMS;
                        parent = parent.parentFunction;
                    }
                    currentFunction.subFunctions.Add(filteredFunction);
                }
                currentFunction.filteredSubFunctions.Clear();

                if (workSet.Count == 0)
                    break;

                currentFunction = workSet.Pop();
            }

            SortFunctionsByTimeRecusively(rootFunction);
        }

        static private void FilterFunctionGraph(CFunction rootFunction, List<string> filterPath, bool includeFilterPath)
        {
            if (s_functionHasBeenFiltered)
            {
                RestoreGraphBeforeFiltering(rootFunction);
            }

            bool functionFiltered = false;

            // Perform filter
            if (filterPath.Count > 0 && !includeFilterPath)
            {
                Stack<CFunction> workSet = new Stack<CFunction>();
                CFunction currentFunction = rootFunction;

                while (currentFunction != null)
                {
                    foreach (var subFunction in currentFunction.subFunctions)
                    {
                        if (subFunction.HasParents(filterPath))
                        {
                            currentFunction.filteredSubFunctions.Add(subFunction);
                            functionFiltered = true;
                            continue;
                        }
                        workSet.Push(subFunction);
                    }

                    foreach (var filteredFunction in currentFunction.filteredSubFunctions)
                    {
                        CFunction parent = currentFunction;
                        while(parent != null)
                        {
                            parent.timeMS -= filteredFunction.timeMS;
                            parent = parent.parentFunction;
                        }
                        currentFunction.subFunctions.Remove(filteredFunction);
                    }

                    if (workSet.Count == 0)
                        break;

                    currentFunction = workSet.Pop();
                }
            }

            s_functionHasBeenFiltered = functionFiltered;
        }

        static private void RecurseFunctionCallGraph(CFunction func, int depth, int stopDepth, int stopBreath, List<string> callPath, bool includeCallPath, CThreadAnalyzedData result, CFunction parent)
        {
            if (depth > stopDepth)
            {
                return;
            }

            bool includeInTheSearch = callPath.Count > 0 && includeCallPath ? func.HasParents(callPath) : true;
            if (includeInTheSearch)
            {
                parent.CopyBasicData(func);
                if (parent.parentFunction != null)
                {
                    parent.parentFunction.subFunctions.Add(parent);
                }
                result.callgraphAsString += func.name;
            }

            int end = includeInTheSearch ? Math.Min(stopBreath, func.subFunctions.Count) : func.subFunctions.Count;
            for (int i = 0; i < end; i++)
            {
                CFunction subFunction = parent;
                if (includeInTheSearch)
                {
                    subFunction = new CFunction(func.subFunctions[i], parent);
                }
                RecurseFunctionCallGraph(func.subFunctions[i], depth + 1, stopDepth, stopBreath, callPath, includeCallPath, result, subFunction);
            }
        }

        static public CThreadAnalyzedData GetSortedCalltreeForThread(string filename, int threadIndex, int stopDepth, int stopBreath, List<string> callPath, bool includeCallPath)
        {
            CThreadAnalyzedData result = new CThreadAnalyzedData();

            CThreadTag threadData = new CThreadTag();

            XmlDocument xmlDoc = new XmlDocument();

            try
            {
                xmlDoc.Load(filename);
            }
            catch (System.Exception ex)
            {
                return result;
            }

            XmlNode rootNode = xmlDoc.SelectSingleNode("root");
            if(rootNode == null)
            {
                return result;
            }

            int idx = 0;
            foreach (XmlNode threadNode in rootNode)
            {
                if (idx != threadIndex)
                {
                    ++idx;
                    continue;
                }
                string threadName = threadNode.Attributes.GetNamedItem("name").Value;
                float threadTimeMS = Single.Parse(threadNode.Attributes.GetNamedItem("totalTimeMS").Value, System.Globalization.CultureInfo.InvariantCulture);
                long threadStart = Convert.ToInt64(threadNode.Attributes.GetNamedItem("startTime").Value);
                long threadEnd = Convert.ToInt64(threadNode.Attributes.GetNamedItem("stopTime").Value);
                long threadTime = threadEnd - threadStart;

                threadData.m_Times = new CBlockData(threadName, threadTimeMS, threadStart, threadEnd, 0, 0, "");

                getBlocksFromXML(threadNode, threadData, 0, threadIndex);
                ++idx;
            }// foreach (XmlNode threadNode in rootNode)

            // Create and sort the threadCallTree
            CThreadTimeline threadCallTree = new CThreadTimeline(threadData.m_Times.name);
            for (int j = 0; j < threadData.m_StackEnties.Count; ++j)
            {
                CBlockTag blockTag = threadData.m_StackEnties[j];
                for (int k = 0; k < blockTag.blocks.Count; ++k)
                {
                    CBlockData blockData = blockTag.blocks[k];
                    threadCallTree.AddFunction(blockData.name, blockData.timeMS, blockData.timeStart, blockData.timeEnd, blockData.args);
                }
            }

            threadCallTree.functions.Sort(delegate (CFunction funcA, CFunction funcB) { return funcA.timeMS <= funcB.timeMS ? 1 : -1; } );

            for (int j = 0; j < threadCallTree.functions.Count; ++j)
            {
                CFunction func = threadCallTree.functions[j];
                SortFunctionsByTimeRecusively(func);
            }
            // ~Create and sort the threadCallTree

            // Apply any set filters
            for (int j = 0; j < threadCallTree.functions.Count; j++)
            {
                FilterFunctionGraph(threadCallTree.functions[j], callPath, includeCallPath);
            }

            // Get the result analyzed data
            if (threadCallTree.functions.Count > 1)
            {
                s_bumpDepth = 1;
                float timeMsAcc = 0.0f;
                long start = threadCallTree.functions.First().timeRange.First().Item1;
                long end = threadCallTree.functions.Last().timeRange.Last().Item2;

                foreach (var item in threadCallTree.functions)
                {
                    timeMsAcc += item.timeMS;
                }

                CFunction root = new CFunction(threadCallTree.name, timeMsAcc, start, end, "", null);
                root.subFunctions.AddRange(threadCallTree.functions);
                threadCallTree.functions.Clear();
                threadCallTree.functions.Add(root);
            }
            else
            {
                s_bumpDepth = 0;
            }

            if (threadCallTree.functions.Count > 0)
            {
                RecurseFunctionCallGraph(threadCallTree.functions.First(), 0, stopDepth + s_bumpDepth, stopBreath, callPath, includeCallPath, result, result.callgraph);
            }

            result.threadTimelineSorted = threadCallTree;

            return result;
        }

        static public CThreadAnalyzedData GetSortedCalltreeForThread(CThreadTimeline threadCallTree, int stopDepth, int stopBreath, List<string> callPath, bool includeCallPath)
        {
            CThreadAnalyzedData result = new CThreadAnalyzedData();

            // Apply any set filters
            for (int j = 0; j < threadCallTree.functions.Count; j++)
            {
                FilterFunctionGraph(threadCallTree.functions[j], callPath, includeCallPath);
            }

            for (int j = 0; j < threadCallTree.functions.Count; ++j)
            {
                CFunction func = threadCallTree.functions[j];
                SortFunctionsByTimeRecusively(func);
            }

            // Get the result analyzed data
            for (int j = 0; j < threadCallTree.functions.Count; j++)
            {
                RecurseFunctionCallGraph(threadCallTree.functions[j], 0, stopDepth + s_bumpDepth, stopBreath, callPath, includeCallPath, result, result.callgraph);
            }

            result.threadTimelineSorted = threadCallTree;

            return result;
        }

        public void OpenSession(string filename)
        {
            if (m_loadingWorker.IsBusy)
                return;

            // 0 seed for same colors
            m_random = new Random(0);

            dataGridView_Prof.Rows.Clear();

            callGraphTreeView.Nodes.Clear();

            DataGridViewRow timeRow = new DataGridViewRow();
            DataGridViewCell textCell = new DataGridViewTextBoxCell();
            textCell.Value = "Time";
            timeRow.Cells.Add(textCell);
            timeRow.Height = 40;
            timeRow.Tag = new CTimeTag();
            timeRow.Frozen = true;
            dataGridView_Prof.Rows.Add(timeRow);

            progressBar1.Value = 0;
            progressBar1.Visible = true;
            progressBar1.Enabled = true;
            menuStrip1.Enabled = false;

            m_loadingWorker.RunWorkerAsync(filename);
        }

        private void getBlocksFromXML(XmlNode node, int depth, int threadIndex)
        {
            if (node.SelectSingleNode("block")!=null)
            {
                CThreadTag threadData = m_threadsData[threadIndex];
                if (depth >= threadData.m_StackEnties.Count)
                {
                    threadData.m_StackEnties.Add(new CBlockTag()); //new stack level
                    threadData.m_StackEnties[depth].threadIndex = threadIndex;
                }

                int prevClrIndex = 0;
                foreach (XmlNode blockNode in node)
                {
                    string name = blockNode.Attributes.GetNamedItem("name").Value;
                    float totalTimeMS = Single.Parse(blockNode.Attributes.GetNamedItem("totalTimeMS").Value, System.Globalization.CultureInfo.InvariantCulture);
                    long startTime = Convert.ToInt64(blockNode.Attributes.GetNamedItem("startTime").Value);
                    long stopTime = Convert.ToInt64(blockNode.Attributes.GetNamedItem("stopTime").Value);
                    XmlNode argsNode = blockNode.Attributes.GetNamedItem("args");
                    string args = "";
                    if(argsNode != null)
                        args = argsNode.Value;

                    int clrIndex = m_random.Next(0, s_clrBars.Length);
                    if (clrIndex == prevClrIndex)
                    {
                        clrIndex++;
                        if (clrIndex == s_clrBars.Length)
                            clrIndex = 0;
                    }

                    threadData.m_StackEnties[depth].blocks.Add(new CBlockData(name, totalTimeMS, startTime, stopTime, clrIndex, depth, args));
                    prevClrIndex = clrIndex;

                    getBlocksFromXML(blockNode, depth+1, threadIndex);
                }
            }
        }

        private void dataGridView_Prof_MouseWheel(object sender, MouseEventArgs e)
        {
            if (ModifierKeys == Keys.Control)
            {
                if (!dataGridView_Prof.IsOffsetSet())
                {
                    return;
                }

                int oldOffset = dataGridView_Prof.HorizontalScrollingOffset;

                dataGridView_Prof.Columns[1].AutoSizeMode = DataGridViewAutoSizeColumnMode.None;
                double zoomFactor = Math.Pow(2, (double)e.Delta * 0.001);
                int newWidth = (int)(dataGridView_Prof.Columns[1].Width * zoomFactor);

                bool clampWidth = newWidth < 0 || newWidth >= 65535;
                newWidth = newWidth < 65535 ? newWidth : 65535;
                newWidth = newWidth >= 0 ? newWidth : 0;

                dataGridView_Prof.Columns[1].Width = newWidth;

                int visibleColumn1Width = dataGridView_Prof.Width - dataGridView_Prof.Columns[0].Width;
                int mouseColumn1Offset = e.X - dataGridView_Prof.Columns[0].Width;
                double mousePrecentage = (double)mouseColumn1Offset / (double)visibleColumn1Width;

                int newOffset = clampWidth ? oldOffset : ((int)((double)oldOffset * zoomFactor) + (int)(visibleColumn1Width * mousePrecentage * (double)e.Delta * 0.001));
                newOffset = newOffset >= 0 ? newOffset : 0;
                dataGridView_Prof.SetOffset(newOffset);
            }                
        }

        private void dataGridView_Prof_MouseClick(object sender, MouseEventArgs e)
        {
            if (ModifierKeys == Keys.Control && e.Button != MouseButtons.Middle)
            {
                dataGridView_Prof.Columns[1].AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill;
            }
            else if (e.Button == MouseButtons.Left)
            {
                if (m_pickRegionMode && m_scanTool != null && m_hoveredData != null)
                {
                    List<string> callPath = GetParentsOfHoverData(m_hoveredData.timeStart, m_hoveredData.timeEnd);
                    m_scanTool.SetRegion(callPath, m_hoveredData.depth + 1);
                }
                else
                {
                    if (m_hoveredData != null)
                    {
                        m_filterTime = new List<Tuple<long, long>>(1);
                        m_filterTime.Add(new Tuple<long, long>(m_hoveredData.timeStart, m_hoveredData.timeEnd));
                    }
                    else
                    {
                        m_filterTime = null;
                    }
                    RefreshFilter();
                }
            }

            if (e.Button == MouseButtons.Right)
            {
                if (m_hoveredData != null)
                {
                    ContextMenu m = new ContextMenu();
                    MenuItem copyName = new MenuItem("Copy name");
                    copyName.Click += new System.EventHandler(
                            delegate(object o, EventArgs click_args)
                            {
                                Clipboard.SetText(m_hoveredData.name);
                            }
                        );
                    m.MenuItems.Add(copyName);
                    MenuItem copyArgs = new MenuItem("Copy arguments");
                    copyArgs.Click += new System.EventHandler(
                            delegate(object o, EventArgs click_args)
                            {
                                Clipboard.SetText(m_hoveredData.args);
                            }
                        );
                    m.MenuItems.Add(copyArgs);
                    m.Show(dataGridView_Prof, e.Location);
                }
            }
        }

        private void dataGridView_Prof_CellPainting(object sender, DataGridViewCellPaintingEventArgs e)
        {
            if (e.RowIndex < 0 || e.ColumnIndex < 0)
                return;

            if (e.ColumnIndex != 1)
                return;

           
            object tag = dataGridView_Prof.Rows[e.RowIndex].Tag;
            if (tag != null)
            {
                IRowTag rowTag = tag as IRowTag;
                if (rowTag != null)
                    rowTag.Draw(ref e, this);
            }
            else
            {
                //DRAW timeline
            }
        }

        private void onTreeViewMouseClick(object sender, MouseEventArgs e)
        {
            TreeViewHitTestInfo info = callGraphTreeView.HitTest(e.Location);

            if (info.Location == TreeViewHitTestLocations.Label)
            {
                TreeNode node = callGraphTreeView.GetNodeAt(e.Location);

                if (node != null && node.Tag != null)
                {
                    CFunction functionInfo = node.Tag as CFunction;
                    if (functionInfo != null)
                    {
                        m_filterTime = functionInfo.timeRange;
                        RefreshFilter();
                    }
                }
            }
        }

        private void onTreeViewNodeHover(object sender, TreeNodeMouseHoverEventArgs e)
        {
            CFunction functionInfo = e.Node.Tag as CFunction;
            if (functionInfo != null)
            {
                if(functionInfo.args != "")
                {
                    m_tooltip.ToolTipTitle = functionInfo.name;
                    m_tooltip.Show(string.Format("Args: {0}", functionInfo.args), callGraphTreeView, callGraphTreeView.PointToClient(new Point(Cursor.Position.X + 20, Cursor.Position.Y + 20)));
                }
                else
                {
                    m_tooltip.Hide(callGraphTreeView);
                }
            }
            else
            {
                m_tooltip.Hide(callGraphTreeView);
            }
        }

        private int binarySearch(List<CBlockData> list, int value)
        {
            int min = 0;
            int max = list.Count - 1;

            while(min <= max)
            {
                int mid = min + (max - min)/2;
                
                if (value >= list[mid].drawXStart)
                {
                    if (value <= list[mid].drawXEnd)
                        return mid;
                    else
                        min = mid + 1;
                }
                else
                    max = mid - 1;
            }
            return -1;
        }
        static public int binarySearch(List<Tuple<long,long>> list, long value)
        {
            int min = 0;
            int max = list.Count - 1;

            while (min <= max)
            {
                int mid = min + (max - min) / 2;

                if (value >= list[mid].Item1)
                {
                    if (value <= list[mid].Item2)
                        return mid;
                    else
                        min = mid + 1;
                }
                else
                    max = mid - 1;
            }
            return -1;
        }

        static public int binarySearch(List<CFunction> list, long value)
        {
            int min = 0;
            int max = list.Count - 1;

            while (min <= max)
            {
                int mid = min + (max - min) / 2;

                if (value >= list[mid].timeRange.First().Item1)
                {
                    if (value <= list[mid].timeRange.Last().Item2)
                    {
                        if (binarySearch(list[mid].timeRange, value) != -1)
                            return mid;
                        else
                            min = mid + 1;
                    }
                    else
                        min = mid + 1;
                }
                else
                    max = mid - 1;
            }
            return -1;
        }

        private void dataGridView_Prof_MouseMove(object sender, MouseEventArgs e)
        {
            if (m_loadingWorker.IsBusy)
                return;
            if (m_PrevMousePos != e.Location)
            {
                if (e.Button == MouseButtons.Middle)
                {
                    dataGridView_Prof.HorizontalScrollingOffset = Math.Max(0, dataGridView_Prof.HorizontalScrollingOffset + (m_PrevMousePos.X - e.Location.X));
                }

                m_PrevMousePos = e.Location;

                DataGridView.HitTestInfo hittest = dataGridView_Prof.HitTest(e.Location.X, e.Location.Y);

                m_tooltip.RemoveAll();
                if (hittest.ColumnIndex == 1 && hittest.RowIndex >= 0 && hittest.RowIndex < dataGridView_Prof.RowCount)
                {
                    object tag = dataGridView_Prof.Rows[hittest.RowIndex].Tag;
                     if (tag != null)
                     {
                         CBlockTag info = tag as CBlockTag;
                         if (info != null)
                         {
                             List<CBlockData> filteredBlocks = info.blocks.Where(x => x.visible).ToList();
                             int idx = binarySearch(filteredBlocks, e.Location.X);
                             if (idx != -1)
                             {
                                 m_hoveredData = filteredBlocks[idx];
                                 m_tooltip.Hide(dataGridView_Prof);

                                 Rectangle columnRect = info.cellBounds;

                                 float threadMS = m_threadsData[0].m_Times.timeMS;
                                 float mouseRel = ((e.X - columnRect.X) * 100.0f) / columnRect.Width;
                                 float mouseRelMS = (mouseRel / 100.0f) * threadMS;

                                 string text;

                                 string title = filteredBlocks[idx].name;
                                 if (title.Length > 90)
                                 {
                                     title = filteredBlocks[idx].name.Substring(0, 90);
                                     title += "...";
                                 }
                                 m_tooltip.ToolTipTitle = title;
                                 
                                 if (filteredBlocks[idx].args.Length > 0)
                                 {
                                     text = string.Format("totalTime = {0} ms\ncursorAbsolute = {1} ms ({2:##.##} %)\nargs = {3}",
                                                        filteredBlocks[idx].timeMS, mouseRelMS, mouseRel, filteredBlocks[idx].args);
                                 }
                                 else
                                 {
                                     text = string.Format("totalTime = {0} ms\ncursorAbsolute = {1} ms ({2:##.##} %)",
                                                        filteredBlocks[idx].timeMS, mouseRelMS, mouseRel);
                                 }

                                 m_tooltip.Show(text, dataGridView_Prof, e.X + 20, e.Y + 20);
                                 
                             }
                             else
                             {
                                 m_hoveredData = null;
                                 m_tooltip.Hide(dataGridView_Prof);
                             }
                         }
                         CThreadTag td = tag as CThreadTag;
                         if (td != null)
                         {

                             m_tooltip.Hide(dataGridView_Prof);
                             m_tooltip.Show(string.Format("time = {0} ms", td.m_Times.timeMS), dataGridView_Prof, e.X + 20, e.Y + 20);
                             m_tooltip.ToolTipTitle = td.m_Times.name;
                         }
                     }
                     else
                     {
                         m_hoveredData = null;
                         m_tooltip.Hide(dataGridView_Prof);
                     }
                }
                else
                {
                    m_hoveredData = null;
                    m_tooltip.Hide(dataGridView_Prof);
                }
            }

            // base.OnMouseMove(e);
        }

        private void Form1_DragDrop(object sender, DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
            {
                string[] files = (string[])e.Data.GetData(DataFormats.FileDrop);


                if (files.Length == 1)
                {
                    OpenSession(files[0]);
                }
            }
        }

        private void Form1_DragEnter(object sender, DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
            {
                e.Effect = DragDropEffects.Move;
            }
        }

        private void openToolStripMenuItem_Click(object sender, EventArgs e)
        {
            OpenFileDialog myDialog = new OpenFileDialog();

            myDialog.Filter = "All files (*.xml)|*.xml";
            myDialog.CheckFileExists = true;
            myDialog.Multiselect = false;

            if (myDialog.ShowDialog() == DialogResult.OK)
            {
                OpenSession(myDialog.FileName);
            }
        }

        private void reloadToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (m_sessionName != null)
                OpenSession(m_sessionName);
        }

        private void searchTextBox_KeyUp(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter)
            {
                m_filter = searchTextBox.Text;
                RefreshFilter();
            }
        }

        private void RefreshFilter()
        {
            m_filteredTimePerThread = new List<float>(16);

            for (int i = 0; i < m_threadsData.Count; ++i)
            {
                m_filteredTimeMSAcc = 0.0f;
                m_filterTimeRanges = new List<Tuple<long, long>>(128);

                m_threadsData[i].UpdateFilter();

                m_filteredTimePerThread.Add(m_filteredTimeMSAcc);
            }

            dataGridView_Prof.Refresh();

            if (m_threadsData.Count > 0)
            {
                listBox1.Items.Clear();
                for (int i = 0; i < m_threadsData.Count; ++i)
                {
                    if(m_filteredTimePerThread[i] > 0)
                    {
                        string label = "Thread \"" + m_threadsData[i].m_Times.name + "\" filter time total " + m_filteredTimePerThread[i].ToString() + " ms";
                        listBox1.Items.Add(label);
                    }
                }
            }
        }

        private void timeThreshold_ValueChanged(object sender, EventArgs e)
        {
            m_timeThresholdMs = Decimal.ToSingle(timeThreshold.Value);
            RefreshFilter();
        }

        private void argSearchTextBox_KeyUp(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter)
            {
                m_argFilter = argTextBox.Text;
                RefreshFilter();
            }
        }

        private void scanFolderToolStripMenuItem_Click(object sender, EventArgs e)
        {
           if (m_scanTool == null)
           {
               m_scanTool = new ThreadAnalyzer(this);
           }
           m_scanTool.Visible = true;
        }
    }

    public class CThreadAnalyzedData
    {
        public string callgraphAsString;
        public CFunction callgraph;
        public CThreadTimeline threadTimelineSorted;

        public CThreadAnalyzedData()
        {
            callgraphAsString = "";
            callgraph = new CFunction("", 0.0f, 0, 0, "", null);
            threadTimelineSorted = null;
        }
    };

    public class CFunction
    {
        public string name;
        public string args;
        public float timeMS;
        public long time;
        public int callCount;
        public List<Tuple<long, long>> timeRange;
        public List<CFunction> subFunctions;
        public List<CFunction> filteredSubFunctions;
        public CFunction parentFunction;

        public CFunction(string n, float t, long start, long end, string a, CFunction parent)
        {
            name = n;
            args = a;
            timeMS = t;
            time = end - start;
            callCount = 1;
            timeRange = new List<Tuple<long, long>>(4);
            timeRange.Add(new Tuple<long, long>(start, end));
            subFunctions = new List<CFunction>(64);
            filteredSubFunctions = new List<CFunction>();
            parentFunction = parent;
        }

        public CFunction(CFunction copy, CFunction parentFunc)
        {
            CopyBasicData(copy);
            parentFunction = parentFunc;
        }

        public void CopyBasicData(CFunction copy)
        {
            name = copy.name;
            args = copy.args;
            timeMS = copy.timeMS;
            time = copy.time;
            callCount = copy.callCount;
            timeRange = new List<Tuple<long, long>>();
            subFunctions = new List<CFunction>(16);
            filteredSubFunctions = new List<CFunction>();
        }

        public List<string> ExtractCallPath()
        {
            List<string> result = new List<string>();
            CFunction parent = this;
            while (parent != null)
            {
                result.Add(parent.name);
                parent = parent.parentFunction;
            }

            return result;
        }

        public bool HasParents(List<string> parentsName)
        {
            CFunction parent = this;
            int i = 0;
            while (parent != null && i < parentsName.Count)
            {
                if (parent.name.Equals(parentsName[i]))
                {
                    ++i;
                }

                parent = parent.parentFunction;
            }

            return i == parentsName.Count;
        }

        public bool AddSubfunction(string n, float t, long start, long end, string a)
        {
            int idx = Form1.binarySearch(subFunctions, start);
            if (idx != -1)
            {
                if (subFunctions[idx].AddSubfunction(n, t, start, end, a))
                {
                    return true;
                }
            }

            for (int i = 0; i < timeRange.Count; ++i)
            {
                if (start >= timeRange[i].Item1 && end <= timeRange[i].Item2)
                {
                    for (int j = 0; j < subFunctions.Count; ++j)
                    {
                        if (subFunctions[j].name == n)
                        {
                            subFunctions[j].timeRange.Add(new Tuple<long, long>(start, end));
                            subFunctions[j].timeMS += t;
                            ++subFunctions[j].callCount;
                            return true;
                        }
                    }
                    subFunctions.Add(new CFunction(n, t, start, end, a, this));
                    return true;
                }
            }

            return false;
        }
    };

    public class CThreadTimeline
    {
        public string name;
        public List<CFunction> functions;

        public CThreadTimeline(string n)
        {
            name = n;
            functions = new List<CFunction>(64);
        }

        public void AddFunction(string n, float t, long start, long end, string a)
        {
            int i = Form1.binarySearch(functions, start);
            if (i != -1)
            {
                if (functions[i].AddSubfunction(n, t, start, end, a))
                {
                    return;
                }
            }

            functions.Add(new CFunction(n, t, start, end, a, null));
        }
    };

    public class DGW : DataGridView
    {
        private bool reapplyOffset = false;
        private int deferredOffset = 0;        

        public DGW()
        {
            this.DoubleBuffered = true;
        }

        protected override void OnPaintBackground(PaintEventArgs pevent)
        {
            if (reapplyOffset)
            {
                HorizontalScrollingOffset = deferredOffset;
                reapplyOffset = false;
            }
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            if (reapplyOffset)
            {
                HorizontalScrollingOffset = deferredOffset;
                reapplyOffset = false;
                Invalidate(true);
            }

            base.OnPaint(e);
        }

        public bool IsOffsetSet()
        {
            return !reapplyOffset;
        }

        public void SetOffset(int offset)
        {
            if (reapplyOffset)
            {
                deferredOffset += offset;
                return;
            }

            reapplyOffset = true;
            deferredOffset = offset;
        }
    }

    public class CBlockData
    {
        public string name;
        public float timeMS;
        public long timeStart;
        public long timeEnd;
        public long time;
        public int depth;

        public string args;

        public int drawXStart;
        public int drawXEnd;
        public int colorIdx;

        public bool visible;

        public SizeF m_labelSize;
        public CBlockData(string n, float t, long start, long end, int clrIdx, int d, string a)
        {
            name = n;
            timeMS = t;
            timeStart = start;
            timeEnd = end;
            time = end - start;
            depth = d;

            args = a;

            colorIdx = clrIdx;

            visible = true;
        }
    }

    public abstract class IRowTag
    {
        public abstract void Draw(ref DataGridViewCellPaintingEventArgs e, Form1 form);
    }

    public class CBlockTag : IRowTag
    {
        public int threadIndex;
        public List<CBlockData> blocks;
        public Rectangle cellBounds;
        public CBlockTag()
        {
            blocks = new List<CBlockData>();
        }

       public void UpdateFilter()
        {
            bool textFilterEmpty = Form1.m_filter == "";
            string lowerCaseFilter = Form1.m_filter.ToLower();
            bool argFilterEmpty = Form1.m_argFilter == "";
            string lowerCaseArgFilter = Form1.m_argFilter.ToLower();
            List<Tuple<long, long>> ranges = Form1.m_filterTimeRanges;

            for (int i = 0; i < blocks.Count; i++)
            {
                CBlockData blockData = blocks[i];
                blockData.visible = (Form1.IsTimeInFilterRange(blockData.timeStart, blockData.timeEnd)) &&
                                    (textFilterEmpty || blockData.name.ToLower().Contains(lowerCaseFilter)) &&
                                    (argFilterEmpty  || blockData.args.ToLower().Contains(lowerCaseArgFilter)) &&
                                    (blockData.timeMS >= Form1.m_timeThresholdMs);

                if (blockData.visible)
                {
                    bool inside_range = false;
                    for (int j = 0; j < ranges.Count; j++)
                    {
                        if (ranges[j].Item1 <= blockData.timeStart && blockData.timeEnd <= ranges[j].Item2)
                        {
                            inside_range = true;
                            break;
                        }
                    }

                    if (!inside_range)
                    {
                       Form1.m_filteredTimeMSAcc += blockData.timeMS;
                       ranges.Add(new Tuple<long, long>(blockData.timeStart, blockData.timeEnd));
                    }
                }
            }
        }

        public override void Draw(ref DataGridViewCellPaintingEventArgs e, Form1 form)
        {
            e.Handled = true;
            Graphics g = e.Graphics;
            cellBounds = e.CellBounds;

            const int offset = 2;
            Rectangle cellBound = new Rectangle(e.CellBounds.X, e.CellBounds.Y + offset,
                e.CellBounds.Width, e.CellBounds.Height - offset - 1);

            using (Brush backBrush = new SolidBrush(Form1.s_clrBack[e.RowIndex % 2]))
            {
                using (Pen borderPen = new Pen(Color.Black, 1))
                {
                    g.FillRectangle(backBrush, e.CellBounds);
                    g.DrawRectangle(borderPen, e.CellBounds);

                    borderPen.Dispose();
                }
            }

            CThreadTag threadData = form.m_threadsData[threadIndex];
            CBlockData threadTime = threadData.m_Times;
            long ttime = threadTime.time; // 100%

            bool smallBlock = false;
            int smallBlockPos = 0;

            for (int i = 0; i < blocks.Count; i++)
            {
                CBlockData blockData = blocks[i];
                if (!blockData.visible)
                {
                    continue;
                }

                long blockWidth = e.CellBounds.Width * blockData.time / ttime;
                cellBound.Width = (int)blockWidth;

                long blockStart = (blockData.timeStart - threadTime.timeStart) * e.CellBounds.Width / ttime;
                cellBound.X = e.CellBounds.X + (int)blockStart;

                blockData.drawXStart = cellBound.X;
                blockData.drawXEnd = cellBound.X + cellBound.Width;

                if (smallBlock && smallBlockPos != cellBound.X)
                {
                    Rectangle rec = new Rectangle(smallBlockPos, e.CellBounds.Y + offset,
                                    1, e.CellBounds.Height - offset - 1);

                    g.FillRectangle(form.m_BarsBrushes[blocks[i - 1].colorIdx], rec);

                    smallBlock = false;
                }

                if (cellBound.Width < 1)
                {
                    //cellBound.Width = 1;

                    smallBlockPos = cellBound.X;
                    smallBlock = true;
                    continue;
                }

                if ((cellBound.X < e.ClipBounds.X + e.ClipBounds.Width) &&
                    (cellBound.X + cellBound.Width > e.ClipBounds.X))
                {
                    g.FillRectangle(form.m_BarsBrushes[blockData.colorIdx], cellBound);

                    SizeF textSize = blockData.m_labelSize;
                    if (textSize.Width < cellBound.Width - 3)
                    {
                        StringFormat formatter = new StringFormat();
                        formatter.LineAlignment = StringAlignment.Center;
                        formatter.Alignment = StringAlignment.Center;
                        float tx = cellBound.X + cellBound.Width / 2;
                        float ty = cellBound.Y + cellBound.Height / 2;
                        g.DrawString(blockData.name, form.m_barFont, Brushes.White, tx, ty, formatter);
                    }
                }
            } //for (int i = 0; i < info.blocks.Count; i++)

            if (smallBlock)
            {
                Rectangle rec = new Rectangle(smallBlockPos, e.CellBounds.Y + offset,
                                   1, e.CellBounds.Height - offset - 1);

                g.FillRectangle(form.m_BarsBrushes[blocks[blocks.Count - 1].colorIdx], rec);
            }
        }

    }

    public class CThreadTag : IRowTag
    {
        public CBlockData m_Times;
        public List<CBlockTag> m_StackEnties; //each stack level have a list of blocks
        public CThreadTag()
        {
            m_StackEnties = new List<CBlockTag>();
        }

        public void UpdateFilter()
        {
            for (int i = 0; i < m_StackEnties.Count; i++)
            {
                CBlockTag tag = m_StackEnties[i];
                tag.UpdateFilter();
            }
        }

        public override void Draw(ref DataGridViewCellPaintingEventArgs e, Form1 form)
        {
            e.Handled = true;
            Graphics g = e.Graphics;

            const int offset = 2;
            Rectangle cellBound = new Rectangle(e.CellBounds.X, e.CellBounds.Y + offset,
                e.CellBounds.Width, e.CellBounds.Height - offset - 1);

            string threadText = "Thread: " + m_Times.name;
            g.FillRectangle(Brushes.DimGray, e.CellBounds);
            Font font = new Font(e.CellStyle.Font, FontStyle.Bold);

            SizeF textSize = g.MeasureString(threadText, font);
            if (textSize.Width < cellBound.Width - 3)
            {
                StringFormat formatter = new StringFormat();
                formatter.LineAlignment = StringAlignment.Center;
                formatter.Alignment = StringAlignment.Center;
                float tx = cellBound.X + cellBound.Width / 2;
                float ty = cellBound.Y + cellBound.Height / 2;
                g.DrawString(threadText, font, Brushes.White, tx, ty, formatter);
            }
        }
    }

    public class CTimeTag : IRowTag
    {
        private void DrawLinesBetween(ref DataGridViewCellPaintingEventArgs e, Pen pen, Font font, int pos1, int pos2, float pos2MS, float blockMS, int markerHeight)
        {
            int blockThreshold = 200; // in pixels
            //int blockDivCount = 5;

            int diff = pos2 - pos1;

            if (diff != 0 && diff > blockThreshold && markerHeight > 1)
            {

                Graphics g = e.Graphics;
                
                int X = e.CellBounds.X;
                int Y = e.CellBounds.Y + e.CellBounds.Height - 1;

                int linePos = pos1 + (pos2 - pos1)/2;
                int DrawPos = X + linePos;


                float pos1MS = pos2MS - blockMS;
                float lineMS = pos1MS + (pos2MS - pos1MS) / 2.0f; 


                //if (DrawPos > e.ClipBounds.X && DrawPos < e.ClipBounds.X + e.ClipBounds.Width) //artifacts ;(
                {
                    g.DrawLine(pen, DrawPos, Y - markerHeight, DrawPos, Y);


                    string s = string.Format("{0:##.##}", (lineMS));

                    StringFormat formatter = new StringFormat();
                    formatter.LineAlignment = StringAlignment.Center;
                    formatter.Alignment = StringAlignment.Center;
                    g.DrawString(s, font, Brushes.Black, DrawPos, Y - markerHeight - 5, formatter);

                    blockMS = pos2MS - lineMS;
                    DrawLinesBetween(ref e, pen, font, pos1, linePos, lineMS, blockMS, markerHeight - 2);
                    DrawLinesBetween(ref e, pen, font, linePos, pos2, pos2MS, blockMS, markerHeight - 2);
                }
            }
        }

        public override void Draw(ref DataGridViewCellPaintingEventArgs e, Form1 form)
        {
            e.Handled = true;
            Graphics g = e.Graphics;

            g.FillRectangle(Brushes.White, e.CellBounds);

            if (form.m_threadsData != null && form.m_threadsData.Count > 0)
            {
                Int32 cellWidth = e.CellBounds.Width;
                Int32 lineYpos = e.CellBounds.Y + e.CellBounds.Height - 1;

                using (Pen btPen = new Pen(Color.Black, 2))
                {
                    // bottom line
                    g.DrawLine(btPen,
                        e.CellBounds.X, lineYpos,
                        e.CellBounds.X + cellWidth, lineYpos);

                    btPen.Dispose();
                }


                //form.dataGridView_Prof.PointToScreen()
                g.DrawString("Milliseconds", form.m_barFont, Brushes.Black, e.CellBounds.X + 1 , e.CellBounds.Y + 1);
                Pen pen = new Pen(Color.Black, 1);

                CThreadTag threadData = form.m_threadsData[0];
                CBlockData threadTime = threadData.m_Times;
                float totalTimeMS = threadTime.timeMS;
                //long totalTime = threadTime.time;

                int marketHeight = 16;

                int[] blocks = {10000, 5000, 2000, 1000, 500, 200, 100, 50, 20, 10, 5, 2, 1};

                float timeMSperBlock = 0.0f;
                int blockInx = 0;
                while (timeMSperBlock < 10.0f && blockInx > blocks.Count())
                {
                     timeMSperBlock = totalTimeMS / blocks[blockInx++];
                }
                if (blockInx != 0 )
                    --blockInx;

                float blockMS = blocks[blockInx];
                


                int linePx = 0;
                int linePrevPx = 0;
                int i = 0;
                while (linePx < cellWidth)
                {
                    float lineMS = i *blockMS;
                    ++i;

                    linePrevPx = linePx;
                    linePx = Convert.ToInt32(Math.Round((float)(lineMS * cellWidth) / totalTimeMS));

                    DrawLinesBetween(ref e, pen, form.m_barFont, linePrevPx, linePx, lineMS, blockMS, marketHeight);
                    
                    int Xpos = e.CellBounds.X + linePx;
                    g.DrawLine(pen, Xpos, lineYpos - marketHeight, Xpos, lineYpos);

                    string s = string.Format("{0}", Convert.ToInt32(lineMS));

                    StringFormat formatter = new StringFormat();
                    formatter.LineAlignment = StringAlignment.Center;
                    formatter.Alignment = StringAlignment.Center;
                    float tx = Xpos;
                    float ty = lineYpos - marketHeight - 5;

                    g.DrawString(s, form.m_barFont, Brushes.Black, tx, ty, formatter);
                }

                pen.Dispose();
            }
        }
    }
}

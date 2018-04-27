// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.IO;
using LuaRemoteDebugger.Properties;
using System.Collections.Specialized;
using System.Xml.Serialization;
using System.Diagnostics;
using System.Security.Cryptography;
using System.Runtime.InteropServices;

namespace LuaRemoteDebugger
{
	public partial class MainForm : Form
	{
		public class EditorBreakpoint
		{
			public string FileName;
			public int Line;
			public bool Enabled = true;
		}

		private enum DebugState
		{
			NotConnected,
			WaitingForBreak,
			Executing,
			Halted,
		}

		LuaRemoteDebugger debugger;
		string rootScriptsFolder = string.Empty;
		List<EditorBreakpoint> editorBreakpoints = new List<EditorBreakpoint>();
		LuaVariablesModel luaVariablesModel = new LuaVariablesModel();
		LuaWatchModel luaWatchModel = new LuaWatchModel();
		FindTextDialog findTextDialog;
		Point findTextLocation;
		RichTextBoxFinds findTextOptions;
		ToolTip toolTip = new ToolTip();
		bool waitingForBreakpointToTrigger = true;

		[StructLayout(LayoutKind.Sequential)]
		private struct FLASHWINFO
		{
			public UInt32 cbSize;
			public IntPtr hwnd;
			public Int32 dwFlags;
			public UInt32 uCount;
			public Int32 dwTimeout;
		}

		//Stop flashing. The system restores the window to its original state. 
		private const UInt32 FLASHW_STOP = 0;
		//Flash the window caption. 
		private const UInt32 FLASHW_CAPTION = 1;
		//Flash the taskbar button. 
		private const UInt32 FLASHW_TRAY = 2;
		//Flash both the window caption and taskbar button.
		//This is equivalent to setting the FLASHW_CAPTION | FLASHW_TRAY flags. 
		private const UInt32 FLASHW_ALL = 3;
		//Flash continuously, until the FLASHW_STOP flag is set. 
		private const UInt32 FLASHW_TIMER = 4;
		//Flash continuously until the window comes to the foreground. 
		private const UInt32 FLASHW_TIMERNOFG = 12; 

		[DllImport("user32.dll")]
		static extern Int32 FlashWindowEx(ref FLASHWINFO pwfi);

		public MainForm()
		{
			InitializeComponent();

			debugger = new LuaRemoteDebugger();
			debugger.MessageLogged += LogMessage;
			debugger.ExecutionLocationChanged += OnExecutionLocationChanged;
			debugger.LuaVariablesChanged += OnLuaVariablesChanged;
			debugger.VersionInformationReceived += OnVersionInformationReceived;
			debugger.BinaryFileDetected += OnBinaryFileDetected;
			debugger.GameFolderReceived += OnGameFolderReceived;
			debugger.FileMD5Received += OnFileMD5Received;
			debugger.FileContentsReceived += OnFileContentsReceived;
			debugger.Connected += OnConnected;
			debugger.Disconnected += OnDisconnected;
			debugger.ScriptError += OnScriptError;

			LoadSettings();

			Timer timer = new Timer();
			timer.Tick += timer_Tick;
			timer.Interval = 50;
			timer.Start();

			treeViewAdvLocals.Model = luaVariablesModel;
			treeViewAdvWatch.Model = luaWatchModel;

			watchKey.LabelChanged += watchKey_LabelChanged;
			watchKey.IsEditEnabledValueNeeded += watchKey_IsEditEnabledValueNeeded;

			SetDebugState(DebugState.NotConnected);
		}

		protected override void OnClosing(CancelEventArgs e)
		{
			base.OnClosing(e);

			SaveSettings();
		}

		private void LoadSettings()
		{
			if (!String.IsNullOrEmpty(Settings.Default.ScriptsFolder))
			{
				SetScriptsFolder(Settings.Default.ScriptsFolder);
			}

			if (Settings.Default.OpenFiles != null)
			{
				foreach (string fileName in Settings.Default.OpenFiles)
				{
					OpenFile(fileName);
				}
			}

			if (!string.IsNullOrEmpty(Settings.Default.Breakpoints))
			{
				XmlSerializer serializer = new XmlSerializer(typeof(List<EditorBreakpoint>));

				using (StringReader stringReader = new StringReader(Settings.Default.Breakpoints))
				{
					List<EditorBreakpoint> breakPoints = serializer.Deserialize(stringReader) as List<EditorBreakpoint>;
					if (breakPoints != null)
					{
						editorBreakpoints = breakPoints;
						OnBreakpointsChanged(true);
					}
				}
			}

			findTextOptions = (RichTextBoxFinds)Settings.Default.FindTextOptions;

			SetCppCallStackMode((CppCallStackMode)Settings.Default.EnableCppCallstack);

			if (Settings.Default.WatchVariables != null)
			{
				foreach (string watchVariable in Settings.Default.WatchVariables)
				{
					AddWatchVariable(watchVariable);
				}
			}
		}

		private void SaveSettings()
		{
			Settings.Default.OpenFiles = new StringCollection();

			foreach (TabPage page in tabControlSourceFiles.TabPages)
			{
				LuaSourceEditor editor = page.Tag as LuaSourceEditor;
				if (editor != null)
				{
					Settings.Default.OpenFiles.Add(editor.FileName);
				}
			}

			XmlSerializer serializer = new XmlSerializer(typeof(List<EditorBreakpoint>));

			using (MemoryStream memStream = new MemoryStream())
			{
				serializer.Serialize(memStream, editorBreakpoints);

				memStream.Position = 0;

				StreamReader reader = new StreamReader(memStream);
				string xmlData = reader.ReadToEnd();

				Settings.Default.Breakpoints = xmlData;
			}

			Settings.Default.FindTextOptions = (int)findTextOptions;

			Settings.Default.EnableCppCallstack = (int)GetCppCallStackMode();

			Settings.Default.WatchVariables = new StringCollection();
			foreach (LuaWatchVariable watchVariable in luaWatchModel.WatchVariables)
			{
				Settings.Default.WatchVariables.Add(watchVariable.Name);
			}

			Settings.Default.Save();
		}

		public void LogMessage(string format, params object[] args)
		{
			LogMessage(string.Format(format, args));
		}

		public void LogMessage(string message)
		{
			richTextBoxLog.AppendText(message + "\n");
		}

		private string GetFullSourcePath(string relativePath)
		{
			string fullPathName = rootScriptsFolder + relativePath.Replace("@scripts", "");
			try
			{
				fullPathName = Path.GetFullPath(fullPathName);
			}
			catch (System.ArgumentException)
			{
				fullPathName = string.Empty;
			}
			return fullPathName;
		}

		private string GetRelativeSourcePath(string fullPath)
		{
			string relativePath = fullPath.Substring(rootScriptsFolder.Length);
			relativePath = "@scripts" + relativePath.Replace('\\', '/').ToLower();
			return relativePath;
		}

		private void OnExecutionLocationChanged(string source, int line, List<CallStackItem> callstack)
		{
			string fullPathName = GetFullSourcePath(source);
			if (!string.IsNullOrEmpty(fullPathName))
			{
				LuaSourceEditor editor = OpenFile(fullPathName);
				if (editor != null)
				{
					editor.OnExecutionLocationChanged(line);

					if (this.waitingForBreakpointToTrigger)
					{
						this.TopMost = true;	// will be reset to false by timer_Tick() to prevent this application to reside on top of all other applications, even when alt-tabbing to a different one
						this.waitingForBreakpointToTrigger = false;
					}
				}
			}

			// Update the call stack
			listViewCallStack.Items.Clear();
			foreach (CallStackItem item in callstack)
			{
				ListViewItem listItem = listViewCallStack.Items.Add(item.Description);
				listItem.SubItems.Add(item.Line == 0 ? string.Empty : item.Line.ToString());
				listItem.SubItems.Add(item.Source);
				listItem.Tag = item;
			}

			SetDebugState(DebugState.Halted);
			if (debugger.HostVersion >= 7)
			{
				luaVariablesModel.SetFilter(0, true);
			}
		}

		private void OnVersionInformationReceived(Platform hostPlatform, int hostVersion, int clientVersion)
		{
			if (hostVersion != clientVersion)
			{
				string text = string.Format("Version mismatch, host version: {0}, client version: {1}\n", hostVersion, clientVersion);
				if (hostVersion > clientVersion)
				{
					text += "You need to update your remote debugger client.\n\n";
				}
				else
				{
					text += "The game is not using the latest remote debugger code.\n\n";
				}
				text += "Continue anyway?";
				if (MessageBox.Show(this, text, "Version mismatch", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.No)
				{
					// Bail!
					debugger.Disconnect();
					return;
				}
			}
			if (hostPlatform == Platform.PS3)
			{
				string ps3BinPath = Settings.Default.Ps3BinPath;
				if (string.IsNullOrEmpty(ps3BinPath) || !File.Exists(ps3BinPath))
				{
					ps3BinPath = null;
					// Try to find the PS3 bin folder automatically
					string ps3Root = Environment.GetEnvironmentVariable("SCE_PS3_ROOT");
					if (ps3Root != null)
					{
						ps3BinPath = Path.Combine(ps3Root, "host-win32/sn/bin/ps3bin.exe");
						if (!File.Exists(ps3BinPath))
						{
							ps3BinPath = null;
						}
					}
				}
				if (string.IsNullOrEmpty(ps3BinPath))
				{
					// As a last resort, ask the user to locate it
					string caption = "In order to resolve C++ callstacks on the PS3, we need access to ps3bin.exe.\n";
					caption += "It is a part of the CELL SDK and should be located under <cell>/host-win32/sn/bin/ps3bin.exe.\n\n";
					caption += "Do you want to locate it now?";
					if (MessageBox.Show(this, caption, "Locate ps3bin.exe", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
					{
						using (OpenFileDialog fileDialog = new OpenFileDialog())
						{
							fileDialog.CheckFileExists = true;
							fileDialog.Filter = "ps3bin.exe|ps3bin.exe";
							if (fileDialog.ShowDialog(this) == DialogResult.OK && fileDialog.FileName.EndsWith("ps3bin.exe", StringComparison.OrdinalIgnoreCase))
							{
								ps3BinPath = fileDialog.FileName;
							}
						}
					}
				}
				if (!string.IsNullOrEmpty(ps3BinPath))
				{
					string selfPath = null;
					// Try to determine the executing self from the target manager API
					if (Ps3TmApi.Available && debugger.ConnectedIpAddress != null)
					{
						selfPath = Ps3TmApi.FindTargetSelf(debugger.ConnectedIpAddress);
					}
					
					// As a last resort, ask the user to locate it
					if (string.IsNullOrEmpty(selfPath))
					{
						string caption = "In order to resolve C++ callstacks on the PS3, we need access to the compiled self file.\n\n";
						caption += "Do you want to locate it now?";
						if (MessageBox.Show(this, caption, "Locate self", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
						{
							using (OpenFileDialog fileDialog = new OpenFileDialog())
							{
								fileDialog.CheckFileExists = true;
								fileDialog.Filter = "self files (*.self)|*.self";
								if (fileDialog.ShowDialog(this) == DialogResult.OK)
								{
									selfPath = fileDialog.FileName;
								}
							}
						}
					}
					if (!string.IsNullOrEmpty(selfPath))
					{
						debugger.SetSymbolsManager(new Ps3SymbolsManager(ps3BinPath, selfPath));
					}
					Settings.Default.Ps3BinPath = ps3BinPath;		// Save the path for next time
				}
			}

			SendBreakpointsToDebugger();
			CheckFileDifferences();
			debugger.EnableCppCallstack(GetCppCallStackMode());
		}

		private void SetCppCallStackMode(CppCallStackMode mode)
		{
			disabledToolStripMenuItem.Checked = (mode == CppCallStackMode.Disabled);
			condensedToolStripMenuItem.Checked = (mode == CppCallStackMode.Condensed);
			fullToolStripMenuItem.Checked = (mode == CppCallStackMode.Full);
			debugger.EnableCppCallstack(mode);
		}

		private CppCallStackMode GetCppCallStackMode()
		{
			if (fullToolStripMenuItem.Checked)
				return CppCallStackMode.Full;
			else if (condensedToolStripMenuItem.Checked)
				return CppCallStackMode.Condensed;
			return CppCallStackMode.Disabled;
		}

		private void OnBinaryFileDetected()
		{
			string text = "The game is running pre-compiled Lua files which can't be debugged.\nIn order for the debugger to work correctly the game must use plain text Lua files.";
			MessageBox.Show(this, text, "Binary Lua file detected", MessageBoxButtons.OK, MessageBoxIcon.Warning);
		}

		private void OnGameFolderReceived(string gameFolder)
		{
			string newScriptsFolder = Path.Combine(gameFolder, "Scripts");
			newScriptsFolder = Path.GetFullPath(newScriptsFolder);
			if (!Directory.Exists(newScriptsFolder))
			{
				string text = string.Format("Game was built from {0} but no scripts folder was found under that path.\nIf the game was built from another machine you must manually set the scripts folder.", gameFolder);
				MessageBox.Show(this, text, "Scripts folder", MessageBoxButtons.OK, MessageBoxIcon.Warning);
			}
			else if (newScriptsFolder.ToLower() != rootScriptsFolder.ToLower())
			{
				string text = string.Format("Found scripts folder at {0}, would you like to set this as your scripts folder?", newScriptsFolder);
				if (MessageBox.Show(this, text, "Scripts folder", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
				{
					SetScriptsFolder(newScriptsFolder);
				}
			}
		}

		private void OnFileMD5Received(string fileName, byte[] md5)
		{
			string fullPath = GetFullSourcePath(fileName);
			MD5 md5Compute = MD5.Create();
			byte[] data = File.ReadAllBytes(fullPath);
			byte[] computedMd5 = md5Compute.ComputeHash(data);
			if (!md5.SequenceEqual(computedMd5))
			{
				LogMessage("MD5 check failed for {0}, files are different between host and client", fullPath);
				string text = string.Format("{0} is different to the one on the remote machine!\n\nDo you want to view the remote machine's version of the file?", fullPath);
				if (MessageBox.Show(this, text, "Files are different", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.Yes)
				{
					debugger.RequestFileContents(fileName);
				}
			}
		}

		private void OnFileContentsReceived(string fileName, string contents)
		{
			string fullPath = GetFullSourcePath(fileName);
			LuaSourceEditor editor = FindEditor(fullPath);
			if (editor != null)
			{
				editor.SetContents(contents);
			}
		}

		private void OnConnected()
		{
			SetDebugState(DebugState.Executing);
		}

		private void OnDisconnected()
		{
			SetDebugState(DebugState.NotConnected);
		}

		private void OnScriptError(string error)
		{
			MessageBox.Show(this, error, "Script Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
		}

		private void OnLuaVariablesChanged(LuaTable table)
		{
			int vScrollValue = treeViewAdvLocals.VScrollBar.Value;
			luaVariablesModel.SetLuaTable(table, treeViewAdvLocals.Root);
			if (vScrollValue > treeViewAdvLocals.VScrollBar.Maximum)
				vScrollValue = treeViewAdvLocals.VScrollBar.Maximum;
			treeViewAdvLocals.VScrollBar.Value = vScrollValue;

			foreach (var variable in luaWatchModel.WatchVariables)
			{
				variable.LuaValue = GetVariable(variable.Name);
			}

			vScrollValue = treeViewAdvWatch.VScrollBar.Value;
			luaWatchModel.RefreshStructure(treeViewAdvWatch.Root);
			if (vScrollValue > treeViewAdvWatch.VScrollBar.Maximum)
				vScrollValue = treeViewAdvWatch.VScrollBar.Maximum;
			treeViewAdvWatch.VScrollBar.Value = vScrollValue;
		}

		private void timer_Tick(object sender, EventArgs e)
		{
			// If any modal windows are open (e.g. MessageBox), don't update until they are closed
			if (!this.CanFocus)
				return;
			debugger.Update();
			if (this.TopMost) // Note: need to check state, the "set" will cause topmost-fighting with child dialogs (like find dialog)
			{
				this.TopMost = false;	// this will allow for alt-tabbing to other applications without having us always stay on top of them anymore
			}
		}

		private void treeViewFiles_NodeMouseDoubleClick(object sender, TreeNodeMouseClickEventArgs e)
		{
			FileInfo fileInfo = e.Node.Tag as FileInfo;
			if (fileInfo != null)
			{
				OpenFile(fileInfo.FullName);
			}
		}

		private LuaSourceEditor FindEditor(string fileName)
		{
			// Normalize path
			fileName = Path.GetFullPath(fileName);

			foreach (TabPage page in tabControlSourceFiles.TabPages)
			{
				LuaSourceEditor editor = page.Tag as LuaSourceEditor;
				if (editor != null)
				{
					if (fileName.ToLower() == editor.FileName.ToLower())
					{
						// This file is already open, just select it
						tabControlSourceFiles.SelectedTab = page;
						return page.Tag as LuaSourceEditor;
					}
				}
			}

			return null;
		}

		private LuaSourceEditor GetCurrentEditor()
		{
			TabPage tabPage = tabControlSourceFiles.SelectedTab;
			if (tabPage != null)
			{
				return tabPage.Tag as LuaSourceEditor;
			}
			return null;
		}

		private LuaSourceEditor OpenFile(string fileName)
		{
			// Normalize path
			fileName = Path.GetFullPath(fileName);

			LuaSourceEditor editor = FindEditor(fileName);
			if (editor != null)
			{
				return editor;
			}

			if (!File.Exists(fileName))
			{
				MessageBox.Show(this, string.Format("Source file not found: {0}", fileName), "File not found error", MessageBoxButtons.OK, MessageBoxIcon.Error);
				return null;
			}

			debugger.RequestFileMD5(GetRelativeSourcePath(fileName));

			// File not yet open, lets open it
			TabPage newPage = new TabPage(Path.GetFileName(fileName));
			LuaSourceEditor newEditor = new LuaSourceEditor();
			newEditor.OnHoverOverVariable += newEditor_OnHoverOverVariable;
			newEditor.BackColor = System.Drawing.Color.WhiteSmoke;
			newEditor.BorderStyle = System.Windows.Forms.BorderStyle.None;
			newEditor.Dock = System.Windows.Forms.DockStyle.Fill;
			newEditor.OpenFile(fileName);
			newPage.Controls.Add(newEditor);
			newPage.Tag = newEditor;
			newPage.ToolTipText = fileName;
			tabControlSourceFiles.TabPages.Add(newPage);
			tabControlSourceFiles.SelectedTab = newPage;

			UpdateEditorBreakpoints(newEditor);

			return newEditor;
		}

		private void CloseSelectedTab()
		{
			TabPage selected = tabControlSourceFiles.SelectedTab;
			if (selected != null)
			{
				tabControlSourceFiles.TabPages.Remove(selected);
			}
		}

		private void SetScriptsFolder(string scriptsFolder)
		{
			// Clear the breakpoints because we don't want breakpoints pointing to another scripts folder
			// May be better to store breakpoints as relative paths then we wouldn't need to clear them
			editorBreakpoints.Clear();
			OnBreakpointsChanged(true);

			// Clear open files as well
			tabControlSourceFiles.TabPages.Clear();

			// Save the script folder to the settings so it is available on next run
			Settings.Default.ScriptsFolder = scriptsFolder;

			treeViewFiles.Nodes.Clear();
			scriptsFolder = Path.GetFullPath(scriptsFolder);
			rootScriptsFolder = scriptsFolder;

			DirectoryInfo info = new DirectoryInfo(scriptsFolder);
			if (info.Exists)
			{
				TreeNode rootNode = new TreeNode(info.Name);
				rootNode.Tag = info;
				GetDirectories(info.GetDirectories(), rootNode);
				GetFiles(info.GetFiles(), rootNode);
				rootNode.Expand();
				treeViewFiles.Nodes.Add(rootNode);
			}
		}

		private void GetDirectories(DirectoryInfo[] subDirs, TreeNode nodeToAddTo)
		{
			TreeNode aNode;
			foreach (DirectoryInfo subDir in subDirs)
			{
				aNode = new TreeNode(subDir.Name, 0, 0);
				aNode.Tag = subDir;
				GetDirectories(subDir.GetDirectories(), aNode);
				GetFiles(subDir.GetFiles(), aNode);
				nodeToAddTo.Nodes.Add(aNode);
			}
		}

		private void GetFiles(FileInfo[] files, TreeNode nodeToAddTo)
		{
			TreeNode aNode;
			foreach (FileInfo file in files)
			{
				if (Path.GetExtension(file.Name).ToLower() == ".lua")
				{
					aNode = new TreeNode(file.Name, 1, 1);
					aNode.Tag = file;
					nodeToAddTo.Nodes.Add(aNode);
				}
			}
		}

		private void ToggleBreakpoint()
		{
			if (tabControlSourceFiles.SelectedTab != null)
			{
				LuaSourceEditor editor = tabControlSourceFiles.SelectedTab.Tag as LuaSourceEditor;
				if (editor != null)
				{
					bool addBreakpoint = true;
					EditorBreakpoint newBreakpoint = new EditorBreakpoint();
					newBreakpoint.FileName = editor.FileName;
					newBreakpoint.Line = editor.GetSelectedLine();
					foreach (EditorBreakpoint breakpoint in editorBreakpoints)
					{
						if (breakpoint.FileName.ToLower() == newBreakpoint.FileName.ToLower() &&
							breakpoint.Line == newBreakpoint.Line)
						{
							// Breakpoint already exists
							if (breakpoint.Enabled)
							{
								// Let's remove it if it was enabled
								editorBreakpoints.Remove(breakpoint);
								editor.ClearBreakpoint(breakpoint.Line);
							}
							else
							{
								// Let's enabled it if it was disabled
								breakpoint.Enabled = true;
								editor.SetBreakpoint(breakpoint.Line, true);
							}
							addBreakpoint = false;
							break;
						}
					}
					if (addBreakpoint)
					{
						editorBreakpoints.Add(newBreakpoint);
						editor.SetBreakpoint(newBreakpoint.Line, true);
					}
					OnBreakpointsChanged(true);
				}
			}
		}

		private void DeleteBreakpoint(EditorBreakpoint deleteBreakpoint)
		{
			foreach (EditorBreakpoint breakpoint in editorBreakpoints)
			{
				if (breakpoint.FileName.ToLower() == deleteBreakpoint.FileName.ToLower() &&
					breakpoint.Line == deleteBreakpoint.Line)
				{
					// Breakpoint found, let's remove it
					editorBreakpoints.Remove(breakpoint);
					break;
				}
			}
		}

		private void UpdateEditorBreakpoints(LuaSourceEditor editor)
		{
			editor.ClearBreakpoints();
			foreach (EditorBreakpoint editorBreakpoint in editorBreakpoints)
			{
				if (editor.FileName.ToLower() == editorBreakpoint.FileName.ToLower())
				{
					editor.SetBreakpoint(editorBreakpoint.Line, editorBreakpoint.Enabled);
				}
			}
		}

		private void OnBreakpointsChanged(bool updateListView)
		{
			// Update the breakpoints window
			if (updateListView)
			{
				listViewBreakpoints.Items.Clear();
				foreach (EditorBreakpoint editorBreakpoint in editorBreakpoints)
				{
					string text = string.Format("{0}, line {1}", Path.GetFileName(editorBreakpoint.FileName), editorBreakpoint.Line);
					ListViewItem item = listViewBreakpoints.Items.Add(text);
					item.Tag = editorBreakpoint;
					item.Checked = editorBreakpoint.Enabled;
				}
			}

			// Update the source editors
			foreach (TabPage page in tabControlSourceFiles.TabPages)
			{
				LuaSourceEditor editor = page.Tag as LuaSourceEditor;
				if (editor != null)
				{
					UpdateEditorBreakpoints(editor);
				}
			}
			
			// Update the debugger
			SendBreakpointsToDebugger();
		}

		private void SendBreakpointsToDebugger()
		{
			List<Breakpoint> breakpoints = new List<Breakpoint>();

			foreach (EditorBreakpoint editorBreakpoint in editorBreakpoints)
			{
				if (editorBreakpoint.Enabled)
				{
					Breakpoint breakpoint;

					string relativePath = GetRelativeSourcePath(editorBreakpoint.FileName);

					breakpoint.FileName = relativePath;
					breakpoint.Line = editorBreakpoint.Line;
					breakpoints.Add(breakpoint);
				}
			}

			debugger.SetBreakpoints(breakpoints);
		}

		private void CheckFileDifferences()
		{
			foreach (TabPage page in tabControlSourceFiles.TabPages)
			{
				LuaSourceEditor editor = page.Tag as LuaSourceEditor;
				if (editor != null)
				{
					debugger.RequestFileMD5(GetRelativeSourcePath(editor.FileName));
				}
			}
		}

		private void setScriptsFolderToolStripMenu_Click(object sender, EventArgs e)
		{
			using (FolderBrowserDialog folderDialog = new FolderBrowserDialog())
			{
				folderDialog.ShowNewFolderButton = false;
				if (!string.IsNullOrEmpty(rootScriptsFolder))
				{
					folderDialog.SelectedPath = rootScriptsFolder;
				}
				else
				{
					folderDialog.SelectedPath = Path.GetDirectoryName(Application.ExecutablePath);
				}
				DialogResult result = folderDialog.ShowDialog(this);
				if (result == DialogResult.OK)
				{
					SetScriptsFolder(folderDialog.SelectedPath);
				}
			}
		}

		private void SetDebugState(DebugState state)
		{
			if (state != DebugState.Halted)
			{
				foreach (TabPage page in tabControlSourceFiles.TabPages)
				{
					LuaSourceEditor editor = page.Tag as LuaSourceEditor;
					if (editor != null)
					{
						editor.ClearExecutionLocation();
					}
				}
				listViewCallStack.Items.Clear();
				treeViewAdvLocals.Enabled = false;
				treeViewAdvWatch.Enabled = false;
			}
			else
			{
				treeViewAdvLocals.Enabled = true;
				treeViewAdvWatch.Enabled = true;
				if (!Focused)
				{
					// Flash window to grab user's attention
					FlashWindow();
				}
			}
			continueToolStripMenuItem.Enabled = toolStripButtonContinue.Enabled = (state == DebugState.Halted);
			breakToolStripMenuItem.Enabled = toolStripButtonBreak.Enabled = (state == DebugState.Executing);
			stepIntoToolStripMenuItem.Enabled = toolStripButtonStepInto.Enabled = (state == DebugState.Halted);
			stepOverToolStripMenuItem.Enabled = toolStripButtonStepOver.Enabled = (state == DebugState.Halted);
			stepOutToolStripMenuItem.Enabled = toolStripButtonStepOut.Enabled = (state == DebugState.Halted);
		}

		private void FlashWindow()
		{
			FLASHWINFO flashInfo = new FLASHWINFO();
			flashInfo.cbSize = Convert.ToUInt32(Marshal.SizeOf(typeof(FLASHWINFO)));
			flashInfo.hwnd = this.Handle;
			flashInfo.dwFlags = (int)(FLASHW_TRAY | FLASHW_TIMERNOFG);
			FlashWindowEx(ref flashInfo);
		}

		private void connectToolStripMenuItem_Click(object sender, EventArgs e)
		{
			using (ConnectMessageBox messageBox = new ConnectMessageBox(this))
			{
				bool keepTrying = true;
				while (keepTrying)
				{
					keepTrying = false;
					string error;

					if (messageBox.ShowDialog(this) == DialogResult.OK)
					{
						if (!debugger.Connect(messageBox.IpAddress, messageBox.Port, out error))
						{
							DialogResult result = MessageBox.Show(this, error, "Connection Error", MessageBoxButtons.RetryCancel, MessageBoxIcon.Error);
							if (result == DialogResult.Retry)
							{
								keepTrying = true;
							}
						}
					}
				}
			}
		}

		private void disconnectToolStripMenuItem_Click(object sender, EventArgs e)
		{
			debugger.Disconnect();
		}

		private void exitToolStripMenuItem_Click(object sender, EventArgs e)
		{
			Close();
		}

		private void toolStripButtonContinue_Click(object sender, EventArgs e)
		{
			debugger.Continue();
			SetDebugState(DebugState.Executing);
		}

		private void toolStripButtonBreak_Click(object sender, EventArgs e)
		{
			debugger.Break();
			SetDebugState(DebugState.WaitingForBreak);
		}

		private void toolStripButtonStepInto_Click(object sender, EventArgs e)
		{
			debugger.StepInto();
			SetDebugState(DebugState.WaitingForBreak);
		}

		private void toolStripButtonStepOver_Click(object sender, EventArgs e)
		{
			debugger.StepOver();
			SetDebugState(DebugState.WaitingForBreak);
		}

		private void toolStripButtonStepOut_Click(object sender, EventArgs e)
		{
			debugger.StepOut();
			SetDebugState(DebugState.WaitingForBreak);
		}

		private void toolStripButtonBreakpoint_Click(object sender, EventArgs e)
		{
			ToggleBreakpoint();
		}

		private void closeToolStripMenuItem_Click(object sender, EventArgs e)
		{
			CloseSelectedTab();
		}

		private void listViewBreakpoints_KeyDown(object sender, KeyEventArgs e)
		{
			if (e.KeyCode == Keys.Delete)
			{
				foreach (ListViewItem item in listViewBreakpoints.SelectedItems)
				{
					EditorBreakpoint breakpoint = item.Tag as EditorBreakpoint;
					if (breakpoint != null)
					{
						DeleteBreakpoint(breakpoint);
					}
				}
				OnBreakpointsChanged(true);
			}
		}

		private void listViewBreakpoints_ItemChecked(object sender, ItemCheckedEventArgs e)
		{
			EditorBreakpoint breakpoint = e.Item.Tag as EditorBreakpoint;
			if (breakpoint != null)
			{
				breakpoint.Enabled = e.Item.Checked;
				OnBreakpointsChanged(false);
			}
		}

		private void GetFiles(TreeNodeCollection treeNodes, List<string> filenames)
		{
			foreach (TreeNode node in treeNodes)
			{
				GetFiles(node.Nodes, filenames);
				FileInfo fileInfo = node.Tag as FileInfo;
				if (fileInfo != null)
				{
					filenames.Add(fileInfo.FullName);
				}
			}
		}

		private void ShowFindFileDialog()
		{
			using (FindFileDialog dialog = new FindFileDialog())
			{
				List<string> filenames = new List<string>();
				GetFiles(treeViewFiles.Nodes, filenames);
				dialog.SetFiles(filenames);
				if (dialog.ShowDialog(this) == DialogResult.OK && !string.IsNullOrEmpty(dialog.SelectedFile))
				{
					OpenFile(dialog.SelectedFile);
				}
			}
		}

		private void findFileToolStripMenuItem_Click(object sender, EventArgs e)
		{
			ShowFindFileDialog();
		}

		private void goToToolStripMenuItem_Click(object sender, EventArgs e)
		{
			LuaSourceEditor editor = GetCurrentEditor();
			if (editor != null)
			{
				editor.ShowGoToLine();
			}
		}

		private void continueToolStripMenuItem_Click(object sender, EventArgs e)
		{
			debugger.Continue();
			SetDebugState(DebugState.Executing);
			SendToBack();
			this.waitingForBreakpointToTrigger = true;
		}

		private void breakToolStripMenuItem_Click(object sender, EventArgs e)
		{
			debugger.Break();
			SetDebugState(DebugState.WaitingForBreak);
		}

		private void stepIntoToolStripMenuItem_Click(object sender, EventArgs e)
		{
			debugger.StepInto();
			SetDebugState(DebugState.WaitingForBreak);
		}

		private void stepOverToolStripMenuItem_Click(object sender, EventArgs e)
		{
			debugger.StepOver();
			SetDebugState(DebugState.WaitingForBreak);
		}

		private void stepOutToolStripMenuItem_Click(object sender, EventArgs e)
		{
			debugger.StepOut();
			SetDebugState(DebugState.WaitingForBreak);
		}

		private void toggleBreakpointToolStripMenuItem_Click(object sender, EventArgs e)
		{
			ToggleBreakpoint();
		}

		private void aboutLuaRemoteDebuggerToolStripMenuItem_Click(object sender, EventArgs e)
		{
			using (AboutBox aboutBox = new AboutBox())
			{
				aboutBox.ShowDialog();
			}
		}

		private void listViewBreakpoints_ItemDoubleClicked(object sender, LuaRemoteDebuggerControls.ItemDoubleClickEventArgs e)
		{
			EditorBreakpoint breakpoint = e.Item.Tag as EditorBreakpoint;
			if (breakpoint != null)
			{
				LuaSourceEditor editor = OpenFile(breakpoint.FileName);
				if (editor != null)
				{
					editor.GoToLine(breakpoint.Line);
				}
			}
		}

		private void listViewCallStack_ItemDoubleClicked(object sender, LuaRemoteDebuggerControls.ItemDoubleClickEventArgs e)
		{
			CallStackItem? item = e.Item.Tag as CallStackItem?;
			if (item != null && item.Value.Source != null && item.Value.Source.EndsWith(".lua", StringComparison.OrdinalIgnoreCase))
			{
				LuaSourceEditor editor = OpenFile(GetFullSourcePath(item.Value.Source));
				if (editor != null)
				{
					editor.GoToLine(item.Value.Line);
				}
			}

			if (debugger.HostVersion >= 7)
			{
				luaVariablesModel.SetFilter(item != null ? item.Value.FrameIndex : -1, false);
			}
		}

		private void findToolStripMenuItem_Click(object sender, EventArgs e)
		{
			if (findTextDialog == null)
			{
				findTextDialog = new FindTextDialog();
				findTextDialog.FormClosed += new FormClosedEventHandler(findTextDialog_FormClosed);
				findTextDialog.FindText += new EventHandler<FindTextEventArgs>(findTextDialog_FindText);
				findTextDialog.FindOptions = findTextOptions;
				if (!findTextLocation.IsEmpty)
				{
					findTextDialog.Location = findTextLocation;
				}
				else
				{
					Point pos = Location;
					pos.Y += 50;
					pos.X += Width - findTextDialog.Width - 100;
					findTextDialog.Location = pos;
				}
				findTextDialog.Show(this);
			}
			else
			{
				findTextDialog.Focus();
			}
		}

		private void findTextDialog_FindText(object sender, FindTextEventArgs e)
		{
			LuaSourceEditor editor = GetCurrentEditor();
			if (editor != null)
			{
				editor.FindText(e.Text, e.Options);
			}
		}

		private void findTextDialog_FormClosed(object sender, FormClosedEventArgs e)
		{
			Debug.Assert(sender == findTextDialog);
			findTextLocation = findTextDialog.Location;
			findTextOptions = findTextDialog.FindOptions;
			findTextDialog = null;
		}

		private void ClearToolTip()
		{
			toolTip.Hide(tabControlSourceFiles);
		}

		private void newEditor_OnHoverOverVariable(object sender, HoverVariableEventArgs e)
		{
			if (string.IsNullOrEmpty(e.VariableName))
			{
				ClearToolTip();
			}
			else
			{
				ILuaAnyValue value = GetVariable(e.VariableName);
				if (value != LuaNil.Instance)
				{
					string toolTipText = string.Format("{0} : {1}", e.VariableName, value.ToString());
					Point toolTipPos = tabControlSourceFiles.PointToClient(e.HoverScreenLocation);
					toolTip.Show(toolTipText, tabControlSourceFiles, toolTipPos);
				}
				else
				{
					toolTip.Hide(tabControlSourceFiles);
				}
			}
		}

		private ILuaAnyValue GetVariable(string variableName)
		{
			string[] parts = variableName.Split('.');
			LuaTable rootTable = luaVariablesModel.RootLuaTable;
			IEnumerable<LuaTable> tablesToSearch = new LuaTable[] { rootTable };
			if (debugger.HostVersion >= 7)
			{
				if (rootTable != null)
				{
					tablesToSearch = rootTable.Value.Values.OfType<LuaTable>();
				}
			}
			foreach (LuaTable hayStack in tablesToSearch)
			{
				ILuaAnyValue result = hayStack;
				foreach (string part in parts)
				{
					LuaTable table = result as LuaTable;
					if (table != null)
					{
						if (table.Value.TryGetValue(new LuaString(part), out result)) continue;
					}
					result = null;
					break;
				}
				if (result != null) return result;
			}
			return LuaNil.Instance;
		}

		private void AddWatchVariable(string name)
		{
			LuaWatchVariable watchVariable = new LuaWatchVariable(name, GetVariable(name));
			luaWatchModel.AddWatchVariable(watchVariable);
		}

		private void watchKey_LabelChanged(object sender, Aga.Controls.Tree.NodeControls.LabelEventArgs e)
		{
			if (e.Subject == luaWatchModel.NewWatchVariable)
			{
				// New watch item
				AddWatchVariable(e.NewLabel.Trim());
			}
			else
			{
				// Existing watch item
				LuaWatchVariable variable = e.Subject as LuaWatchVariable;
				if (variable != null)
				{
					variable.Name = e.NewLabel.Trim();
					variable.LuaValue = GetVariable(variable.Name);
					luaWatchModel.WatchVariableChanged(variable);
				}
			}
		}

		private void watchKey_IsEditEnabledValueNeeded(object sender, Aga.Controls.Tree.NodeControls.NodeControlValueEventArgs e)
		{
			e.Value = e.Node.Tag is LuaWatchVariable;
		}

		private void treeViewAdvWatch_KeyDown(object sender, KeyEventArgs e)
		{
			if (e.KeyCode == Keys.Delete)
			{
				var node = treeViewAdvWatch.SelectedNode;
				if (node != null)
				{
					LuaWatchVariable variable = node.Tag as LuaWatchVariable;
					if (variable != null)
					{
						luaWatchModel.DeleteWatchVariable(variable);
					}
				}
			}
			else if (e.KeyCode >= Keys.A && e.KeyCode <= Keys.Z)
			{
				// If the user starts typing letters then enter edit mode and start with the letter he typed
				watchKey.BeginEdit();
				TextBox editor = treeViewAdvWatch.CurrentEditor as TextBox;
				if (editor != null)
				{
					int index = e.KeyCode - Keys.A;
					char character;
					if (e.Shift)
					{
						character = (char)('A' + index);
					}
					else
					{
						character = (char)('a' + index);
					}
					editor.Text = character.ToString();
					editor.SelectionStart = 1;
				}
				e.SuppressKeyPress = true;
			}
		}

		private void treeViewAdvWatch_MouseDoubleClick(object sender, MouseEventArgs e)
		{
			watchKey.BeginEdit();
		}

		private void treeViewAdvWatch_DragDrop(object sender, DragEventArgs e)
		{
			string text = e.Data.GetData(DataFormats.Text) as string;
			if (text != null)
			{
				AddWatchVariable(text.Trim());
			}
		}

		private void treeViewAdvWatch_DragEnter(object sender, DragEventArgs e)
		{
			if (e.Data.GetDataPresent(DataFormats.Text))
			{
				e.Effect = DragDropEffects.Copy;
			}
			else
			{
				e.Effect = DragDropEffects.None;
			}
		}

		private void treeViewAdvWatch_ItemDrag(object sender, ItemDragEventArgs e)
		{
			Aga.Controls.Tree.TreeNodeAdv[] items = e.Item as Aga.Controls.Tree.TreeNodeAdv[];
			if (items != null && items.Length == 1)
			{
				var item = items[0];
				string fullName = string.Empty;
				do 
				{
					string subPart = string.Empty;
					LuaWatchVariable watchVariable = item.Tag as LuaWatchVariable;
					if (watchVariable != null)
					{
						subPart = watchVariable.Name;
					}
					else
					{
						LuaVariableItem variableItem = item.Tag as LuaVariableItem;
						if (variableItem != null)
						{
							LuaString keyString = variableItem.LuaKey as LuaString;
							if (keyString != null)
							{
								subPart = keyString.Value;
							}
						}
					}

					if (subPart != string.Empty)
					{
						if (fullName == string.Empty)
						{
							fullName = subPart;
						}
						else
						{
							fullName = subPart + "." + fullName;
						}
					}

					item = item.Parent;
				} while (item.Parent != null);
				if (!string.IsNullOrEmpty(fullName))
				{
					DoDragDrop(fullName, DragDropEffects.Copy);
				}
			}
		}

		private void fullToolStripMenuItem_Click(object sender, EventArgs e)
		{
			SetCppCallStackMode(CppCallStackMode.Full);
		}

		private void condensedToolStripMenuItem_Click(object sender, EventArgs e)
		{
			SetCppCallStackMode(CppCallStackMode.Condensed);
		}

		private void disabledToolStripMenuItem_Click(object sender, EventArgs e)
		{
			SetCppCallStackMode(CppCallStackMode.Disabled);
		}
	}
}

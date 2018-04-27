// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.IO;
using ORTMAPILib;

namespace Statoscope
{
	partial class ConnectMessageBox : Form
	{
		private class TargetItem
		{
			public ETargetType Type;
			public string Name;
			public string IpAddress;
			public int Port;

			public TargetItem(ETargetType type, string name, string ipAddress, int port)
			{
				Type = type;
				Name = name;
				IpAddress = ipAddress;
				Port = port;
			}

			public override string ToString()
			{
				return Name;
			}
		}

		enum ETargetType
		{
			// keep these names the same as the images in targetImageList
			PC,
			X360,
			PS3,
			Durango,
      Orbis
		}

		bool suppressChangeEvents = false;	// Used to prevent feedback loop
		static ConnectMessageBox currentConnectMessageBox;
		const int StatoscopeCryEnginePort = 29527;

		public string IpAddress { get { return textBoxIpAddress.Text; } }
		public int Port
		{
			get
			{
				int result = StatoscopeCryEnginePort;
				int.TryParse(textBoxPort.Text, out result);
				return result;
			}
		}

		public string ConnectionName
		{
			get
			{
				if (targetListView.SelectedItems.Count > 0)
					return targetListView.SelectedItems[0].Text;
				else
					return string.Format("{0}:{1}", IpAddress, Port);
			}
		}

		#region Kernel32Functions

		[DllImport("kernel32.dll")]
		private static extern IntPtr LoadLibrary(string dllFilePath);

		[DllImport("kernel32.dll")]
		private static extern IntPtr GetProcAddress(IntPtr hModule, String procname);

		[DllImport("kernel32.dll")]
		private static extern Int32 GetLastError();

		[DllImport("kernel32.dll")]
		private static extern Int32 SetDllDirectory(string directory);

		#endregion

		#region XBoxFunctions

		// Functions and structs defined in xbdm.h
		[DllImport("xbdm.dll")]
		static extern Int32 DmGetAltAddress(out uint address);

		[DllImport("xbdm.dll")]
		static extern Int32 DmGetNameOfXbox(StringBuilder name, out uint length, bool resolvable);

		#endregion

		public ConnectMessageBox()
		{
			InitializeComponent();

			string currentIp = "127.0.0.1";
			int currentPort = StatoscopeCryEnginePort;
			textBoxIpAddress.Text = currentIp;
			textBoxPort.Text = currentPort.ToString();

			PopulateTargets();
			FindMatchingTarget(currentIp, currentPort);
		}

		private void buttonConnect_Click(object sender, EventArgs e)
		{
			/*
			Settings.Default.IpAddress = textBoxIpAddress.Text;
			Settings.Default.Port = Port;
			*/
		}

		void AddTarget(ETargetType type, string name, string ipAddress, int port)
		{
			AddTarget(new TargetItem(type, name, ipAddress, port));
		}

		delegate void AddTargetDelegate(TargetItem ti);

		void AddTarget(TargetItem ti)
		{
			if (InvokeRequired)
			{
				BeginInvoke(new AddTargetDelegate(AddTarget), new object[] { ti });
				return;
			}

			ListViewItem lvi = new ListViewItem(ti.Name);
			lvi.Tag = ti;
			lvi.ImageKey = ti.Type.ToString();
			targetListView.Items.Add(lvi);
		}

		private void PopulateTargets()
		{
			// Add local host
			AddTarget(ETargetType.PC, "PC", "127.0.0.1", StatoscopeCryEnginePort);

#if !__MonoCS__
			// these are done asynchronously to improve responsiveness
			// the biggest problem is when no active 360 kit is found and that takes a few seconds to be picked up
			ThreadPool.QueueUserWorkItem(o => AddX360Targets());
			ThreadPool.QueueUserWorkItem(o => AddPS3Targets());
			ThreadPool.QueueUserWorkItem(o => AddDurangoTargets());
#endif
      ThreadPool.QueueUserWorkItem(o => AddOrbisTargets());
		}

		void AddX360Targets()
		{
			// Get Xbox IP address
			string xekd = Environment.GetEnvironmentVariable("XEDK");
			if (xekd != null)
			{
				string dllPath;
				if (IntPtr.Size == 4)
				{
					dllPath = Path.Combine(xekd, "bin\\win32");
				}
				else
				{
					dllPath = Path.Combine(xekd, "bin\\x64");
				}

				// Set the DLL search directory so that it is able to find xbdm.dll
				SetDllDirectory(dllPath);

				uint xboxAddress = 0;
				StringBuilder xboxName = new StringBuilder(1024);
				bool success = true;
				try
				{
					DmGetAltAddress(out xboxAddress);
					uint xboxNameLength = (uint)xboxName.Capacity;
					DmGetNameOfXbox(xboxName, out xboxNameLength, true);
				}
				catch (System.Exception ex)
				{
					System.Console.WriteLine("Failed to get Xbox IP address: {0}", ex.Message);
					success = false;
				}
				if (success && (xboxName.Length > 0))
				{
					byte p1 = (byte)(xboxAddress >> 24 & 0xFF);
					byte p2 = (byte)(xboxAddress >> 16 & 0xFF);
					byte p3 = (byte)(xboxAddress >> 8 & 0xFF);
					byte p4 = (byte)(xboxAddress >> 0 & 0xFF);
					string xboxIp = string.Format("{0}.{1}.{2}.{3}", p1, p2, p3, p4);

					AddTarget(ETargetType.X360, xboxName.ToString(), xboxIp, StatoscopeCryEnginePort);
				}

				// Restore the default DLL search order
				SetDllDirectory(null);
			}
			else
			{
				System.Console.WriteLine("Failed to find Xbox XDK");
			}
		}

		void AddPS3Targets()
		{
			// Get the PS3 IP addresses
			if (Ps3TmApi.Available)
			{
				int result = Ps3TmApi.SNPS3InitTargetComms();
				currentConnectMessageBox = this;
				result = Ps3TmApi.SNPS3EnumerateTargetsEx(GetPS3TargetInfoCallback, IntPtr.Zero);
				currentConnectMessageBox = null;
				result = Ps3TmApi.SNPS3CloseTargetComms();
			}
			else
			{
				System.Console.WriteLine("Failed to load PS3 Target Manager API");
			}
		}

		private static int GetPS3TargetInfoCallback(int target, IntPtr userData)
		{
			Ps3TmApi.SNPS3TargetInfo targetInfo = new Ps3TmApi.SNPS3TargetInfo();
			targetInfo.hTarget = target;
			targetInfo.nFlags = Ps3TmApi.SN_TI_TARGETID | Ps3TmApi.SN_TI_NAME | Ps3TmApi.SN_TI_INFO;
			int result = Ps3TmApi.SNPS3GetTargetInfo(ref targetInfo);
			string type = Marshal.PtrToStringAnsi(targetInfo.pszType);
			string name = Marshal.PtrToStringAnsi(targetInfo.pszName);
			string info = Marshal.PtrToStringAnsi(targetInfo.pszInfo);
			string ipAddress = string.Empty;
			Ps3TmApi.SNPS3GamePortIPAddressData gameIpAddress;
			// Note: We can only get the game port IP address if we are connected to the kit and it is running a game
			result = Ps3TmApi.SNPS3GetGamePortIPAddrData(target, IntPtr.Zero, out gameIpAddress);
			if (result == 0 && gameIpAddress.uReturnValue == 0)
			{
				byte p1 = (byte)(gameIpAddress.uIPAddress >> 24 & 0xFF);
				byte p2 = (byte)(gameIpAddress.uIPAddress >> 16 & 0xFF);
				byte p3 = (byte)(gameIpAddress.uIPAddress >> 8 & 0xFF);
				byte p4 = (byte)(gameIpAddress.uIPAddress >> 0 & 0xFF);
				ipAddress = string.Format("{0}.{1}.{2}.{3}", p1, p2, p3, p4);
			}
			else if (type == "PS3_DBG_DEX")
			{
				// Test kits only have 1 IP address, and we can get it from the info string
				string[] parts = info.Split(',');
				string lastPart = parts[parts.Length - 1].Trim();
				ipAddress = lastPart.Split(':')[0];
			}
			if (!string.IsNullOrEmpty(ipAddress))
			{
				currentConnectMessageBox.AddTarget(ETargetType.PS3, name, ipAddress, StatoscopeCryEnginePort);
			}
			return 0;
		}

		void AddDurangoTargets()
		{
			try
			{
				string cmd = Environment.ExpandEnvironmentVariables("%DurangoXDK%\\bin\\xbconnect.exe");

				Process p = new Process();
				p.StartInfo.FileName = cmd;
				p.StartInfo.Arguments = "/S";
				p.StartInfo.UseShellExecute = false;
				p.StartInfo.RedirectStandardOutput = true;
				p.StartInfo.CreateNoWindow = true;
				p.Start();

				string output = p.StandardOutput.ReadToEnd();
				string ip = output.Trim();

				AddTarget(ETargetType.Durango, "Durango", ip, StatoscopeCryEnginePort);

				p.WaitForExit();
			}
			catch (Exception ex)
			{
				Console.WriteLine(ex);
			}
		}

    void AddOrbisTargets()
    {
      try
      {
        ORTMAPI orbisAPI = new ORTMAPI();
        orbisAPI.CheckCompatibility((uint)eCompatibleVersion.BuildVersion);

        foreach (ITarget tgt in (Array)orbisAPI.Targets)
        {
          if (tgt.PowerStatus == ePowerStatus.POWER_STATUS_ON)
          {
            string name = (tgt.Name != "") ? tgt.Name : tgt.HostName;
            AddTarget(ETargetType.Orbis, name, tgt.HostName, StatoscopeCryEnginePort);
          }
        }
      }
      catch (Exception ex)
      {
        Console.WriteLine(ex);
      }
    }

		private void FindMatchingTarget(string ipAddress, int port)
		{
			foreach (ListViewItem lvi in targetListView.Items)
			{
				TargetItem item = lvi.Tag as TargetItem;
				if (item.IpAddress == ipAddress && item.Port == port)
				{
					lvi.Selected = true;
					return;
				}
			}

			// None found
			if (targetListView.SelectedItems.Count > 0)
				targetListView.SelectedItems[0].Selected = false;
		}

		private void targetListView_SelectedIndexChanged(object sender, EventArgs e)
		{
			if (!suppressChangeEvents)
			{
				suppressChangeEvents = true;
				if (targetListView.SelectedItems.Count > 0)
				{
					TargetItem selectedItem = targetListView.SelectedItems[0].Tag as TargetItem;
					textBoxIpAddress.Text = selectedItem.IpAddress;
					textBoxPort.Text = selectedItem.Port.ToString();
				}
				suppressChangeEvents = false;
			}
		}

		private void textBoxIpAddress_TextChanged(object sender, EventArgs e)
		{
			if (!suppressChangeEvents)
			{
				suppressChangeEvents = true;
				FindMatchingTarget(textBoxIpAddress.Text, Port);
				suppressChangeEvents = false;
			}
		}

		private void textBoxPort_TextChanged(object sender, EventArgs e)
		{
			if (!suppressChangeEvents)
			{
				suppressChangeEvents = true;
				FindMatchingTarget(textBoxIpAddress.Text, Port);
				suppressChangeEvents = false;
			}
		}

		private void refreshButton_Click(object sender, EventArgs e)
		{
			targetListView.Items.Clear();
			PopulateTargets();
		}

		private void targetListView_ItemActivate(object sender, EventArgs e)
		{
			this.buttonConnect.PerformClick();
		}
	}
}

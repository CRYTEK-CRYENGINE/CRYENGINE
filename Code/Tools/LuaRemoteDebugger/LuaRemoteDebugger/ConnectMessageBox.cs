// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using LuaRemoteDebugger.Properties;
using System.Runtime.InteropServices;
using System.IO;

namespace LuaRemoteDebugger
{
	public partial class ConnectMessageBox : Form
	{
		private class TargetItem
		{
			public string Name { get; set; }
			public string IpAddress { get; set; }
			public int Port { get; set; }
			public TargetItem(string name, string ipAddress, int port) { Name = name; IpAddress = ipAddress; Port = port; }

			public override string ToString()
			{
				return Name;
			}
		}

		MainForm parent;
		bool suppressChangeEvents = false;	// Used to prevent feedback loop
		static ConnectMessageBox currentConnectMessageBox;

		public string IpAddress { get { return textBoxIpAddress.Text; } }
		public int Port
		{
			get
			{
				int result = CryNotificationNetworkClient.NN_PORT;
				int.TryParse(textBoxPort.Text, out result);
				return result;
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

		public ConnectMessageBox(MainForm parent)
		{
			InitializeComponent();

			this.parent = parent;
			string currentIp = Settings.Default.IpAddress;
			int currentPort = Settings.Default.Port;
			textBoxIpAddress.Text = currentIp;
			textBoxPort.Text = currentPort.ToString();

			PopulateTargets();
			FindMatchingTarget(currentIp, currentPort);
		}

		private void buttonConnect_Click(object sender, EventArgs e)
		{
			Settings.Default.IpAddress = textBoxIpAddress.Text;
			Settings.Default.Port = Port;
		}

		private void PopulateTargets()
		{
			// Add local host
			comboBoxTarget.Items.Add(new TargetItem("PC (Game)", "127.0.0.1", CryNotificationNetworkClient.NN_PORT));
			comboBoxTarget.Items.Add(new TargetItem("PC (Editor)", "127.0.0.1", CryNotificationNetworkClient.NN_PORT_EDITOR));

			/* No more support for Xenon and PS3, let's not look for them anymore
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
					parent.LogMessage("Failed to get Xbox IP address: {0}", ex.Message);
					success = false;
				}
				if (success)
				{
					byte p1 = (byte)(xboxAddress >> 24 & 0xFF);
					byte p2 = (byte)(xboxAddress >> 16 & 0xFF);
					byte p3 = (byte)(xboxAddress >> 8 & 0xFF);
					byte p4 = (byte)(xboxAddress >> 0 & 0xFF);
					string xboxIp = string.Format("{0}.{1}.{2}.{3}", p1, p2, p3, p4);

					comboBoxTarget.Items.Add(new TargetItem(string.Format("[Xbox 360] {0}", xboxName), xboxIp, CryNotificationNetworkClient.NN_PORT));
				}

				// Restore the default DLL search order
				SetDllDirectory(null);
			}
			else
			{
				parent.LogMessage("Failed to find Xbox XDK");
			}

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
				parent.LogMessage("Failed to load PS3 Target Manager API");
			}
			*/
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
				name += " (connected)";
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
				currentConnectMessageBox.comboBoxTarget.Items.Add(new TargetItem(string.Format("[PS3] {0}", name), ipAddress, CryNotificationNetworkClient.NN_PORT));
			}
			return 0;
		}

		private void FindMatchingTarget(string ipAddress, int port)
		{
			foreach (TargetItem item in comboBoxTarget.Items)
			{
				if (item.IpAddress == ipAddress && item.Port == port)
				{
					comboBoxTarget.SelectedItem = item;
					return;
				}
			}
			// None found
			comboBoxTarget.SelectedItem = null;
		}

		private void comboBoxTarget_SelectedIndexChanged(object sender, EventArgs e)
		{
			if (!suppressChangeEvents)
			{
				suppressChangeEvents = true;
				TargetItem selectedItem = comboBoxTarget.SelectedItem as TargetItem;
				if (selectedItem != null)
				{
					textBoxIpAddress.Text = selectedItem.IpAddress;
					textBoxPort.Text = selectedItem.Port.ToString();
				}
				else
				{
					textBoxIpAddress.Text = string.Empty;
					textBoxPort.Text = CryNotificationNetworkClient.NN_PORT.ToString();
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
	}
}

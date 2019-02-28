// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using System.IO;

namespace LuaRemoteDebugger
{
	// Functions and structs defined in ps3tmapi.h
	public static class Ps3TmApi
	{
		#region Kernel32Functions

		[DllImport("kernel32.dll")]
		private static extern IntPtr LoadLibrary(string dllFilePath);

		[DllImport("kernel32.dll")]
		private static extern IntPtr GetProcAddress(IntPtr hModule, String procname);

		[DllImport("kernel32.dll")]
		private static extern Int32 GetLastError();

		#endregion

		public const int SN_TI_TARGETID = 0x00000001;
		public const int SN_TI_NAME = 0x00000002;
		public const int SN_TI_INFO = 0x00000004;
		public const int SN_TI_HOMEDIR = 0x00000008;
		public const int SN_TI_FILESERVEDIR = 0x00000010;
		public const int SN_TI_BOOT = 0x00000020;

		[StructLayout(LayoutKind.Sequential)]
		///	@brief		This structure contains the parameters that define a target. (see SNPS3GetTargetInfo() and SNPS3SetTargetInfo())
		public struct SNPS3TargetInfo
		{
			/// Flags indicating what values returned/to be set (see defines SN_TI_TARGETID to SN_TI_BOOT)
			public UInt32 nFlags;
			/// Handle to target
			public Int32 hTarget;
			/// Target name
			public IntPtr pszName;
			/// Target type
			public IntPtr pszType;
			/// Target information
			public IntPtr pszInfo;
			/// Home directory of target
			public IntPtr pszHomeDir;
			/// File serving directory of target
			public IntPtr pszFSDir;
			/// Boot flags for target
			public UInt64 boot;
		}

		[StructLayout(LayoutKind.Sequential)]
		///	@brief  This structure holds information about the game port.		 
		public struct SNPS3GamePortIPAddressData
		{
			public UInt32 uReturnValue;
			public UInt32 uIPAddress;
			public UInt32 uSubnetMask;
			public UInt32 uBroadcastAddress;
		}

		[StructLayout(LayoutKind.Sequential)]
		public struct SNPS3PROCESSINFO_HDR
		{
			/// Process status (see \link PS3tmapi.h::ESNPS3PROCESSSTATUS ESNPS3PROCESSSTATUS \endlink).
			public UInt32 uStatus;			
			/// Number of PPU threads.
			public UInt32 uNumPPUThreads;		
			/// Number of SPU threads.
			public UInt32 uNumSPUThreads;		
			/// Parent process ID.
			public UInt32 uParentProcessID;	
			/// Maximum memory size for process.
			public UInt64 uMaxMemorySize;		
			/// Path to the loaded elf
			[MarshalAs(UnmanagedType.ByValTStr, SizeConst = 512)]
			public string szPath;
		};

		[StructLayout(LayoutKind.Sequential)]
		public struct SNPS3PROCESSINFO
		{
			public SNPS3PROCESSINFO_HDR Hdr;
			[MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)]
			public UInt64[] ThreadIDs;
		}

		public enum ECONNECTSTATUS : int
		{
			/// Connected State
			CS_CONNECTED, 
			/// Attempting to connect.     
			CS_CONNECTING,
			/// Not connected, available for use.
			CS_NOT_CONNECTED,
			/// In use by another user.
			CS_IN_USE,
			/// Unavailable, could be offline.
			CS_UNAVAILABLE    
		};

		public delegate int TMAPI_EnumTargetsExCallback(int target, IntPtr userData);

		public delegate Int32 SNPS3InitTargetCommsDelegate();
		public delegate Int32 SNPS3CloseTargetCommsDelegate();
		public delegate Int32 SNPS3EnumerateTargetsExDelegate(TMAPI_EnumTargetsExCallback callback, IntPtr userData);
		public delegate Int32 SNPS3GetTargetInfoDelegate(ref SNPS3TargetInfo targetInfo);
		public delegate Int32 SNPS3GetGamePortIPAddrDataDelegate(int target, IntPtr deviceName, out SNPS3GamePortIPAddressData data);
		public delegate Int32 SNPS3GetConnectStatusDelegate(int target, out ECONNECTSTATUS connectionStatus, out IntPtr ppszUsage);
		public delegate Int32 SNPS3ConnectDelegate(int target, IntPtr application);
		public delegate Int32 SNPS3ProcessListDelegate(int target, ref int count, int[] buffer);
		public delegate Int32 SNPS3ProcessInfoDelegate(int target, int processId, ref int bufferSize, out SNPS3PROCESSINFO processInfo);

		public static SNPS3InitTargetCommsDelegate SNPS3InitTargetComms;
		public static SNPS3CloseTargetCommsDelegate SNPS3CloseTargetComms;
		public static SNPS3EnumerateTargetsExDelegate SNPS3EnumerateTargetsEx;
		public static SNPS3GetTargetInfoDelegate SNPS3GetTargetInfo;
		public static SNPS3GetGamePortIPAddrDataDelegate SNPS3GetGamePortIPAddrData;
		public static SNPS3GetConnectStatusDelegate SNPS3GetConnectStatus;
		public static SNPS3ConnectDelegate SNPS3Connect;
		public static SNPS3ProcessListDelegate SNPS3ProcessList;
		public static SNPS3ProcessInfoDelegate SNPS3ProcessInfo;

		private static bool available;
		private static string searchIpAddress;
		private static string foundSelfPath;

		public static bool Available { get { return available; } }

		static Ps3TmApi()
		{
			available = Initialize();
		}

		private static void GetDelegate<T>(IntPtr dllHandle, string functionName, out T result) where T : class
		{
			result = Marshal.GetDelegateForFunctionPointer(GetProcAddress(dllHandle, functionName), typeof(T)) as T;
		}

		private static bool Initialize()
		{
			IntPtr dllHandle;
			if (IntPtr.Size == 4)
			{
				dllHandle = LoadLibrary("ps3tmapi.dll");
			}
			else
			{
				dllHandle = LoadLibrary("ps3tmapix64.dll");
			}

			if (dllHandle != null)
			{
				try
				{
					GetDelegate(dllHandle, "SNPS3InitTargetComms", out SNPS3InitTargetComms);
					GetDelegate(dllHandle, "SNPS3InitTargetComms", out SNPS3InitTargetComms);
					GetDelegate(dllHandle, "SNPS3CloseTargetComms", out SNPS3CloseTargetComms);
					GetDelegate(dllHandle, "SNPS3EnumerateTargetsEx", out SNPS3EnumerateTargetsEx);
					GetDelegate(dllHandle, "SNPS3GetTargetInfo", out SNPS3GetTargetInfo);
					GetDelegate(dllHandle, "SNPS3GetGamePortIPAddrData", out SNPS3GetGamePortIPAddrData);
					GetDelegate(dllHandle, "SNPS3GetConnectStatus", out SNPS3GetConnectStatus);
					GetDelegate(dllHandle, "SNPS3Connect", out SNPS3Connect);
					GetDelegate(dllHandle, "SNPS3ProcessList", out SNPS3ProcessList);
					GetDelegate(dllHandle, "SNPS3ProcessInfo", out SNPS3ProcessInfo);
					return true;
				}
				catch (System.Exception)
				{
					return false;
				}
			}
			return false;
		}

		public static string FindTargetSelf(string gameIpAddress)
		{
			int result = SNPS3InitTargetComms();

			searchIpAddress = gameIpAddress;
			foundSelfPath = null;
			SNPS3EnumerateTargetsEx(FindTargetSelfCallback, IntPtr.Zero);
			searchIpAddress = null;

			result = SNPS3CloseTargetComms();

			string selfPath = foundSelfPath;
			foundSelfPath = null;
			if (!string.IsNullOrEmpty(selfPath) && File.Exists(selfPath))
			{
				return selfPath;
			}
			return null;
		}

		private static int FindTargetSelfCallback(int target, IntPtr userData)
		{
			Ps3TmApi.SNPS3GamePortIPAddressData gameIpAddress;
			int result = Ps3TmApi.SNPS3GetGamePortIPAddrData(target, IntPtr.Zero, out gameIpAddress);
			if (result == 0 && gameIpAddress.uReturnValue == 0)
			{
				byte p1 = (byte)(gameIpAddress.uIPAddress >> 24 & 0xFF);
				byte p2 = (byte)(gameIpAddress.uIPAddress >> 16 & 0xFF);
				byte p3 = (byte)(gameIpAddress.uIPAddress >> 8 & 0xFF);
				byte p4 = (byte)(gameIpAddress.uIPAddress >> 0 & 0xFF);
				string ipAddress = string.Format("{0}.{1}.{2}.{3}", p1, p2, p3, p4);
				if (ipAddress == searchIpAddress)
				{
					// Found the target we are looking for, now get the self path
					int[] processIds = new int[64];
					int processesCount = processIds.Length;
					result = SNPS3ProcessList(target, ref processesCount, processIds);
					// Should only be 1 process running, let's assume it is the first once
					if (processesCount > 0)
					{
						SNPS3PROCESSINFO processInfo = new SNPS3PROCESSINFO();
						int processInfoSize = Marshal.SizeOf(processInfo);
						result = SNPS3ProcessInfo(target, processIds[0], ref processInfoSize, out processInfo);
						if (result == 0)
						{
							string prefix = "/app_home/";
							if (processInfo.Hdr.szPath.StartsWith(prefix))
							{
								foundSelfPath = processInfo.Hdr.szPath.Substring(prefix.Length);
								return 1;		// Return 1 to stop enumeration
							}
						}
					}
				}
			}
			return 0;
		}
	}
}

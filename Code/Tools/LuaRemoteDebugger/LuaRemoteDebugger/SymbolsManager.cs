// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using Dia2Lib;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Text.RegularExpressions;

namespace LuaRemoteDebugger
{
	public abstract class SymbolsManager
	{
		Dictionary<UInt32, CallStackItem> symbolCache = new Dictionary<UInt32, CallStackItem>();

		public List<CallStackItem> GetSymbolNamesFromAddresses(List<UInt32> addresses)
		{
			// Get a list of addresses that need to be resolved
			List<UInt32> uncachedAddresses = addresses.FindAll(x => !symbolCache.ContainsKey(x));
			if (uncachedAddresses.Count > 0)
			{
				// Resolve uncached addresses
				List<CallStackItem> items = new List<CallStackItem>(GetSymbolNamesFromAddressesInternal(uncachedAddresses));
				Debug.Assert(items.Count == uncachedAddresses.Count);
				for (int i = 0; i < items.Count; ++i)
				{
					symbolCache[uncachedAddresses[i]] = items[i];
				}
			}
			// Return results
			List<CallStackItem> results = new List<CallStackItem>();
			foreach (UInt32 address in addresses)
			{
				results.Add(symbolCache[address]);
			}
			return results;
		}

		protected abstract List<CallStackItem> GetSymbolNamesFromAddressesInternal(List<UInt32> addresses);
	}

	public class XboxSymbolsManager : SymbolsManager
	{
		private List<XboxSymbolResolver> symbolResolvers = new List<XboxSymbolResolver>();

		public void AddModuleInformation(string path, uint baseAddress, uint size, Guid guid, uint age)
		{
			try
			{
				IDiaSession diaSession;
				DiaSourceClass diaSource = new DiaSourceClass();
				diaSource.loadAndValidateDataFromPdb(path, ref guid, 0, age);
				diaSource.openSession(out diaSession);
				diaSession.loadAddress = baseAddress;

				XboxSymbolResolver symbolResolver = new XboxSymbolResolver(diaSession, size);
				symbolResolvers.Add(symbolResolver);

				Marshal.ReleaseComObject(diaSource);
			}
			catch (System.Runtime.InteropServices.COMException ex)
			{
				switch ((UInt32)ex.ErrorCode)
				{
					// look in dia.h for error codes
					case 0x806D0005:
						Console.WriteLine("Couldn't find pdb: " + path);
						break;
					case 0x806D0006:
						Console.WriteLine("Signature invalid for: " + path);
						break;
					default:
						Console.WriteLine("Unknown pdb load error for: " + path + ", errorcode: " + ex.ErrorCode);
						break;
				}
			}
		}

		protected override List<CallStackItem> GetSymbolNamesFromAddressesInternal(List<UInt32> addresses)
		{
			List<CallStackItem> items = new List<CallStackItem>();
			foreach (UInt32 address in addresses)
			{
				bool found = false;
				foreach (XboxSymbolResolver symbolResolver in symbolResolvers)
				{
					if (symbolResolver.AddressIsInRange(address))
					{
						found = true;
						items.Add(symbolResolver.GetSymbolNameFromAddress(address));
					}
				}
				if (!found)
				{
					CallStackItem result = new CallStackItem();
					result.Description = "<symbol 0x" + Convert.ToString(address, 16) + " not found>";
					items.Add(result);
				}
			}
			return items;
		}
	}

	public class XboxSymbolResolver : IDisposable
	{
		IDiaSession session;
		UInt32 size;

		public XboxSymbolResolver(IDiaSession session, UInt32 size)
		{
			this.session = session;
			this.size = size;
		}

		~XboxSymbolResolver()
		{
			Dispose(false);
		}

		public bool AddressIsInRange(UInt32 address)
		{
			return (session.loadAddress <= address) && (address < session.loadAddress + size);
		}

		public CallStackItem GetSymbolNameFromAddress(UInt32 address)
		{
			CallStackItem result = new CallStackItem();
			if (AddressIsInRange(address))
			{
				IDiaSymbol diaSymbol;
				session.findSymbolByVA(address, SymTagEnum.SymTagFunction, out diaSymbol);

				result.Description = diaSymbol.name + "()";

				IDiaEnumLineNumbers lineNumbers;
				session.findLinesByVA(address, 4, out lineNumbers);

				IDiaLineNumber lineNum;
				uint celt;
				lineNumbers.Next(1, out lineNum, out celt);

				if (celt == 1)
				{
					string baseFilename = lineNum.sourceFile.fileName.Substring(lineNum.sourceFile.fileName.LastIndexOf('\\') + 1);
					result.Source = baseFilename;
					result.Line = (int)lineNum.lineNumber;
				}
			}
			else
			{
				result.Description = "<address out of range>";
			}
			return result;
		}

    #region IDisposable Members

    public void Dispose()
    {
			Dispose(true);
			GC.SuppressFinalize(this);
    }

		protected virtual void Dispose(bool disposing)
		{
			if (disposing)
			{
				// free the state of any contained objects
				// we don't contain any other objects!
			}
			// free my own state
			if (session != null)
			{
				Marshal.ReleaseComObject(session);
				session = null;
			}
		}

    #endregion
	}

	public class Ps3SymbolsManager : SymbolsManager
	{
		string ps3BinPath;
		string selfPath;

		public Ps3SymbolsManager(string ps3BinPath, string selfPath)
		{
			this.ps3BinPath = ps3BinPath;
			this.selfPath = selfPath;
		}

		protected override List<CallStackItem> GetSymbolNamesFromAddressesInternal(List<UInt32> addresses)
		{
			string arguments = selfPath + " --addr2line=";
			for (int i = 0; i < addresses.Count; ++i)
			{
				if (i > 0)
					arguments += ",";
				arguments += addresses[i].ToString();
			}
			
			// Start the child process.
			Process p = new Process();
			p.StartInfo.UseShellExecute = false;
			p.StartInfo.CreateNoWindow = true;
			p.StartInfo.RedirectStandardOutput = true;
			p.StartInfo.FileName = ps3BinPath;
			p.StartInfo.Arguments = arguments;
			p.Start();
			string output = p.StandardOutput.ReadToEnd();
			p.WaitForExit();

			// output looks something like this:
			// Address:       0x010C8FF4
			// Directory:     E:/perforce/depot/dev/c3/Code/CryEngine/CryScriptSystem/LuaRemoteDebug
			// File Name:     LuaRemoteDebug.cpp
			// Line Number:   571
			// Symbol:        CLuaRemoteDebug::SendLuaState(lua_Debug*)
			// Address:       0x010CA180
			// Directory:     E:/perforce/depot/dev/c3/Code/CryEngine/CryScriptSystem/LuaRemoteDebug
			// File Name:     LuaRemoteDebug.cpp
			// Line Number:   315
			// Symbol:        CLuaRemoteDebug::OnNotificationNetworkReceive(void const*, unsigned int)
			// etc...

			string[] lines = output.Split('\n');

			List<CallStackItem> items = new List<CallStackItem>();

			CallStackItem item = new CallStackItem();
			bool foundAddress = false;
			foreach (string line in lines)
			{
				if (line.StartsWith("Address:"))
				{
					// The addresses should be returned in the order we requested them
					if (foundAddress)
					{
						items.Add(item);
					}
					foundAddress = true;
					item = new CallStackItem();
				}
				else if (line.StartsWith("File Name:"))
				{
					item.Source = line.Substring(line.IndexOf(':') + 1).Trim();
				}
				else if (line.StartsWith("Line Number:"))
				{
					int.TryParse(line.Substring(line.IndexOf(':') + 1).Trim(), out item.Line);
				}
				else if (line.StartsWith("Symbol:"))
				{
					item.Description = line.Substring(line.IndexOf(':') + 1).Trim();
				}
			}
			if (foundAddress)
			{
				items.Add(item);
			}

			if (items.Count != addresses.Count)
			{
				// Something went wrong! Just return not found symbols
				items.Clear();
				foreach (UInt32 address in addresses)
				{
					item = new CallStackItem();
					item.Description = "<symbol 0x" + Convert.ToString(address, 16) + " not found>";
					items.Add(item);
				}
			}

			return items;
		}
	}
}

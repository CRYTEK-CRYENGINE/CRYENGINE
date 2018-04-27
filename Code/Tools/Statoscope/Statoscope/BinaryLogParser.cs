// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using Dia2Lib;

namespace Statoscope
{
  class BinaryLogParser
  {
    List<PerfStatLogLineDescriptor> m_logLineDescriptors = new List<PerfStatLogLineDescriptor>();
    List<SymbolResolver> m_symbolResolvers = new List<SymbolResolver>();
    Dictionary<UInt64, string> m_symbolCache = new Dictionary<UInt64, string>();

    IBinaryLogDataStream m_logDataStream = null;
    LogData m_logData = null;

    Dictionary<uint, string> m_stringPool = new Dictionary<uint, string>();

    bool m_bUsingStringPool = false;
    uint m_logVersion = 0;

    string m_logFilePath;
    
    //**** Must match statoscope.h in the engine
    const UInt32 STATOSCOPE_BINARY_VERSION = 2;

    public BinaryLogParser(IBinaryLogDataStream logDataStream, LogData logData, string filePath)
    {
      m_logDataStream = logDataStream;
      m_logData = logData;

      if (filePath.Length > 0)
      {
        m_logFilePath = filePath.Substring(0, filePath.LastIndexOf('\\') + 1);
      }
    }

    void ReadModules()
    {
      int nLoadedModules = m_logDataStream.ReadInt32();

      //while (!(line = m_logDataStream.ReadLine()).Equals("<\\modules>"))
      for (int i = 0; i < nLoadedModules; i++)
      {
        string path = ReadString();
        ulong baseAddress = m_logDataStream.ReadUInt32();
        uint size = m_logDataStream.ReadUInt32();
				string guidStr = ReadString();
        Guid guid = new Guid(guidStr);
        uint age = m_logDataStream.ReadUInt32();
        bool bTryLocal = false;

        // Try pdb path and local dir
        do
        {
          if (bTryLocal)
          {
            path = m_logFilePath + path.Substring(path.LastIndexOf('\\') + 1);
          }

          try
          {
            IDiaSession diaSession;
            DiaSourceClass diaSource = new DiaSourceClass();
            diaSource.loadAndValidateDataFromPdb(path, ref guid, 0, age);
            diaSource.openSession(out diaSession);
            diaSession.loadAddress = baseAddress;

            SymbolResolver symbolResolver = new SymbolResolver(diaSession, size);
            m_symbolResolvers.Add(symbolResolver);

            Marshal.ReleaseComObject(diaSource);
            bTryLocal = false;
          }
          catch (System.Runtime.InteropServices.COMException ex)
          {
            switch ((UInt32)ex.ErrorCode)
            {
              // look in dia.h for error codes
              case 0x806D0005:
                bTryLocal = bTryLocal ? false : true;
                Console.WriteLine("Couldn't find pdb: " + path);
                break;
              case 0x806D0006:
                bTryLocal = bTryLocal ? false : true;
                Console.WriteLine("Signature invalid for: " + path);
                break;
              default:
                bTryLocal = bTryLocal ? false : true;
                Console.WriteLine("Unknown pdb load error for: " + path + ", errorcode: " + ex.ErrorCode);
                break;
            }
          }
        } while (bTryLocal);
      }
    }

    public string GetSymbolNameFromAddress(UInt64 address)
    {
      if (m_symbolCache.ContainsKey(address))
      {
        return m_symbolCache[address];
      }

      string symbolName = null;

      foreach (SymbolResolver symbolResolver in m_symbolResolvers)
      {
        if (symbolResolver.AddressIsInRange(address))
        {
          symbolName = symbolResolver.GetSymbolNameFromAddress(address);
          break;
        }
      }

      if (symbolName == null)
      {
        symbolName = "<symbol 0x" + Convert.ToString((long)address, 16) + " not found>";
      }

      m_symbolCache[address] = symbolName;
      return symbolName;
    }

    EFrameElementType ReadFrameElementType()
    {
      if (m_logVersion >= 2)
      {
        return (EFrameElementType)m_logDataStream.ReadUInt32();
      }
      else
      {
        UInt32 oldType = m_logDataStream.ReadUInt32();
        switch (oldType)
        {
          case 0: return EFrameElementType.None;
          case 1: return EFrameElementType.Float;
          case 2: return EFrameElementType.Int;
          case 4: return EFrameElementType.String;
          case 8: return EFrameElementType.B64Texture;
          default: throw new ApplicationException("Unknown old frame element type");
        }
      }
    }

    void ReadFormat()
    {
      uint nDataGroups = m_logDataStream.ReadUInt32();
      
      m_logLineDescriptors.Clear();

      for(uint i=0; i<nDataGroups; i++)
      {
        PerfStatLogLineDescriptor lld = new PerfStatLogLineDescriptor();

        string path = ReadString();

				lld.m_path = path;

				// Add new format element for each "$" placeholder
				int numOfPlaceholders = path.Split('$').Length - 1;
				for (int j = 0; j < numOfPlaceholders; j++)
				{
					lld.m_formatElements.Add(new PerfStatFrameElement("string", "path", "units"));
				}

				uint numElems = m_logDataStream.ReadUInt32();

				for (uint j = 0; j < numElems; j++)
				{
          EFrameElementType type = ReadFrameElementType();
					string str = ReadString();
					string[] elements = str.Split(new char[] { ' ' });
					string name = elements[0];
					string units = elements.Length > 1 ? elements[1] : "";

          PerfStatFrameElement fe = new PerfStatFrameElement(type, name, units);
          lld.m_formatElements.Add(fe);
        }

        m_logLineDescriptors.Add(lld);
      }
    }

    string ReadString_Raw()
    {
      return m_logDataStream.ReadString();
    }

    string ReadString()
    {
      string s;
      if (m_bUsingStringPool)
      {
        UInt32 crc = m_logDataStream.ReadUInt32();
        
        //if the key is unknown, then the raw string will be output directly after
        if (m_stringPool.ContainsKey(crc))
        {
          s = m_stringPool[crc];
        }
        else
        {
          s = ReadString_Raw();
          m_stringPool[crc] = s;
        }
      }
      else
      {
        s = ReadString_Raw();
      }
			return s;
		}

		public void ProcessDataStream()
		{
			CultureInfo decimalCulture = new CultureInfo("en-US");
			Regex callstackAddressesRegex = new Regex(@"0x[0-9|a-f|A-F]*");

      IIntervalSink[] ivSinks = new IIntervalSink[] {
        new StreamingIntervalSink(m_logData),
        new StreamingObjectSink(m_logData, "/Objects", 111),
        new StreamingTextureSink(m_logData, "/Textures", 116),
      };
      EventReader eventReader = new EventReader(ivSinks);

			try
			{
				//Endian 
				byte[] endianByte = m_logDataStream.ReadBytes(1);
				EEndian endian = (EEndian)endianByte[0];
				m_logDataStream.SetEndian(endian);

				//Version
				UInt32 version = m_logDataStream.ReadUInt32();

				if (version > STATOSCOPE_BINARY_VERSION)
				{
					throw new Exception("Binary file version mismatch");
				}

        m_logVersion = version;

				//Using string pool?
				m_bUsingStringPool = m_logDataStream.ReadBool();

				List<KeyValuePair<string, EItemType>> newmds = new List<KeyValuePair<string, EItemType>>();
				FrameRecordPathCollection paths = new FrameRecordPathCollection();

				while (!m_logDataStream.IsEndOfStream)
				{
					// header present
					if (m_logDataStream.ReadBool())
					{
						// modules present
						if (m_logDataStream.ReadBool())
						{
							ReadModules();
						}

						ReadFormat();
					}

					//FRAME RECORD BEGIN
					FrameRecordValues values = new FrameRecordValues(paths);
					float frameTimeInS = 0.0f;
					MemoryStream screenshotImage = null;
					List<UserMarker> userMarkers = new List<UserMarker>();
					List<Callstack> callstacks = new List<Callstack>();

					//check for fps or screen shot
					float frameTimeValue = m_logDataStream.ReadFloat();

					if (frameTimeValue > 0)
					{
						frameTimeInS = (float)Convert.ToDouble(frameTimeValue, decimalCulture);
					}

          EFrameElementType type = ReadFrameElementType();

					if (type == EFrameElementType.B64Texture)
					{
						//process texture
						int size = m_logDataStream.ReadInt32();
						byte[] screenShotBytes = m_logDataStream.ReadBytes(size);
						screenshotImage = ImageProcessor.CreateImageStreamFromScreenshotBytes(screenShotBytes);
					}

					//On to specific data groups
					foreach (PerfStatLogLineDescriptor lld in m_logLineDescriptors)
					{
						//number of stats, so records have a variable number of entries, like frame profiler
						int nStats = m_logDataStream.ReadInt32();

						for (int i = 0; i < nStats; i++)
						{
							string path = lld.m_path;

							foreach (PerfStatFrameElement fe in lld.m_formatElements)
							{
								if (fe.m_type == EFrameElementType.String && fe.m_name.Equals("path"))
								{
									// Replace first occurrence of "$" in path with its placeholder value
									string valueString = ReadString();
									StringBuilder sbModifiedString = new StringBuilder(path);
									sbModifiedString.Replace("$", valueString, path.IndexOf("$"), 1);
									path = sbModifiedString.ToString();
								}
								else
								{
									switch (fe.m_type)
									{
										case EFrameElementType.Float:
											{
												float value = m_logDataStream.ReadFloat();
                        string itemname = path + fe.m_name;
												values[itemname] += value;	// += as there may be duplicate records per frame
												newmds.Add(new KeyValuePair<string, EItemType>(itemname, EItemType.Float));
											}
											break;

										case EFrameElementType.Int:
											{
												int value = m_logDataStream.ReadInt32();
                        string itemname = path + fe.m_name;
												values[itemname] += (float)value;	// += as there may be duplicate records per frame
												newmds.Add(new KeyValuePair<string, EItemType>(itemname, EItemType.Int));
											}
											break;

										case EFrameElementType.B64Texture:
											throw new Exception("Screen shot in frame profiler not supported");
										//break;

										case EFrameElementType.String:
											string userMarkersInitialPath = "/UserMarkers/";
											string callstacksInitialPath = "/Callstacks/";

											string valueString = ReadString();

											if (path.StartsWith(userMarkersInitialPath))
											{
												string pathNoEndSlash = path.TrimEnd(new char[] { '/' });	// old format compatibility
												UserMarker userMarker = new UserMarker(pathNoEndSlash, valueString);
												userMarkers.Add(userMarker);
											}
											else if (path.StartsWith(callstacksInitialPath))
											{
												Callstack callstack = new Callstack(path.Substring(callstacksInitialPath.Length));
												MatchCollection addressesMatches = callstackAddressesRegex.Matches(valueString);

												foreach (Match addressMatch in addressesMatches)
												{
													UInt64 address = Convert.ToUInt64(addressMatch.Value, 16);
													callstack.m_functions.Add(GetSymbolNameFromAddress(address));
												}

												callstack.m_functions.Reverse();

												callstacks.Add(callstack);
											}
											break;
									}
								}
							}
						}
					}

          if (version > 1)
            eventReader.ReadEvents(m_logDataStream);

					UInt32 magic = m_logDataStream.ReadUInt32();

					if (magic != 0xdeadbeef)
					{
						throw new Exception("Frame Record is corrupt");
					}

          for (int si = 0, sc = ivSinks.Length; si != sc; ++si)
            ivSinks[si].OnFrameRecord(values);

					FrameRecord fr = new FrameRecord(m_logData.FrameRecords.Count, frameTimeInS, screenshotImage, m_logData.CreateNewFrameRecordValues(values), userMarkers, callstacks);
					m_logData.AddFrameRecord(fr, newmds);
					m_logDataStream.FlushWriteStream();

					newmds.Clear();
				}
			}
			catch (Exception ex)
			{
				Console.WriteLine(ex);
			}

      foreach (var resolver in m_symbolResolvers)
      {
        resolver.Dispose();
      }

      m_symbolResolvers.Clear();

			m_logDataStream.CloseWriteStream();
    }
  }
}

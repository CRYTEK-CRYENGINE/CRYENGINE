// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using Dia2Lib;

namespace Statoscope
{
	class LogParser
	{
		List<PerfStatLogLineDescriptor> m_logLineDescriptors = new List<PerfStatLogLineDescriptor>();
		List<SymbolResolver> m_symbolResolvers = new List<SymbolResolver>();
		Dictionary<UInt32, string> m_symbolCache = new Dictionary<uint, string>();

		ILogDataStream m_logDataStream = null;
		LogData m_logData = null;

		public LogParser(ILogDataStream logDataStream, LogData logData)
		{
			m_logDataStream = logDataStream;
			m_logData = logData;
		}

		void ReadMetaData()
		{
			string line;

			line = m_logDataStream.ReadLine();

			switch (line)
			{
				case "<format>":
					ReadFormat();
					break;
				case "<modules>":
					ReadModules();
					break;
				default:
					throw new Exception("Unrecognised metadata: " + line);
			}
		}

		void ReadModules()
		{
			string line;
			Regex moduleLineRegex = new Regex(@"'(?<path>.*)' (?<baseAddress>0x[0-9|a-f|A-F]*) (?<size>\d*) (?<guid>[0-9|a-f|A-F]*) (?<age>\d*)");

			while (!(line = m_logDataStream.ReadLine()).Equals("<\\modules>"))
			{
				Match moduleLineMatch = moduleLineRegex.Match(line);

				string path = moduleLineMatch.Groups["path"].Value;
				ulong baseAddress = Convert.ToUInt32(moduleLineMatch.Groups["baseAddress"].Value, 16);
				uint size = Convert.ToUInt32(moduleLineMatch.Groups["size"].Value);
				Guid guid = new Guid(moduleLineMatch.Groups["guid"].Value);
				uint age = Convert.ToUInt32(moduleLineMatch.Groups["age"].Value);

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
		}

		public string GetSymbolNameFromAddress(UInt32 address)
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
				symbolName = "<symbol 0x" + Convert.ToString(address, 16) + " not found>";
			}

			m_symbolCache[address] = symbolName;
			return symbolName;
		}

		void ReadFormat()
		{
			string line;
			Regex formatLineRegex = new Regex(@"\[(?<path>'[\w:_\.\/$]*') (?<elements>(\(\w* \w*[? \w:_\.\/$+]*\) *)*)\]");
			Regex elementsRegex = new Regex(@"\((?<type>float|int|string|b64texture) (?<name>\w*)(?<units>[ \w:_\.\/$+]*)\)");

			m_logLineDescriptors.Clear();

			while (!(line = m_logDataStream.ReadLine()).Equals("<\\format>"))
			{
				Match formatLineMatch = formatLineRegex.Match(line);
				PerfStatLogLineDescriptor lld = new PerfStatLogLineDescriptor();

				string idValue = formatLineMatch.Groups["path"].Value;
				string path = idValue.Substring(1, idValue.Length - 2);

				lld.m_path = path;

				if (path.Contains("$"))
				{
					lld.m_formatElements.Add(new PerfStatFrameElement("string", "path", "units"));
				}

				string elements = formatLineMatch.Groups["elements"].Value;
				MatchCollection elementsMatches = elementsRegex.Matches(elements);

				foreach (Match elementMatch in elementsMatches)
				{
					string type = elementMatch.Groups["type"].Value;
					string name = elementMatch.Groups["name"].Value;
					string units = elementMatch.Groups["units"].Value;

					PerfStatFrameElement fe = new PerfStatFrameElement(type, name, units);
					lld.m_formatElements.Add(fe);
				}

				lld.CreateRegex();

				m_logLineDescriptors.Add(lld);
			}
		}

		public void ProcessDataStream()
		{
			CultureInfo decimalCulture = new CultureInfo("en-US");
			Regex callstackAddressesRegex = new Regex(@"0x[0-9|a-f|A-F]*");

			List<KeyValuePair<string, EItemType>> newmds = new List<KeyValuePair<string, EItemType>>();
			FrameRecordPathCollection paths = new FrameRecordPathCollection();

			while (!m_logDataStream.IsEndOfStream)
			{
				if (m_logDataStream.Peek() == '<')
				{
					ReadMetaData();
					continue;
				}

        if (m_logLineDescriptors.Count == 0)
        {
          m_logDataStream.ReadLine();
          continue;
        }

				FrameRecordValues values = new FrameRecordValues(paths);
				float frameTimeInS = 0.0f;
				MemoryStream screenshotImage = null;
				List<UserMarker> userMarkers = new List<UserMarker>();
				List<Callstack> callstacks = new List<Callstack>();

				foreach (PerfStatLogLineDescriptor lld in m_logLineDescriptors)
				{
					string line = m_logDataStream.ReadLine();

					if (line == null)
					{
						// it looks like the log's broken
						break;
					}

					MatchCollection lineElements = lld.m_regex.Matches(line);

					foreach (Match lineElement in lineElements)
					{
						string frameTimeValue = lineElement.Groups["frameTimeInS"].Value;
						if (frameTimeValue.Length > 0)
						{
							frameTimeInS = (float)Convert.ToDouble(frameTimeValue, decimalCulture);
						}

						string path = lld.m_path;

						foreach (PerfStatFrameElement fe in lld.m_formatElements)
						{
							string valueString = lineElement.Groups[fe.m_name].Value;

							if (fe.m_type == EFrameElementType.String)
							{
								if (!valueString.StartsWith("'") || !valueString.EndsWith("'"))
								{
									throw new Exception("strings need to be surrounded by 's");
								}

								valueString = valueString.Substring(1, valueString.Length - 2);
							}

							if (fe.m_name.Equals("path"))
							{
								path = path.Replace("$", valueString);
							}
							else
							{
								switch (fe.m_type)
								{
									case EFrameElementType.Float:
									case EFrameElementType.Int:
										float value = string.Compare(valueString, "1.#QNAN0") == 0 ? 1.0f : (float)Convert.ToDouble(valueString, decimalCulture);
										string itemname = string.Intern(path + fe.m_name);
										values[itemname] += value;	// += as there may be duplicate records per frame
										newmds.Add(new KeyValuePair<string,EItemType>(itemname, fe.m_type == EFrameElementType.Float ? EItemType.Float : EItemType.Int));
										break;

									case EFrameElementType.B64Texture:
										if (screenshotImage != null)
										{
											throw new Exception("multiple screenshots found for a frame");
										}

										if (valueString.Length > 0)
										{
                      try
                      {
                        byte[] bm = Convert.FromBase64String(valueString);
                        screenshotImage = ImageProcessor.CreateImageStreamFromScreenshotBytes(bm);
                      }
                      catch (System.Exception)
                      {
                      	
                      }
										}

										break;

									case EFrameElementType.String:
										string userMarkersInitialPath = "/UserMarkers/";
										string callstacksInitialPath = "/Callstacks/";

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
												UInt32 address = Convert.ToUInt32(addressMatch.Value, 16);
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

				FrameRecord fr = new FrameRecord(m_logData.FrameRecords.Count, frameTimeInS, screenshotImage, m_logData.CreateNewFrameRecordValues(values), userMarkers, callstacks);
				m_logData.AddFrameRecord(fr, newmds);
			}

			foreach (var resolver in m_symbolResolvers)
			{
				resolver.Dispose();
			}

			m_symbolResolvers.Clear();
		}
	}
}

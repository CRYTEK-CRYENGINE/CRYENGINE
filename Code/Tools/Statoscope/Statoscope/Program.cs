// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.ServiceModel;
using System.Windows.Forms;
using System.Xml;
using Microsoft.Win32;

namespace Statoscope
{
	static class Program
	{
		public static bool IsRunningOnWindows()
		{
			PlatformID pid = Environment.OSVersion.Platform;
			switch (pid) 
			{
			case PlatformID.Win32NT:
			case PlatformID.Win32S:
			case PlatformID.Win32Windows:
			case PlatformID.WinCE:
				return true;
			default:
				return false;
			}
		}

		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main(string[] args)
		{
			Application.CurrentCulture = new CultureInfo("en-US");
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);
			bool success = false;

			InstallProtocolHandler();

			try
			{
				if (args.Length == 1 && !args[0].StartsWith("/"))
				{
					if (WCF.CheckForExistingToolProcess(args[0]))
						Environment.Exit(0);

					success = true;
					RunForm(args[0]);
				}
				else if (args.Length > 0)
				{
					string logfilename = string.Empty;
					string uploadSettings = string.Empty;
					string platform = string.Empty;
					string buildid = string.Empty;
					string metricManifest = string.Empty;
					string buildInfo = string.Empty;
					bool uploadSummary = true;
					string[] groups = null;

					for (int idx = 0; idx < args.Length; idx++)
					{
						if (args[idx].StartsWith("/dumplog"))
						{
							logfilename = args[idx + 1];
						}

						if (args[idx].StartsWith("/statgroups"))
						{
							groups = args[idx + 1].Split(',');
						}

						if (args[idx].StartsWith("/autoupload"))
						{
							uploadSettings = args[idx + 1];
						}

						if (args[idx].StartsWith("/metrics"))
						{
							metricManifest = args[idx + 1];
						}

						if (args[idx].StartsWith("/nosummary"))
						{
							uploadSummary = false;
						}

						if (args[idx].StartsWith("/buildinfo"))
						{
							buildInfo = args[idx + 1];
						}
					}

					if (logfilename != string.Empty)
						success = WriteXMLForLog(logfilename, groups);

					if (uploadSettings != string.Empty && success)
					{
						if (buildInfo != string.Empty)
						{
							StreamReader stream = new StreamReader(buildInfo);
							while (!stream.EndOfStream)
							{
								string line = stream.ReadLine();
								if (line.StartsWith("BuildName="))
								{
									buildid = line.Substring(line.IndexOf('=') + 1);
									string[] tokens = buildid.Split('(')[0].Split('_');
									platform = tokens[tokens.Length - 2];
								}
							}
							stream.Close();

							buildid += " - Auto-testing Statistics - ";
							platform += " - Auto-testing Statistics - ";
						}
						success = UploadConfluenceData(uploadSettings, logfilename, groups, platform, buildid, metricManifest, uploadSummary);
					}
				}
				else
				{
					//If launching graphical front end, assume success here.
					success = true;
					RunForm("");
				}
			}
			catch (System.Exception ex)
			{
				// Use this as a last ditch attempt to catch failures without the crash dialog being displayed
				Console.WriteLine("Unhandled exception thrown! {0}: {1}", ex.Message, ex.StackTrace);
			}

			Environment.Exit(success ? 0 : 1);
		}

		static void RunForm(string logFilename)
		{
			StatoscopeForm ssForm = new StatoscopeForm(logFilename);
			WCF.CreateServiceHost(ssForm);
			Application.Run(ssForm);
		}

		static bool WriteXMLForLog(string logFilename, string[] groups)
		{
			FileLogProcessor logProcessor = new FileLogProcessor(logFilename);
			LogData logData = logProcessor.ProcessLog();

			XmlWriterSettings settings = new XmlWriterSettings();
			settings.Indent = true;
			settings.IndentChars = ("    ");

			char[] delims = { '.' };
			string[] fileNameElements = logFilename.Split(delims);
			if (fileNameElements.Length < 2)
			{
				Console.WriteLine("Input file name not parseable {0} : Canceling XML Write", logFilename);
				return false;
			}

			string outputFile = fileNameElements[0] + ".xml";
			Console.WriteLine("Writing XML output: {0}", outputFile);

			using (XmlWriter writer = XmlWriter.Create(outputFile, settings))
			{
				writer.WriteStartElement("testsuites");
				CultureInfo decimalCulture = new CultureInfo("en-US");

				UserMarkerLocation currentStartMarker = null;
				writer.WriteStartElement("testsuite");
				writer.WriteAttributeString("name", "BasicPerformance");

				foreach (FrameRecord fr in logData.FrameRecords)
				foreach (UserMarkerLocation markerLocation in fr.UserMarkers)
				{
					if (markerLocation.m_name.Split(' ')[0].EndsWith("_start"))
					{
						if (currentStartMarker != null)
						{
							//Warn about unmatched start marker
							Console.WriteLine("Warning: Performance Marker End Not Found For {0} at frame index: {1} : Skipping", currentStartMarker.m_name, currentStartMarker.m_fr.Index);
						}
						currentStartMarker = markerLocation;
						continue;
					}

					if (markerLocation.m_name.EndsWith("_end"))
					{
						if (currentStartMarker == null)
						{
							//Warn and skip if we didn't have a start marker we were looking for
							Console.WriteLine("Warning: Performance Marker End Found With No Matching Start For {0} at frame index: {1} : Skipping", currentStartMarker != null ? currentStartMarker.m_name : "invalid start marker", currentStartMarker != null ? currentStartMarker.m_fr.Index : 0);
							continue;
						}

						string markerNameStart = currentStartMarker.m_name.Replace("_start", "").Split(' ')[0];
						string markerExportName = currentStartMarker.m_name.Replace("_start", "");
						string markerNameEnd = markerLocation.m_name.Replace("_end", "");

						if (markerNameStart != markerNameEnd)
						{
							Console.WriteLine("Warning: Performance Marker End: {0} Does not Match Start Marker {1}: Skipping", markerNameStart, markerNameEnd);
							currentStartMarker = null;
							continue;
						}

						int startIdx = currentStartMarker.m_fr.Index;
						int endIdx = markerLocation.m_fr.Index;
						currentStartMarker = null;

						writer.WriteStartElement("phase");
						writer.WriteAttributeString("name", markerExportName);
						writer.WriteAttributeString("duration", (logData.FrameRecords[endIdx].FrameTimeInS - logData.FrameRecords[startIdx].FrameTimeInS).ToString(decimalCulture));
						{
							float minFrameTime, maxFrameTime, avgFrameTime;
							logData.GetFrameTimeData(startIdx, endIdx, out minFrameTime, out maxFrameTime, out avgFrameTime);

							writer.WriteStartElement("metrics");
							writer.WriteAttributeString("name", "frameTimes");

							writer.WriteStartElement("metric");
							writer.WriteAttributeString("name", "min");
							writer.WriteAttributeString("value", minFrameTime.ToString(decimalCulture));
							writer.WriteEndElement();

							writer.WriteStartElement("metric");
							writer.WriteAttributeString("name", "max");
							writer.WriteAttributeString("value", maxFrameTime.ToString(decimalCulture));
							writer.WriteEndElement();

							writer.WriteStartElement("metric");
							writer.WriteAttributeString("name", "avg");
							writer.WriteAttributeString("value", avgFrameTime.ToString(decimalCulture));
							writer.WriteEndElement();

							writer.WriteEndElement();
						}
						writer.WriteEndElement();
					}
				}
				//End testsuite block
				writer.WriteEndElement();

				if (groups != null)
				{
					currentStartMarker = null;
					writer.WriteStartElement("testsuite");
					writer.WriteAttributeString("name", "GroupStatistics");

					foreach (FrameRecord fr in logData.FrameRecords)
					foreach (UserMarkerLocation markerLocation in fr.UserMarkers)
					{
						if (markerLocation.m_name.Split(' ')[0].EndsWith("_start"))
						{
							if (currentStartMarker != null)
							{
								//Warn about unmatched start marker
								Console.WriteLine("Warning: Performance Marker End Not Found For {0} at frame index: {1} : Skipping", currentStartMarker.m_name, currentStartMarker.m_fr.Index);
							}
							currentStartMarker = markerLocation;
							continue;
						}

						if (markerLocation.m_name.EndsWith("_end"))
						{
							if (currentStartMarker == null)
							{
								//Warn and skip if we didn't have a start marker we were looking for
								Console.WriteLine("Warning: Performance Marker End Found With No Matching Start For {0} at frame index: {1} : Skipping", currentStartMarker != null ? currentStartMarker.m_name : "invalid start marker", currentStartMarker != null ? currentStartMarker.m_fr.Index : 0);
								continue;
							}

							string markerNameStart = currentStartMarker.m_name.Replace("_start", "").Split(' ')[0];
							string markerExportName = currentStartMarker.m_name.Replace("_start", "");
							string markerNameEnd = markerLocation.m_name.Replace("_end", "");

							if (markerNameStart != markerNameEnd)
							{
								Console.WriteLine("Warning: Performance Marker End: {0} Does not Match Start Marker {1}: Skipping", markerNameStart, markerNameEnd);
								currentStartMarker = null;
								continue;
							}

							int startIdx = currentStartMarker.m_fr.Index;
							int endIdx = markerLocation.m_fr.Index;
							currentStartMarker = null;

							writer.WriteStartElement("phase");
							writer.WriteAttributeString("name", markerExportName);
							writer.WriteAttributeString("duration", (logData.FrameRecords[endIdx].FrameTimeInS - logData.FrameRecords[startIdx].FrameTimeInS).ToString(decimalCulture));
							{
								GroupsStats stats = new GroupsStats();
								logData.GetFrameGroupData(startIdx, endIdx, groups, stats);
								foreach (string group in stats.m_groupStats.Keys)
								{
									GroupItemStats groupsStats = stats.GetGroupItemsStats(group);
									foreach (string item in groupsStats.m_groupItemStats.Keys)
									{
										Stats itemStats = groupsStats.GetItemStats(item);
										writer.WriteStartElement("metrics");
										writer.WriteAttributeString("name", item);

										writer.WriteStartElement("metric");
										writer.WriteAttributeString("name", "min");
										writer.WriteAttributeString("value", itemStats.m_min.ToString(decimalCulture));
										writer.WriteEndElement();

										writer.WriteStartElement("metric");
										writer.WriteAttributeString("name", "max");
										writer.WriteAttributeString("value", itemStats.m_max.ToString(decimalCulture));
										writer.WriteEndElement();

										writer.WriteStartElement("metric");
										writer.WriteAttributeString("name", "avg");
										writer.WriteAttributeString("value", itemStats.m_avg.ToString(decimalCulture));
										writer.WriteEndElement();

										writer.WriteEndElement();
									}
								}
							}
							writer.WriteEndElement();
						}
					}
					//End testsuite block
					writer.WriteEndElement();
				}

				//End testsuites block
				writer.WriteEndElement();
				writer.Flush();

				return true;
			}
		}

		static bool UploadConfluenceData(string settingsfile, string logfile, string[] groups, string platform, string buildid, string metrics, bool uploadSummary)
		{
			string username = string.Empty;
			string password = string.Empty;
			string space = string.Empty;
			string parent = string.Empty;
			string pagepostfix = string.Empty;

			try
			{
				StreamReader stream = new StreamReader(settingsfile);
				XmlDocument xmlDoc = new XmlDocument();
				String xmlstring = stream.ReadToEnd();
				stream.Close();
				xmlDoc.LoadXml(xmlstring);
				XmlNode settings = xmlDoc.SelectSingleNode("/settings");

				stream = new StreamReader(metrics);
				xmlDoc = new XmlDocument();
				xmlstring = stream.ReadToEnd();
				stream.Close();
				xmlDoc.LoadXml(xmlstring);
				XmlNodeList metricNodes = xmlDoc.SelectNodes("//metrics");

				foreach (XmlAttribute attrib in settings.Attributes)
				{
					if (attrib.Name == "username")
					{
						username = attrib.Value;
					}

					if (attrib.Name == "password")
					{
						password = attrib.Value;
					}

					if (attrib.Name == "space")
					{
						space = attrib.Value;
					}

					if (attrib.Name == "parent")
					{
						parent = attrib.Value;
					}

					if (attrib.Name == "pageuniqueid")
					{
						pagepostfix = attrib.Value;
					}
				}

				FileLogProcessor logProcessor = new FileLogProcessor(logfile);
				LogData logData = logProcessor.ProcessLog();

				if (platform == string.Empty)
				{
					platform = logData.BuildInfo.PlatformString + " - Auto-testing Statistics - ";
				}

				if (buildid == string.Empty)
				{
					buildid = logData.BuildInfo.BuildNumberString + " - Auto-testing Statistics - ";
				}

				platform += pagepostfix;
				buildid += pagepostfix;

				ConfluenceService confluence = new ConfluenceService(logData);
				if (confluence.TryLogin(true, username, password))
				{
					UploadStatsPages(confluence, logData, groups, space, parent, platform, buildid, metricNodes, uploadSummary, logfile);
				}
			}
			catch (System.Exception ex)
			{
				Console.WriteLine("Error: " + ex.Message + System.Environment.NewLine + "Stack: " + ex.StackTrace + System.Environment.NewLine);
				System.Environment.Exit(2);
			}
			return true;
		}

		static void UploadStatsPages(ConfluenceService confluence, LogData logData, string[] groups, string spaceName, string rootPageName, string platformPageName, string buildPageName, XmlNodeList metrics, bool uploadSummary, string logfile)
		{
			try
			{
				List<ConfluenceSOAP.RemoteSpaceSummary> spaces = confluence.GetSpaces();

				bool spaceExists = false;
				bool rootPageExists = false;
				bool platformPageExists = false;
				bool buildPageExists = false;

				foreach (ConfluenceSOAP.RemoteSpaceSummary space in spaces)
				{
					if (space.key == spaceName)
					{
						spaceExists = true;
						break;
					}
				}

				if (!spaceExists)
				{
					confluence.CreateSpace(spaceName);
				}

				List<ConfluenceSOAP.RemotePageSummary> pages = confluence.GetPages(spaceName);

				foreach (ConfluenceSOAP.RemotePageSummary page in pages)
				{
					if (page.title == rootPageName)
					{
						rootPageExists = true;
					}

					if (page.title == platformPageName)
					{
						platformPageExists = true;
					}

					if (page.title == buildPageName)
					{
						buildPageExists = true;
					}
				}

				if (!rootPageExists)
				{
					confluence.CreatePage(rootPageName, spaceName);
				}

				if (!platformPageExists)
				{
					confluence.CreatePage(platformPageName, rootPageName, spaceName);
				}

				if (!buildPageExists)
				{
					confluence.CreatePage(buildPageName, platformPageName, spaceName);
				}

				string timeDateString = DateTime.Now.ToString("dd/MM/yy hh:mm:ss \\(\\U\\T\\Cz\\)");

				ConfluenceService.PageContentMetaData pageContentMD = new ConfluenceService.PageContentMetaData(timeDateString, logData.Name, logData.BuildInfo.PlatformString, logData.BuildInfo.BuildNumberString, "", confluence.m_username, "automated build system", false);
				ConfluenceService.PageMetaData pageMetaData = new ConfluenceService.PageMetaData(spaceName, buildPageName, platformPageName, pageContentMD);
				confluence.UploadStatsDataPage(pageMetaData, logData, groups, metrics, uploadSummary);

				foreach (FrameRecordRange frr in logData.LevelRanges)
				{
					LogData levelLogData = new LogData(logData, frr);
					pageContentMD.m_levelName = levelLogData.Name;
					ConfluenceService.PageMetaData levelPageMD = new ConfluenceService.PageMetaData(spaceName, levelLogData.Name + " - " + buildPageName, buildPageName, pageContentMD);
					confluence.UploadStatsDataPage(levelPageMD, levelLogData, groups, metrics, true);
				}
			}
			catch (Exception ex)
			{
				Console.WriteLine("Something went wrong! :(\n\n" + ex.ToString() + Environment.NewLine + "Stack trace: " + ex.StackTrace);
				System.Environment.Exit(3);
			}
		}

		static void InstallProtocolHandler()
		{
			try
			{
				RegistryKey statoscopeKey = Registry.ClassesRoot.CreateSubKey("statoscope");
				if (statoscopeKey != null)
				{
					statoscopeKey.SetValue("", "URL:Statoscope Protocol");
					statoscopeKey.SetValue("URL Protocol", "");

					RegistryKey iconKey = statoscopeKey.CreateSubKey("DefaultIcon");
					if (iconKey != null)
					{
                        iconKey.SetValue("", Application.ExecutablePath + ",1");
						iconKey.Close();
					}

					RegistryKey shellKey = statoscopeKey.CreateSubKey("shell");
					if (shellKey != null)
					{
						RegistryKey openKey = shellKey.CreateSubKey("open");
						if (openKey != null)
						{
							RegistryKey commandKey = openKey.CreateSubKey("command");
							if (commandKey != null)
							{
                                commandKey.SetValue("", string.Format("\"{0}\" \"%1\"", Application.ExecutablePath));
								commandKey.Close();
							}
							openKey.Close();
						}
						shellKey.Close();
					}

					statoscopeKey.Close();
				}
			}
			catch (Exception ex)
			{
				Console.WriteLine("Exception while trying to install statoscope:// protocol handler to the registry: " + ex.ToString());
			}
		}
	}
}

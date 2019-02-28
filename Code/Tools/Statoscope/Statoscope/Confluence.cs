// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using Statoscope.ConfluenceSOAP;
using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Xml;
using System.IO;
using System.IO.Compression;

namespace Statoscope
{
	class ConfluenceService : IDisposable
	{
		ConfluenceSoapServiceService m_soapService;
		string m_soapAuthToken = "";
		public string m_username;

		LogData m_logData;

		public ConfluenceService(LogData logData)
		{
			m_soapService = new ConfluenceSoapServiceService();
			m_logData = logData;
			m_username = "";
		}

		~ConfluenceService()
		{
			Dispose();
		}

		public void Dispose()
		{
			if (m_soapAuthToken != "")
			{
				m_soapService.logout(m_soapAuthToken);
				m_soapAuthToken = "";
			}
            m_soapService.Dispose();
		}

		public bool TryLogin(bool automatic, string username, string password)
		{
			if (!automatic)
			{
				ConfluenceLoginForm loginForm = new ConfluenceLoginForm(this, "", "");
				loginForm.ShowDialog();
			}
			else
			{
				Login(username, password);
			}
			return m_soapAuthToken != "";
		}

		public bool Login(string username, string password)
		{
			try
			{
				m_soapAuthToken = m_soapService.login(username, password);
				m_username = username;
				return true;
			}
			catch (Exception)
			{
				return false;
			}
		}

		public class PageMetaData
		{
			public string m_space;
			public string m_title;
			public string m_parentTitle;
			public PageContentMetaData m_contentMetaData;

			public PageMetaData(string space, string title, string parentTitle, PageContentMetaData contentMetaData)
			{
				m_space = space;
				m_title = title;
				m_parentTitle = parentTitle;
				m_contentMetaData = contentMetaData;
			}
		}

		public class PageContentMetaData
		{
			public string m_timeDate;
			public string m_logFilename;
			public string m_platform;
			public string m_buildNumber;
			public string m_levelName;
			public string m_uploadedBy;
			public string m_userBuildInfo;
			public bool m_append;

			public PageContentMetaData(string timeDate, string logFilename, string platform, string buildNumber, string levelName,
																	string uploadedBy, string userBuildInfo, bool append)
			{
				m_timeDate = timeDate;
				m_logFilename = logFilename;
				m_platform = platform;
				m_buildNumber = buildNumber;
				m_levelName = levelName;
				m_uploadedBy = uploadedBy;
				m_userBuildInfo = userBuildInfo;
				m_append = append;
			}
		}

		class FPSBucketsRemotePage
		{
			public RemotePage m_page;
			LogData m_logData;

			float m_canvasWidth = 1000;
			float m_canvasHeight = 400;

			public FPSBucketsRemotePage(RemotePage page, LogData logData, PageContentMetaData pageContentMD)
			{
				m_page = page;
				m_logData = logData;

				if (!pageContentMD.m_append)
				{
					m_page.content = "";
				}
				
				string logfilename = Path.GetFileName(pageContentMD.m_logFilename);

				m_page.content += "h1.Build Info\n";
				m_page.content += "Log filename: [" + logfilename + "|^" + logfilename + "]\n";
				m_page.content += "Uploaded at: " + pageContentMD.m_timeDate + "\n";
				m_page.content += "Platform: " + pageContentMD.m_platform + "\n";
				m_page.content += "Build number: " + pageContentMD.m_buildNumber + "\n";
				if (pageContentMD.m_levelName != "")
				{
					m_page.content += "Level name: " + pageContentMD.m_levelName + "\n";
				}
				m_page.content += "Uploaded by: " + pageContentMD.m_uploadedBy + "\n";
				m_page.content += "User build info: " + pageContentMD.m_userBuildInfo + "\n";
			}

			public void AddHeading(string item)
			{
				float startTimeInS = m_logData.FrameRecords[0].FrameTimeInS;
				float endTimeInS = m_logData.FrameRecords[m_logData.FrameRecords.Count - 1].FrameTimeInS;
				float totalTimeInS = endTimeInS - startTimeInS;
				m_page.content += "h2. " + item + "\n";
				m_page.content += string.Format("Time period: {0}s - {1}s ({2}s)\n", startTimeInS, endTimeInS, totalTimeInS);
				m_page.content += "(h:mm:ss.ss) " + GetHMSTimeString(startTimeInS) + " - " + GetHMSTimeString(endTimeInS) + "\n";
			}

			string GetHMSTimeString(float fTotalSeconds)
			{
				float fMinutes = fTotalSeconds / 60.0f;
				float fHours = fMinutes / 60.0f;
				int iHours = (int)fHours;
				int iMinutes = (int)((fHours - iHours) * 60.0f);
				float fSeconds = fTotalSeconds - (iHours * 60.0f * 60.0f) - (iMinutes * 60.0f);
				int iSeconds = (int)fSeconds;
				int iHundredths = (int)Math.Round((fSeconds - iSeconds) * 100.0f);
				return string.Format("{0}:{1:d2}:{2:d2}.{3:d2}", iHours, iMinutes, iSeconds, iHundredths);	// stupid format doesn't let you have floats with a minimum width
			}

			public void AddBucketsTable(string path)
			{
				BucketSet bucketSet = m_logData.m_bucketSets[path];

        if (bucketSet is TimeBucketSet)
        {
          m_page.content += "|| Range || % of time || cumulative % ||\n";

          for (int i = 0; i < bucketSet.m_buckets.Count; i++)
          {
            TimeBucket bucket = bucketSet.m_buckets[i] as TimeBucket;
            m_page.content += string.Format("| {0} | {1:n2}% | {2:n2}% |\n", bucket.RangeDescription, bucket.m_timePercent, bucket.m_timePercentCumulative);
          }
        }
			}

			public void StartCanvasSection()
			{
				Random random = new Random();
				string histogramName = "histogram_" + random.Next();	// done so that we can have more than one per page
				m_page.content += "{html}\n";
				m_page.content += string.Format("<canvas width=\"{0}\" height=\"{1}\" id=\"" + histogramName + "\"></canvas>\n", m_canvasWidth, m_canvasHeight);
				m_page.content += "<script type=\"text/javascript\">\n";
				m_page.content += "var histogramCanvas = document.getElementById('" + histogramName + "');\n";
				m_page.content += "context = histogramCanvas.getContext('2d');\n";
				m_page.content += "context.font = \"15px arial\";\n";
			}

			public void EndCanvasSection()
			{
				m_page.content += "</script>\n";
				m_page.content += "{html}\n";
			}

			public void AddHistogram(string path)
			{
				int barX = 100;
				int barY = 30;
				int textX = barX - 60;
				int textY = barY + 12;
				int maxWidth = 200;
				int barHeight = 12;
				int verticalGap = 4;

				BucketSet bucketSet = m_logData.m_bucketSets[path];

        if (bucketSet is TimeBucketSet)
        {
          for (int i = 0; i < bucketSet.m_buckets.Count; i++)
          {
            TimeBucket bucket = bucketSet.m_buckets[i] as TimeBucket;
            m_page.content += "context.fillStyle = \"rgb(255,0,0)\";\n";
            m_page.content += string.Format("context.fillRect({0}, {1}, {2}, {3});\n", barX, barY, (bucket.m_timePercent / 100.0f) * maxWidth, barHeight);
            m_page.content += "context.fillStyle = \"rgb(0,0,0)\";\n";
            m_page.content += string.Format("context.fillText(\"{0}\", {1}, {2});\n", bucket.RangeDescription, textX, textY);
            barY += barHeight + verticalGap;
            textY += barHeight + verticalGap;
          }
        }
			}

			public void AddTimeLineGraph(string [] path)
			{
				float graphStartX = 400;
				float graphStartY = 20;
				float graphWidth = m_canvasWidth - graphStartX - 20;
				float graphHeight = (m_canvasHeight - 100) - graphStartY - 20;
				float timeStart = m_logData.FrameRecords[0].FrameTimeInS;
				float timeRange = m_logData.FrameRecords[m_logData.FrameRecords.Count - 1].FrameTimeInS - timeStart;

				float maxGraphValue = 0.0f;
				foreach (string item in path)
				{
					maxGraphValue = Math.Max(maxGraphValue,m_logData.GetPercentileValue(fr => 1000.0f / fr.Values[item], 90.0f));	// try to fit 90% of the values in
				}
				float graphScaleX = graphWidth / timeRange;
				float graphScaleY = graphHeight / maxGraphValue;

				m_page.content += "context.strokeStyle = 'rgb(0,0,0)';\n";
				m_page.content += "context.beginPath();\n";
				m_page.content += string.Format("context.moveTo({0}, {1});\n", graphStartX, graphStartY);
				m_page.content += string.Format("context.lineTo({0}, {1});\n", graphStartX, graphStartY + graphHeight);
				m_page.content += string.Format("context.lineTo({0}, {1});\n", graphStartX + graphWidth, graphStartY + graphHeight);
				m_page.content += "context.stroke();\n";
				m_page.content += "context.closePath();\n";

				m_page.content += GetCanvasTargetLineString(60.0f, new RGB(1.0f, 0.0f, 0.0f), graphStartX, graphStartY, graphWidth, graphHeight, graphScaleY);
				m_page.content += GetCanvasTargetLineString(30.0f, new RGB(0.0f, 1.0f, 0.0f), graphStartX, graphStartY, graphWidth, graphHeight, graphScaleY);
				m_page.content += GetCanvasTargetLineString(25.0f, new RGB(0.0f, 0.0f, 1.0f), graphStartX, graphStartY, graphWidth, graphHeight, graphScaleY);
				m_page.content += GetCanvasTargetLineString(20.0f, new RGB(0.9f, 0.9f, 0.0f), graphStartX, graphStartY, graphWidth, graphHeight, graphScaleY);
				m_page.content += GetCanvasTargetLineString(15.0f, new RGB(1.0f, 0.0f, 1.0f), graphStartX, graphStartY, graphWidth, graphHeight, graphScaleY);
				m_page.content += GetCanvasTargetLineString(10.0f, new RGB(0.0f, 1.0f, 1.0f), graphStartX, graphStartY, graphWidth, graphHeight, graphScaleY);

				m_page.content += "context.fillStyle = \"rgb(0,0,0)\";\n";
				m_page.content += string.Format("context.fillText(\"{0}\", {1}, {2});\n", Math.Round(maxGraphValue,0).ToString(), graphStartX-20, graphStartY);
				m_page.content += "context.stroke();\n";

				int count = 0;
				int r = 0;
				int g = 0;
				int b = 255;
				
				foreach (string item in path)
				{
					m_page.content += "context.strokeStyle = 'rgb(" + r + "," + g + "," + b + ")';\n";
					m_page.content += "context.beginPath();\n";
					m_page.content += string.Format("context.moveTo({0}, {1});\n", graphStartX, graphStartY + graphHeight);
					
					float lastPlottedX = -float.MaxValue;

					for (int i = 0; i < m_logData.FrameRecords.Count; i++)
					{
						FrameRecord fr = m_logData.FrameRecords[i];
						float x = (fr.FrameTimeInS - timeStart) * graphScaleX + graphStartX;

						if (x > lastPlottedX + 1.0f)
						{
							float value = fr.Values[item];
							float fps = value > 0.0f ? 1000.0f / value : 0.0f;
							float y = graphStartY + graphHeight - fps * graphScaleY;
							m_page.content += string.Format("context.lineTo({0}, {1});\n", x, y);
							lastPlottedX = x;
						}
					}

					m_page.content += "context.stroke();\n";
					m_page.content += "context.closePath();\n";

					m_page.content += "context.font = \"10px arial\";\n";
					m_page.content += "context.fillStyle = \"rgb(0,0,0)\";\n";
					m_page.content += string.Format("context.fillText(\"{0}\", {1}, {2});\n", item, 20.0f, graphStartY + graphHeight + (10 * count));
					m_page.content += "context.stroke();\n";

					m_page.content += "context.strokeStyle = 'rgb(" + r + "," + g + "," + b + ")';\n";
					m_page.content += "context.beginPath();\n";
					m_page.content += string.Format("context.moveTo({0}, {1});\n", 0.0f, graphStartY + graphHeight + (10*count));
					m_page.content += string.Format("context.lineTo({0}, {1});\n", 15.0f, graphStartY + graphHeight + (10 * count));
					m_page.content += "context.stroke();\n";
					m_page.content += "context.closePath();\n";

					count++;
					r = 255;
					g = (255 / path.Length) * count;
					b = (255 / path.Length) * count;
				}

				m_page.content += "context.font = \"10px arial\";\n";
				m_page.content += "context.textAlign = 'center';\n";
				m_page.content += "context.fillStyle = \"rgb(0,0,0)\";\n";

				m_page.content += "context.strokeStyle = 'rgb(0,0,0)';\n";
				m_page.content += "context.beginPath();\n";

				foreach (FrameRecordRange frr in m_logData.LevelRanges)
				{
					float startX = (m_logData.FrameRecords[frr.StartIdx].FrameTimeInS - timeStart) * graphScaleX + graphStartX;
					float endX = (m_logData.FrameRecords[frr.EndIdx].FrameTimeInS - timeStart) * graphScaleX + graphStartX;

					m_page.content += string.Format("context.moveTo({0}, {1});\n", startX, graphStartY);
					m_page.content += string.Format("context.lineTo({0}, {1});\n", startX, graphStartY + graphHeight);

					m_page.content += string.Format("context.fillText(\"{0}\", {1}, {2});\n", frr.Name, (startX + endX) / 2.0f, graphStartY + graphHeight + 10);

					m_page.content += string.Format("context.moveTo({0}, {1});\n", endX, graphStartY);
					m_page.content += string.Format("context.lineTo({0}, {1});\n", endX, graphStartY + graphHeight);
				}

				m_page.content += "context.stroke();\n";
				m_page.content += "context.closePath();\n";
			}

			string GetCanvasTargetLineString(float value, RGB rgb, float graphStartX, float graphStartY, float graphWidth, float graphHeight, float graphScaleY)
			{
				float y = graphStartY + graphHeight - value * graphScaleY;

				string ret = "";

				if (y >= graphStartY)
				{
					ret += "context.strokeStyle = '" + string.Format("rgb({0},{1},{2})", (int)(rgb.r * 255.0f), (int)(rgb.g * 255.0f), (int)(rgb.b * 255.0f)) + "';\n";
					ret += "context.beginPath();\n";
					ret += string.Format("context.moveTo({0}, {1});\n", graphStartX, y);
					ret += string.Format("context.lineTo({0}, {1});\n", graphStartX + graphWidth, y);
					ret += "context.stroke();\n";
					ret += "context.closePath();\n";
					ret += "context.textBaseline = 'middle';\n";
					ret += "context.fillStyle = '" + string.Format("rgb({0},{1},{2})", (int)(rgb.r * 255.0f), (int)(rgb.g * 255.0f), (int)(rgb.b * 255.0f)) + "';\n";
					ret += string.Format("context.fillText(\"{0:n0}\", {1}, {2});\n", value, graphStartX - 25, y);
				}

				return ret;
			}
		}

		class StatisticsRemotePage
		{
			public RemotePage m_page = null;
			private LogData m_logData = null;
			private string[] m_groups = null;

			float m_canvasWidth = 1000;
			float m_canvasHeight = 300;

			public StatisticsRemotePage(RemotePage page, LogData logData, PageContentMetaData pageContentMD, string [] groups)
			{
				m_page = page;
				m_logData = logData;
				m_groups = groups;

				float startTimeInS = logData.FrameRecords[0].FrameTimeInS;
				float endTimeInS = logData.FrameRecords[logData.FrameRecords.Count - 1].FrameTimeInS;
				float totalTimeInS = endTimeInS - startTimeInS;

				if (!pageContentMD.m_append)
				{
					m_page.content = "";
				}

				m_page.content += "h1.Build Info\n";
				m_page.content += "Log filename: [" + Path.GetFileName(pageContentMD.m_logFilename) + ".gz|^" + Path.GetFileName(pageContentMD.m_logFilename) + ".gz]\n";
				m_page.content += "Uploaded at: " + pageContentMD.m_timeDate + "\n";
				m_page.content += "Platform: " + pageContentMD.m_platform + "\n";
				m_page.content += "Build number: " + pageContentMD.m_buildNumber + "\n";
				if (pageContentMD.m_levelName != "")
				{
					m_page.content += "Level name: " + pageContentMD.m_levelName + "\n";
				}
				m_page.content += "Uploaded by: " + pageContentMD.m_uploadedBy + "\n";
				m_page.content += "User build info: " + pageContentMD.m_userBuildInfo + "\n";

				m_page.content += "h2.Statistics data\n";
				m_page.content += string.Format("Time period: {0}s - {1}s ({2}s)\n", startTimeInS, endTimeInS, totalTimeInS);
				m_page.content += "(h:mm:ss.ss) " + GetHMSTimeString(startTimeInS) + " - " + GetHMSTimeString(endTimeInS) + "\n";
			}

			string GetHMSTimeString(float fTotalSeconds)
			{
				float fMinutes = fTotalSeconds / 60.0f;
				float fHours = fMinutes / 60.0f;
				int iHours = (int)fHours;
				int iMinutes = (int)((fHours - iHours) * 60.0f);
				float fSeconds = fTotalSeconds - (iHours * 60.0f * 60.0f) - (iMinutes * 60.0f);
				int iSeconds = (int)fSeconds;
				int iHundredths = (int)Math.Round((fSeconds - iSeconds) * 100.0f);
				return string.Format("{0}:{1:d2}:{2:d2}.{3:d2}", iHours, iMinutes, iSeconds, iHundredths);	// stupid format doesn't let you have floats with a minimum width
			}

			public void StartCanvasSection(int histogramid)
			{
				string histogramName = "histogram_" + histogramid;	// done so that we can have more than one per page
				m_page.content += "{html}\n";
				m_page.content += string.Format("<canvas width=\"{0}\" height=\"{1}\" id=\"" + histogramName + "\"></canvas>\n", m_canvasWidth, m_canvasHeight);
				m_page.content += "<script type=\"text/javascript\">\n";
				m_page.content += "var histogramCanvas = document.getElementById('" + histogramName + "');\n";
				m_page.content += "context = histogramCanvas.getContext('2d');\n";
				m_page.content += "context.font = \"15px arial\";\n";
			}

			public void EndCanvasSection()
			{
				m_page.content += "</script>\n";
				m_page.content += "{html}\n";
			}

			public void AddStatOverview(float min, float max, float avg, float metricMax, float metricTarget)
			{
				m_page.content += "||*Actual Mininum*||*Actual Maximum*||*Actual Average*||*Budget Max*||*Budget Target*||\n";
				m_page.content += "|{color:red}" + min + "{color}|{color:red}" + max + "{color}|{color:red}" + avg + "{color}|{color:red}" + metricMax + "{color}|{color:red}" + metricTarget + "{color}|\n";
			}

			public void AddTimeLineGraph(string group, string item, Stats stat, float max, float target, string units)
			{
				string timeLinePath = "/" + group + "/" + item;
				float timeStart = m_logData.FrameRecords[0].FrameTimeInS;
				float timeRange = m_logData.FrameRecords[m_logData.FrameRecords.Count - 1].FrameTimeInS - timeStart;
				float maxGraphValue = Math.Max(stat.m_max,max) + 10.0f;

				float graphStartX = 50.0f;
				float graphStartY = 20.0f;
				float graphWidth = (m_canvasWidth - graphStartX) - 50.0f;
				float graphHeight = (m_canvasHeight - graphStartY) - 20.0f;
				float graphScaleX = graphWidth / timeRange;
				float graphScaleY = graphHeight / maxGraphValue;

				m_page.content += "context.strokeStyle = 'rgb(0,0,0)';\n";
				m_page.content += "context.beginPath();\n";
				m_page.content += string.Format("context.moveTo({0}, {1});\n", graphStartX, graphStartY);
				m_page.content += string.Format("context.lineTo({0}, {1});\n", graphStartX, graphStartY + graphHeight);
				m_page.content += string.Format("context.lineTo({0}, {1});\n", graphStartX + graphWidth, graphStartY + graphHeight);
				m_page.content += string.Format("context.fillText(\"{0:n0}\", {1}, {2});\n", 0, graphStartX - 15, graphStartY + graphHeight + 15);
				m_page.content += string.Format("context.fillText(\"{0:n0}\", {1}, {2});\n", "Time(s)", graphStartX + (graphWidth / 2), graphStartY + graphHeight + 15);
				m_page.content += string.Format("context.fillText(\"{0:n0}\", {1}, {2});\n", units, 0, graphStartY + (graphHeight / 2));
				m_page.content += string.Format("context.fillText(\"{0:n0}\", {1}, {2});\n", (int)maxGraphValue, 0, graphStartY);
				m_page.content += string.Format("context.fillText(\"{0:n0}\", {1}, {2});\n", (int)timeRange, graphStartX + graphWidth, graphStartY + graphHeight + 15);


				m_page.content += "context.stroke();\n";
				m_page.content += "context.closePath();\n";

				if (target != 0.0f)
					m_page.content += GetCanvasTargetLineString(target, "target", true, new RGB(0.0f, 0.0f, 1.0f), graphStartX, graphStartY, graphWidth, graphHeight, graphScaleY);
				if (max != 0.0f)
					m_page.content += GetCanvasTargetLineString(max, "max", true, new RGB(1.0f, 0.0f, 1.0f), graphStartX, graphStartY, graphWidth, graphHeight, graphScaleY);

				m_page.content += "context.strokeStyle = 'rgb(0,0,255)';\n";
				m_page.content += "context.beginPath();\n";
				m_page.content += string.Format("context.moveTo({0}, {1});\n", graphStartX, graphStartY + graphHeight);

				float lastPlottedX = -float.MaxValue;

				for (int i = 0; i < m_logData.FrameRecords.Count; i++)
				{
					FrameRecord fr = m_logData.FrameRecords[i];
					float x = (fr.FrameTimeInS - timeStart) * graphScaleX + graphStartX;
					float value = fr.Values[timeLinePath];
					float y = Math.Min(graphStartY+graphHeight, graphStartY + graphHeight - (value * graphScaleY));
					m_page.content += string.Format("context.lineTo({0}, {1});\n", x, y);
					lastPlottedX = x;
				}

				m_page.content += "context.stroke();\n";
				m_page.content += "context.closePath();\n";

				m_page.content += "context.font = \"10px arial\";\n";
				m_page.content += "context.textAlign = 'center';\n";
				m_page.content += "context.fillStyle = \"rgb(0,0,0)\";\n";

				m_page.content += "context.strokeStyle = 'rgb(0,0,0)';\n";
				m_page.content += "context.beginPath();\n";

				foreach (FrameRecordRange frr in m_logData.LevelRanges)
				{
					float startX = (m_logData.FrameRecords[frr.StartIdx].FrameTimeInS - timeStart) * graphScaleX + graphStartX;
					float endX = (m_logData.FrameRecords[frr.EndIdx].FrameTimeInS - timeStart) * graphScaleX + graphStartX;

					m_page.content += string.Format("context.moveTo({0}, {1});\n", startX, graphStartY);
					m_page.content += string.Format("context.lineTo({0}, {1});\n", startX, graphStartY + graphHeight);

					m_page.content += string.Format("context.fillText(\"{0}\", {1}, {2});\n", frr.Name, (startX + endX) / 2.0f, graphStartY + graphHeight + 10);

					m_page.content += string.Format("context.moveTo({0}, {1});\n", endX, graphStartY);
					m_page.content += string.Format("context.lineTo({0}, {1});\n", endX, graphStartY + graphHeight);
				}

				m_page.content += "context.stroke();\n";
				m_page.content += "context.closePath();\n";
			}

			string GetCanvasTargetLineString(float value, RGB rgb, float graphStartX, float graphStartY, float graphWidth, float graphHeight, float graphScaleY)
			{
				return GetCanvasTargetLineString(value, value.ToString(), false, rgb, graphStartX, graphStartY, graphWidth, graphHeight, graphScaleY);
			}

			string GetCanvasTargetLineString(float value, string name, bool textRight, RGB rgb, float graphStartX, float graphStartY, float graphWidth, float graphHeight, float graphScaleY)
			{
				float y = graphStartY + graphHeight - value * graphScaleY;

				string ret = "";

				if (y >= graphStartY)
				{
					ret += "context.strokeStyle = '" + string.Format("rgb({0},{1},{2})", (int)(rgb.r * 255.0f), (int)(rgb.g * 255.0f), (int)(rgb.b * 255.0f)) + "';\n";
					ret += "context.beginPath();\n";
					ret += string.Format("context.moveTo({0}, {1});\n", graphStartX, y);
					ret += string.Format("context.lineTo({0}, {1});\n", graphStartX + graphWidth, y);
					ret += "context.stroke();\n";
					ret += "context.closePath();\n";
					ret += "context.textBaseline = 'middle';\n";
					ret += "context.fillStyle = '" + string.Format("rgb({0},{1},{2})", (int)(rgb.r * 255.0f), (int)(rgb.g * 255.0f), (int)(rgb.b * 255.0f)) + "';\n";
					ret += string.Format("context.fillText(\"{0:n0}\", {1}, {2});\n", name, textRight ? graphStartX + graphWidth + 10 : graphStartX - 25, y);
				}

				return ret;
			}
		}

		public void PromptForUpload(string logfile)
		{
			ConfluenceUploadForm uploadForm = new ConfluenceUploadForm(this, m_logData, "R&D", "(DT) Statoscope", m_logData.BuildInfo.PlatformString, m_logData.BuildInfo.BuildNumberString, logfile);
			uploadForm.ShowDialog();
		}

		public void UploadFPSDataPage(PageMetaData pageMetaData, LogData logData, string logfilepath)
		{
			FPSBucketsRemotePage page = CreateFPSBucketsPage(pageMetaData, logData);
			
			page.AddHeading("Framerate Data");
			page.AddBucketsTable("Overall fps");
			page.StartCanvasSection();
			{
				page.AddHistogram("Overall fps");
				string[] stats = { "/frameLengthMAInMS", "/Graphics/GPUFrameLengthInMS" };
				page.AddTimeLineGraph(stats);
			}
			page.EndCanvasSection();

			StorePage(page.m_page);
			AddAttachment(Compress(logfilepath), pageMetaData.m_title, pageMetaData.m_parentTitle, pageMetaData.m_space, "FPS Statoscope Log");
		}

		public void UploadStatsDataPage(PageMetaData pageMetaData, LogData logData, string [] groups, XmlNodeList metrics, bool upload)
		{
			StatisticsRemotePage page = CreateStatisticsPage(pageMetaData, logData, groups);
			if (upload)
			{
				GroupsStats stats = new GroupsStats();
				logData.GetFrameGroupData(0, logData.FrameRecords.Count - 1, groups, stats);
				Random random = new Random();
				foreach (string group in stats.m_groupStats.Keys)
				{
					page.m_page.content += "\n\n";
					GroupItemStats groupsStats = stats.GetGroupItemsStats(group);
					foreach (string item in groupsStats.m_groupItemStats.Keys)
					{
						float target = 0.0f;
						float max = 0.0f;
						float min = 0.0f;
						float fail = 0.0f;
						string path = "/" + group + "/" + item;
						string units = ""; // logData.FrameRecords[0].m_units[path];

						foreach (XmlNode node in metrics)
						{
							XmlNode attribute = node.Attributes.GetNamedItem("name");
							if (attribute.Value == item)
							{
								XmlNode metricNode = node.ChildNodes.Item(0);
								if (metricNode != null)
								{
									if (metricNode.Attributes.GetNamedItem("target") != null)
										target = (float)int.Parse(metricNode.Attributes.GetNamedItem("target").Value);
									if (metricNode.Attributes.GetNamedItem("heatMax") != null)
										max = (float)int.Parse(metricNode.Attributes.GetNamedItem("heatMax").Value) + target;
									if (metricNode.Attributes.GetNamedItem("heatMin") != null)
										min = (float)int.Parse(metricNode.Attributes.GetNamedItem("heatMin").Value);
									if (metricNode.Attributes.GetNamedItem("failure") != null)
										fail = (float)int.Parse(metricNode.Attributes.GetNamedItem("failure").Value) + target;
									break;
								}
							}
						}

						Stats statitem = groupsStats.GetItemStats(item);
						page.m_page.content += "+*Statistic*: {color:blue}" + group + " - " + item + "{color}+\n\n";
						page.AddStatOverview(Math.Max(0.0f, statitem.m_min), Math.Max(0.0f, statitem.m_max), Math.Max(0.0f, statitem.m_avg), max, target);
						page.StartCanvasSection(random.Next());
						page.AddTimeLineGraph(group, item, statitem, max, target, units);
						page.EndCanvasSection();
					}
				}
			}
			
			StorePage(page.m_page);
			AddAttachment(Compress(pageMetaData.m_contentMetaData.m_logFilename), pageMetaData.m_title, pageMetaData.m_parentTitle, pageMetaData.m_space, "Statoscope Log");
		}

		public List<RemoteSpaceSummary> GetSpaces()
		{
			List<RemoteSpaceSummary> globalSpaces = new List<RemoteSpaceSummary>();

			foreach (RemoteSpaceSummary space in m_soapService.getSpaces(m_soapAuthToken))
			{
				if (space.type == "global")
				{
					globalSpaces.Add(space);
				}
			}

			return globalSpaces;
		}

		public void CreateSpace(string name)
		{
			RemoteSpace space = new RemoteSpace();
			space.name = name;
			space.key = name;
			space.description = name;
			m_soapService.addSpace(m_soapAuthToken, space);
		}

		public List<RemotePageSummary> GetPages(string space)
		{
			List<RemotePageSummary> pages = new List<RemotePageSummary>();

			foreach (RemotePageSummary page in m_soapService.getPages(m_soapAuthToken, space))
			{
				pages.Add(page);
			}

			return pages;
		}

		RemotePage GetPage(string title, string parentTitle, string space)
		{
			RemotePage page;

			try
			{
				page = m_soapService.getPage(m_soapAuthToken, space, title);
			}
			catch
			{
				page = new RemotePage();
			}

			page.space = space;
			page.title = title;

			if (parentTitle != "")
			{
				page.parentId = m_soapService.getPage(m_soapAuthToken, space, parentTitle).id;
			}

			return page;
		}

		public string Compress(string path)
		{
			FileInfo fileinfo = new FileInfo(path);
			using (FileStream inFile = fileinfo.OpenRead())
			{
				path = fileinfo.FullName + ".gz";
				using (FileStream outFile = File.Create(path))
				{
					using (GZipStream Compress = new GZipStream(outFile, CompressionMode.Compress))
					{
						byte[] buffer = new byte[4096];
						int numRead;
						while ((numRead = inFile.Read(buffer, 0, buffer.Length)) != 0)
						{
							Compress.Write(buffer, 0, numRead);
						}
					}
				}
			}
			return path;
		}

		public RemoteAttachment AddAttachment(string fileName, string title, string parent, string space, string comment)
		{
			try
			{
				byte[] data = null;
				RemoteAttachment attachment = new RemoteAttachment();
				data = File.ReadAllBytes(fileName);
        attachment.fileName = Path.GetFileName(fileName);
				attachment.contentType = "application/zip";
				attachment.comment = comment;
        attachment.created = DateTime.Now;

				RemotePage page = GetPage(title, parent, space);
				attachment = m_soapService.addAttachment(m_soapAuthToken, page.id, attachment, data);

				return attachment;
			}
			catch (System.Exception ex)
			{
				Console.WriteLine("Error adding attachment: " + ex.Message + Environment.NewLine + "Location: " + ex.StackTrace);
			}
			return null;
		}
		
		void StorePage(RemotePage page)
		{
			m_soapService.storePage(m_soapAuthToken, page);
		}

		public void CreatePage(string title, string parentTitle, string space)
		{
			RemotePage page = GetPage(title, parentTitle, space);
			StorePage(page);
		}

		public void CreatePage(string title, string space)
		{
			CreatePage(title, "", space);
		}

		FPSBucketsRemotePage CreateFPSBucketsPage(PageMetaData pageMetaData, LogData logData)
		{
			RemotePage page = GetPage(pageMetaData.m_title, pageMetaData.m_parentTitle, pageMetaData.m_space);
			return new FPSBucketsRemotePage(page, logData, pageMetaData.m_contentMetaData);
		}

		StatisticsRemotePage CreateStatisticsPage(PageMetaData pageMetaData, LogData logData, string [] groups)
		{
			RemotePage page = GetPage(pageMetaData.m_title, pageMetaData.m_parentTitle, pageMetaData.m_space);
			return new StatisticsRemotePage(page, logData, pageMetaData.m_contentMetaData, groups);
		}
	}
}

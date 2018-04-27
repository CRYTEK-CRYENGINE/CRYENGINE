// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using System.Windows.Media;
using System.IO;
using CsGL.OpenGL;
using System.Reflection;
#if !__MonoCS__
using System.Windows.Forms.Integration;
using System.Windows.Shapes;
using Excel = Microsoft.Office.Interop.Excel;
using Word = Microsoft.Office.Interop.Word;
#endif


namespace Statoscope
{
	partial class StatoscopeForm
	{
    // Types
    class BucketValue
    {
      public float fps;
      public float dt;
      public BucketValue(float _dt)
      {
        dt = _dt;
        fps = 1.0f / dt;
      }
      public BucketValue(float _dt, float _fps)
      {
        dt = _dt;
        fps = _fps;
      }
    }

	  delegate BucketValue GetBucketValue(FrameRecord record);

    class CSpike : IComparable
    {
      //public string name;
      public float minTime;
      public float maxTime;
      public float totalTime;
      public int count;
      public CSpike()
      {
        minTime = 9999999999999.0f;
        maxTime = totalTime = 0.0f;
        count = 0;
      }
      public int CompareTo(object o)
      {
        CSpike other = (CSpike)o;
        if (maxTime == other.maxTime)
        {
          if (count == other.count)
            return 0;
          else if (count > other.count)
            return -1;
        }
        else if (maxTime > other.maxTime)
        {
          return -1;
        }
        return 1;
      }
    }

    class CSystem : IComparable
    {
      //public string m_name;
      //public float m_timeInMS;
      public int CompareTo(object o)
      {
				/*
        CSystem s = (CSystem)o;
        if (m_timeInMS == s.m_timeInMS)
          return 0;
        else if (m_timeInMS > s.m_timeInMS)
          return -1;
				*/
        return 1;
      }
    }

    class Marker
    {
      public Marker()
      {
				m_system = "";
        m_bStart = true;
        m_start = null;
        m_end = null;
      }
      public string m_system;
      public bool m_bStart;
      public FrameRecord m_start;
      public FrameRecord m_end;
    }

#if !__MonoCS__
    Marker[] m_markers = new Marker[9];

    // Word/Excel Utils

    void AddText(Word.Document oDoc, string bookmarkName, string text)
    {
      object oEndOfDoc = bookmarkName;
      Word.Range insertPoint = oDoc.Bookmarks.get_Item(ref oEndOfDoc).Range;
      insertPoint.InsertAfter(text);
    }

    Excel._Worksheet AddWorksheet(Excel._Workbook oWB, string name)
    {
      Excel._Worksheet oSheet = (Excel._Worksheet)oWB.Sheets.Add(Missing.Value, Missing.Value, Missing.Value, Missing.Value);
      oSheet.Name = name;
      return oSheet;
    }

    Excel._Chart MakeChart(Excel._Workbook oWB, Excel.Range input, Excel.XlChartType type, Excel.XlRowCol rowcol, int numXValues, int numLabels, bool bLegend)
    {
      Excel._Chart oChart = (Excel._Chart)oWB.Charts.Add(Missing.Value, Missing.Value, Missing.Value, Missing.Value);
      oChart.ChartWizard(input, type, Missing.Value, rowcol, numXValues, numLabels, bLegend, Missing.Value, Missing.Value, Missing.Value, Missing.Value);
      oChart.ChartArea.Format.Line.Visible = Microsoft.Office.Core.MsoTriState.msoFalse;
      return oChart;
    }
    
    void SetupAxes(Excel._Chart oChart, string xLabel, float minX, float maxX, string yLabel, float minY, float maxY)
    {
      Excel.Axis axis = (Excel.Axis)oChart.Axes(Excel.XlAxisType.xlCategory, Excel.XlAxisGroup.xlPrimary);
      if (maxX != -1.0f)
        axis.MaximumScale = maxX;
      if (minX != -1.0f)
        axis.MinimumScale = minX;
      axis.HasTitle = true;
      axis.AxisTitle.Text = xLabel;
      axis = (Excel.Axis)oChart.Axes(Excel.XlAxisType.xlValue, Excel.XlAxisGroup.xlPrimary);
      if (maxY != -1.0f)
        axis.MaximumScale = maxY;
      if (minY != -1.0f)
        axis.MinimumScale = minY;
      axis.HasMajorGridlines = false;
      axis.HasMinorGridlines = false;
      axis.HasTitle = true;
      axis.AxisTitle.Text = yLabel;
    }

    Word.InlineShape InsertChart(Excel._Chart oChart, Word.Document oDoc, string filename, string placementName, float width, float height, float scale)
    {
      object oMissing = Missing.Value;
      string filepath = System.IO.Path.GetTempPath() + filename + ".png";
      oChart.ChartArea.Width = width;
      oChart.ChartArea.Height = height;
      oChart.Export(filepath, "png", false);
      object oEndOfDoc = placementName;
      Word.Range insertPoint = oDoc.Bookmarks.get_Item(ref oEndOfDoc).Range;
      Word.InlineShape oShape = insertPoint.InlineShapes.AddPicture(filepath, ref oMissing, ref oMissing, ref oMissing);
      oShape.Width = width * scale;
      oShape.Height = height * scale;
      return oShape;
    }

    // Report Utils

    void FillBuckets(Excel._Worksheet oSheet, GetBucketValue grv, int minValue, int maxValue, float ignoreStart)
    {
			/*
      float totalTime = 0.0f;
      List<float> frameBuckets = new List<float>(maxValue - minValue + 1);
      for (int i = 0; i <= maxValue - minValue; i++)
      {
        frameBuckets.Add(0.0f);
      }
      ignoreStart += m_logData.m_frameRecords[0].FrameTimeInS;
      foreach (FrameRecord fr in m_logData.m_frameRecords)
      {
        if (fr.FrameTimeInS >= ignoreStart)
        {
          BucketValue bv = grv(fr);
          float fps = bv.fps;
          int idx = (int)fps - minValue;
          idx = Math.Min(idx, maxValue - minValue);
          idx = Math.Max(idx, 0);
          frameBuckets[idx] += bv.dt;
          totalTime += bv.dt;
        }
      }
      float acc = 0.0f;
      for (int i = 0; i < frameBuckets.Count(); i++)
      {
        oSheet.Cells[i + 1, 1] = i + minValue;
        oSheet.Cells[i + 1, 2] = 100.0f * frameBuckets[i] / totalTime;
        oSheet.Cells[i + 1, 3] = i + minValue;
        oSheet.Cells[i + 1, 4] = acc;
        acc += 100.0f * frameBuckets[i] / totalTime;
      }
			 * */
    }

    void CreateSpikeList(List<CSpike> spikeList)
    {
			/*
      for (int i = 1; i < m_logData.m_frameRecords.Count() - 1; i++)
      {
        FrameRecord fr = m_logData.m_frameRecords[i];
        if (fr.FrameTimeInS >= 1.0f) // profile data normally a bit missed up at this point
        {
          FrameRecord lastfr = m_logData.m_frameRecords[i - 1];
          FrameRecord nextfr = m_logData.m_frameRecords[i + 1];

					foreach (KeyValuePair<string, CPerfStatRecordData> moduleRDPair in fr.m_data.m_children["Modules"].m_children)
					{
						foreach (KeyValuePair<string, CPerfStatRecordData> functionRDPair in moduleRDPair.Value.m_children)
						{
							string functionName = functionRDPair.Key;
							float selfTimeInMS = functionRDPair.Value.m_values["selfTimeInMS"];
							float k_spikeThreshold = 10.0f;
							float k_spikeMultiplier = 4.0f;
							bool spike = (selfTimeInMS > k_spikeThreshold);

							if (spike)
							{
								if (!functionName.Contains("CFrameProfileSystem::EndFrame"))
								{
									if (!functionName.Contains("SRenderThread::FlushFrame"))
									{
										foreach (KeyValuePair<string, CPerfStatRecordData> lastModuleRDPair in lastfr.m_data.m_children["Modules"].m_children)
										{
											foreach (KeyValuePair<string, CPerfStatRecordData> lastFunctionRDPair in lastModuleRDPair.Value.m_children)
											{
												if (functionName == lastFunctionRDPair.Key && selfTimeInMS < k_spikeMultiplier * lastFunctionRDPair.Value.m_values["selfTimeInMS"])
												{
													spike = false;
												}
											}
										}
										if (spike)
										{
											foreach (KeyValuePair<string, CPerfStatRecordData> nextModuleRDPair in nextfr.m_data.m_children["Modules"].m_children)
											{
												foreach (KeyValuePair<string, CPerfStatRecordData> nextFunctionRDPair in nextModuleRDPair.Value.m_children)
												{
													if (functionName == nextFunctionRDPair.Key && selfTimeInMS < k_spikeMultiplier * nextFunctionRDPair.Value.m_values["selfTimeInMS"])
													{
														spike = false;
													}
												}
											}
											if (spike)
											{
												CSpike found = null;
												foreach (CSpike s in spikeList)
												{
													if (s.name == functionName)
													{
														found = s;
														break;
													}
												}
												if (found == null)
												{
													found = new CSpike();
													found.name = functionName;
													spikeList.Add(found);
												}
												found.count++;
												found.maxTime = Math.Max(found.maxTime, selfTimeInMS);
												found.minTime = Math.Min(found.minTime, selfTimeInMS);
												found.totalTime += selfTimeInMS;
												functionRDPair.Value.m_values["spike"] = spike ? 1.0f : 0.0f;
											}
										}
									}
									else
									{
										functionRDPair.Value.m_values["spike"] = 1.0f;
									}
								}
							}
						}
					}
        }
      }
      spikeList.Sort();
			 * */
    }

    BucketValue GetFrameTimeMinusSpikes(FrameRecord fr)
    {
			/*
      float profileTime = fr.m_data.m_values["totalProfileTime"];
      float frameTime = fr.m_data.m_values["frameLengthInMS"];
      frameTime = profileTime + Math.Min(frameTime - profileTime, 4.0f); // clamp "LostTime" to 4ms
      float origFrameTime = frameTime;

      string basePath = "Modules";
      string valueString = "selfTimeInMS";
      CPerfStatRecordData rd = fr.FindRecordData(basePath);

      foreach (KeyValuePair<string, CPerfStatRecordData> moduleRDPair in rd.m_children)
      {
        foreach (KeyValuePair<string, CPerfStatRecordData> functionRDPair in moduleRDPair.Value.m_children)
        {
          if (!functionRDPair.Key.Contains("CFrameProfileSystem::EndFrame"))
          {
            string path = basePath + "/" + moduleRDPair.Key + "/" + functionRDPair.Key;
            float value = functionRDPair.Value.m_values[valueString];

            float selfTime = value;

            if (functionRDPair.Value.m_values.ContainsKey("spike") && functionRDPair.Value.m_values["spike"] == 1.0f)
              frameTime -= functionRDPair.Value.m_values["selfTimeInMS"];
            else if (!m_profilerRecordDisplayInfoTree[path].m_displayed)
              frameTime -= functionRDPair.Value.m_values["selfTimeInMS"];
          }
        }
      }
      return new BucketValue(0.001f * origFrameTime, Math.Min(1.0f / (0.001f * frameTime), 30.0f));
			 * */
			return new BucketValue(0.0f);
    }

    // Report Generation

    void BuildSpikeTable(Word.Document oDoc, List<CSpike> spikeList)
    {
			/*
      while (spikeList.Count() > 15)
        spikeList.RemoveAt(15);
      Word.Table oTable;
      object oEndOfDoc = "SpikeTable";
      Word.Range wrdRng = oDoc.Bookmarks.get_Item(ref oEndOfDoc).Range;
      object noSpacingStyle = "No Spacing";
      object objDefaultBehaviorWord8 = Word.WdDefaultTableBehavior.wdWord8TableBehavior;
      object oDefaultAutoFit = Word.WdAutoFitBehavior.wdAutoFitWindow;
      oTable = oDoc.Tables.Add(wrdRng, spikeList.Count() + 1, 4, ref objDefaultBehaviorWord8, ref oDefaultAutoFit);
      oTable.Cell(1, 1).Range.Text = "Function Name";
      oTable.Cell(1, 1).Range.set_Style(ref noSpacingStyle);
      oTable.Cell(1, 2).Range.Text = "Occurrences";
      oTable.Cell(1, 2).Range.set_Style(ref noSpacingStyle);
      oTable.Cell(1, 3).Range.Text = "Max Time (ms)";
      oTable.Cell(1, 3).Range.set_Style(ref noSpacingStyle);
      oTable.Cell(1, 4).Range.Text = "Average Time (ms)";
      oTable.Cell(1, 4).Range.set_Style(ref noSpacingStyle);
      oTable.Columns[2].AutoFit();
      oTable.Columns[3].AutoFit();
      oTable.Columns[4].AutoFit();
      oTable.Rows[1].Range.Font.Bold = 1;
      int row = 2;
      foreach (CSpike s in spikeList)
      {
        oTable.Cell(row, 1).Range.Text = s.name;
        oTable.Cell(row, 1).Range.set_Style(ref noSpacingStyle);
        oTable.Cell(row, 2).Range.Text = String.Format("{0:d}", s.count);
        oTable.Cell(row, 2).Range.set_Style(ref noSpacingStyle);
        oTable.Cell(row, 3).Range.Text = String.Format("{0:g}", s.maxTime);
        oTable.Cell(row, 3).Range.set_Style(ref noSpacingStyle);
        oTable.Cell(row, 4).Range.Text = String.Format("{0:g}", s.totalTime / s.count);
        oTable.Cell(row, 4).Range.set_Style(ref noSpacingStyle);
        row++;
      }
      oTable.Columns[1].AutoFit();
      oTable.Borders.Enable = 1;
			*/
    }

    void BuildSystemGraphs(Excel._Workbook oWB, Word.Document oDoc, List<CSystem> systemList, float ignoreTime)
    {
			/*
      const float aspect = 9.0f / 16.0f;
      const float k_size = 800.0f;
      int k_step = Math.Max(m_logData.m_frameRecords.Count() / 500, 50);
      int count = 0;
      int col = 0;
      int row = 2;
      Excel._Worksheet oSheet = AddWorksheet(oWB, "Systems Analysis");
      ignoreTime += m_logData.m_frameRecords[0].FrameTimeInS;
      foreach (FrameRecord fr in m_logData.m_frameRecords)
      {
        if (fr.FrameTimeInS > ignoreTime)
        {
          if (count == 0)
          {
            foreach (CSystem s in systemList)
            {
              s.m_timeInMS = 0.0f;
            }
            count = k_step;
          }
          count--;
					foreach (KeyValuePair<string, CPerfStatRecordData> moduleRDPair in fr.m_data.m_children["Modules"].m_children)
					{
						foreach (KeyValuePair<string, CPerfStatRecordData> functionRDPair in moduleRDPair.Value.m_children)
						{
							if ((!functionRDPair.Value.m_values.ContainsKey("spike") || functionRDPair.Value.m_values["spike"] == 0.0f) && !functionRDPair.Key.Contains("CFrameProfileSystem::EndFrame"))
							{
								CSystem found = null;
								foreach (CSystem s in systemList)
								{
									if (s.m_name == moduleRDPair.Key)
									{
										found = s;
										break;
									}
								}
								if (found == null)
								{
									found = new CSystem();
									found.m_name = moduleRDPair.Key;
									systemList.Add(found);
								}
								found.m_timeInMS += functionRDPair.Value.m_values["selfTimeInMS"];
							}
						}
          }
          if (count == 0)
          {
            col = 1;
            oSheet.Cells[row, col] = string.Format("{0:g}", fr.FrameTimeInS);
            foreach (CSystem s in systemList)
            {
              col++;
              oSheet.Cells[row, col] = string.Format("{0:g}", s.m_timeInMS / k_step);
            }
            row++;
          }
        }
      }
      col = 1;
      oSheet.Cells[1, col++] = "Time";
      foreach (CSystem s in systemList)
      {
        oSheet.Cells[1, col++] = string.Format("{0:s}", s.m_name);
      }
      Excel._Chart oChart = MakeChart(oWB, oSheet.get_Range("A1", string.Format("{0}{1:g}", (char)('A' + systemList.Count), row - 1)), Excel.XlChartType.xlXYScatterLinesNoMarkers, Excel.XlRowCol.xlColumns, 1, 1, true);
      SetupAxes(oChart, "Game Time(s)", -1.0f, -1.0f, "Time Taken(ms)", 0.0f, -1.0f);
      oChart.ChartArea.Width = k_size;
      oChart.ChartArea.Height = k_size * aspect;
      Excel.Axis xaxis = (Excel.Axis)oChart.Axes(Excel.XlAxisType.xlCategory, Excel.XlAxisGroup.xlPrimary);
      Excel.Axis yaxis = (Excel.Axis)oChart.Axes(Excel.XlAxisType.xlValue, Excel.XlAxisGroup.xlPrimary);
      for (int i=0; i<systemList.Count; i++)
      {
        Excel.Series oSeries = (Excel.Series) oChart.SeriesCollection(i+1);
        oSeries.Format.Line.ForeColor.RGB = oSeries.Format.Line.ForeColor.RGB;  // Need to do this to set transparency
        oSeries.Format.Line.Transparency = 0.6f;
      }
      for (int i = 0; i < m_markers.Count(); i++)
      {
        Marker m = m_markers[i];
        if (m != null && m.m_endIdx != null)
        {
          double normStart = (m.m_startIdx.FrameTimeInS - xaxis.MinimumScale) / (xaxis.MaximumScale - xaxis.MinimumScale);
          double normWidth = (m.m_endIdx.FrameTimeInS - m.m_startIdx.FrameTimeInS) / (xaxis.MaximumScale - xaxis.MinimumScale);
          Excel.Shape oBox = oChart.Shapes.AddShape(Microsoft.Office.Core.MsoAutoShapeType.msoShapeRectangle, (float)(xaxis.Left + normStart * xaxis.Width), (float)yaxis.Top, (float)(normWidth * xaxis.Width), (float)yaxis.Height);
          oBox.Fill.Transparency = 0.8f;
          oBox.Fill.ForeColor.RGB = 0xFF0000;
          oBox.Left = (float)(xaxis.Left + normStart * xaxis.Width);
          oBox.Line.Visible = Microsoft.Office.Core.MsoTriState.msoFalse;
          Excel.Shape oText = oChart.Shapes.AddTextEffect(Microsoft.Office.Core.MsoPresetTextEffect.msoTextEffect1, String.Format("{0:d}", i+1), "Courier New", 54.0f, Microsoft.Office.Core.MsoTriState.msoTrue, Microsoft.Office.Core.MsoTriState.msoFalse, oBox.Left, oBox.Top);
          oText.Width = oBox.Width;
          string filepath = System.IO.Path.GetTempPath() + "screen" + String.Format("{0:d}", i+1) + ".png";
          FrameRecord m_screen = FindClosestScreenshotFrameFromTime(m.m_startIdx.FrameTimeInS);
          if (m_screen.m_screenshotImage!=null)
          {
            float picAspect = (float)m_screen.m_screenshotImage.Width / (float)m_screen.m_screenshotImage.Height;
            float w = oText.Height * picAspect;
            m_screen.m_screenshotImage.Save(filepath);
            Excel.Shape oPicture = oChart.Shapes.AddPicture(filepath, Microsoft.Office.Core.MsoTriState.msoFalse, Microsoft.Office.Core.MsoTriState.msoTrue, oText.Left+oText.Width*0.5f-w*0.5f, oText.Top, w, oText.Height);
            oPicture.ZOrder(Microsoft.Office.Core.MsoZOrderCmd.msoSendBackward);
          }
          for (int j=0; j<systemList.Count(); j++)
          {
            CSystem s = systemList[j];
            if (s.m_name == m.m_system)
            {
              Excel.Series oSeries = (Excel.Series)oChart.SeriesCollection(j + 1);
              oSeries.Format.Line.Transparency = 0.0f;
              //oSeries.PlotOrder = i + 1;
              break;
            }
          }
        }
      }
      Word.InlineShape oShape = InsertChart(oChart, oDoc, "systemgraph", "SystemGraph", k_size, k_size * aspect, 0.5f);
			 * */
    }

		class CProfileFunction : IComparable<CProfileFunction>
		{
			public string m_name;
			public float m_selfTimeInMS;
		
			public CProfileFunction(string name)
			{
				m_name = name;
				m_selfTimeInMS = 0.0f;
			}

			public int CompareTo(CProfileFunction other)
			{
				if (m_selfTimeInMS == other.m_selfTimeInMS)
					return 0;
				else if (m_selfTimeInMS > other.m_selfTimeInMS)
					return -1;
				return 1;
			}
		}

    void BuildProfileSectionGraphs(Excel._Workbook oWB, Word.Document oDoc, List<CSystem> systemList, string sectionName, string profileSystem, float startTime, float endTime, string bookmarkName)
    {
			/*
      Excel._Worksheet oSheet = AddWorksheet(oWB, sectionName + "Systems");
      float totalTime = 0.0f;
      int row = 1;
      foreach (CSystem s in systemList)
      {
        s.m_timeInMS = 0.0f;
      }
      foreach (FrameRecord fr in m_logData.m_frameRecords)
      {
        if (fr.FrameTimeInS > startTime && fr.FrameTimeInS < endTime)
				{
					foreach (KeyValuePair<string, CPerfStatRecordData> moduleRDPair in fr.m_data.m_children["Modules"].m_children)
					{
						foreach (KeyValuePair<string, CPerfStatRecordData> functionRDPair in moduleRDPair.Value.m_children)
						{
							if ((!functionRDPair.Value.m_values.ContainsKey("spike") || functionRDPair.Value.m_values["spike"] == 0.0f) && !functionRDPair.Key.Contains("CFrameProfileSystem::EndFrame"))
							{
								CSystem found = null;
								foreach (CSystem s in systemList)
								{
									if (s.m_name == moduleRDPair.Key)
									{
										found = s;
										break;
									}
								}
								found.m_timeInMS += functionRDPair.Value.m_values["selfTimeInMS"];
								totalTime += functionRDPair.Value.m_values["selfTimeInMS"];
							}
						}
					}
				}
      }
      systemList.Sort();
      foreach (CSystem s in systemList)
      {
        oSheet.Cells[row, 1] = string.Format("{0:s}", s.m_name);
        oSheet.Cells[row, 2] = string.Format("{0:g}", 100.0f * s.m_timeInMS / totalTime);
        row++;
      }
      
      Excel._Chart oChart = MakeChart(oWB, oSheet.get_Range("A1", string.Format("B{0:g}", row - 1)), Excel.XlChartType.xl3DPieExploded, Excel.XlRowCol.xlColumns, 0, 0, false);
      oChart.Elevation = 60;
      oChart.ApplyDataLabels(Excel.XlDataLabelsType.xlDataLabelsShowLabel, Missing.Value, Missing.Value, Missing.Value, Missing.Value, Missing.Value, Missing.Value, Missing.Value, Missing.Value, Missing.Value);
      
      Excel.Series oSeries = (Excel.Series)oChart.SeriesCollection(1);
      for (int i=0; i<systemList.Count(); i++)
      {
        Excel.Point oPoint = (Excel.Point)oSeries.Points(i + 1);
        if (systemList[i].m_name != profileSystem)
        {
          oPoint.Format.Fill.ForeColor.RGB = oPoint.Format.Fill.ForeColor.RGB; // Needed to get transparency to work
          oPoint.Format.Fill.Transparency = 0.6f;
        }
      }

      Excel._Chart oSystems = oChart;

      oSheet = AddWorksheet(oWB, sectionName + "Breakdown");
      row = 1;

			List<CProfileFunction> profileList = new List<CProfileFunction>();
      totalTime = 0.0f;
      foreach (FrameRecord fr in m_logData.m_frameRecords)
      {
        if (fr.FrameTimeInS > startTime && fr.FrameTimeInS < endTime)
        {
					foreach (KeyValuePair<string, CPerfStatRecordData> moduleRDPair in fr.m_data.m_children["Modules"].m_children)
					{
						foreach (KeyValuePair<string, CPerfStatRecordData> functionRDPair in moduleRDPair.Value.m_children)
						{
							if ((!functionRDPair.Value.m_values.ContainsKey("spike") || functionRDPair.Value.m_values["spike"] == 0.0f) && !functionRDPair.Key.Contains("CFrameProfileSystem::EndFrame"))
							{
								if (moduleRDPair.Key == profileSystem)
								{
									CProfileFunction found = null;
									foreach (CProfileFunction r in profileList)
									{
										if (functionRDPair.Key == r.m_name)
										{
											found = r;
											break;
										}
									}
									if (found == null)
									{
										found = new CProfileFunction(functionRDPair.Key);
										profileList.Add(found);
									}
									found.m_selfTimeInMS += functionRDPair.Value.m_values["selfTimeInMS"];
									totalTime += functionRDPair.Value.m_values["selfTimeInMS"];
								}
							}
						}
					}
        }
      }
      profileList.Sort();
      float percentLeft = 100.0f;
      foreach (CProfileFunction r in profileList)
      {
        float percent = 100.0f * r.m_selfTimeInMS / totalTime;
        if (percent < 5.0f && percentLeft < 25.0f)
          break;
        percentLeft -= percent;
        oSheet.Cells[row, 1] = string.Format("{0:s}", r.m_name);
        oSheet.Cells[row, 2] = string.Format("{0:g}", percent);
        row++;
      }
      oSheet.Cells[row, 1] = "Other";
      oSheet.Cells[row, 2] = string.Format("{0:g}", percentLeft);

      oChart = MakeChart(oWB, oSheet.get_Range("A1", string.Format("B{0:g}", row)), Excel.XlChartType.xl3DPieExploded, Excel.XlRowCol.xlColumns, 0, 0, false);
      oChart.Elevation = 60;
      oChart.ApplyDataLabels(Excel.XlDataLabelsType.xlDataLabelsShowLabel, Missing.Value, Missing.Value, Missing.Value, Missing.Value, Missing.Value, Missing.Value, Missing.Value, Missing.Value, Missing.Value);
      Word.InlineShape oShape = InsertChart(oChart, oDoc, sectionName + "breakdown", bookmarkName, 500, 500, 0.4f);
      oShape = InsertChart(oSystems, oDoc, sectionName + "pie", bookmarkName, 500, 500, 0.4f);
			 * */
    }

    void BuildGPUGraphs(Excel._Workbook oWB, Word.Document oDoc)
    {
			/*
      Excel._Worksheet oSheet = AddWorksheet(oWB, "GPU Info");
      const float aspect = 0.35f;
      int row = 1;
      int k_step = Math.Max(m_logData.m_frameRecords.Count() / 500, 50);
      int count = k_step;
      oSheet.Cells[row, 1] = "Time";
      oSheet.Cells[row, 2] = "GPU Time";
      oSheet.Cells[row, 3] = "Time";
      oSheet.Cells[row, 4] = "Draw Calls";
      oSheet.Cells[row, 5] = "Time";
      oSheet.Cells[row, 6] = "Num Tris";
      row++;
      float GPUTime = 0.0f;
      Int64 drawCalls = 0;
      Int64 numTris = 0;
      foreach (FrameRecord fr in m_logData.m_frameRecords)
      {
        GPUTime += fr.m_data.m_values["GPUFrameLengthMAInMS"];
        drawCalls += (Int64)fr.m_data.m_values["numDrawCalls"];
        numTris += (Int64)fr.m_data.m_values["numTris"];
        count--;
        if (count == 0)
        {
          oSheet.Cells[row, 1] = string.Format("{0:g}", fr.FrameTimeInS);
          oSheet.Cells[row, 2] = string.Format("{0:g}", GPUTime / k_step);
          oSheet.Cells[row, 3] = string.Format("{0:g}", 32.0f);
          oSheet.Cells[row, 4] = string.Format("{0:g}", fr.FrameTimeInS);
          oSheet.Cells[row, 5] = string.Format("{0:g}", drawCalls / k_step);
          oSheet.Cells[row, 6] = string.Format("{0:g}", 2000);
          oSheet.Cells[row, 7] = string.Format("{0:g}", fr.FrameTimeInS);
          oSheet.Cells[row, 8] = string.Format("{0:g}", numTris / k_step);
          oSheet.Cells[row, 9] = string.Format("{0:g}", 750000);
          row++;
          GPUTime = 0.0f;
          drawCalls = 0;
          numTris = 0;
          count = k_step;
        }
      }

      Excel._Chart oChart = MakeChart(oWB, oSheet.get_Range("G2", string.Format("I{0:g}", row - 1)), Excel.XlChartType.xlXYScatterLinesNoMarkers, Excel.XlRowCol.xlColumns, 1, 0, false);
      SetupAxes(oChart, "Time (s)", 0.0f, -1.0f, "Triangles", 0.0f, -1.0f);
      Word.InlineShape oShape = InsertChart(oChart, oDoc, "triangles", "GPUAnalysis", 1000, 1000 * aspect, 0.375f);

      oChart = MakeChart(oWB, oSheet.get_Range("D2", string.Format("F{0:g}", row - 1)), Excel.XlChartType.xlXYScatterLinesNoMarkers, Excel.XlRowCol.xlColumns, 1, 0, false);
      SetupAxes(oChart, "Time (s)", 0.0f, -1.0f, "Draw Calls", 0.0f, -1.0f);
      oShape = InsertChart(oChart, oDoc, "triangles", "GPUAnalysis", 1000, 1000 * aspect, 0.375f);

      oChart = MakeChart(oWB, oSheet.get_Range("A2", string.Format("C{0:g}", row - 1)), Excel.XlChartType.xlXYScatterLinesNoMarkers, Excel.XlRowCol.xlColumns, 1, 0, false);
      SetupAxes(oChart, "Time (s)", 0.0f, -1.0f, "GPU Time (ms)", 0.0f, -1.0f);
      oShape = InsertChart(oChart, oDoc, "triangles", "GPUAnalysis", 1000, 1000 * aspect, 0.375f);
			 * */
    }

    void BuildFramerateAnalysis(Excel._Workbook oWB, Word.Document oDoc, GetBucketValue grv, string uniqueName, string bookmarkName, float k_ignoreTime, int maxFPS)
    {
      const float k_fpsSize = 400.0f;
      const float aspect = 3.0f / 4.0f;
      Excel._Worksheet oSheet = AddWorksheet(oWB, uniqueName);
      FillBuckets(oSheet, grv, 0, maxFPS, k_ignoreTime);
      Excel._Chart oChart = MakeChart(oWB, oSheet.get_Range("C1", "D61"), Excel.XlChartType.xlXYScatterSmoothNoMarkers, Excel.XlRowCol.xlColumns, 1, 0, false);
      SetupAxes(oChart, "Frame Rate", 0.0f, maxFPS, "Percentage of Time Below", 0.0f, -1.0f);
      Word.InlineShape oShape = InsertChart(oChart, oDoc, uniqueName+"acc", bookmarkName, k_fpsSize, k_fpsSize * aspect, 0.5f);
      oChart = MakeChart(oWB, oSheet.get_Range("A1", "B61"), Excel.XlChartType.xlXYScatterSmoothNoMarkers, Excel.XlRowCol.xlColumns, 1, 0, false);
      SetupAxes(oChart, "Frame Rate", 0.0f, maxFPS, "Percentage of Time", 0.0f, -1.0f);
      oShape = InsertChart(oChart, oDoc, uniqueName, bookmarkName, k_fpsSize, k_fpsSize * aspect, 0.5f);
    }

    void GenerateReport()
		{
			Word.Application oWord = null;
			Word.Document oDoc;
			Excel.Application oXL = null;
			Excel._Workbook oWB;
			object oMissing = Missing.Value;
			const float k_ignoreTime = 1.0f;

			oXL = new Excel.Application();
			oXL.Visible = true;
			oWB = (Excel._Workbook)oXL.Workbooks.Add(Missing.Value);

			oWord = new Word.Application();
			oWord.Visible = true;
			object fileName = Application.StartupPath + @"\Autogen.doc";
			oDoc = oWord.Documents.Open(ref fileName, ref oMissing, ref oMissing, ref oMissing, ref oMissing, ref oMissing, ref oMissing, ref oMissing, ref oMissing, ref oMissing, ref oMissing, ref oMissing, ref oMissing, ref oMissing, ref oMissing, ref oMissing);

			//AddText(oDoc, "LevelName", m_logFilename);

			//BuildFramerateAnalysis(oWB, oDoc, fr => new BucketValue(0.001f * fr.m_data.m_values["frameLengthInMS"]), "fpsanal", "FramerateGraph", k_ignoreTime, 60);

			List<CSpike> spikeList = new List<CSpike>();
			CreateSpikeList(spikeList);
			BuildSpikeTable(oDoc, spikeList);

			List<CSystem> systemList = new List<CSystem>();
			BuildSystemGraphs(oWB, oDoc, systemList, k_ignoreTime);

			if (m_markers[0] != null && m_markers[0].m_end != null)
				BuildProfileSectionGraphs(oWB, oDoc, systemList, "point1", m_markers[0].m_system, m_markers[0].m_start.FrameTimeInS, m_markers[0].m_end.FrameTimeInS, "Point1Analysis");
			if (m_markers[1] != null && m_markers[1].m_end != null)
				BuildProfileSectionGraphs(oWB, oDoc, systemList, "point2", m_markers[1].m_system, m_markers[1].m_start.FrameTimeInS, m_markers[1].m_end.FrameTimeInS, "Point2Analysis");

			BuildGPUGraphs(oWB, oDoc);

      //BuildFramerateAnalysis(oWB, oDoc, fr => new BucketValue(0.001f * fr.m_data.m_values["frameLengthInMS"], 1000.0f / fr.m_data.m_values["GPUFrameLengthMAInMS"]), "fpsanalgpu", "FramerateGraphGPU", k_ignoreTime, 60);
			//BuildFramerateAnalysis(oWB, oDoc, fr => GetFrameTimeMinusSpikes(fr), "fpsanalnospikes", "FramerateGraphNoSpikes", k_ignoreTime, 30);

      /*
       * No longer works after recent changes
      foreach (CSystem sys in systemList)
      {
        foreach (KeyValuePair<string, CProfilerRecordDisplayInfo> prdiPair in m_profilerRecordDisplayInfoTree[sys.m_name])
        {
          CProfilerRecordDisplayInfo rdi = prdiPair.Value;
          if (!rdi.m_displayed) // being excluded from expectations graph
          {
            float maxSaving = 0.0f;
            float saving = 0.0f;
            uint frames = 0;
            foreach (FrameRecord fr in m_logData.m_frameRecords)
            {
              foreach (KeyValuePair<string, CPerfStatRecordData> moduleRDPair in fr.m_data.m_children["Modules"].m_children)
              {
                if (moduleRDPair.Key==sys.m_name)
                {
                  if (moduleRDPair.Value.m_children.ContainsKey(prdiPair.Key))
                  {
                    CPerfStatRecordData rd=moduleRDPair.Value.m_children[prdiPair.Key];
									  saving+=rd.m_values["selfTimeInMS"];
                    maxSaving = Math.Max(maxSaving, rd.m_values["selfTimeInMS"]);
                    frames++;
                  }
                }
              }
            }
            if (maxSaving > 0.0f)
            {
              AddText(oDoc, "FramerateGraphNoSpikes", string.Format("{0:s} {1:d} {2:g} {3:g} {4:g}\n", prdiPair.Key, frames, saving, maxSaving, saving / frames));
            }
          }
        }
      }
      */

			oXL.Quit();
			((Word._Application)oWord).Quit(ref oMissing, ref oMissing, ref oMissing);
		}

    private void generateToolStripMenuItem_Click(object sender, EventArgs e)
    {
      GenerateReport();
    }
#endif // __MonoCS__
  }
}

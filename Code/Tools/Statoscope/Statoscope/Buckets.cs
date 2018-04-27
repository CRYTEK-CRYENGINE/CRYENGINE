// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Windows.Forms;
using System.ComponentModel;
using System.Reflection;

namespace Statoscope
{
  public class Bucket
  {
    private string m_rangeStr;

    [Browsable(false)]
    public string RangeDescription
    {
      get { return m_rangeStr; }
      set { m_rangeStr = value; }
    }

    public Bucket()
    {
    }

    public Bucket(string rangeDesc)
    {
      m_rangeStr = rangeDesc;
    }
  }

	class TimeBucket : Bucket
	{
    [Description("Count")]
    public int Num
    {
      get { return m_num; }
    }

    [Description("% Count")]
    public float NumPc
    {
      get { return m_numPercent; }
    }

    [Description("% Cumulative")]
    public float NumPcCumulative
    {
      get { return m_numPercentCumulative; }
    }

    [Description("Time")]
    public TimeSpan Time
    {
      get { return new TimeSpan((long)((double)m_timeInMS * 1000.0 * 10.0)); }
    }

    [Description("% Time")]
    public float TimePc
    {
      get { return m_timePercent; }
    }

    [Description("% Cumulative")]
    public float TimePcCumulative
    {
      get { return m_timePercentCumulative; }
    }

    [Description("Time (5fps clamp)")]
    public TimeSpan Time5FpsClamp
    {
      get { return new TimeSpan((long)((double)m_timeInMSClampingTo5fps * 1000.0 * 10.0)); }
    }

    [Description("% Time")]
    public float Time5FpsClampPc
    {
      get { return m_timeClamp5Percent; }
    }

    [Description("% Cumulative")]
    public float Time5FpsClampPcCumulative
    {
      get { return m_timeClamp5PercentCumulative; }
    }

		public int m_num;
		public float m_numPercent;
		public float m_numPercentCumulative;

		public float m_timeInMS;
		public float m_timePercent;
		public float m_timePercentCumulative;

		public float m_timeInMSClampingTo5fps;
		public float m_timeClamp5Percent;
		public float m_timeClamp5PercentCumulative;
	}

  class FrameTimeBucket : TimeBucket
  {
    [Description("Frames")]
    public new int Num
    {
      get { return base.Num; }
    }
  }

  class AllocationBucket : Bucket
  {
    [Description("Allocs")]
    public int Num
    {
      get { return m_num; }
    }

    [Description("% Count")]
    public float NumPc
    {
      get { return m_numPercent; }
    }

    [Description("% Cumulative")]
    public float NumPcCumulative
    {
      get { return m_numPercentCumulative; }
    }

		public int m_num;
		public float m_numPercent;
		public float m_numPercentCumulative = 0.0f;
  }

	public class BucketSet
	{
		public List<Bucket> m_buckets;

    public virtual bool IsEmpty
    {
      get { return true; }
    }

		public BucketSet(List<Bucket> buckets)
		{
			m_buckets = buckets;
		}
	}

  class TimeBucketSet : BucketSet
  {
		// <-- only used for intermediate calculations
		public int m_cumulativeNum;
		public float m_cumulativeTimeInMS;
		public float m_cumulativeTimeInMSClampingTo5fps;
		// -->

    public override bool IsEmpty
    {
      get { return m_cumulativeNum == 0; }
    }

    public TimeBucketSet(List<Bucket> buckets)
      : base(buckets)
    {
			m_cumulativeNum = 0;
			m_cumulativeTimeInMS = 0.0f;
			m_cumulativeTimeInMSClampingTo5fps = 0.0f;
    }
  }

  class AllocationBucketSet : BucketSet
  {
		// <-- only used for intermediate calculations
		public int m_totalAllocs;
    // -->

    public override bool IsEmpty
    {
      get { return m_totalAllocs == 0; }
    }

    public AllocationBucketSet(List<Bucket> buckets)
      : base(buckets)
    {
      m_totalAllocs = 0;
    }
  }

	partial class LogData
	{
		public Dictionary<string, BucketSet> m_bucketSets = new Dictionary<string, BucketSet>();

		public void CalculateBucketSets()
		{
			CalculateBucketSets(0);
		}

		public void CalculateBucketSets(int fromIdx)
		{
			lock (m_bucketSets)
			{
				GetRecordValue getFrameLengthInMS = fr => fr.Values["/frameLengthInMS"];
				CalculateBucketSet<FrameTimeBucket>("Overall fps", fr => fr.Values.ContainsPath("/frameLengthInMS"), fr => fr.Values["/frameLengthInMS"] > 0.0f ? 1000.0f / fr.Values["/frameLengthInMS"] : 0.0f, getFrameLengthInMS, 0, 5, 35, false, fromIdx);
				CalculateBucketSet<FrameTimeBucket>("RT fps", fr => fr.Values.ContainsPath("/Threading/RTLoadInMS") && fr.Values.ContainsPath("/frameLengthInMS"), fr => fr.Values["/Threading/RTLoadInMS"] > 0.0f ? 1000.0f / fr.Values["/Threading/RTLoadInMS"] : 0.0f, getFrameLengthInMS, 0, 5, 35, false, fromIdx);
				CalculateBucketSet<FrameTimeBucket>("GPU fps", fr => fr.Values.ContainsPath("/Graphics/GPUFrameLengthInMS") && fr.Values.ContainsPath("/frameLengthInMS"), fr => fr.Values["/Graphics/GPUFrameLengthInMS"] > 0.0f ? 1000.0f / fr.Values["/Graphics/GPUFrameLengthInMS"] : 0.0f, getFrameLengthInMS, 0, 5, 50, false, fromIdx);
				CalculateBucketSet<TimeBucket>("Triangles", fr => fr.Values.ContainsPath("/Graphics/numTris") && fr.Values.ContainsPath("/frameLengthInMS"), fr => fr.Values["/Graphics/numTris"], getFrameLengthInMS, 300000, 100000, 1000000, false, fromIdx);
				CalculateBucketSet<TimeBucket>("Total Draw Calls", fr => fr.Values.ContainsPath("/Graphics/numTotalDrawCalls") && fr.Values.ContainsPath("/frameLengthInMS"), fr => fr.Values["/Graphics/numTotalDrawCalls"], getFrameLengthInMS, 0, 500, 3000, true, fromIdx);
				CalculateBucketSet<TimeBucket>("Shadow Draw Calls", fr => fr.Values.ContainsPath("/Graphics/numShadowDrawCalls") && fr.Values.ContainsPath("/frameLengthInMS"), fr => fr.Values["/Graphics/numShadowDrawCalls"], getFrameLengthInMS, 0, 500, 3000, true, fromIdx);
				CalculateBucketSet<TimeBucket>("Draw Calls", fr => fr.Values.ContainsPath("/Graphics/numDrawCalls") && fr.Values.ContainsPath("/frameLengthInMS"), fr => fr.Values["/Graphics/numDrawCalls"], getFrameLengthInMS, 0, 500, 3000, true, fromIdx);
				CalculateAllocationBucketSet("Texture Pool", "/TexturePool", 16, 2, 256, fromIdx);
			}
		}

		void CalculateBucketSet<T>(string name, AcceptRecord acceptRecord, GetRecordValue grv, GetRecordValue grvTime, float start, float range, float max, bool lessThanMode, int fromIdx) where T : Bucket, new()
		{
			List<Bucket> buckets;
			TimeBucketSet bucketSet;

			if (m_bucketSets.ContainsKey(name))
			{
				bucketSet = (TimeBucketSet)m_bucketSets[name];
				buckets = bucketSet.m_buckets;
			}
			else
			{
				buckets = new List<Bucket>();

				int numBuckets = (int)((max - start) / range) + (start == 0.0f ? 1 : 2);
				for (int i = 0; i < numBuckets; i++)
				{
					buckets.Add(new T());
				}

        bucketSet = new TimeBucketSet(buckets);
				m_bucketSets.Add(name, bucketSet);
			}

			for (int i = fromIdx; i < FrameRecords.Count; i++)
			{
				FrameRecord fr = FrameRecords[i];

				if (!acceptRecord(fr))
					continue;

				float value = grv(fr);
				float frameTimeInMS = grvTime(fr);

				if (float.IsInfinity(value))
					value = 0.0f;

				int offsetScaledValue = (int)((value - start) / range);

				int bucketIdx;

				if (start == 0.0f)
				{
					bucketIdx = offsetScaledValue;
				}
				else
				{
					bucketIdx = (offsetScaledValue < 0) ? 0 : (offsetScaledValue + 1);
				}

				bucketIdx = Math.Max(0, Math.Min(bucketIdx, buckets.Count - 1));
				bucketIdx = buckets.Count - 1 - bucketIdx;

        TimeBucket bucket = buckets[bucketIdx] as TimeBucket;

				bucket.m_num++;
				bucket.m_timeInMS += frameTimeInMS;
				bucket.m_timeInMSClampingTo5fps += Math.Min(frameTimeInMS, 1000.0f / 5.0f);

				bucketSet.m_cumulativeNum++;
				bucketSet.m_cumulativeTimeInMS += frameTimeInMS;
				bucketSet.m_cumulativeTimeInMSClampingTo5fps += Math.Min(frameTimeInMS, 1000.0f / 5.0f);
			}

			float numPercentCumulative = 0.0f;
			float timePercentCumulative = 0.0f;
			float timeClamp5PercentCumulative = 0.0f;

			if( lessThanMode )
			{
				numPercentCumulative = 100.0f;
				timePercentCumulative = 100.0f;
				timeClamp5PercentCumulative = 100.0f;
			}

			for (int i = 0; i < buckets.Count; i++)
			{
				TimeBucket bucket = buckets[i] as TimeBucket;

				bucket.m_numPercent = 100.0f * bucket.m_num / (float)bucketSet.m_cumulativeNum;
				bucket.m_timePercent = 100.0f * bucket.m_timeInMS / bucketSet.m_cumulativeTimeInMS;
				bucket.m_timeClamp5Percent = 100.0f * bucket.m_timeInMSClampingTo5fps / bucketSet.m_cumulativeTimeInMSClampingTo5fps;

				if( !lessThanMode )
				{
					numPercentCumulative += bucket.m_numPercent;
					timePercentCumulative += bucket.m_timePercent;
					timeClamp5PercentCumulative += bucket.m_timeClamp5Percent;
				}

				bucket.m_numPercentCumulative = numPercentCumulative;
				bucket.m_timePercentCumulative = timePercentCumulative;
				bucket.m_timeClamp5PercentCumulative = timeClamp5PercentCumulative;

				if( lessThanMode )
				{
					numPercentCumulative -= bucket.m_numPercent;
					timePercentCumulative -= bucket.m_timePercent;
					timeClamp5PercentCumulative -= bucket.m_timeClamp5Percent;
				}

				float begin;
				float end;
				int bucketIdx = buckets.Count - 1 - i;

				if (start == 0.0f)
				{
					begin = bucketIdx * range;
					end = (bucketIdx + 1) * range;
				}
				else
				{
					begin = (bucketIdx == 0) ? 0.0f : start + ((bucketIdx - 1) * range);
					end = start + (bucketIdx * range);
				}

				if (bucketIdx < buckets.Count - 1)
				{
					if( lessThanMode )
					{
						bucket.RangeDescription = string.Format("<{0:n0}", end);
					}
					else
					{
						bucket.RangeDescription = string.Format("{0:n0} - {1:n0}", begin, end);
					}
				}
				else
				{
					if( lessThanMode )
					{
						bucket.RangeDescription = string.Format("{0:n0}+", begin);
					}
					else
					{
						bucket.RangeDescription = string.Format("{0:n0} -", begin);
					}
				}
			}
		}

		void CalculateAllocationBucketSet(string name, string path, float start, float multistep, float max, int fromIdx)
		{
			List<Bucket> buckets;
			AllocationBucketSet bucketSet;

			float bucketsize = start;

			if (m_bucketSets.ContainsKey(name))
			{
				bucketSet = (AllocationBucketSet)m_bucketSets[name];
				buckets = bucketSet.m_buckets;
			}
			else
			{
				buckets = new List<Bucket>();
				buckets.Add(new AllocationBucket());

				while (bucketsize <= max)
				{
					bucketsize *= multistep;
					buckets.Add(new AllocationBucket());
				}

        bucketSet = new AllocationBucketSet(buckets);
				m_bucketSets.Add(name, bucketSet);
			}

			for (int i = fromIdx; i < FrameRecords.Count; i++)
			{
				FrameRecord fr = FrameRecords[i];
				foreach (string valuepath in fr.Values.Paths)
				{
					if (!valuepath.StartsWith(path))
						continue;

					float value = fr.Values[valuepath] / 1024.0f;

					int bucketIdx = 0;
					bucketsize = start;
					while ( value > bucketsize && bucketsize <= max )
					{
						bucketsize *= multistep;
						bucketIdx++;
					}

					bucketIdx = Math.Min(bucketIdx, buckets.Count - 1);

          AllocationBucket bucket = buckets[bucketIdx] as AllocationBucket;

					bucket.m_num++;
					bucketSet.m_totalAllocs++;
				}
			}

			float begin = 0;
			float end = start;

			for (int bucketIdx = 0; bucketIdx < buckets.Count; bucketIdx++)
			{
				AllocationBucket bucket = buckets[bucketIdx] as AllocationBucket;
				
				bucket.m_numPercent = 100.0f * bucket.m_num / bucketSet.m_totalAllocs;
				
				if (bucketIdx < buckets.Count - 1)
				{
					bucket.RangeDescription = string.Format("{0:n0} - {1:n0}", begin, end);
				}
				else
				{
					bucket.RangeDescription = string.Format("{0:n0} -", begin);
				}
				begin = end;
				end *= multistep;
			}
		}
	}

	partial class LogControl
	{
		public static ColumnHeader CreateColumnHeader(string text, int width)
		{
			ColumnHeader ch = new ColumnHeader();
			ch.Text = text;
			ch.Width = width;
			ch.TextAlign = HorizontalAlignment.Right;
			return ch;
		}

		void AddBucketListTabPage(string name, BucketSet bucketSet)
		{
			TabPage tabPage = new TabPage(name);
			tabPage.Name = name;
			tabPage.Controls.Add(new BucketListView(bucketSet));
			bucketsTabControl.TabPages.Add(tabPage);
		}

		void CreateSummaryFPSEntry(ListView summaryListView, string lviLabel, TabPage[] tabPages, int itemIdx, int subItemIdx)
		{
			ListViewItem lvi = new ListViewItem(lviLabel);
			foreach (TabPage tabPage in tabPages)
			{
				if (tabPage != null)
				{
					string above20PCString = ((ListView)tabPage.Controls[0]).Items[itemIdx].SubItems[subItemIdx].Text;
					lvi.SubItems.Add(string.Format("{0:n2}", 100.0f - Convert.ToDouble(above20PCString.TrimEnd('%'))));
				}
			}
			summaryListView.Items.Add(lvi);
		}

		/*
		void GetStartEndPeakValues(GetRecordValue grv, out float start, out float end, out float peak)
		{
			start = grv(m_logData.FrameRecords[0]);
			end = grv(m_logData.FrameRecords[m_logData.FrameRecords.Count-1]);
			peak = start;

			foreach (FrameRecord fr in m_logData.FrameRecords)
			{
				peak = Math.Max(peak, grv(fr));
			}
		}
		*/

		void AddMemSummaryListViewItem(ListView summaryListView, string lviLabel, float mainMem, float gpuMem)
		{
			ListViewItem lvi = new ListViewItem(lviLabel);
			lvi.SubItems.Add(string.Format("{0}", mainMem));
			lvi.SubItems.Add(string.Format("{0}", gpuMem));
			summaryListView.Items.Add(lvi);
		}

		void CreateSummaryTabPage()
		{
			ListView summaryListView = new ListView();
			summaryListView.View = View.Details;
			summaryListView.Columns.Add(CreateColumnHeader("Stat", 175));
			summaryListView.Columns.Add(CreateColumnHeader("Main", 100));
			summaryListView.Columns.Add(CreateColumnHeader("GPU/RSX", 100));
			summaryListView.Dock = DockStyle.Fill;
			summaryListView.GridLines = true;
			summaryListView.FullRowSelect = true;

			TabPage summaryTabPage = new TabPage("Summary");
			summaryTabPage.Controls.Add(summaryListView);
			bucketsTabControl.TabPages.Add(summaryTabPage);

			TabPage[] tabPages = { bucketsTabControl.TabPages["Overall fps"], bucketsTabControl.TabPages["GPU fps"] };
			CreateSummaryFPSEntry(summaryListView, "% time (5fps clamp) above 20", tabPages, 3, 9);
			CreateSummaryFPSEntry(summaryListView, "% time (5fps clamp) above 25", tabPages, 4, 9);
			CreateSummaryFPSEntry(summaryListView, "% time (5fps clamp) above 30", tabPages, 5, 9);

			//float startMainMem, endMainMem, peakMainMem;
			//GetStartEndPeakValues(fr => fr.Values["/Memory/mainMemUsageInMB"], out startMainMem, out endMainMem, out peakMainMem);

			//float startGPUMem, endGPUMem, peakGPUMem;
			//GetStartEndPeakValues(fr => fr.Values["/Memory/vidMemUsageInMB"], out startGPUMem, out endGPUMem, out peakGPUMem);

			//AddMemSummaryListViewItem(summaryListView, "start mem (MB)", startMainMem, startGPUMem);
			//AddMemSummaryListViewItem(summaryListView, "end mem (MB)", endMainMem, endGPUMem);
			//AddMemSummaryListViewItem(summaryListView, "peak mem (MB)", peakMainMem, peakGPUMem);
		}
	}
}

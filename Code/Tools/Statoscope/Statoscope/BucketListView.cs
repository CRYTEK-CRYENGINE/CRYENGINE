// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Reflection;
using System.ComponentModel;
using System.Windows.Forms;

namespace Statoscope
{
  class BucketListView : ListView
  {
    public BucketListView(BucketSet bucketSet)
    {
      InitializeComponent();

      View = View.Details;
      Dock = DockStyle.Fill;
      GridLines = true;
      FullRowSelect = true;
      VirtualMode = true;
      VirtualListSize = bucketSet.m_buckets.Count;
      RetrieveVirtualItem += new RetrieveVirtualItemEventHandler(bucketsListView_RetrieveVirtualItem);
      CacheVirtualItems += new CacheVirtualItemsEventHandler(bucketsListView_CacheVirtualItems);
      StatoscopeForm.SetDoubleBuffered(this);

      int numColHeaders = CountHeaders(bucketSet);
      int colHeaderWidth = Math.Max(50, (Width) / numColHeaders);

      Columns.Add(LogControl.CreateColumnHeader("Bucket", colHeaderWidth));
      CreateHeaders(bucketSet, colHeaderWidth);

      m_bucketSet = bucketSet;
      m_lviCache = new List<ListViewItem>(bucketSet.m_buckets.Count);
      UpdateLviCache();
    }

    public void UpdateLviCache()
    {
      m_lviCache.Clear();

      foreach (Bucket bucket in m_bucketSet.m_buckets)
      {
        ListViewItem lvi = new ListViewItem(bucket.RangeDescription);

        foreach (PropertyInfo pi in m_columnProperties)
          lvi.SubItems.Add(GetPropertyValAsString(pi, bucket));

        m_lviCache.Add(lvi);
      }
    }

    protected override void OnCreateControl()
    {
      base.OnCreateControl();

      ContextMenuStrip = m_contextMenu;
      m_copyMenuItem.Click += new EventHandler(m_copyMenuItem_Click);
    }

    void bucketsListView_CacheVirtualItems(object sender, CacheVirtualItemsEventArgs e)
    {
      BucketListView blv = (BucketListView)sender;
      blv.UpdateLviCache();
    }

    void bucketsListView_RetrieveVirtualItem(object sender, RetrieveVirtualItemEventArgs e)
    {
      BucketListView blv = (BucketListView)sender;
      e.Item = blv.m_lviCache[e.ItemIndex];
    }

    private string GetPropertyValAsString(PropertyInfo pi, Bucket b)
    {
      object val = pi.GetValue(b, null);

      string s = string.Empty;
      if (val is int)
        s = string.Format("{0:n0}", val);
      else if (val is float)
        s = string.Format("{0:n2}%", val);
      else if (val is TimeSpan)
        s = string.Format("{0:n2}s", ((TimeSpan)val).TotalMilliseconds / 1000.0f);

      return s;
    }

    void m_copyMenuItem_Click(object sender, EventArgs e)
    {
      StringBuilder sb = new StringBuilder();

      sb.Append("Bucket\t");
      foreach (PropertyInfo pi in m_columnProperties)
        sb.AppendFormat("{0}\t", pi.Name.ToString());
      sb.AppendLine();

      foreach (Bucket bucket in m_bucketSet.m_buckets)
      {
        sb.AppendFormat("{0}\t", bucket.RangeDescription);
        foreach (PropertyInfo pi in m_columnProperties)
          sb.AppendFormat("{0}\t", GetPropertyValAsString(pi, bucket));

        sb.AppendLine();
      }

      Clipboard.SetText(sb.ToString());
    }

    private Attribute GetFirstAttribute(PropertyInfo pi, Type attrType, bool inherits)
    {
      foreach (var attr in pi.GetCustomAttributes(attrType, inherits))
        return (Attribute)attr;
      return null;
    }

    private IEnumerable<PropertyInfo> EnumColumns(BucketSet bset)
    {
      if (bset.m_buckets.Count > 0)
      {
        Type bucketType = bset.m_buckets[0].GetType();

        // Assuming that all buckets have the same type..

        foreach (PropertyInfo prop in bucketType.GetProperties())
        {
          // Ensure it's browseable
          var ba = GetFirstAttribute(prop, typeof(BrowsableAttribute), true);
          bool isBrowseable = (ba != null) ? ((BrowsableAttribute)ba).Browsable : true;

          if (isBrowseable)
            yield return prop;
        }
      }
    }

    private int CountHeaders(BucketSet bucketSet)
    {
      int numHeaders = 0;

      foreach (var pi in EnumColumns(bucketSet))
        ++numHeaders;

      return numHeaders;
    }

    private void CreateHeaders(BucketSet bucketSet, int colHeaderWidth)
    {
      m_columnProperties = new List<PropertyInfo>();
      foreach (var pi in EnumColumns(bucketSet))
      {
        var descAttr = GetFirstAttribute(pi, typeof(DescriptionAttribute), true);
        string desc = descAttr != null ? ((DescriptionAttribute)descAttr).Description : pi.Name;
        Columns.Add(LogControl.CreateColumnHeader(desc, colHeaderWidth));
        m_columnProperties.Add(pi);
      }
    }

    private BucketSet m_bucketSet;
    private List<PropertyInfo> m_columnProperties;
    private IContainer components;
    private ContextMenuStrip m_contextMenu;
    private ToolStripMenuItem m_copyMenuItem;
    private List<ListViewItem> m_lviCache;

    private void InitializeComponent()
    {
      this.components = new System.ComponentModel.Container();
      this.m_contextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
      this.m_copyMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.m_contextMenu.SuspendLayout();
      this.SuspendLayout();
      // 
      // m_contextMenu
      // 
      this.m_contextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.m_copyMenuItem});
      this.m_contextMenu.Name = "m_contextMenu";
      this.m_contextMenu.Size = new System.Drawing.Size(103, 26);
      // 
      // m_copyMenuItem
      // 
      this.m_copyMenuItem.Name = "m_copyMenuItem";
      this.m_copyMenuItem.Size = new System.Drawing.Size(102, 22);
      this.m_copyMenuItem.Text = "&Copy";
      this.m_contextMenu.ResumeLayout(false);
      this.ResumeLayout(false);

    }
  }
}

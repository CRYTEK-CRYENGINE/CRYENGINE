// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.IO;
using System.Reflection;
using System.Windows.Forms;
using System.Collections.Generic;
using System.Text;

namespace Aga.Controls
{
    public static class ResourceHelper
    {
        // VSpilt Cursor with Innerline (symbolisize hidden column)
        private static Cursor _dVSplitCursor = GetCursor(Properties.Resources.DVSplit);
        public static Cursor DVSplitCursor
        {
            get { return _dVSplitCursor; }
        }

		private static GifDecoder _loadingIcon = GetGifDecoder(Properties.Resources.loading_icon);
		public static GifDecoder LoadingIcon
		{
			get { return _loadingIcon; }
		}

        /// <summary>
        /// Help function to convert byte[] from resource into Cursor Type 
        /// </summary>
        /// <param name="data"></param>
        /// <returns></returns>
        private static Cursor GetCursor(byte[] data)
        {
            using (MemoryStream s = new MemoryStream(data))
                return new Cursor(s);
        }

		/// <summary>
		/// Help function to convert byte[] from resource into GifDecoder Type 
		/// </summary>
		/// <param name="data"></param>
		/// <returns></returns>
		private static GifDecoder GetGifDecoder(byte[] data)
		{
			using(MemoryStream ms = new MemoryStream(data))
				return new GifDecoder(ms, true);
		}

    }
}

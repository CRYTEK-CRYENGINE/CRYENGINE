// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Collections.Generic;

namespace CryEngine.Resources
{
	/// <summary>
	/// Holds a reference to all ImageSource objects which have been instantiated though it to avoid multiple instancing.
	/// </summary>
	public class ResourceManager
	{
		static ResourceManager _instance;

		/// <summary>
		/// Called to instantiate the Singleton class or to get the current instance.
		/// </summary>
		public static ResourceManager Instance { get { return _instance ?? (_instance = new ResourceManager()); } }

		readonly Dictionary<string, ImageSource> _imageSources = new Dictionary<string, ImageSource>();

		/// <summary>
		/// Creates an ImageSource if not yet existing, hands out the existing one otherwise.
		/// </summary>
		/// <returns>The created or registered ImageSource.</returns>
		/// <param name="url">Image source file.</param>
		/// <param name="filtered">If set to <c>true</c>, texture's high quality filtering is enabled.</param>
		public static ImageSource ImageFromFile(string url, bool filtered = true)
		{
			if (Instance._imageSources.ContainsKey(url))
				return Instance._imageSources[url];
			return Instance._imageSources[url] = new ImageSource(url, filtered);
		}

		/// <summary>
		/// Removes all cached resources from manager.
		/// </summary>
		public static void Clear()
		{
			Instance._imageSources.Clear();
		}
	}
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.IO;
using System.Runtime.Serialization.Json;
using System.Text;

namespace CryEngine
{
	/// <summary>
	/// Supportive functions for Serialization.
	/// </summary>
	public static class Tools
	{
		/// <summary>
		/// Converts an object to JSON.
		/// </summary>
		/// <returns>JSON string.</returns>
		/// <param name="o">Target Object</param>
		public static string ToJSON(object o)
		{
			using(var ms = new MemoryStream())
			{
				new DataContractJsonSerializer(o.GetType()).WriteObject(ms, o);
				return Encoding.UTF8.GetString(ms.GetBuffer(), 0, (int)ms.Position);
			}
		}

		/// <summary>
		/// Parses a JSON string to generate an object of type T.
		/// </summary>
		/// <returns>The object.</returns>
		/// <param name="content">The JSON description of the target Object.</param>
		/// <typeparam name="T">The target type.</typeparam>
		public static T FromJSON<T>(string content)
		{
			return (T)FromJSON(content, typeof(T));
		}

		/// <summary>
		/// Parses a JSON string to generate an object of Type 't'.
		/// </summary>
		/// <returns>The object.</returns>
		/// <param name="content">The JSON description of the target Object.</param>
		/// <param name="t">The target type.</param>
		public static object FromJSON(string content, Type t)
		{
			using(var ms = new MemoryStream(Encoding.UTF8.GetBytes(content)))
				return new DataContractJsonSerializer(t).ReadObject(ms);
		}
	}

	/// <summary>
	/// Automatically updates delta time between frames. Can be used to normalize variable changes by current FPS.
	/// </summary>
	public static class FrameTime
	{
		/// <summary>
		/// Gets the last calculated frametime in seconds.
		/// </summary>
		/// <returns>The last.</returns>
		public static float Delta { get; internal set; }
	}
}

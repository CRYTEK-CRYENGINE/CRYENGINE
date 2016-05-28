// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Runtime.Serialization.Json;
using System.IO;
using System.Text;
using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Supportive functions for Serialization.
	/// </summary>
	public class Tools
	{
		/// <summary>
		/// Converts an object to JSON.
		/// </summary>
		/// <returns>JSON string.</returns>
		/// <param name="o">Target Object</param>
		public static string ToJSON(object o)
		{
			using (var ms = new MemoryStream())
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
			using (var ms = new MemoryStream(Encoding.UTF8.GetBytes(content)))
				return new DataContractJsonSerializer(t).ReadObject(ms);
		}
	}

	/// <summary>
	/// Provides some math for convenience.
	/// </summary>
	public static class Utils
	{
		public const float Deg2RadMul = (float)Math.PI / 180f;

		/// <summary>
		/// Convert degrees to radiants.
		/// </summary>
		/// <returns>The Radiant.</returns>
		/// <param name="degrees">Degrees.</param>
		public static float Deg2Rad(float degrees)
		{
			return degrees * Deg2RadMul; 
		}

		/// <summary>
		/// Convert Radiants to degrees.
		/// </summary>
		/// <returns>The degree.</returns>
		/// <param name="rad">RAD.</param>
		public static float Rad2Deg(float rad)
		{
			return rad / Deg2RadMul;
		}
	}

	/// <summary>
	/// Automatically updates delta time between frames. Can be used to normalize variable changes by current FPS.
	/// </summary>
	public static class FrameTime
	{
		private static float _deltaTime;

		/// <summary>
		/// Gets the last calculated frametime in seconds.
		/// </summary>
		/// <returns>The last.</returns>
		public static float Delta { get { return _deltaTime; } }

		/// <summary>
		/// Retrieves the last frametime from engine. Called internally.
		/// </summary>
		public static void Update()
		{			
			_deltaTime = Env.Timer.GetFrameTime();
		}
	}
}

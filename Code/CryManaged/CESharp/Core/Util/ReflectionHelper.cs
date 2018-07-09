using System;
using System.Reflection;

namespace CryEngine
{
	internal static class ReflectionHelper
	{
		public static string FindPluginInstance(Assembly assembly)
		{
			Type pluginType = typeof(ICryEnginePlugin);

			foreach (Type t in assembly.GetTypes())
			{
				if (pluginType.IsAssignableFrom(t))
				{
					return t.FullName;
				}
			}

			return null;
		}
	}
}
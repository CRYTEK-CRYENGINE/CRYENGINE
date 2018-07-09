// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;

namespace CryEngine.Animations
{
	/// <summary>
	/// Manages animation databases and controller definitions.
	/// </summary>
	public sealed class AnimationDatabaseManager
	{
		[SerializeValue]
		internal IAnimationDatabaseManager NativeHandle { get; private set; }

		internal AnimationDatabaseManager(IAnimationDatabaseManager nativeHandle)
		{
			NativeHandle = nativeHandle;
		}

		/// <summary>
		/// Loads and returns the <see cref="ControllerDefinition"/> found at the path specified by <paramref name="controllerDefinitionPath"/>.
		/// </summary>
		/// <returns>The <see cref="ControllerDefinition"/> if found, null otherwise.</returns>
		/// <param name="controllerDefinitionPath">Path to the file describing the <see cref="ControllerDefinition"/>.</param>
		public ControllerDefinition LoadControllerDefinition(string controllerDefinitionPath)
		{
			if(string.IsNullOrWhiteSpace(controllerDefinitionPath))
			{
				throw new ArgumentException(string.Format("{0} cannot be null or empty!", nameof(controllerDefinitionPath)));
			}

			var nativeObj = NativeHandle.LoadControllerDef(controllerDefinitionPath);
			return nativeObj != null ? new ControllerDefinition(nativeObj) : null;
		}

		/// <summary>
		/// Load the <see cref="AnimationDatabase"/> located at the specified <paramref name="databasePath"/>.
		/// </summary>
		/// <returns>The loaded <see cref="AnimationDatabase"/>.</returns>
		/// <param name="databasePath">Path to the database file.</param>
		public AnimationDatabase Load(string databasePath)
		{
			if(string.IsNullOrWhiteSpace(databasePath))
			{
				throw new ArgumentException(string.Format("{0} cannot be null or empty!", nameof(databasePath)));
			}

			var nativeObj = NativeHandle.Load(databasePath);

			return nativeObj != null ? new AnimationDatabase(nativeObj) : null;
		}

		/// <summary>
		/// Release this ControllerDefinition from the managed and unmanaged side.
		/// </summary>
		public void Release()
		{
			NativeHandle.Dispose();
			// TODO Set self to disposed.
		}
	}
}

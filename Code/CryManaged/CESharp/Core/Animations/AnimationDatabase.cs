// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Common;

namespace CryEngine.Animations
{
	/// <summary>
	/// Manages an animation database file and returns requested information from the database.
	/// </summary>
	public sealed class AnimationDatabase
	{
		/// <summary>
		/// The filename of the database-file.
		/// </summary>
		/// <value>The name of the file.</value>
		public string FileName { get { return NativeHandle.GetFilename(); } }

		[SerializeValue]
		internal IAnimationDatabase NativeHandle { get; private set; }

		internal AnimationDatabase(IAnimationDatabase nativeHandle)
		{
			NativeHandle = nativeHandle;
		}

		/// <summary>
		/// Use <paramref name="actionName"/> to find the ID of a fragment.
		/// </summary>
		/// <returns>The ID of the fragment.</returns>
		/// <param name="actionName">Identifying name of the fragment.</param>
		public int GetFragmentId(string actionName)
		{
			return NativeHandle.GetFragmentID(actionName);
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

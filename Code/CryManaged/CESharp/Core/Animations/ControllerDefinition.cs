// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Common;

namespace CryEngine.Animations
{
	/// <summary>
	/// Handles scope contexts and fragments for animations.
	/// </summary>
	public class ControllerDefinition
	{
		[SerializeValue]
		internal SControllerDef NativeHandle { get; private set; }

		internal ControllerDefinition(SControllerDef nativeHandle)
		{
			NativeHandle = nativeHandle;
		}

		/// <summary>
		/// Finds the ID of the context which name is the same as the value of <paramref name="contextName"/>.
		/// </summary>
		/// <returns>The ID of the context.</returns>
		/// <param name="contextName">The name of the context.</param>
		public int FindScopeContext(string contextName)
		{
			return NativeHandle.m_scopeContexts.Find(contextName);
		}

		/// <summary>
		/// Finds the ID of the fragment which's name is the same as <paramref name="fragmentName"/>.
		/// </summary>
		/// <returns>The ID of the fragment.</returns>
		/// <param name="fragmentName">Name of the fragment.</param>
		public int FindFragmentId(string fragmentName)
		{
			return NativeHandle.m_fragmentIDs.Find(fragmentName);
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

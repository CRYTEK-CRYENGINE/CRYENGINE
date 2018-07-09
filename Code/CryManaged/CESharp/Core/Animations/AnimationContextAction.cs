// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Common;

namespace CryEngine.Animations
{
	/// <summary>
	/// Defines an action of an animation context.
	/// </summary>
	public sealed class AnimationContextAction
	{
		[SerializeValue]
		internal AnimationContextActionList NativeHandle { get; private set; }

		internal AnimationContextAction(AnimationContextActionList nativeHandle)
		{
			NativeHandle = nativeHandle;
		}

		/// <summary>
		/// Creates a new <c>AnimationContextAction</c> that can be used on an <c>ActionController</c>.
		/// </summary>
		/// <returns>The animation context action.</returns>
		/// <param name="priority">Priority of the action.</param>
		/// <param name="fragmentId">Fragment ID.</param>
		/// <param name="tagState">Required TagState.</param>
		/// <param name="flags">Flags.</param>
		/// <param name="scopeMask">Scope mask.</param>
		/// <param name="userToken">User token.</param>
		public static AnimationContextAction CreateAnimationContextAction(int priority, int fragmentId, TagState tagState = TagState.Empty, uint flags = 0, uint scopeMask = 0, uint userToken = 0)
		{
			Common.TagState nativeTag = null;
			switch(tagState)
			{
			case TagState.Empty:
				nativeTag = new Common.TagState(ETagStateEmpty.TAG_STATE_EMPTY);
				break;
			case TagState.Full:
				nativeTag = new Common.TagState(ETagStateFull.TAG_STATE_FULL);
				break;
			}

			var nativeObj = AnimationContextActionList.CreateSAnimationContext(priority, fragmentId, nativeTag, flags, scopeMask, userToken);
			return nativeObj == null ? null : new AnimationContextAction(nativeObj);
		}
	}
}

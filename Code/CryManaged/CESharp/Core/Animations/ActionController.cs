// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;

using CryEngine.Common;

namespace CryEngine.Animations
{
	// This mirrors EResumeFlags in ICryMannequin.h
	/// <summary>
	/// Flags for definining the behaviour when Resume is being called on an <see cref="ActionController"/>.
	/// </summary>
	[Flags]
	public enum ResumeFlags
	{
		RestartAnimations              = 1,
		RestoreLoopingAnimationTime    = 2,
		RestoreNonLoopingAnimationTime = 4,
		Default                        = RestartAnimations | RestoreLoopingAnimationTime | RestoreNonLoopingAnimationTime
	}
	
	public sealed class ActionController
	{
		/// <summary>
		/// The amount of scopes in this <see cref="ActionController"/>.
		/// </summary>
		/// <value>The scopes count.</value>
		public int ScopesCount
		{
			get
			{
				return (int)NativeHandle.GetTotalScopes();
			}
		}

		/// <summary>
		/// The <see cref="CryEngine.Entity"/> that this <see cref="ActionController"/> belongs to.
		/// </summary>
		/// <value>The entity.</value>
		public Entity Entity
		{
			get
			{
				var nativeEntity = NativeHandle.GetEntity();
				return new Entity(nativeEntity);
			}
		}

		/// <summary>
		/// The ID of the <see cref="CryEngine.Entity"/> that this <see cref="ActionController"/> belongs to.
		/// </summary>
		/// <value>The ID of the entity identifier.</value>
		public EntitySystem.EntityId EntityId
		{
			get
			{
				return NativeHandle.GetEntityId();
			}
		}

		/// <summary>
		/// The timescale of the <see cref="ActionController"/>.
		/// </summary>
		/// <value>The time scale.</value>
		public float TimeScale
		{
			get
			{
				return NativeHandle.GetTimeScale();
			}
			set
			{
				NativeHandle.SetTimeScale(value);
			}
		}

		/// <summary>
		/// The <see cref="AnimationContext"/> of the <see cref="ActionController"/>.
		/// </summary>
		/// <value>The AnimationContext.</value>
		public AnimationContext Context
		{
			get
			{
				var nativeObj = NativeHandle.GetContext();
				return nativeObj == null ? null : new AnimationContext(nativeObj);
			}
		}

		internal IActionController NativeHandle { get; private set; }

		internal ActionController(IActionController actionController)
		{
			NativeHandle = actionController;
		}

		/// <summary>
		/// Completely resets the state of the <see cref="ActionController"/>.
		/// </summary>
		public void Reset()
		{
			NativeHandle.Reset();
		}

		/// <summary>
		/// Flushes all currently playing and queued actions.
		/// </summary>
		public void Flush()
		{
			NativeHandle.Flush();
		}

		/// <summary>
		/// Set the context of a scope of the <see cref="ActionController"/>.
		/// </summary>
		/// <param name="scopeContextId">Scope context identifier.</param>
		/// <param name="entity">Target Entity.</param>
		/// <param name="character">Target Character.</param>
		/// <param name="animationDatabase">Animation database.</param>
		public void SetScopeContext(uint scopeContextId, Entity entity, Character character, AnimationDatabase animationDatabase)
		{
			if(entity == null)
			{
				throw new ArgumentNullException(nameof(entity));
			}

			if(character == null)
			{
				throw new ArgumentNullException(nameof(character));
			}

			if(animationDatabase == null)
			{
				throw new ArgumentNullException(nameof(animationDatabase));
			}

			NativeHandle.SetScopeContext(scopeContextId, entity.NativeHandle, character.NativeHandle, animationDatabase.NativeHandle);
		}

		/// <summary>
		/// Clears the context of a scope.
		/// </summary>
		/// <param name="scopeContextId">ID of a scope.</param>
		/// <param name="flushAnimations">If set to <c>true</c> flush animations.</param>
		public void ClearScopeContext(uint scopeContextId, bool flushAnimations = true)
		{
			NativeHandle.ClearScopeContext(scopeContextId, flushAnimations);
		}

		/// <summary>
		/// Check if a scope is currently active or not.
		/// </summary>
		/// <returns><c>true</c>, if scope the scope is active, <c>false</c> otherwise.</returns>
		/// <param name="scopeId">Scope ID.</param>
		public bool IsScopeActive(uint scopeId)
		{
			return NativeHandle.IsScopeActive(scopeId);
		}

		/// <summary>
		/// Returns the ID of the scope with the value of <paramref name="scopeName"/> as its name.
		/// </summary>
		/// <returns>The ID of the scope.</returns>
		/// <param name="scopeName">Scope name.</param>
		public uint GetScopeId(string scopeName)
		{
			return NativeHandle.GetScopeID(scopeName);
		}

		/// <summary>
		/// Returns the ID of the fragment that corresponds to <paramref name="crc"/>.
		/// </summary>
		/// <returns>The ID of the fragment.</returns>
		/// <param name="crc">Crc.</param>
		public int GetFragmentId(uint crc)
		{
			return NativeHandle.GetFragID(crc);
		}

		/// <summary>
		/// Returns the global tag ID that corresponds to <paramref name="crc"/>.
		/// </summary>
		/// <returns>The ID of the global tag.</returns>
		/// <param name="crc">Crc.</param>
		public int GetGlobalTagId(uint crc)
		{
			return NativeHandle.GetGlobalTagID(crc);
		}

		/// <summary>
		/// Returns the ID of the fragment tag that corresponds to <paramref name="fragmentId"/> and <paramref name="crc"/>.
		/// </summary>
		/// <returns>The fragment tag identifier.</returns>
		/// <param name="fragmentId">Fragment identifier.</param>
		/// <param name="crc">Crc.</param>
		public int GetFragmentTagId(int fragmentId, uint crc)
		{
			return NativeHandle.GetFragTagID(fragmentId, crc);
		}

		// TODO On the C++ side the Queue and Requeue methods accepts IActions, instead of AnimationContextAction which wraps a TAction<SAnimationContext>.

		/// <summary>
		/// Queue the specified <paramref name="action"/>.
		/// </summary>
		/// <param name="action">Action list.</param>
		public void Queue(AnimationContextAction action)
		{
			if(action == null)
			{
				throw new ArgumentNullException(nameof(action));
			}
			NativeHandle.Queue(action.NativeHandle);
		}

		/// <summary>
		/// Requeue the specified <paramref name="action"/>..
		/// </summary>
		/// <param name="action">Action list.</param>
		public void Requeue(AnimationContextAction action)
		{
			NativeHandle.Requeue(action.NativeHandle);
		}

		/// <summary>
		/// Set the slave <see cref="ActionController"/> of this <see cref="ActionController"/>.
		/// </summary>
		/// <param name="target">Target slave controller.</param>
		/// <param name="targetContext">Target context.</param>
		/// <param name="enslave">If set to <c>true</c> enslave.</param>
		/// <param name="targetDatabase">Target database.</param>
		public void SetSlaveController(ActionController target, uint targetContext, bool enslave, AnimationDatabase targetDatabase = null)
		{
			NativeHandle.SetSlaveController(target.NativeHandle, targetContext, enslave, targetDatabase?.NativeHandle);
		}

		/// <summary>
		/// Update the <see cref="ActionController"/> with the specified frameTime.
		/// </summary>
		/// <param name="frameTime">Frame time.</param>
		public void Update(float frameTime)
		{
			NativeHandle.Update(frameTime);
		}

		/// <summary>
		/// Pause the <see cref="ActionController"/>.
		/// </summary>
		public void Pause()
		{
			NativeHandle.Pause();
		}

		/// <summary>
		/// Resume the <see cref="ActionController"/>.
		/// </summary>
		/// <param name="resumeFlags">Flags to define the resume behaviour.</param>
		public void Resume(ResumeFlags resumeFlags = ResumeFlags.Default)
		{
			NativeHandle.Resume((uint)resumeFlags);
		}

		/// <summary>
		/// Release this <see cref="ActionController"/> from the managed and unmanaged side.
		/// </summary>
		public void Release()
		{
			NativeHandle.Release();
			NativeHandle.Dispose();
			// TODO Set self to disposed.
		}
	}
}

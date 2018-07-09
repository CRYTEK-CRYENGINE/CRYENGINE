// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Managed wrapper of the internal IView interface.
	/// </summary>
	public sealed class View
	{
		[SerializeValue]
		internal IView NativeHandle { get; private set; }
		internal View(IView nativeHandle)
		{
			NativeHandle = nativeHandle;
		}

		/// <summary>
		/// Creates a new <see cref="View"/> that can be used to modify the camera-properties.
		/// </summary>
		/// <returns>The created <see cref="View"/>.</returns>
		public static View Create()
		{
			var nativeObj = Engine.GameFramework.GetIViewSystem().CreateView();

			return nativeObj == null ? null : new View(nativeObj);
		}

		/// <summary>
		/// Removes this <see cref="View"/> from the game.
		/// </summary>
		public void RemoveView()
		{
			Engine.GameFramework.GetIViewSystem().RemoveView(NativeHandle);
			NativeHandle.Dispose();
		}

		/// <summary>
		/// Link this view to an <see cref="Entity"/>.
		/// </summary>
		/// <param name="entity"><see cref="Entity"/> that this <see cref="View"/> will be linked to.</param>
		public void LinkTo(Entity entity)
		{
			if(entity == null)
			{
				throw new ArgumentNullException(nameof(entity));
			}

			NativeHandle.LinkTo(entity.NativeHandle);
		}

		/// <summary>
		/// Enable or disable this <see cref="View"/> as the active view. If a new view is activated the previous active view is deactivated.
		/// </summary>
		/// <param name="active">If set to <c>true</c> this will become the active view. If set to <c>false</c> this view will be set to inactive.</param>
		public void SetActive(bool active)
		{
			NativeHandle.SetActive(active);
			if(active)
			{
				Engine.GameFramework.GetIViewSystem().SetActiveView(NativeHandle);
			}
		}

		/// <summary>
		/// Resets the blendings that are currently active on this <see cref="View"/>.
		/// </summary>
		public void ResetBlending ()
		{
			NativeHandle.ResetBlending();
		}

		/// <summary>
		/// Resets all the shakes that are currently active on this <see cref="View"/>.
		/// </summary>
		public void ResetShaking ()
		{
			NativeHandle.ResetShaking();
		}

		// TODO Wrap SViewParams in a managed struct
		//public void SetCurrentParams (SViewParams arg0);

		/// <summary>
		/// Set the additive camera angles for this <see cref="View"/>.
		/// </summary>
		/// <param name="addFrameAngles">Add frame angles.</param>
		public void SetFrameAdditiveCameraAngles (Angles3 addFrameAngles)
		{
			NativeHandle.SetFrameAdditiveCameraAngles(addFrameAngles);
		}

		/// <summary>
		/// Set the scale of this <see cref="View"/>.
		/// </summary>
		/// <param name="scale">Scale.</param>
		public void SetScale (float scale)
		{
			NativeHandle.SetScale(scale);
		}

		/// <summary>
		/// Set new shake parameters for a new or existing shake on this <see cref="View"/>.
		/// </summary>
		/// <param name="shakeAngle">The angle the view will shake to.</param>
		/// <param name="shakeShift">The direction and magnitude of the shake.</param>
		/// <param name="duration">Duration of the full shake.</param>
		/// <param name="frequency">The time it takes for a single oscilation to complete.</param>
		/// <param name="randomness">Amount of randomness that's added to the shake direction and angle.</param>
		/// <param name="shakeId">ID of the Shake. If a shake with this ID already exists, it will be adjusted with the new parameters, otherwise a new shake is created.</param>
		public void SetViewShake (Angles3 shakeAngle, Vector3 shakeShift, float duration, float frequency, float randomness, int shakeId)
		{
			NativeHandle.SetViewShake(shakeAngle, shakeShift, duration, frequency, randomness, shakeId);
		}

		/// <summary>
		/// Set new shake parameters for a new or existing shake on this <see cref="View"/>.
		/// </summary>
		/// <param name="shakeAngle">The angle the view will shake to.</param>
		/// <param name="shakeShift">The direction and magnitude of the shake.</param>
		/// <param name="duration">Duration of the full shake.</param>
		/// <param name="frequency">The time it takes for a single oscilation to complete.</param>
		/// <param name="randomness">Amount of randomness that's added to the shake direction and angle.</param>
		/// <param name="shakeId">ID of the Shake. If a shake with this ID already exists, it will be adjusted with the new parameters, otherwise a new shake is created.</param>
		/// <param name="flipVector">If set to <c>true</c> the direction and angle of the shake will be flipped after each oscilation.</param>
		public void SetViewShake (Angles3 shakeAngle, Vector3 shakeShift, float duration, float frequency, float randomness, int shakeId, bool flipVector)
		{
			NativeHandle.SetViewShake(shakeAngle, shakeShift, duration, frequency, randomness, shakeId, flipVector);
		}

		/// <summary>
		/// Set new shake parameters for a new or existing shake on this <see cref="View"/>.
		/// </summary>
		/// <param name="shakeAngle">The angle the view will shake to.</param>
		/// <param name="shakeShift">The direction and magnitude of the shake.</param>
		/// <param name="duration">Duration of the full shake.</param>
		/// <param name="frequency">The time it takes for a single oscilation to complete.</param>
		/// <param name="randomness">Amount of randomness that's added to the shake direction and angle.</param>
		/// <param name="shakeId">ID of the Shake. If a shake with this ID already exists, it will be adjusted with the new parameters, otherwise a new shake is created.</param>
		/// <param name="flipVector">If set to <c>true</c> the direction and angle of the shake will be flipped after each oscilation.</param>
		/// <param name="updateOnly">If set to <c>true</c> this will only update an existing shake.</param>
		public void SetViewShake (Angles3 shakeAngle, Vector3 shakeShift, float duration, float frequency, float randomness, int shakeId, bool flipVector, bool updateOnly)
		{
			NativeHandle.SetViewShake(shakeAngle, shakeShift, duration, frequency, randomness, shakeId, flipVector, updateOnly);
		}

		// TODO There's also a SetViewShake with an extra groundOnly parameter, but it's not entirely clear if that parameter is still used.

		// TODO Wrap IView.SShakeParams
		//public void SetViewShakeEx (IView.SShakeParams arg0);

		/// <summary>
		/// Set the zoomed scale of this <see cref="View"/>.
		/// </summary>
		/// <param name="scale">Scale.</param>
		public void SetZoomedScale (float scale)
		{
			NativeHandle.SetZoomedScale(scale);
		}

		/// <summary>
		/// Stop a shake with the specified ID from shaking.
		/// </summary>
		/// <param name="shakeID">ID of the shake that needs to stop.</param>
		public void StopShake (int shakeID)
		{
			NativeHandle.StopShake(shakeID);
		}

		/// <summary>
		/// Manually update this <see cref="View"/>.
		/// </summary>
		/// <param name="frameTime">Time between this and last frame.</param>
		/// <param name="isActive">If set to <c>true</c> this <see cref="View"/> is updated, otherwise it will not be updated.</param>
		public void Update (float frameTime, bool isActive)
		{
			NativeHandle.Update(frameTime, isActive);
		}
	}
}

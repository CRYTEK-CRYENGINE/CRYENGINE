// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using CryEngine.Animations;

namespace CryEngine.Game
{
	/// <summary>
	/// Helper class for entities to easily manage animations.
	/// </summary>
	[EntityComponent(Category = "Animation", Guid = "78583bcc-4cc4-67e7-b83d-88a4bdc53e0f")]
	public class CharacterAnimator : EntityComponent
	{
		[SerializeValue]
		private string _characterGeometry = "";
		[SerializeValue]
		private int _characterSlot = 0;
		[SerializeValue]
		private string _controllerDefinition = "";
		[SerializeValue]
		private string _mannequinContext = "";
		[SerializeValue]
		private string _animationDatabase = "";
		[SerializeValue]
		private bool _animationDrivenMotion = false;
		[SerializeValue]
		private float _playbackScale = 1.0f;

		private ActionController _actionController;
		private AnimationContext _animationContext;
		private Character _character;

		private bool _dirty = true;
		private bool _destroyed = false;
		private float _accumulatedTime;
		private readonly Dictionary<int, bool> _tagValues = new Dictionary<int, bool>();
		private readonly Dictionary<MotionParameterId, float> _motionValues = new Dictionary<MotionParameterId, float>();


		/// <summary>
		/// If true, the animations will not be updated more than <see cref="MaxFramerate"/> per second.
		/// </summary>
		/// <value><c>true</c> if the animation's framerate is capped, otherwise <c>false</c>.</value>
		[EntityProperty(Description = "If true, the animations will not be updated more than MaxFramerate per second.")]
		public bool CapFramerate { get; set; } = false;

		/// <summary>
		/// The max framerate at which this animation will update. 
		/// Higher framerate can cause jittering when the animation is steered by input from example a mouse with a polling rate of 125hz.
		/// </summary>
		/// <value>The max framerate.</value>
		[EntityProperty(Description = "The max framerate at which this animation will update. Higher framerate can cause jittering when the animation is steered by input from example a mouse with a polling rate of 125hz.")]
		public int MaxFramerate { get; set; } = 30;

		/// <summary>
		/// The name of the fragment that is set active when initializing this Animator.
		/// </summary>
		/// <value>The start name of the fragment.</value>
		[EntityProperty(Description = "The name of the fragment that is set active when initializing this Animator.")]
		public string StartFragmentName { get; set; }

		/// <summary>
		/// The Character geometry file to load. Usually a .cdf file.
		/// </summary>
		[EntityProperty(Description = "The Character geometry file to load. Usually a .cdf file.", Type = EntityPropertyType.Character)]
		public string CharacterGeometry
		{
			get
			{
				return _characterGeometry;
			}
			set
			{
				if(_characterGeometry != value)
				{
					_characterGeometry = value;
					CharacterSlot = Entity.LoadCharacter(CharacterSlot, value);
					_dirty = true;
				}
			}
		}

		/// <summary>
		/// The Animator will try to animate the character in this slot of its Entity.
		/// </summary>
		/// <value>The character slot.</value>
		[EntityProperty(Description = "The Animator will try to animate the character in this slot of its Entity.")]
		public int CharacterSlot
		{
			get
			{
				return _characterSlot;
			}
			set
			{
				if(_characterSlot != value)
				{
					_characterSlot = value;
					_dirty = true;
				}
			}
		}

		/// <summary>
		/// Path to the Controller Definition xml-file.
		/// </summary>
		/// <value>Path to the xml-file.</value>
		[EntityProperty(EntityPropertyType.AnyFile, "Path to the Controller Definition xml-file.")]
		public string ControllerDefinition
		{
			get
			{
				return _controllerDefinition;
			}
			set
			{
				if(_controllerDefinition != value)
				{
					_controllerDefinition = value;
					_dirty = true;
				}
			}
		}

		/// <summary>
		/// Name of the Context that's required from the Controller Definition file.
		/// </summary>
		/// <value>The name of the context.</value>
		[EntityProperty(Description = "Name of the Context that's required from the Controller Definition file.")]
		public string MannequinContext
		{
			get
			{
				return _mannequinContext;
			}
			set
			{
				if(_mannequinContext != value)
				{
					_mannequinContext = value;
					_dirty = true;
				}
			}
		}

		/// <summary>
		/// Path to the Animation Database adb-file.
		/// </summary>
		/// <value>The path to the animation database file.</value>
		[EntityProperty(EntityPropertyType.AnyFile, "Path to the Animation Database adb-file.")]
		public string AnimationDatabase
		{
			get
			{
				return _animationDatabase;
			}
			set
			{
				if(_animationDatabase != value)
				{
					_animationDatabase = value;
					_dirty = true;
				}
			}
		}

		/// <summary>
		/// Gets or sets a value indicating whether this <see cref="T:CryEngine.CharacterAnimator"/> is allowed to move the Entity.
		/// </summary>
		/// <value><c>true</c> if animation driven motion; otherwise, <c>false</c>.</value>
		[EntityProperty(Description = "If true, the animation will move this Entity.")]
		public bool AnimationDrivenMotion
		{
			get
			{
				var skeleton = _character?.AnimationSkeleton;
				if(skeleton == null)
				{
					return _animationDrivenMotion;
				}

				return skeleton.GetAnimationDrivenMotion() == 0u;
			}
			set
			{
				var skeleton = _character?.AnimationSkeleton;
				if(skeleton != null)
				{
					skeleton.SetAnimationDrivenMotion(value ? 0u : 1u);
				}
				_animationDrivenMotion = value;
			}
		}

		/// <summary>
		/// Influences the speed at which animations play.
		/// 0 = paused animations (stuck at some frame)
		/// 1 = normal playback
		/// 2 = twice as fast playback
		/// 0.5 = twice as slow playback
		/// </summary>
		/// <value>The animation speed.</value>
		[EntityProperty(Description = "Influences the speed at which animations play.")]
		public float PlaybackScale
		{
			get
			{
				if(_character == null)
				{
					return _playbackScale;
				}
				return _character.PlaybackScale;
			}
			set
			{
				if(_character != null)
				{
					_character.PlaybackScale = value;
				}
				//Store the value in case the character isn't initialized yet.
				//When the character is initialized the requested scale can be set.
				_playbackScale = value;
			}
		}

		protected override void OnInitialize()
		{
			// The user might have just added the component, or it might have been loaded from the map-data.
			// We try to initialize it, but we don't want to spam the log if the user might not have put in all the right values.
			Initialize(true);
		}

		protected override void OnRemove()
		{
			Deinitialize();
		}

		protected override void OnGameplayStart()
		{
			// When the gameplay starts all values are assumed to be set correctly, so log any errors that might occur.
			Initialize(false);
		}

		protected override void OnEditorGameModeChange(bool enterGame)
		{
			if(!enterGame)
			{
				Deinitialize();
			}
			else
			{
				//If we're going back into game-mode we need to start initializing again.
				_destroyed = false;
			}
		}

		protected override void OnUpdate(float frameTime)
		{
			if(_destroyed)
			{
				return;
			}

			if(_dirty)
			{
				Initialize(false);
			}

			UpdateAnimation(frameTime);
		}

		private void Deinitialize()
		{
			_character?.Release();
			_character = null;
			_actionController?.Release();
			_actionController = null;
			_animationContext?.Release();
			_animationContext = null;
			_dirty = true;
			_destroyed = true;
		}

		private void Initialize(bool silent)
		{
			_destroyed = false;
			_dirty = false;
			_character?.Release();
			_character = null;
			_actionController?.Release();
			_actionController = null;
			_animationContext?.Release();
			_animationContext = null;

			var entity = Entity;
			if(entity == null)
			{
				return;
			}

			string characterGeometry = _characterGeometry;
			string mannequinControllerDefinition = ControllerDefinition;
			string mannequinContextName = MannequinContext;
			string animationDatabasePath = AnimationDatabase;
			string startFragmentName = StartFragmentName;


			if(string.IsNullOrWhiteSpace(characterGeometry) ||
			   string.IsNullOrWhiteSpace(mannequinControllerDefinition) ||
			   string.IsNullOrWhiteSpace(mannequinContextName) ||
			   string.IsNullOrWhiteSpace(animationDatabasePath) ||
			   string.IsNullOrWhiteSpace(startFragmentName))
			{
				if(!silent)
				{
					Log.Error<CharacterAnimator>("Unable to initialize animations for Entity {0} because some of the properties are not set!", entity.Name);
				}
				return;
			}

			CharacterSlot = entity.LoadCharacter(CharacterSlot, characterGeometry);
			var character = entity.GetCharacter(CharacterSlot);
			if(character == null)
			{
				if(!silent)
				{
					Log.Error("Unable to retrieve Character from Entity {0} at slot {1}!", entity.Name, CharacterSlot);
				}
				return;
			}
			_character = character;

			var mannequinInterface = Mannequin.MannequinInterface;
			var animationDatabaseManager = mannequinInterface.AnimationDatabaseManager;


			// Load the Mannequin controller definition.
			// This is owned by the animation database manager
			var controllerDefinition = animationDatabaseManager.LoadControllerDefinition(mannequinControllerDefinition);
			if(controllerDefinition == null)
			{
				if(!silent)
				{
					Log.Error<CharacterAnimator>("Failed to load controller definition at {0} for Entity {1}.", mannequinControllerDefinition, entity.Name);
				}
				return;
			}

			// Load the animation database
			var animationDatabase = animationDatabaseManager.Load(animationDatabasePath);
			if(animationDatabase == null)
			{
				if(!silent)
				{
					Log.Error<CharacterAnimator>("Failed to load animation database at {0} for Entity {1}!", animationDatabasePath, entity.Name);
				}
				return;
			}

			// Create a new animation context for the controller definition we loaded above
			_animationContext = AnimationContext.Create(controllerDefinition);

			// Now create the controller that will be handling animation playback
			_actionController = mannequinInterface.CreateActionController(entity, _animationContext);

			if(_actionController == null)
			{
				if(!silent)
				{
					Log.Error<CharacterAnimator>("Unable to create ActionController from context for Entity {0}!", entity.Name);
				}
				return;
			}

			// Activate the Main context we'll be playing our animations in
			ActivateMannequinContext(mannequinContextName, character, controllerDefinition, animationDatabase);

			// Create this idle fragment
			// This implementation handles switching to various sub-fragments by itself, based on input and physics data
			int priority = 0;
			var idleFragmentId = controllerDefinition.FindFragmentId(startFragmentName);

			if(idleFragmentId < 0)
			{
				if(!silent)
				{
					Log.Error<CharacterAnimator>("Unable to find fragment {1} for Entity {0}!", entity.Name, startFragmentName);
				}
				return;
			}

			var animationContextAction = AnimationContextAction.CreateAnimationContextAction(priority, idleFragmentId, TagState.Empty, 0, 0, 0);

			// Queue the idle fragment to start playing immediately on next update
			_actionController.Queue(animationContextAction);

			// Decide if movement is coming from the animation (root joint offset). If false, we control this entirely via physics.
			var skeleton = character?.AnimationSkeleton;
			if(skeleton == null)
			{
				if(!silent)
				{
					Log.Error<CharacterAnimator>("Unable to retrieve Animation Skeleton from Entity {0} at character-slot {1}!", entity.Name, CharacterSlot);
				}
				return;
			}

			//Apply stored values
			skeleton.SetAnimationDrivenMotion(_animationDrivenMotion ? 0u : 1u);

			//Set the stored playback scale to the character.
			character.PlaybackScale = _playbackScale;
		}

		private void ActivateMannequinContext(string contextName, Character character, ControllerDefinition controllerDefinition, AnimationDatabase animationDatabase)
		{
			var entity = Entity;

			var scopeContextId = controllerDefinition.FindScopeContext(contextName);
			//If the id < 0 it means it's invalid.
			if(scopeContextId < 0)
			{
				Log.Error<CharacterAnimator>("Failed to find {0} scope context id in controller definition for Entity {1}.", contextName, Entity.Name);
				return;
			}

			// Setting Scope contexts can happen at any time, and what entity or character instance we have bound to a particular scope context
			// can change during the lifetime of an action controller.
			_actionController.SetScopeContext((uint)scopeContextId, entity, character, animationDatabase);
		}

		private void UpdateAnimation(float frameTime)
		{
			if(CapFramerate)
			{
				_accumulatedTime += frameTime;

				if(_accumulatedTime < 1.0f / MaxFramerate)
				{
					return;
				}

				ApplyTagValues();
				ApplyMotionValues();

				_actionController?.Update(_accumulatedTime);

				_accumulatedTime = 0;
			}
			else
			{
				_actionController?.Update(frameTime);
			}
		}

		private void ApplyTagValues()
		{
			if(_tagValues.Count == 0)
			{
				return;
			}

			foreach(var keyValue in _tagValues)
			{
				_animationContext?.SetTagValue(keyValue.Key, keyValue.Value);
			}
			_tagValues.Clear();
		}

		private void ApplyMotionValues()
		{
			if(_motionValues.Count == 0)
			{
				return;
			}

			foreach(var keyValue in _motionValues)
			{
				_character?.SetAnimationSkeletonParameter(keyValue.Key, keyValue.Value);
			}
			_motionValues.Clear();
		}

		/// <summary>
		/// Find an AnimationTag by name on this Entity.
		/// </summary>
		/// <returns>The tag with the specified name. Logs an error and returns null if none was found.</returns>
		/// <param name="name">Name of the tag.</param>
		public AnimationTag FindTag(string name)
		{
			//If the engine is shutdown ignore any incoming calls.
			if(_destroyed)
			{
				return null;
			}

			if(_dirty)
			{
				Initialize(false);
			}

			if(_animationContext == null)
			{
				Log.Error<CharacterAnimator>("Unable to find the specified tag because the Animator is not yet initialized properly for Entity {1}!", name, Entity.Name);
				return null;
			}

			return _animationContext.FindAnimationTag(name);
		}

		/// <summary>
		/// Set the value of the specified AnimationTag.
		/// </summary>
		/// <param name="tag">The target AnimationTag to be set.</param>
		/// <param name="value">The value the specified tag should be set to.</param>
		public void SetTagValue(AnimationTag tag, bool value)
		{
			//If the engine is shutdown ignore any incoming calls.
			if(_destroyed)
			{
				return;
			}

			if(_dirty)
			{
				Initialize(false);
			}

			if(tag == null)
			{
				throw new ArgumentNullException(nameof(tag));
			}

			var id = tag.Id;
			if(id < 0)
			{
				Log.Error<CharacterAnimator>("Tried to set the value for an invalid AnimationTag on Entity {0}!", Entity.Name);
				return;
			}

			if(CapFramerate)
			{
				_tagValues[id] = value;
			}
			else
			{
				_animationContext.SetTagValue(tag, value);
			}
		}

		/// <summary>
		/// Set the specified MotionParameter to the desired value.
		/// Values have a maximum value of 10, and will be clamped.
		/// </summary>
		/// <param name="id">The id of the MotionParameter.</param>
		/// <param name="value">The value of the MotionParameter.</param>
		public void SetMotionParameter(MotionParameterId id, float value)
		{
			//If the engine is shutdown ignore any incoming calls.
			if(_destroyed)
			{
				return;
			}

			if(_dirty)
			{
				Initialize(false);
			}

			if(_character == null)
			{
				Log.Error<CharacterAnimator>("Unable to retrieve Character from Entity {0} at slot {1}!", Entity.Name, CharacterSlot);
				return;
			}

			if(CapFramerate)
			{
				//If the key isn't assigned yet, prepare it for easier access later on.
				if(!_motionValues.ContainsKey(id))
				{
					_motionValues[id] = 0;
				}

				switch(id)
				{
					// Most values can have the accumulated values from over multiple frames
					case MotionParameterId.TravelSpeed:
					case MotionParameterId.TurnSpeed:
					case MotionParameterId.TurnAngle:
					case MotionParameterId.TravelDist:
					case MotionParameterId.StopLeg:
					case MotionParameterId.BlendWeight1:
					case MotionParameterId.BlendWeight2:
					case MotionParameterId.BlendWeight3:
					case MotionParameterId.BlendWeight4:
					case MotionParameterId.BlendWeight5:
					case MotionParameterId.BlendWeight6:
					case MotionParameterId.BlendWeight7:
						_motionValues[id] += value;
						break;

					// Some values need absolute values
					case MotionParameterId.TravelAngle:
					case MotionParameterId.TravelSlope:
						_motionValues[id] = value;
						break;
					default:
						throw new ArgumentOutOfRangeException();
				}
			}
			else
			{
				_character.SetAnimationSkeletonParameter(id, value);
			}
		}
	}
}

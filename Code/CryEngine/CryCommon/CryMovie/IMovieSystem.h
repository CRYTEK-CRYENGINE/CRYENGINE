// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/CryVariant.h>
#include <CryMath/Range.h>
#include <CryMovie/AnimKey.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryString/CryName.h>
#include <CryExtension/CryGUID.h>

// forward declaration.
struct IAnimTrack;
struct IAnimNode;
struct IAnimSequence;
struct IMovieSystem;
struct STrackKey;
class XmlNodeRef;
struct ISplineInterpolator;
struct ILightAnimWrapper;

typedef IMovieSystem* (* PFNCREATEMOVIESYSTEM)(struct ISystem*);

#define MAX_ANIM_NAME_LENGTH 64

#if CRY_PLATFORM_DESKTOP
	#define MOVIESYSTEM_SUPPORT_EDITING
#endif

typedef std::vector<IAnimSequence*> AnimSequences;
typedef std::vector<string>         TrackEvents;

//! \cond INTERNAL
//! Node-Types.
//! You need to register new types in Movie.cpp/RegisterNodeTypes for serialization.
//! \note Enums are serialized by string now, there is no need for specific IDs anymore for new parameters. Values are for backward compatibility.
enum EAnimNodeType
{
	eAnimNodeType_Invalid         = 0x00,
	eAnimNodeType_Entity          = 0x01,
	eAnimNodeType_Director        = 0x02,
	eAnimNodeType_Camera          = 0x03,
	eAnimNodeType_CVar            = 0x04,
	eAnimNodeType_ScriptVar       = 0x05,
	eAnimNodeType_Material        = 0x06,
	eAnimNodeType_Event           = 0x07,
	eAnimNodeType_Group           = 0x08,
	eAnimNodeType_Layer           = 0x09,
	eAnimNodeType_Comment         = 0x10,
	eAnimNodeType_RadialBlur      = 0x11,
	eAnimNodeType_ColorCorrection = 0x12,
	eAnimNodeType_DepthOfField    = 0x13,
	eAnimNodeType_ScreenFader     = 0x14,
	eAnimNodeType_Light           = 0x15,
	eAnimNodeType_HDRSetup        = 0x16,
	eAnimNodeType_ShadowSetup     = 0x17,
	eAnimNodeType_Alembic         = 0x18, // Used in cinebox, added so nobody uses that number.
	eAnimNodeType_GeomCache       = 0x19,
	eAnimNodeType_Environment,
	eAnimNodeType_ScreenDropsSetup,
	eAnimNodeType_Audio,
	eAnimNodeType_Num
};

//! Flags that can be set on animation node.
enum EAnimNodeFlags
{
	eAnimNodeFlags_CanChangeName = BIT(2),  //!< Set if this node allow changing of its name.
	eAnimNodeFlags_Disabled      = BIT(3),  //!< Disable this node.
};

enum ENodeExportType
{
	eNodeExportType_Global = 0,
	eNodeExportType_Local  = 1,
};

//! Static common parameters IDs of animation node.
//! You need to register new params in Movie.cpp/RegisterParamTypes for serialization.
//! If you want to expand CryMovie to control new stuff this is probably the enum you want to change.
//! For named params see eAnimParamType_ByString & CAnimParamType.
//! \see eAnimParamType_ByString, CAnimParamType
//! \note Enums are serialized by string now, there is no need for specific IDs anymore for new parameters. Values are for backward compatibility.
enum EAnimParamType
{
	//! Parameter is specified by string. See CAnimParamType.
	eAnimParamType_ByString            = 8,

	eAnimParamType_FOV                 = 0,
	eAnimParamType_Position            = 1,
	eAnimParamType_Rotation            = 2,
	eAnimParamType_Scale               = 3,
	eAnimParamType_Event               = 4,
	eAnimParamType_Visibility          = 5,
	eAnimParamType_Camera              = 6,
	eAnimParamType_Animation           = 7,
	eAnimParamType_AudioSwitch         = 9,
	eAnimParamType_AudioTrigger        = 10,
	eAnimParamType_AudioFile           = 11,
	eAnimParamType_AudioParameter      = 12,
	eAnimParamType_Sequence            = 13,
	eAnimParamType_Expression          = 14,
	eAnimParamType_Console             = 17,
	eAnimParamType_Float               = 19,
	eAnimParamType_FaceSequence        = 20,
	eAnimParamType_LookAt              = 21,
	eAnimParamType_TrackEvent          = 22,

	eAnimParamType_ShakeAmplitudeA     = 23,
	eAnimParamType_ShakeAmplitudeB     = 24,
	eAnimParamType_ShakeFrequencyA     = 25,
	eAnimParamType_ShakeFrequencyB     = 26,
	eAnimParamType_ShakeMultiplier     = 27,
	eAnimParamType_ShakeNoise          = 28,
	eAnimParamType_ShakeWorking        = 29,
	eAnimParamType_ShakeAmpAMult       = 61,
	eAnimParamType_ShakeAmpBMult       = 62,
	eAnimParamType_ShakeFreqAMult      = 63,
	eAnimParamType_ShakeFreqBMult      = 64,

	eAnimParamType_DepthOfField        = 30,
	eAnimParamType_FocusDistance       = 31,
	eAnimParamType_FocusRange          = 32,
	eAnimParamType_BlurAmount          = 33,

	eAnimParamType_Capture             = 34,
	eAnimParamType_TransformNoise      = 35,
	eAnimParamType_TimeWarp            = 36,
	eAnimParamType_FixedTimeStep       = 37,
	eAnimParamType_NearZ               = 38,
	eAnimParamType_Goto                = 39,

	eAnimParamType_PositionX           = 51,
	eAnimParamType_PositionY           = 52,
	eAnimParamType_PositionZ           = 53,

	eAnimParamType_RotationX           = 54,
	eAnimParamType_RotationY           = 55,
	eAnimParamType_RotationZ           = 56,

	eAnimParamType_ScaleX              = 57,
	eAnimParamType_ScaleY              = 58,
	eAnimParamType_ScaleZ              = 59,

	eAnimParamType_ColorR              = 82,
	eAnimParamType_ColorG              = 83,
	eAnimParamType_ColorB              = 84,

	eAnimParamType_CommentText         = 70,
	eAnimParamType_ScreenFader         = 71,

	eAnimParamType_LightDiffuse        = 81,
	eAnimParamType_LightRadius         = 85,
	eAnimParamType_LightDiffuseMult    = 86,
	eAnimParamType_LightHDRDynamic     = 87,
	eAnimParamType_LightSpecularMult   = 88,
	eAnimParamType_LightSpecPercentage = 89,

	eAnimParamType_MaterialDiffuse     = 90,
	eAnimParamType_MaterialSpecular    = 91,
	eAnimParamType_MaterialEmissive    = 92,
	eAnimParamType_MaterialOpacity     = 93,
	eAnimParamType_MaterialSmoothness  = 94,

	eAnimParamType_TimeRanges          = 95,  //!< Generic track with keys that have time ranges.

	eAnimParamType_Physics             = 96,  //! Not used anymore, see Physicalize & PhysicsDriven.

	eAnimParamType_ProceduralEyes      = 97,

	eAnimParamType_Mannequin           = 98,  //!< Mannequin parameter.

	// Add new param types without explicit ID here.
	// Note: don't forget to register in Movie.cpp.
	eAnimParamType_GameCameraInfluence,

	eAnimParamType_GSMCache,

	eAnimParamType_ShutterSpeed,

	eAnimParamType_Physicalize,
	eAnimParamType_PhysicsDriven,

	eAnimParamType_SunLongitude,
	eAnimParamType_SunLatitude,
	eAnimParamType_MoonLongitude,
	eAnimParamType_MoonLatitude,

	eAnimParamType_DynamicResponseSignal,

	eAnimParamType_User    = 100000,              //!< User node params.

	eAnimParamType_Invalid = 0xFFFFFFFF
};

//! Common parameters of animation node.
class CAnimParamType
{
	friend class CMovieSystem;

public:
	static const uint kParamTypeVersion = 9;

	CAnimParamType() : m_type(eAnimParamType_Invalid) {}

	CAnimParamType(const string& name)
	{
		*this = name;
	}

	CAnimParamType(const EAnimParamType type)
	{
		*this = type;
	}

	//! Convert from old enum or int.
	void operator=(const int type)
	{
		m_type = (EAnimParamType)type;
	}

	void operator=(const string& name)
	{
		m_type = eAnimParamType_ByString;
		m_name = name;
	}

	//! Convert to enum. This needs to be explicit, otherwise operator== will be ambiguous.
	EAnimParamType GetType() const { return m_type; }

	const char*    GetName() const { return m_name.c_str(); }

	bool           operator==(const CAnimParamType& animParamType) const
	{
		if (m_type == eAnimParamType_ByString && animParamType.m_type == eAnimParamType_ByString)
		{
			return m_name == animParamType.m_name;
		}

		return m_type == animParamType.m_type;
	}

	bool operator!=(const CAnimParamType& animParamType) const
	{
		return !(*this == animParamType);
	}

	bool operator<(const CAnimParamType& animParamType) const
	{
		if (m_type == eAnimParamType_ByString && animParamType.m_type == eAnimParamType_ByString)
		{
			return m_name < animParamType.m_name;
		}
		else if (m_type == eAnimParamType_ByString)
		{
			return false;  // Always sort named params last
		}
		else if (animParamType.m_type == eAnimParamType_ByString)
		{
			return true;  // Always sort named params last
		}

		if (m_type < animParamType.m_type)
		{
			return true;
		}

		return false;
	}

	//! Serialization. Defined in Movie.cpp.
	inline void Serialize(XmlNodeRef& xmlNode, bool bLoading, const uint version = kParamTypeVersion);

	void        Serialize(Serialization::IArchive& ar)
	{
		ar((int&)m_type, "type", "Type");

		string name = m_name.c_str() ? m_name.c_str() : "";
		ar(name, "name", "Name");

		if (ar.isInput())
		{
			m_name = name;
		}
	}

private:
	EAnimParamType m_type;
	CCryName       m_name;
};

//! Values that animation track can hold.
//! Do not change values - they are serialized.
//! Attention: This should only be expanded if you add a completely new value type that tracks can control!
//! If you just want to control a new parameter of an entity etc. extend EParamType.
//! \note If the param type of a track is known and valid these can be derived from the node. These are serialized in case the parameter got invalid (for example for material nodes).
enum EAnimValue
{
	eAnimValue_Float         = 0,
	eAnimValue_Vector        = 1,
	eAnimValue_Quat          = 2,
	eAnimValue_Bool          = 3,
	eAnimValue_Vector4       = 15,
	eAnimValue_DiscreteFloat = 16,
	eAnimValue_RGB           = 20,

	eAnimValue_Unknown       = 0xFFFFFFFF
};

enum ETrackMask
{
	eTrackMask_MaskSound = 1 << 11, // Old: 1 << ATRACK_SOUND
};

//! Structure passed to Animate function.
struct SAnimContext
{
	SAnimContext()
	{
		bSingleFrame = false;
		bForcePlay = false;
		bResetting = false;
		pSequence = NULL;
		trackMask = 0;
		m_activeCameraEntity = INVALID_ENTITYID;
	}

	SAnimTime      time;         //!< Current time in seconds.
	SAnimTime      dt;           //!< Delta of time from previous animation frame in seconds.
	bool           bSingleFrame; //!< This is not a playing animation, more a single-frame update
	bool           bForcePlay;   //!< Set when force playing animation
	bool           bResetting;   //!< Set when animation sequence is reset.

	IAnimSequence* pSequence;            //!< Sequence in which animation performed.
	EntityId       m_activeCameraEntity; //!< Used for editor to pass viewport camera to CryMovie.

	// TODO: Replace trackMask with something more type safe
	// TODO: Mask should be stored with dynamic length
	uint32    trackMask;  //!< To update certain types of tracks only
	SAnimTime startTime;  //!< The start time of this playing sequence

	void      Serialize(XmlNodeRef& xmlNode, bool bLoading);
};

//! Parameters for cut-scene cameras!
struct SCameraParams
{
	SCameraParams()
	{
		cameraEntityId = INVALID_ENTITYID;
		fFOV = 0.0f;
		fGameCameraInfluence = 0.0f;
		fNearZ = DEFAULT_NEAR;
		justActivated = false;
	}
	EntityId cameraEntityId; //!< Camera entity id.
	float    fFOV;
	float    fNearZ;
	float    fGameCameraInfluence;
	bool     justActivated;
};

struct SMovieSystemVoid
{
};
inline bool operator==(const SMovieSystemVoid&, const SMovieSystemVoid&)
{
	return true;
}

struct SSequenceAudioTrigger
{
	SSequenceAudioTrigger()
		: m_onStopTrigger(CryAudio::InvalidControlId)
		, m_onPauseTrigger(CryAudio::InvalidControlId)
		, m_onResumeTrigger(CryAudio::InvalidControlId)
	{}

	void Serialize(XmlNodeRef xmlNode, bool bLoading);
	void Serialize(Serialization::IArchive& ar);

	CryAudio::ControlId m_onStopTrigger;
	CryAudio::ControlId m_onPauseTrigger;
	CryAudio::ControlId m_onResumeTrigger;

	string              m_onStopTriggerName;
	string              m_onPauseTriggerName;
	string              m_onResumeTriggerName;
};

typedef CryVariant<
    SMovieSystemVoid,
    float,
    Vec3,
    Vec4,
    Quat,
    bool
    > TMovieSystemValue;

enum EMovieTrackDataTypes
{
	eTDT_Void  = cry_variant::get_index<SMovieSystemVoid, TMovieSystemValue>::value,
	eTDT_Float = cry_variant::get_index<float, TMovieSystemValue>::value,
	eTDT_Vec3  = cry_variant::get_index<Vec3, TMovieSystemValue>::value,
	eTDT_Vec4  = cry_variant::get_index<Vec4, TMovieSystemValue>::value,
	eTDT_Quat  = cry_variant::get_index<Quat, TMovieSystemValue>::value,
	eTDT_Bool  = cry_variant::get_index<bool, TMovieSystemValue>::value,
};

//! Interface for movie-system implemented by user for advanced function-support
struct IMovieUser
{
	// <interfuscator:shuffle>
	virtual ~IMovieUser() {}
	//! Called when movie system requests a camera-change.
	virtual void SetActiveCamera(const SCameraParams& Params) = 0;
	//! Called when movie system enters into cut-scene mode.
	virtual void BeginCutScene(IAnimSequence* pSeq, unsigned long dwFlags, bool bResetFX) = 0;
	//! Called when movie system exits from cut-scene mode.
	virtual void EndCutScene(IAnimSequence* pSeq, unsigned long dwFlags) = 0;
	//! Called when movie system wants to send a global event.
	virtual void SendGlobalEvent(const char* pszEvent) = 0;

	// Called to play subtitles for specified sound.
	// virtual void PlaySubtitles( IAnimSequence* pSeq, ISound *pSound ) = 0;
	// </interfuscator:shuffle>
};

//! Callback-class.
struct IMovieCallback
{

	//! Called by movie system.
	virtual void OnSetCamera(const SCameraParams& Params) = 0;
};

/**	Interface of Animation Track.
 */
struct IAnimTrack : public _i_reference_target_t
{
	//! Flags that can be set on animation track.
	enum EAnimTrackFlags
	{
		eAnimTrackFlags_Linear   = BIT(1), //!< Use only linear interpolation between keys.
		eAnimTrackFlags_Disabled = BIT(4), //!< Disable this track.

		// Used by editor.
		eAnimTrackFlags_Hidden = BIT(5),   //!< Set when track is hidden in track view.
		eAnimTrackFlags_Muted  = BIT(8),   //!< Mute this sound track. This only affects the playback in editor.
	};

	// <interfuscator:shuffle>
	virtual EAnimValue GetValueType() = 0;

	//! Return what parameter of the node, this track is attached to.
	virtual CAnimParamType GetParameterType() const = 0;

	virtual CryGUID        GetGUID() const = 0;

	//! Animation track can contain sub-tracks (Position XYZ anim track have sub-tracks for x,y,z).
	//! Get count of sub tracks.
	virtual int GetSubTrackCount() const = 0;

	//! Retrieve pointer the specfied sub track.
	virtual IAnimTrack* GetSubTrack(int nIndex) const = 0;
	virtual const char* GetSubTrackName(int nIndex) const = 0;
	virtual void        SetSubTrackName(int nIndex, const char* name) = 0;
	//////////////////////////////////////////////////////////////////////////

	virtual void GetKeyValueRange(float& fMin, float& fMax) const = 0;
	virtual void SetKeyValueRange(float fMin, float fMax) = 0;

	//! Return number of keys in track.
	virtual int GetNumKeys() const = 0;

	//! Return true if keys exists in this track.
	virtual bool HasKeys() const = 0;

	//! Remove specified key.
	virtual void RemoveKey(int num) = 0;

	//! Remove all keys
	virtual void ClearKeys() = 0;

	//! Get key at specified location.
	//! @param key Must be valid pointer to compatable key structure, to be filled with specified key location.
	virtual void GetKey(int index, STrackKey* key) const = 0;

	//! Get time of specified key.
	//! @return key time.
	virtual SAnimTime GetKeyTime(int index) const = 0;

	//! Find key at given time.
	//! @return Index of found key, or -1 if key with this time not found.
	virtual int FindKey(SAnimTime time) = 0;

	//! Set key at specified location. Will not change time of key.
	//! @param key Must be valid pointer to compatable key structure.
	virtual void SetKey(int index, const STrackKey* key) = 0;

	//! Get track flags.
	virtual int GetFlags() = 0;

	//! Check if track is masked by mask.
	// TODO: Mask should be stored with dynamic length.
	virtual bool IsMasked(const uint32 mask) const = 0;

	//! Set track flags.
	virtual void SetFlags(int flags) = 0;

	//! Create key at given time, and return its index.
	//! @return Index of new key.
	virtual int CreateKey(SAnimTime time) = 0;

	//! Gets the key type as a string
	virtual const char* GetKeyType() const = 0;

	//! Returns true if the keys derive from STrackDurationKey
	virtual bool KeysDeriveTrackDurationKey() const = 0;

	//! Gets the wrapped key
	virtual _smart_ptr<IAnimKeyWrapper> GetWrappedKey(int key) = 0;

	//! Get track value at specified time. Interpolates keys if needed.
	virtual TMovieSystemValue GetValue(SAnimTime time) const = 0;

	//! Set track value at specified time. Adds new keys if required.
	virtual void SetValue(SAnimTime time, const TMovieSystemValue& value) {}

	//! Get track default value
	virtual TMovieSystemValue GetDefaultValue() const = 0;

	//! Set track default value
	virtual void SetDefaultValue(const TMovieSystemValue& defaultValue) = 0;

	//! Assign active time range for this track.
	virtual void SetTimeRange(TRange<SAnimTime> timeRange) = 0;

	//! Serialize unique parameters for this track.
	virtual void Serialize(Serialization::IArchive& ar) = 0;

	//! Serialize this animation track to XML.
	virtual bool Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks = true) = 0;

	//! Serialize the keys on this track to XML
	virtual bool SerializeKeys(XmlNodeRef& xmlNode, bool bLoading, std::vector<SAnimTime>& keys, const SAnimTime time = SAnimTime(0)) = 0;

	//! For custom track animate parameters.
	virtual void Animate(SAnimContext& animContext) {};

	//! Return the index of the key which lies right after the given key in time.
	//! In the case of sorted keys, it's just 'key+1', but if not sorted, it can be another value.
	//! \param key Index of of key.
	//! \return Index of the next key in time. If the last key given, this returns -1.
	virtual int NextKeyByTime(int key) const { if (key + 1 < GetNumKeys()) { return key + 1; } else { return -1; } }

	//! Get the animation layer index assigned. (only for character/look-at tracks ATM).
	virtual int GetAnimationLayerIndex() const { return -1; }

	//! Set the animation layer index. (only for character/look-at tracks ATM).
	virtual void SetAnimationLayerIndex(int index) {}

	// </interfuscator:shuffle>
protected:
	virtual ~IAnimTrack() {};
};

//! Callback called by animation node when its animated.
struct IAnimNodeOwner
{
	// <interfuscator:shuffle>
	virtual ~IAnimNodeOwner() {}
	virtual void OnNodeAnimated(IAnimNode* pNode) = 0;
	virtual void OnNodeVisibilityChanged(IAnimNode* pNode, const bool bHidden) = 0;
	virtual void OnNodeReset(IAnimNode* pNode) {}
	// </interfuscator:shuffle>
};

// Interface for entity animation nodes
struct IAnimEntityNode
{
	// Assign an entity guid to the node, (Only for Entity targeting nodes)
	virtual void        SetEntityGuid(const EntityGUID& guid) = 0;
	// Retrieve a pointer to the entity Guid, return NULL if it is not an entity targeting node.
	virtual EntityGUID* GetEntityGuid() = 0;

	// Override the entity ID of a node (only called from game)
	virtual void SetEntityId(const int entityId) {}

	// Query entity of this node, if assigned.
	virtual IEntity* GetEntity() = 0;

	// Assign an entities guides to the lookAt nodes
	virtual void SetEntityGuidTarget(const EntityGUID& guid) = 0;
	virtual void SetEntityGuidSource(const EntityGUID& guid) = 0;

	//! Get current entity position.
	virtual Vec3 GetPos() = 0;
	//! Get current entity rotation.
	virtual Quat GetRotate() = 0;
	//! Get current entity scale.
	virtual Vec3 GetScale() = 0;
};

struct IAnimCameraNode
{
	virtual bool GetShakeRotation(SAnimTime time, Quat& rot) = 0;
	virtual void SetCameraShakeSeed(const uint shakeSeed) = 0;

	// Camera nodes can store parameters even without tracks, so we need this interface to set/get them
	virtual void              SetParameter(SAnimTime time, const EAnimParamType& paramType, const TMovieSystemValue& value) = 0;
	virtual TMovieSystemValue GetParameter(SAnimTime time, const EAnimParamType& paramType) const = 0;
};

struct IAnimSequenceOwner
{
	// <interfuscator:shuffle>
	virtual ~IAnimSequenceOwner() {}
	virtual void OnModified() = 0;
	// </interfuscator:shuffle>
};

//! Base class for all Animation nodes.
//! Can host multiple animation tracks, and execute them other time. Animation node is reference counted.
struct IAnimNode : virtual public _i_reference_target_t
{
public:
	//! Supported params.
	enum ESupportedParamFlags
	{
		eSupportedParamFlags_MultipleTracks = BIT(0), // Set if parameter can be assigned multiple tracks.
	};

public:
	//! Evaluate animation node while not playing animation.
	virtual void StillUpdate() = 0;

	//! Evaluate animation to the given time.
	virtual void Animate(SAnimContext& animContext) {}

	// <interfuscator:shuffle>
	//! Set node name.
	virtual void SetName(const char* name) = 0;

	//! Get node name.
	virtual const char* GetName() = 0;

	//! Get Type of this node.
	virtual EAnimNodeType GetType() const = 0;

	//! Return Animation Sequence that owns this node.
	virtual IAnimSequence* GetSequence() = 0;

	virtual CryGUID        GetGUID() const = 0;

	//! Called when sequence is activated / deactivated
	virtual void Activate(bool bActivate) = 0;

	//! Set AnimNode flags.
	//! \param flags One or more flags from EAnimNodeFlags.
	//! \see EAnimNodeFlags.
	virtual void SetFlags(int flags) = 0;

	//! Get AnimNode flags.
	//! \return flags set on node.
	//! \see EAnimNodeFlags.
	virtual int GetFlags() const = 0;

	//! Returns number of supported parameters by this animation node (position,rotation,scale,etc..).
	//! \return Number of supported parameters.
	virtual unsigned int GetParamCount() const = 0;

	//! Returns the type of a param by index
	//! \param nIndex - parameter index in range 0 <= nIndex < GetSupportedParamCount().
	virtual CAnimParamType GetParamType(unsigned int nIndex) const = 0;

	//! Check if parameter is supported by this node.
	virtual bool IsParamValid(const CAnimParamType& paramType) const = 0;

	//! Returns name of supported parameter of this animation node or NULL if not available.
	//! \param paramType Parameter id.
	virtual const char* GetParamName(const CAnimParamType& paramType) const = 0;

	//! Returns the params value type.
	virtual EAnimValue GetParamValueType(const CAnimParamType& paramType) const = 0;

	//! Returns the params value type.
	virtual ESupportedParamFlags GetParamFlags(const CAnimParamType& paramType) const = 0;

	// Working with Tracks.

	virtual int GetTrackCount() const = 0;

	//! Return track assigned to the specified parameter.
	virtual IAnimTrack* GetTrackByIndex(int nIndex) const = 0;

	//! Return first track assigned to the specified parameter.
	virtual IAnimTrack* GetTrackForParameter(const CAnimParamType& paramType) const = 0;

	//! Return the i-th track assigned to the specified parameter in case of multiple tracks.
	virtual IAnimTrack* GetTrackForParameter(const CAnimParamType& paramType, uint32 index) const = 0;

	//! Get the index of a given track among tracks with the same parameter type in this node.
	virtual uint32 GetTrackParamIndex(const IAnimTrack* pTrack) const = 0;

	//! Creates a new track for given parameter.
	virtual IAnimTrack* CreateTrack(const CAnimParamType& paramType) = 0;

	//! Initializes track default values after de-serialization / user creation. Only called in editor.
	virtual void InitializeTrackDefaultValue(IAnimTrack* pTrack, const CAnimParamType& paramType) = 0;

	//! Assign animation track to parameter.
	//! \param track If NULL, the track with parameter id param will be removed.
	virtual void SetTrack(const CAnimParamType& paramType, IAnimTrack* track) = 0;

	//! Set time range for all tracks in this sequence.
	virtual void SetTimeRange(TRange<SAnimTime> timeRange) = 0;

	//! Add track to anim node.
	virtual void AddTrack(IAnimTrack* pTrack) = 0;

	//! Remove track from anim node.
	virtual bool RemoveTrack(IAnimTrack* pTrack) = 0;

	//! Creates default set of tracks supported by this node.
	virtual void CreateDefaultTracks() = 0;

	//! Callback for animation node used by editor.
	//! Register notification callback with animation node.
	virtual void            SetNodeOwner(IAnimNodeOwner* pOwner) = 0;
	virtual IAnimNodeOwner* GetNodeOwner() = 0;

	//! Serialize this animation node to XML.
	virtual void Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks) = 0;

	// Groups interface
	virtual void       SetParent(IAnimNode* pParent) = 0;
	virtual IAnimNode* GetParent() const = 0;
	virtual IAnimNode* HasDirectorAsParent() const = 0;

	virtual void       Render() = 0;
	virtual bool       NeedToRender() const = 0;

	//! Called from editor if dynamic params need updating.
	virtual void UpdateDynamicParams() = 0;

	//! Returns interface for entity nodes, otherwise nullptr
	virtual IAnimEntityNode* QueryEntityNodeInterface() = 0;

	//! Returns interface for camera nodes, otherwise nullptr
	virtual IAnimCameraNode* QueryCameraNodeInterface() = 0;
	// </interfuscator:shuffle>

protected:
	virtual ~IAnimNode() {};
};

//! Track event listener.
struct ITrackEventListener
{
	enum ETrackEventReason
	{
		eTrackEventReason_Added,
		eTrackEventReason_Removed,
		eTrackEventReason_Renamed,
		eTrackEventReason_Triggered,
		eTrackEventReason_MovedUp,
		eTrackEventReason_MovedDown,
	};

	// <interfuscator:shuffle>
	virtual ~ITrackEventListener() {}

	//! Called when track event is updated
	//! \param pSeq Animation sequence.
	//! \param reason Reason for update (see EReason).
	//! \param event Track event added.
	//! \param pUserData Data to accompany reason.
	virtual void OnTrackEvent(IAnimSequence* pSequence, int reason, const char* event, void* pUserData) = 0;
	// </interfuscator:shuffle>
};

/**	Animation sequence, operates on animation nodes contained in it.
 */
struct IAnimSequence : public _i_reference_target_t
{
	static const int kSequenceVersion = 4;

	//! Flags used for SetFlags(),GetFlags(),SetParentFlags(),GetParentFlags() methods.
	enum EAnimSequenceFlags
	{
		eSeqFlags_PlayOnReset        = BIT(0),  //!< Start playing this sequence immediately after reset of movie system(Level load).
		eSeqFlags_OutOfRangeConstant = BIT(1),  //!< Constant Out-Of-Range,time continues normally past sequence time range.
		eSeqFlags_OutOfRangeLoop     = BIT(2),  //!< Loop Out-Of-Range,time wraps back to the start of range when reaching end of range.
		eSeqFlags_CutScene           = BIT(3),  //!< Cut scene sequence.
		eSeqFlags_NoUI               = BIT(4),  //!< Don`t display any UI
		eSeqFlags_NoPlayer           = BIT(5),  //!< Disable input and drawing of player
		eSeqFlags_NoSeek             = BIT(10), //!< Cannot seek in sequence.
		eSeqFlags_NoAbort            = BIT(11), //!< Cutscene can not be aborted
		eSeqFlags_NoSpeed            = BIT(13), //!< Cannot modify sequence speed - TODO: add interface control if required
		eSeqFlags_CanWarpInFixedTime = BIT(14), //!< Timewarping will work with a fixed time step.
		eSeqFlags_EarlyMovieUpdate   = BIT(15), //!< Turn the 'sys_earlyMovieUpdate' on during the sequence.
		eSeqFlags_LightAnimationSet  = BIT(16), //!< A special unique sequence for light animations
		eSeqFlags_NoMPSyncingNeeded  = BIT(17), //!< this sequence doesn't require MP net syncing
		eSeqFlags_Capture            = BIT(18), //!< this sequence is currently in capture mode
	};

	//! Set the name of this sequence. (e.g. "Intro" in the same case as above).
	virtual void SetName(const char* name) = 0;

	//! Get the name of this sequence. (e.g. "Intro" in the same case as above).
	virtual const char* GetName() const = 0;

	//! Get the ID (unique in a level and consistent across renaming) of this sequence.
	virtual uint32              GetId() const = 0;

	virtual CryGUID             GetGUID() const = 0;

	virtual void                SetOwner(IAnimSequenceOwner* pOwner) = 0;
	virtual IAnimSequenceOwner* GetOwner() const = 0;

	//! Set the currently active director node.
	virtual void       SetActiveDirector(IAnimNode* pDirectorNode) = 0;
	//! Get the currently active director node, if any.
	virtual IAnimNode* GetActiveDirector() const = 0;

	//! Get the fixed time step for timewarping.
	virtual float GetFixedTimeStep() const = 0;
	//! Set the fixed time step for timewarping.
	virtual void  SetFixedTimeStep(float dt) = 0;

	//! Set animation sequence flags.
	virtual void SetFlags(int flags) = 0;

	//! Get animation sequence flags.
	virtual int GetFlags() const = 0;

	//! Get cutscene related animation sequence flags.
	virtual int GetCutSceneFlags(const bool localFlags = false) const = 0;

	//! Set parent animation sequence.
	virtual void SetParentSequence(IAnimSequence* pParentSequence) = 0;

	//! Get parent animation sequence.
	virtual const IAnimSequence* GetParentSequence() const = 0;

	//! Check whether this sequence has the given sequence as a descendant through one of its sequence tracks.
	virtual bool IsAncestorOf(const IAnimSequence* pSequence) const = 0;

	//! Return number of animation nodes in sequence.
	virtual int        GetNodeCount() const = 0;
	//! Get animation node at specified index.
	virtual IAnimNode* GetNode(int index) const = 0;

	//! Add animation node to sequence.
	//! @return True if node added, same node will not be added 2 times.
	virtual bool AddNode(IAnimNode* node) = 0;

	//! Creates a new animation node with specified type.
	//! \param nodeType Type of node, one of EAnimNodeType enums.
	virtual IAnimNode* CreateNode(EAnimNodeType nodeType) = 0;

	//! Creates a new animation node from serialized node XML.
	//! \param node Serialized form of node.
	virtual IAnimNode* CreateNode(XmlNodeRef node) = 0;

	//! Remove animation node from sequence.
	virtual void RemoveNode(IAnimNode* node) = 0;

	//! Finds node by name, can be slow.
	//! If the node belongs to a director, the director node also should be given.
	//! Since there can be multiple instances of the same node(i.e. the same name) across multiple director nodes.
	virtual IAnimNode* FindNodeByName(const char* sNodeName, const IAnimNode* pParentDirector) = 0;

	//! Remove all nodes from sequence.
	virtual void RemoveAll() = 0;

	//! Activate sequence by binding sequence animations to nodes.
	//! Must be called prior animating sequence.
	virtual void Activate() = 0;

	//! Check if sequence is activated.
	virtual bool IsActivated() const = 0;

	//! Deactivates sequence by unbinding sequence animations from nodes.
	virtual void Deactivate() = 0;

	//! Pre-caches data associated with this anim sequence.
	virtual void PrecacheData(SAnimTime startTime = SAnimTime(0.0f)) = 0;

	//! Update sequence while not playing animation.
	virtual void StillUpdate() = 0;

	//! Render function call for some special node.
	virtual void Render() = 0;

	//! Evaluate animations of all nodes in sequence.
	//! Sequence must be activated before animating.
	virtual void Animate(const SAnimContext& animContext) = 0;

	//! Set time range of this sequence.
	virtual void SetTimeRange(TRange<SAnimTime> timeRange) = 0;

	//! Get time range of this sequence.
	virtual TRange<SAnimTime> GetTimeRange() = 0;

	//! Resets the sequence.
	virtual void Reset(bool bSeekToStart) = 0;

	//! Called to pause sequence.
	virtual void Pause() = 0;

	//! Called to resume sequence.
	virtual void Resume() = 0;

	//! Called to check if sequence is paused.
	virtual bool IsPaused() const = 0;

	//! Serialize this sequence to XML.
	virtual void Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks = true, uint32 overrideId = 0, bool bResetLightAnimSet = false) = 0;

	//! Adds/removes track events in sequence.
	virtual bool AddTrackEvent(const char* szEvent) = 0;
	virtual bool RemoveTrackEvent(const char* szEvent) = 0;
	virtual bool RenameTrackEvent(const char* szEvent, const char* szNewEvent) = 0;
	virtual bool MoveUpTrackEvent(const char* szEvent) = 0;
	virtual bool MoveDownTrackEvent(const char* szEvent) = 0;
	virtual void ClearTrackEvents() = 0;

	//! Gets the track events in the sequence.
	virtual int GetTrackEventsCount() const = 0;

	//! Gets the specified track event in the sequence.
	virtual char const* GetTrackEvent(int iIndex) const = 0;

	//! Called to trigger a track event.
	virtual void TriggerTrackEvent(const char* event, const char* param = NULL) = 0;

	//! Track event listener.
	virtual void AddTrackEventListener(ITrackEventListener* pListener) = 0;
	virtual void RemoveTrackEventListener(ITrackEventListener* pListener) = 0;
	// </interfuscator:shuffle>

	//! Sequence audio trigger
	virtual void                         SetAudioTrigger(const SSequenceAudioTrigger& audioTrigger) = 0;
	virtual const SSequenceAudioTrigger& GetAudioTrigger() const = 0;
};

//! Movie Listener interface.
//! Register at movie system to get notified about movie events.
struct IMovieListener
{
	enum EMovieEvent
	{
		eMovieEvent_Started = 0,    //!< Fired when sequence is started.
		eMovieEvent_Stopped,        //!< Fired when sequence ended normally.
		eMovieEvent_Aborted,        //!< Fired when sequence was aborted before normal end (STOP and ABORTED event are mutually exclusive!).
		eMovieEvent_Updated,        //!< Fired after sequence time or playback speed was updated.
		eMovieEvent_CaptureStarted, //!< Fired when sequence capture was started.
		eMovieEvent_CaptureStopped, //!< Fired when sequence capture was stopped.
	};

	// <interfuscator:shuffle>
	virtual ~IMovieListener() {}

	//! callback on movie events.
	virtual void OnMovieEvent(EMovieEvent movieEvent, IAnimSequence* pAnimSequence) = 0;
	// </interfuscator:shuffle>
};

struct IMovieEngineModule : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(IMovieEngineModule, "271a9f97-7e6d-4cfa-b3ae-2a5c3227d302"_cry_guid);
};
//! \endcond

//! Movie System (TrackView) interface.
//! Main entrance point to engine movie capability.
//! Enumerate available movies, update all movies, create animation nodes and tracks.
struct IMovieSystem
{
	enum ESequenceStopBehavior
	{
		eSSB_LeaveTime     = 0,  //!< When sequence is stopped it remains in last played time.
		eSSB_GotoEndTime   = 1,  //!< Default behavior in game, sequence is animated at end time before stop.
		eSSB_GotoStartTime = 2,  //!< Default behavior in editor, sequence is animated at start time before stop.
	};

	// <interfuscator:shuffle>
	virtual ~IMovieSystem(){}

	//virtual void GetMemoryUsage( ICrySizer *pSizer ) const=0;

	//! Release movie system.
	virtual void           Release() = 0;
	//! Set the user.
	virtual void           SetUser(IMovieUser* pUser) = 0;
	//! Get the user.
	virtual IMovieUser*    GetUser() = 0;
	//! Loads all nodes and sequences from a specific file (should be called when the level is loaded).
	virtual bool           Load(const char* pszFile, const char* pszMission) = 0;

	virtual IAnimSequence* CreateSequence(const char* sequence, bool bLoad = false, uint32 id = 0) = 0;
	virtual IAnimSequence* LoadSequence(XmlNodeRef& xmlNode, bool bLoadEmpty = true) = 0;
	virtual void           AddSequence(IAnimSequence* pSequence) = 0;
	virtual void           RemoveSequence(IAnimSequence* pSequence) = 0;
	virtual IAnimSequence* FindSequence(const char* sequence) const = 0;
	virtual IAnimSequence* FindSequenceById(uint32 id) const = 0;
	virtual IAnimSequence* FindSequenceByGUID(CryGUID guid) const = 0;
	virtual IAnimSequence* GetSequence(int i) const = 0;
	virtual int            GetNumSequences() const = 0;
	virtual IAnimSequence* GetPlayingSequence(int i) const = 0;
	virtual int            GetNumPlayingSequences() const = 0;
	virtual bool           IsCutScenePlaying() const = 0;

	virtual uint32         GrabNextSequenceId() = 0;

	//! Adds a listener to a sequence.
	//! \param pSequence Pointer to sequence.
	//! \param pListener Pointer to an IMovieListener.
	//! \return true on successful add, false otherwise.
	virtual bool AddMovieListener(IAnimSequence* pSequence, IMovieListener* pListener) = 0;

	//! Removes a listener from a sequence.
	//! \param pSequence Pointer to sequence.
	//! \param pListener Pointer to an IMovieListener.
	//! \return true on successful removal, false otherwise.
	virtual bool     RemoveMovieListener(IAnimSequence* pSequence, IMovieListener* pListener) = 0;

	virtual ISystem* GetSystem() = 0;

	//! Remove all sequences from movie system.
	virtual void RemoveAllSequences() = 0;

	// Sequence playback.

	//! Start playing sequence. Ignored if sequence is already playing.
	//! \param sequence Name of sequence to play.
	virtual void PlaySequence(const char* pSequenceName, IAnimSequence* pParentSeq, bool bResetFX, bool bTrackedSequence,
	                          SAnimTime startTime = SAnimTime::Min(), SAnimTime endTime = SAnimTime::Min()) = 0;

	//! Start playing sequence. Ignored if sequence is already playing.
	//! \param sequence Pointer to Valid sequence to play.
	virtual void PlaySequence(IAnimSequence* pSequence, IAnimSequence* pParentSeq, bool bResetFX, bool bTrackedSequence,
	                          SAnimTime startTime = SAnimTime::Min(), SAnimTime endTime = SAnimTime::Min()) = 0;

	//! Stops currently playing sequence. Ignored if sequence is not playing.
	//! \param sequence Name of playing sequence to stop.
	//! \return true if sequence has been stopped. false otherwise.
	virtual bool StopSequence(const char* pSequenceName) = 0;

	//! Stop's currently playing sequence. Ignored if sequence is not playing.
	//! \param sequence Pointer to Valid sequence to stop.
	//! \return true if sequence has been stopped. false otherwise.
	virtual bool StopSequence(IAnimSequence* pSequence) = 0;

	//! Aborts a currently playing sequence. Ignored if sequence is not playing.
	//! Calls IMovieListener with MOVIE_EVENT_ABORTED event (MOVIE_EVENT_DONE is NOT called)
	//! \param sequence Pointer to Valid sequence to stop.
	//! \param bLeaveTime If false, uses default stop behavior, otherwise leaves the sequence at time
	//! \return true if sequence has been aborted. false otherwise
	virtual bool AbortSequence(IAnimSequence* pSequence, bool bLeaveTime = false) = 0;

	//! Stops all currently playing sequences.
	virtual void StopAllSequences() = 0;

	//! Stops all playing cut-scene sequences.
	//! This will not stop all sequences, but only those with CUT_SCENE flag set.
	virtual void StopAllCutScenes() = 0;

	//! Checks if specified sequence is playing.
	virtual bool IsPlaying(IAnimSequence* seq) const = 0;

	//! Resets playback state of movie system.
	//! Usually called after loading of level,
	virtual void Reset(bool bPlayOnReset, bool bSeekToStart) = 0;

	//! Sequences with PLAY_ONRESET flag will start playing after this call.
	virtual void PlayOnLoadSequences() = 0;

	//! Update movie system while not playing animation.
	virtual void StillUpdate() = 0;

	//! Updates movie system every frame before the entity system to animate all playing sequences.
	virtual void PreUpdate(const float dt) = 0;

	//! Updates movie system every frame after the entity system to animate all playing sequences.
	virtual void PostUpdate(const float dt) = 0;

	//! Render function call of some special node.
	virtual void Render() = 0;

	//! Signal the capturing start.
	virtual void StartCapture(IAnimSequence* seq, const SCaptureKey& key) = 0;

	//! Signal the capturing end.
	virtual void EndCapture() = 0;

	//! Actually turn on/off the capturing.
	//! This is needed for the timing issue of turning on/off the capturing.
	virtual void ControlCapture() = 0;

	//! Set movie system to recording mode.
	//! While in recording mode any changes made to node will be added as keys to tracks.
	//virtual void SetRecording(bool recording) = 0;
	//virtual bool IsRecording() const = 0;

	virtual void EnableCameraShake(bool bEnabled) = 0;
	virtual bool IsCameraShakeEnabled() const = 0;

	//! Pause any playing sequences.
	virtual void Pause() = 0;

	//! Resume playing sequences.
	virtual void Resume() = 0;

	//! Pause cut scenes in editor.
	//virtual void PauseCutScenes() = 0;

	//! Resume cut scenes in editor.
	//virtual void ResumeCutScenes() = 0;

	//! Callback when animation-data changes.
	virtual void            SetCallback(IMovieCallback* pCallback) = 0;

	virtual IMovieCallback* GetCallback() = 0;

	//! Serialize to XML.
	virtual void                 Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bRemoveOldNodes = false, bool bLoadEmpty = true) = 0;

	virtual const SCameraParams& GetCameraParams() const = 0;
	virtual void                 SetCameraParams(const SCameraParams& Params) = 0;
	virtual void                 SendGlobalEvent(const char* pszEvent) = 0;

	//! Gets the float time value for a sequence that is already playing.
	virtual SAnimTime GetPlayingTime(IAnimSequence* pSeq) = 0;
	virtual float     GetPlayingSpeed(IAnimSequence* pSeq) = 0;
	//! Sets the time progression of an already playing cutscene.
	//! If IAnimSequence:NO_SEEK flag is set on pSeq, this call is ignored.
	virtual bool SetPlayingTime(IAnimSequence* pSeq, SAnimTime fTime) = 0;
	virtual bool SetPlayingSpeed(IAnimSequence* pSeq, float fSpeed) = 0;
	//! Set behavior pattern for stopping sequences.
	virtual void SetSequenceStopBehavior(ESequenceStopBehavior behavior) = 0;

	//! Set the start and end time of an already playing cutscene.
	virtual bool GetStartEndTime(IAnimSequence* pSeq, SAnimTime& fStartTime, SAnimTime& fEndTime) = 0;
	virtual bool SetStartEndTime(IAnimSequence* pSeq, const SAnimTime fStartTime, const SAnimTime fEndTime) = 0;

	//! Make the specified sequence go to a given frame time.
	//! \param seqName A sequence name.
	//! \param targetFrame A target frame to go to in time.
	virtual void GoToFrame(const char* seqName, float targetFrame) = 0;

	//! Get the name of camera used for sequences instead of cameras specified in the director node.
	virtual const char* GetOverrideCamName() const = 0;

	//! Get behavior pattern for stopping sequences.
	virtual IMovieSystem::ESequenceStopBehavior GetSequenceStopBehavior() = 0;

	//! These are used to disable 'Ragdollize' events in the editor when the 'AI/Physics' is off.
	virtual bool               IsPhysicsEventsEnabled() const = 0;
	virtual void               EnablePhysicsEvents(bool enable) = 0;

	virtual void               EnableBatchRenderMode(bool bOn) = 0;
	virtual bool               IsInBatchRenderMode() const = 0;

	virtual ILightAnimWrapper* CreateLightAnimWrapper(const char* szName) const = 0;

	//! Should only be called from CAnimParamType
	virtual void        SerializeParamType(CAnimParamType& animParamType, XmlNodeRef& xmlNode, bool bLoading, const uint version) = 0;

	virtual const char* GetParamTypeName(const CAnimParamType& animParamType) = 0;

#ifdef MOVIESYSTEM_SUPPORT_EDITING
	virtual EAnimNodeType  GetNodeTypeFromString(const char* pString) const = 0;
	virtual CAnimParamType GetParamTypeFromString(const char* pString) const = 0;

	//! Get all node types
	virtual DynArray<EAnimNodeType> GetAllNodeTypes() const = 0;

	// Get defualt node name for this type
	virtual const char* GetDefaultNodeName(EAnimNodeType type) const = 0;
#endif

	// </interfuscator:shuffle>
};

//! \cond INTERNAL
inline void SAnimContext::Serialize(XmlNodeRef& xmlNode, bool bLoading)
{
	if (bLoading)
	{
		XmlString name;

		if (xmlNode->getAttr("sequence", name))
		{
			pSequence = gEnv->pMovieSystem->FindSequence(name.c_str());
		}

		float floatDt = 0.0f;
		float floatTime = 0.0f;
		float floatStartTime = 0.0f;

		xmlNode->getAttr("dt", floatDt);
		xmlNode->getAttr("time", floatTime);
		xmlNode->getAttr("bSingleFrame", bSingleFrame);
		xmlNode->getAttr("bResetting", bResetting);
		xmlNode->getAttr("trackMask", trackMask);
		xmlNode->getAttr("startTime", floatStartTime);

		dt = SAnimTime(floatDt);
		time = SAnimTime(floatTime);
		startTime = SAnimTime(floatStartTime);
	}
	else
	{
		if (pSequence)
		{
			string fullname = pSequence->GetName();
			xmlNode->setAttr("sequence", fullname.c_str());
		}

		const float floatDt = dt.ToFloat();
		const float floatTime = time.ToFloat();
		const float floatStartTime = startTime.ToFloat();

		xmlNode->setAttr("dt", floatDt);
		xmlNode->setAttr("time", floatTime);
		xmlNode->setAttr("bSingleFrame", bSingleFrame);
		xmlNode->setAttr("bResetting", bResetting);
		xmlNode->setAttr("trackMask", trackMask);
		xmlNode->setAttr("startTime", floatStartTime);
	}
}

inline void SSequenceAudioTrigger::Serialize(XmlNodeRef xmlNode, bool bLoading)
{
	if (bLoading)
	{
		if (xmlNode->getAttr("onStopAudioTrigger"))
		{
			m_onStopTriggerName = xmlNode->getAttr("onStopAudioTrigger");
			m_onStopTrigger = CryAudio::StringToId(m_onStopTriggerName.c_str());
		}
		if (xmlNode->getAttr("onPauseAudioTrigger"))
		{
			m_onPauseTriggerName = xmlNode->getAttr("onPauseAudioTrigger");
			m_onPauseTrigger = CryAudio::StringToId(m_onPauseTriggerName.c_str());
		}
		if (xmlNode->getAttr("onResumeAudioTrigger"))
		{
			m_onResumeTriggerName = xmlNode->getAttr("onResumeAudioTrigger");
			m_onResumeTrigger = CryAudio::StringToId(m_onResumeTriggerName.c_str());
		}

	}
	else
	{
		if (m_onStopTrigger != CryAudio::InvalidControlId)
		{
			xmlNode->setAttr("onStopAudioTrigger", m_onStopTriggerName.c_str());
		}
		if (m_onPauseTrigger != CryAudio::InvalidControlId)
		{
			xmlNode->setAttr("onPauseAudioTrigger", m_onPauseTriggerName.c_str());
		}
		if (m_onResumeTrigger != CryAudio::InvalidControlId)
		{
			xmlNode->setAttr("onResumeAudioTrigger", m_onResumeTriggerName.c_str());
		}
	}
}

inline void SSequenceAudioTrigger::Serialize(Serialization::IArchive& ar)
{
	if (ar.isInput())
	{
		string stopTriggerName;
		ar(Serialization::AudioTrigger<string>(stopTriggerName), "onStopAudioTrigger", "onStop");
		if (!stopTriggerName.empty())
		{
			m_onStopTrigger = CryAudio::StringToId(stopTriggerName.c_str());
		}

		string pauseTriggerName;
		ar(Serialization::AudioTrigger<string>(pauseTriggerName), "onPauseAudioTrigger", "onPause");
		if (!pauseTriggerName.empty())
		{
			m_onPauseTrigger = CryAudio::StringToId(pauseTriggerName.c_str());
		}

		string resumeTriggerName;
		ar(Serialization::AudioTrigger<string>(resumeTriggerName), "onResumeAudioTrigger", "onResume");
		if (!resumeTriggerName.empty())
		{
			m_onResumeTrigger = CryAudio::StringToId(resumeTriggerName.c_str());
		}
	}
	else
	{
		ar(Serialization::AudioTrigger<string>(m_onStopTriggerName), "onStopAudioTrigger", "onStop");
		ar(Serialization::AudioTrigger<string>(m_onPauseTriggerName), "onPauseAudioTrigger", "onPause");
		ar(Serialization::AudioTrigger<string>(m_onResumeTriggerName), "onResumeAudioTrigger", "onResume");
	}
}

void CAnimParamType::Serialize(XmlNodeRef& xmlNode, bool bLoading, const uint version)
{
	gEnv->pMovieSystem->SerializeParamType(*this, xmlNode, bLoading, version);
}

//! \endcond
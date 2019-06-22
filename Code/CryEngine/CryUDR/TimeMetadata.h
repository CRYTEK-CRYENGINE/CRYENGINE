// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry 
{
	namespace UDR
	{

		class CTimeMetadata final : public ITimeMetadata
		{
		public:
			// ITimeMetadata
			virtual uint32     GetFrameID() const override;
			virtual CTimeValue GetTimestamp() const override;

			virtual bool       IsValid() const override;
			// ~ITimeMetadata

			//! Initializes the metadata. After a successful initialization, the object is guaranteed to be valid.
			//! \return True if the metadata was successfully initialized. False otherwise.
			bool               Initialize();

			//! Checks if the metadata was initialized in the provided closed interval [frameIDStart, frameIDEnd].
			//! \param frameIDStart Start frame of the closed interval.
			//! \param frameIDEnd End frame of the closed interval.
			//! \return True if the metadata was initialized in the closed interval [frameIDStart, frameIDEnd].
			bool               IsInFrameInterval(uint32 frameIDStart, uint32 frameIDEnd) const;

			//! Checks if the metadata was initialized in the provided closed interval [timeStart, timeEnd].
			//! \param timeStart  Start time of the closed interval.
			//! \param timeEnd End time of the closed interval.
			//! \return True if the metadata was initialized in the closed interval [timeStart, timeEnd].
			bool               IsInTimeInterval(CTimeValue timeStart, CTimeValue timeEnd) const;

			void               Serialize(Serialization::IArchive& ar);

			bool               operator <(const CTimeMetadata& rhs) const;
			bool               operator >(const CTimeMetadata& rhs) const;
			bool               operator >=(const CTimeMetadata& rhs) const;
			bool               operator <=(const CTimeMetadata& rhs) const;
			bool               operator ==(const CTimeMetadata& rhs) const;
			bool               operator !=(const CTimeMetadata& rhs) const;

		private:

			CTimeValue         m_timestamp;
			uint32             m_frameID = 0;
		};

	}
}

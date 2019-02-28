/*
	stream manager declaration which converts coefficient vectors to streams for export, compresses if requested too
*/
#if defined(OFFLINE_COMPUTATION)

#pragma once

#include "PRTTypes.h"
#include "TransferParameters.h"
#include "ITransferConfigurator.h"

#pragma warning (disable : 4512) 

namespace NSH
{
	//!< stream manager, creates streams for export with possible compression
	//!< template arguments for all coefficient types
	//!< individual instance per mesh
	template<class DirectCoeffType>
	class CMeshCoefficientStreams
	{
		typedef prtvector<NSH::SCoeffList_tpl<DirectCoeffType> > TDirectCoeffVec;
		#define TCoeffList prtvector<NSH::SCoeffList_tpl<CoeffType> >

	public:
		//!< constructor taking  the references for the stream creation, stream creation happens on demand
		CMeshCoefficientStreams 
		(
			const NSH::SDescriptor& crDescriptor,								
			const NTransfer::STransferParameters& crParameters, 
			CSimpleIndexedMesh& rMesh, 
			TDirectCoeffVec&		rCoeffsToStore, 
			const NSH::NTransfer::ITransferConfigurator& crTransferConfigurator
		);

		~CMeshCoefficientStreams();

		const TFloatPtrVec& GetStream();	//!< retrieves the direct coeff stream

		void RetrieveCompressedCoeffs(NSH::NFramework::SMeshCompressedCoefficients& rCompressedData);	//!< fill the compressed data struct
		static void FillCompressedCoeffs
		(
			NSH::NFramework::SMeshCompressedCoefficients& rCompressedData,
			const CSimpleIndexedMesh& crMesh,
			const NSH::TFloatPtrVec& crDirectCoeffStream
		);		//!< fill the compressed data struct

	private:
		NSH::SDescriptor m_Descriptor;										//!< transfer sh descriptor	
		NTransfer::STransferParameters m_Parameters;			//!< transfer parameters controlling the transfer and compression
		TDirectCoeffVec&		m_rDirectCoeffsToStore;				//!< direct coefficients to streamconvert
		TFloatPtrVec m_CoeffStreams;											//!< coefficient stream
		CSimpleIndexedMesh& m_rMesh;											//!< mesh belonging too
		const NSH::NTransfer::ITransferConfigurator& m_crTransferConfigurator;	//!< transfer configurator to work with
		//!< compression infos for each material, empty if no compression is requested, compression info index corresponds to the respective m_ObjMaterials index
		TCompressionInfo	m_MaterialCompressionInfo;
		bool m_StreamCreated;															//!< indicates whether the streams were created yet or not(to make it on demand)

		void PrepareCoefficientsForExport();	//!< prepares coefficients for export, changes the coefficients doing so
		//!< compresses the prepared coefficients, changes the coefficient floats to the byte number (0.3 becomes f.i. 240.0)
		template<class CoeffType>
			void Compress(TCoeffList& rCoeffsToCompress);	
		//!< create a single stream, still floats but the meaning is different if it is compressed
		template<class CoeffType>
			void CreateSingleStream(TCoeffList& rCoeffsToStreamConvert);
		void CreateStreams();									//!< creates the streams controlled by data stored in rMesh and m_Parameters
	};
}

#include "MeshCoefficientStreams.inl"
#pragma warning (default : 4512)

#endif
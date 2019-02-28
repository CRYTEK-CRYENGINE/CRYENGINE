/*
stream manager implementation which converts coefficient vectors to streams for export, compresses if requested too
*/

#include "SHRotate.h"
#include "ObjFace.h"

#undef min
#undef max


template<class DirectCoeffType>
inline NSH::CMeshCoefficientStreams<DirectCoeffType>::CMeshCoefficientStreams
(
	const NSH::SDescriptor& crDescriptor,								
	const NTransfer::STransferParameters& crParameters, 
	CSimpleIndexedMesh& rMesh, 
	TDirectCoeffVec&		rCoeffsToStore, 
	const NSH::NTransfer::ITransferConfigurator& crTransferConfigurator
) : 
	m_Descriptor(crDescriptor),
	m_Parameters(crParameters),
	m_rDirectCoeffsToStore(rCoeffsToStore), 
	m_rMesh(rMesh),
	m_StreamCreated(false),
	m_crTransferConfigurator(crTransferConfigurator)
{}

template<class DirectCoeffType>
template<class CoeffType>
void NSH::CMeshCoefficientStreams<DirectCoeffType>::Compress
(	
	TCoeffList& rCoeffsToCompress
)
{
	TCompressionInfo& rCompressionInfoDest = m_MaterialCompressionInfo;
	if(m_Parameters.compressToByte)
	{
		//now resize the compression infos according to number of materials
		if(rCompressionInfoDest.size() != m_rMesh.MaterialCount())
			rCompressionInfoDest.resize(m_rMesh.MaterialCount());
		//gather for each material the max and min value for each coefficient
		TFloatPairVecVec matMinMaxValues(m_rMesh.MaterialCount());
		//reserve the space and reset the values
		const TFloatPairVecVec::iterator cEnd = matMinMaxValues.end();
		for(TFloatPairVecVec::iterator iter = matMinMaxValues.begin(); iter != cEnd; ++iter)
		{
			iter->resize(m_rMesh.GetExportPolicy().coefficientsPerSet);//resize according to number of coefficients per set
			for(TFloatPairVec::iterator pairIter = iter->begin(); pairIter != iter->end(); ++pairIter)
			{
				//initialize max/min accumulators
				pairIter->first	 = std::numeric_limits<float>::max();
				pairIter->second = std::numeric_limits<float>::min();
			}
		}
		for(uint32 i=0; i<m_rMesh.GetFaceCount(); ++i)
		{
			//go through all faces(can't get the material from the vertices directly)
			if(!m_rMesh.ComputeSHCoeffs(i))
				continue;//no sh coeffs for this material
			const CObjFace& rObjFace = m_rMesh.GetObjFace(i);
			for(int v = 0; v<3; ++v)//for each face index
			{
				const uint32 cVertexIndex = rObjFace.v[v];
				const uint32 cMatIndex = m_rMesh.GetMaterialIndexByFaceID(i);
				const SCoeffList_tpl<CoeffType>& crCoeff = rCoeffsToCompress[cVertexIndex];	//cache vertex coeffs
				TFloatPairVec& rMinMaxVec = matMinMaxValues[cMatIndex]; //cache mat minmax vec
				//now fill the min and max values for each coefficient and material
				for(int c=0; c<m_rMesh.GetExportPolicy().coefficientsPerSet; ++c)
				{
					TFloatPair& rMinMax	= rMinMaxVec[c];
					for(int comp=0; comp<CoeffType::Components(); ++comp)
					{
						const float cCoeffValue = (float)((crCoeff[c])[comp]);
						if(rMinMax.first > cCoeffValue)
							rMinMax.first = cCoeffValue;
						if(rMinMax.second < cCoeffValue)
							rMinMax.second = cCoeffValue;
					}
				}
			}
		}
		//determine scale and scale coefficients for each mat and coeff, write into mesh compression info
		for(int matIndex = 0; matIndex<(int)m_rMesh.MaterialCount(); ++matIndex)
		{
			if(!m_rMesh.ComputeSHForMatFromIndex(matIndex))
				continue;//no sh coeffs for this material
			SCompressionInfo& rCompressionInfo = rCompressionInfoDest[matIndex];
			//set compression info
			rCompressionInfo.compressionValue.resize(m_rMesh.GetExportPolicy().coefficientsPerSet);
			rCompressionInfo.isCompressed = true;
			const TFloatPairVec& crMinMaxVec = matMinMaxValues[matIndex]; //cache mat minmax vec
			for(int c=0; c<m_rMesh.GetExportPolicy().coefficientsPerSet; ++c)
			{
				const TFloatPair& crMinMax	= crMinMaxVec[c];
				//shift values into range 0..1 to create the 256 values from that
				rCompressionInfo.compressionValue[c].second = -crMinMax.first; //set compression offset
				rCompressionInfo.compressionValue[c].first = 255.f / (crMinMax.second - crMinMax.first); //set compression scale to fit into 0..255
			}
		}
		//now compress the coefficients, leave it as floats but write the byte index
		//iterate for the second time
		TVertexIndexSet vertexIndexSet;//to not compress a single vertex twice
		for(uint32 i=0; i<m_rMesh.GetFaceCount(); ++i)
		{
			//go through all faces(can't get the material from the vertices directly)
			if(!m_rMesh.ComputeSHCoeffs(i))
				continue;//no sh coeffs for this material
			const SCompressionInfo& rCompressionInfo = rCompressionInfoDest[m_rMesh.GetMaterialIndexByFaceID(i)];
			const CObjFace& rObjFace = m_rMesh.GetObjFace(i);
			for(int v = 0; v<3; ++v)//for each face index
			{
				const uint32 cVertexIndex = rObjFace.v[v];
				if(!(vertexIndexSet.insert(cVertexIndex)).second)
					continue;//already processed vertex
				SCoeffList_tpl<CoeffType>& crCoeffs = rCoeffsToCompress[cVertexIndex];	//cache vertex coeffs
				for(int c=0; c<m_rMesh.GetExportPolicy().coefficientsPerSet; ++c)
				{
					for(int comp=0; comp<CoeffType::Components(); ++comp)
					{
						//compress all coefficient and components
						typename CoeffType::TComponentType& rCoeff = crCoeffs[c][comp];
						rCoeff += rCompressionInfo.compressionValue[c].second;	//add offset, now in range 0...
						rCoeff *= rCompressionInfo.compressionValue[c].first;	  //scale into 0..255.0
						assert(rCoeff > -0.1f && rCoeff < 255.1f);
						//clamp values, it is just about some inaccuracies
						rCoeff = std::max(rCoeff, (typename CoeffType::TComponentType)0.0);
						rCoeff = std::min(rCoeff, (typename CoeffType::TComponentType)255.0);
					}
				}
			}
		}
		m_rMesh.GetExportPolicy().compressed = true; //mark as to be compressed
		m_rMesh.GetExportPolicy().compressionBytesPerComponent = 1;//byte compressed
		m_rMesh.GetExportPolicy().materialCompressionInfo = rCompressionInfoDest;
	}//if(m_Parameters.compressToByte)
}

template<class DirectCoeffType>
void NSH::CMeshCoefficientStreams<DirectCoeffType>::PrepareCoefficientsForExport()
{
	//determine whether the direct coefficients are convoluted with a cos kernel or used for lookups
	const EBumpVisUsage cBumpVisUsage = m_crTransferConfigurator.UseCoefficientLookupMode()?BUMP_VIS_USAGE_LOOKUP : BUMP_VIS_USAGE_COS_CONVOLUTION;
	
	if(m_Descriptor.Bands != 3)
	{
		//not supported yet
		GetSHLog().LogError("PrepareCoefficientsForExport does not yet support more than 3 bands\n");
		//set export policy
		m_rMesh.GetExportPolicy().bytesPerComponent	 = 4;
		m_rMesh.GetExportPolicy().coefficientsPerSet = m_Descriptor.Coefficients;
		m_rMesh.GetExportPolicy().swizzled					 = false;
		m_rMesh.GetExportPolicy().preMultiplied			 = false;
		return;
	}

	Matrix33_tpl<float> rotMat;
	//get rotation matrix for target coord system
	switch(m_Parameters.matrixTargetCS)
	{
	case NTransfer::MATRIX_HEURISTIC_SH:
		rotMat.SetIdentity();
		break;
	case NTransfer::MATRIX_HEURISTIC_MAX:
		//we need a rotation of -90 degree around z
		rotMat.SetIdentity();
		rotMat(0,0) *= -1.f;
		rotMat(1,1) *= -1.f;
		break;
	};

	CRotateMatrix_tpl<float, false> shRotMatrix(m_Descriptor.Coefficients);
	shRotMatrix.SetSHRotation(rotMat);

	//convert shadow coefficients if for bump mapping
	if(m_Parameters.bumpGranularity)
	{
		//prepare matrix for visibility lookup
		const float cv0 = (cBumpVisUsage == BUMP_VIS_USAGE_LOOKUP)? SCoeffUsage<BUMP_VIS_USAGE_LOOKUP>::cv0 : SCoeffUsage<BUMP_VIS_USAGE_COS_CONVOLUTION>::cv0;  
		const float cv1 = (cBumpVisUsage == BUMP_VIS_USAGE_LOOKUP)? SCoeffUsage<BUMP_VIS_USAGE_LOOKUP>::cv1 : SCoeffUsage<BUMP_VIS_USAGE_COS_CONVOLUTION>::cv1;  
		const float cv2 = (cBumpVisUsage == BUMP_VIS_USAGE_LOOKUP)? SCoeffUsage<BUMP_VIS_USAGE_LOOKUP>::cv2 : SCoeffUsage<BUMP_VIS_USAGE_COS_CONVOLUTION>::cv2;  
		const float cv3 = (cBumpVisUsage == BUMP_VIS_USAGE_LOOKUP)? SCoeffUsage<BUMP_VIS_USAGE_LOOKUP>::cv3 : SCoeffUsage<BUMP_VIS_USAGE_COS_CONVOLUTION>::cv3;  
		const float cv4 = (cBumpVisUsage == BUMP_VIS_USAGE_LOOKUP)? SCoeffUsage<BUMP_VIS_USAGE_LOOKUP>::cv4 : SCoeffUsage<BUMP_VIS_USAGE_COS_CONVOLUTION>::cv4;  

		//iterate each  vertex
		//set export policy
		m_rMesh.GetExportPolicy().bytesPerComponent	 = 4;
		m_rMesh.GetExportPolicy().coefficientsPerSet = 8;
		m_rMesh.GetExportPolicy().swizzled					 = true;
		m_rMesh.GetExportPolicy().preMultiplied			 = true;

		TDirectCoeffVec& rCoeffs = m_rDirectCoeffsToStore;
		assert(m_rMesh.GetVertexCount() == m_rMesh.GetNormalCount());	
		for(uint32 i=0; i<m_rMesh.GetVertexCount(); ++i)
		{
			SCoeffList_tpl<DirectCoeffType>& rCoeff = rCoeffs[i];
			SCoeffList_tpl<DirectCoeffType> coeff(rCoeffs[i]);//copy coeff
			shRotMatrix.Transform(coeff, rCoeff);
			double v0[4] = {cv1 * rCoeff[3], cv1 * rCoeff[1], cv1 * rCoeff[2], cv0 * rCoeff[0] - cv3 * rCoeff[6]};
			double v1[4] = {cv4 * rCoeff[8], cv2 * rCoeff[5], 3 * cv3 * rCoeff[6], cv2 * rCoeff[7]};
			for(int c0=0; c0<4; ++c0)
				rCoeff[c0] = (typename DirectCoeffType::TComponentType)v0[c0];
			for(int c1=0; c1<4; ++c1)
				rCoeff[4+c1] = (typename DirectCoeffType::TComponentType)v1[c1];
			rCoeff[8] = 0;
		}
	}
	else
	{
		//just store without Y2-2
		//iterate each vertex
		//set export policy
		m_rMesh.GetExportPolicy().bytesPerComponent		= 4;
		m_rMesh.GetExportPolicy().coefficientsPerSet	= 8;
		m_rMesh.GetExportPolicy().swizzled						= true;
		m_rMesh.GetExportPolicy().preMultiplied				= false;
		TDirectCoeffVec& rCoeffs = m_rDirectCoeffsToStore;

		assert(m_rMesh.GetVertexCount() == m_rMesh.GetNormalCount());	
		for(uint32 i=0; i<m_rMesh.GetVertexCount(); ++i)
		{
			SCoeffList_tpl<DirectCoeffType>& rCoeff = rCoeffs[i];
			SCoeffList_tpl<DirectCoeffType> coeff(rCoeffs[i]);//copy coeff
			shRotMatrix.Transform(coeff, rCoeff);

			for(int c=4; c<8; ++c)
				rCoeff[c] = (typename DirectCoeffType::TComponentType)rCoeff[c+1];
			rCoeff[8] = 0;			
		}
	}
}

template<class DirectCoeffType>
template<class CoeffType>
void NSH::CMeshCoefficientStreams<DirectCoeffType>::CreateSingleStream
(
	TCoeffList& rCoeffsToStreamConvert
)
{
	static CSHAllocator<float> sAllocator;
	
	if(!rCoeffsToStreamConvert.empty())
	{
		const uint8 cCoeffsPerSet = m_rMesh.GetExportPolicy().coefficientsPerSet;
		for(int i=0; i<CoeffType::Components(); ++i)
		{
			//for each component add a separate stream
			//will be deleted in destructor
			float *pStream((float*)(sAllocator.new_mem_array(sizeof(float) * (uint32)rCoeffsToStreamConvert.size() * cCoeffsPerSet)));
			int j=0;
			const TCoeffList::const_iterator cEnd = rCoeffsToStreamConvert.end();
			for(TCoeffList::const_iterator iter=rCoeffsToStreamConvert.begin(); iter != cEnd; ++iter)
			{
				const SCoeffList_tpl<CoeffType>& rCoeff = *iter;
				for(uint8 v=0; v<cCoeffsPerSet; ++v)
				{
					//save all coefficients for one component in a row followed by these for the next vertex and so on
					pStream[j*cCoeffsPerSet+v] = (float)((rCoeff[v])[i]);
				}
				j++;
			}
			m_CoeffStreams.push_back(pStream);
		}
	}
}

template<class DirectCoeffType>
NSH::CMeshCoefficientStreams<DirectCoeffType>::~CMeshCoefficientStreams()
{
	static CSHAllocator<float> sAllocator;	
	const TFloatPtrVec::iterator cEnd = m_CoeffStreams.end();
	for(TFloatPtrVec::iterator iter = m_CoeffStreams.begin(); iter != cEnd; ++iter)
		sAllocator.delete_mem_array(*iter, 0/*dont know size anymore*/);
}

template<class DirectCoeffType>
void NSH::CMeshCoefficientStreams<DirectCoeffType>::CreateStreams()
{
	if(m_StreamCreated)
		return;
	PrepareCoefficientsForExport();	//first prepare coefficients for export
	assert(!m_rMesh.GetExportPolicy().compressed);
	Compress(m_rDirectCoeffsToStore);								//compress direct coeffs
	//now create the streams
	CreateSingleStream(m_rDirectCoeffsToStore);								//create stream for direct coeffs
	m_StreamCreated = true;
}

template<class DirectCoeffType>
inline const NSH::TFloatPtrVec& NSH::CMeshCoefficientStreams<DirectCoeffType>::GetStream
()
{
	if(!m_StreamCreated)
		CreateStreams();
	return m_CoeffStreams;
}

template<class DirectCoeffType>
inline void NSH::CMeshCoefficientStreams<DirectCoeffType>::RetrieveCompressedCoeffs(NSH::NFramework::SMeshCompressedCoefficients& rCompressedData)
{
	if(!m_StreamCreated)
		CreateStreams();
	FillCompressedCoeffs(rCompressedData, m_rMesh, GetStream());
}

template<class DirectCoeffType>
inline void NSH::CMeshCoefficientStreams<DirectCoeffType>::FillCompressedCoeffs
(
	NSH::NFramework::SMeshCompressedCoefficients& rCompressedData,
	const CSimpleIndexedMesh& crMesh,
	const NSH::TFloatPtrVec& crDirectCoeffStream
)
{
	//now process each stream
	const uint8 cCoeffsPerSet = crMesh.GetExportPolicy().coefficientsPerSet;	
	//direct coeffs
	if(!crDirectCoeffStream.empty())
	{
		const float* pStream = 	crDirectCoeffStream[0];
		//cCoeffsPerSet coeffs per coefficient list(and therefore vertex)
		rCompressedData.size = cCoeffsPerSet * crMesh.GetVertexCount();
		if(rCompressedData.pDirectCoeffs)
			delete [] rCompressedData.pDirectCoeffs;
		rCompressedData.pDirectCoeffs = new uint8[rCompressedData.size]; 
		int j=0, byteCount = 0;
		for(uint32 i=0; i<crMesh.GetVertexCount(); ++i)
		{
			for(uint8 v=0; v<cCoeffsPerSet; ++v)
				rCompressedData.pDirectCoeffs[byteCount++] = (uint8)(pStream[j*cCoeffsPerSet+v]);
			j++;
		}
	}
}





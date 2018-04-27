// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SkeletonLoader.h"

#include "../../../CryXML/IXMLSerializer.h"
#include "../../../CryXML/ICryXML.h"
#include <CryCore/CryCrc32.h>
#include "TempFilePakExtraction.h"
#include "StringHelpers.h"

//////////////////////////////////////////////////////////////////////////
static XmlNodeRef LoadXml( const char *filename, ICryXML* pXMLParser )
{
	// Get the xml serializer.
	IXMLSerializer* pSerializer = pXMLParser->GetXMLSerializer();

	// Read in the input file.
	XmlNodeRef root;
	{
		const bool bRemoveNonessentialSpacesFromContent = false;
		char szErrorBuffer[1024];
		root = pSerializer->Read(FileXmlBufferSource(filename), bRemoveNonessentialSpacesFromContent, sizeof(szErrorBuffer), szErrorBuffer);
		if (!root)
		{
			//RCLogError("Cannot open XML file '%s': %s\n", filename, szErrorBuffer);
			return 0;
		}
	}
	return root;
}


//////////////////////////////////////////////////////////////////////////
static uint32 LoadIKSetup( const char* pNameXML, CSkinningInfo* pModelSkeleton, ICryXML* pXMLParser )
{
	XmlNodeRef TopRoot = LoadXml( pNameXML, pXMLParser );
	if (TopRoot==0)
	{
		return 0;
	}

	uint32 numChildren = TopRoot->getChildCount();
	for (uint32 i=0; i<numChildren; i++)
	{
		XmlNodeRef ParamNode = TopRoot->getChild(i);
		const char* TopRootXMLTAG = ParamNode->getTag();
		if (stricmp(TopRootXMLTAG,"IK_Definition"))
		{
			continue;
		}

		uint32 numIKTypes = ParamNode->getChildCount();
		for (uint32 iktype=0; iktype<numIKTypes; iktype++) 
		{
			XmlNodeRef IKNode = ParamNode->getChild(iktype);


			const char* IKTypeTAG = IKNode->getTag();


			//-----------------------------------------------------------
			//check LookIK-Setup  
			//-----------------------------------------------------------
			if (!stricmp(IKTypeTAG,"LookIK_Definition"))
			{

				uint32 numChilds = IKNode->getChildCount();
				for (uint32 c=0; c<numChilds; c++) 
				{
					XmlNodeRef node = IKNode->getChild(c);
					const char* strTag = node->getTag();

					//----------------------------------------------------------------------------
					if (stricmp(strTag,"RotationList")==0) 
					{
						uint32 num = node->getChildCount();
						pModelSkeleton->m_LookIK_Rot.reserve(num);
						for (uint32 i=0; i<num; i++) 
						{
							SJointsAimIK_Rot LookIK_Rot;
							XmlNodeRef nodeRot = node->getChild(i);
							const char* RotTag = nodeRot->getTag();
							if (stricmp(RotTag,"Rotation")==0) 
							{
								const char* pJointName = nodeRot->getAttr( "JointName" );
								int32 jidx = pModelSkeleton->GetJointIDByName(pJointName);
								if (jidx>0)
								{
									LookIK_Rot.m_nJointIdx = jidx;
									LookIK_Rot.m_strJointName = pModelSkeleton->GetJointNameByID(jidx);
								}
								//		assert(jidx>0);
								nodeRot->getAttr( "Primary",    LookIK_Rot.m_nPreEvaluate );
								nodeRot->getAttr( "Additive",   LookIK_Rot.m_nAdditive );
								//	LookIK_Rot.m_nAdditive=0;
								pModelSkeleton->m_LookIK_Rot.push_back( LookIK_Rot );
							}
						}
					}

					//----------------------------------------------------------------------------------------

					if (stricmp(strTag,"PositionList")==0) 
					{
						uint32 num = node->getChildCount();
						pModelSkeleton->m_LookIK_Pos.reserve(num);
						for (uint32 i=0; i<num; i++) 
						{
							SJointsAimIK_Pos LookIK_Pos;
							XmlNodeRef nodePos = node->getChild(i);
							const char* PosTag = nodePos->getTag();
							if (stricmp(PosTag,"Position")==0) 
							{
								const char* pJointName = nodePos->getAttr( "JointName" );
								int32 jidx = pModelSkeleton->GetJointIDByName(pJointName);
								if (jidx>0)
								{
									LookIK_Pos.m_nJointIdx = jidx;
									LookIK_Pos.m_strJointName = pModelSkeleton->GetJointNameByID(jidx);
								}
								//	assert(jidx>0);
								pModelSkeleton->m_LookIK_Pos.push_back( LookIK_Pos );
							}
						}
					}

					//----------------------------------------------------------------------------------------

					if (stricmp(strTag,"DirectionalBlends")==0) 
					{
						uint32 num = node->getChildCount();
						pModelSkeleton->m_LookDirBlends.reserve(num);
						for (uint32 i=0; i<num; i++) 
						{
							DirectionalBlends DirBlend;
							XmlNodeRef nodePos = node->getChild(i);
							const char* DirTag = nodePos->getTag();
							if (stricmp(DirTag,"Joint")==0) 
							{
								const char* pAnimToken = nodePos->getAttr( "AnimToken" );
								DirBlend.m_AnimToken = pAnimToken;
								DirBlend.m_AnimTokenCRC32 = CCrc32::ComputeLowercase(pAnimToken);

								const char* pJointName = nodePos->getAttr( "ParameterJoint" );
								int jidx1 = pModelSkeleton->GetJointIDByName(pJointName);
								if (jidx1>0)
								{
									DirBlend.m_nParaJointIdx = jidx1;
									DirBlend.m_strParaJointName = pModelSkeleton->GetJointNameByID(jidx1);
								}
								else
								{
									RCLogError("Error while parsing chrparams file '%s': The'ParameterJoint' parameter at line '%d' refers to joint '%s', but the model doesn't seem to have a joint with that name! Lookposes for this character might not work as expected.", pNameXML, nodePos->getLine(), pJointName);
								}

								const char* pStartJointName = nodePos->getAttr( "StartJoint" );
								int jidx2 = pModelSkeleton->GetJointIDByName(pStartJointName);
								if (jidx2>0)
								{
									DirBlend.m_nStartJointIdx	= jidx2;
									DirBlend.m_strStartJointName = pModelSkeleton->GetJointNameByID(jidx2);
								}
								else
								{
									RCLogError("Error while parsing chrparams file '%s': The'StartJoint' parameter at line '%d' refers to joint '%s', but the model doesn't seem to have a joint with that name! Lookposes for this character might not work as expected.", pNameXML, nodePos->getLine(), pStartJointName);
								}

								pModelSkeleton->m_LookDirBlends.push_back( DirBlend );
							}
						}
					}
				}
			}



			//-----------------------------------------------------------
			//check AimIK-Setup  
			//-----------------------------------------------------------
			if (!stricmp(IKTypeTAG,"AimIK_Definition"))
			{
				uint32 numChilds = IKNode->getChildCount();
				for (uint32 c=0; c<numChilds; c++) 
				{
					XmlNodeRef node = IKNode->getChild(c);
					const char* strTag = node->getTag();

					//----------------------------------------------------------------------------
					if (stricmp(strTag,"RotationList")==0) 
					{
						uint32 num = node->getChildCount();
						pModelSkeleton->m_AimIK_Rot.reserve(num);
						for (uint32 i=0; i<num; i++) 
						{
							SJointsAimIK_Rot AimIK_Rot;
							XmlNodeRef nodeRot = node->getChild(i);
							const char* RotTag = nodeRot->getTag();
							if (stricmp(RotTag,"Rotation")==0) 
							{
								const char* pJointName = nodeRot->getAttr( "JointName" );
								int32 jidx = pModelSkeleton->GetJointIDByName(pJointName);
								if (jidx>0)
								{
									AimIK_Rot.m_nJointIdx = jidx;
									AimIK_Rot.m_strJointName = pModelSkeleton->GetJointNameByID(jidx);
								}
								//		assert(jidx>0);
								nodeRot->getAttr( "Primary",    AimIK_Rot.m_nPreEvaluate );
								nodeRot->getAttr( "Additive",   AimIK_Rot.m_nAdditive );
								//	AimIK_Rot.m_nAdditive=0;
								pModelSkeleton->m_AimIK_Rot.push_back( AimIK_Rot );
							}
						}
					}

					//----------------------------------------------------------------------------------------

					if (stricmp(strTag,"PositionList")==0) 
					{
						uint32 num = node->getChildCount();
						pModelSkeleton->m_AimIK_Pos.reserve(num);
						for (uint32 i=0; i<num; i++) 
						{
							SJointsAimIK_Pos AimIK_Pos;
							XmlNodeRef nodePos = node->getChild(i);
							const char* PosTag = nodePos->getTag();
							if (stricmp(PosTag,"Position")==0) 
							{
								const char* pJointName = nodePos->getAttr( "JointName" );
								int32 jidx = pModelSkeleton->GetJointIDByName(pJointName);
								if (jidx>0)
								{
									AimIK_Pos.m_nJointIdx = jidx;
									AimIK_Pos.m_strJointName = pModelSkeleton->GetJointNameByID(jidx);
								}
								//	assert(jidx>0);
								pModelSkeleton->m_AimIK_Pos.push_back( AimIK_Pos );
							}
						}
					}

					//----------------------------------------------------------------------------------------

					if (stricmp(strTag,"DirectionalBlends")==0) 
					{
						uint32 num = node->getChildCount();
						pModelSkeleton->m_AimDirBlends.reserve(num);
						for (uint32 i=0; i<num; i++) 
						{
							DirectionalBlends DirBlend;
							XmlNodeRef nodePos = node->getChild(i);
							const char* DirTag = nodePos->getTag();
							if (stricmp(DirTag,"Joint")==0) 
							{
								const char* pAnimToken = nodePos->getAttr( "AnimToken" );
								DirBlend.m_AnimToken = pAnimToken;
								DirBlend.m_AnimTokenCRC32 = CCrc32::ComputeLowercase(pAnimToken);

								const char* pJointName = nodePos->getAttr( "ParameterJoint" );
								int jidx1 = pModelSkeleton->GetJointIDByName(pJointName);
								if (jidx1>0)
								{
									DirBlend.m_nParaJointIdx = jidx1;
									DirBlend.m_strParaJointName = pModelSkeleton->GetJointNameByID(jidx1);
								}
								else
								{
									RCLogError("Error while parsing chrparams file '%s': The'ParameterJoint' parameter at line '%d' refers to joint '%s', but the model doesn't seem to have a joint with that name! Aimposes for this character might not work as expected.", pNameXML, nodePos->getLine(), pJointName);
								}

								const char* pStartJointName = nodePos->getAttr( "StartJoint" );
								int jidx2 = pModelSkeleton->GetJointIDByName(pStartJointName);
								if (jidx2>0)
								{
									DirBlend.m_nStartJointIdx	= jidx2;
									DirBlend.m_strStartJointName = pModelSkeleton->GetJointNameByID(jidx2);
								}
								else
								{
									RCLogError("Error while parsing chrparams file '%s': The'StartJoint' parameter at line '%d' refers to joint '%s', but the model doesn't seem to have a joint with that name! Aimposes for this character might not work as expected.", pNameXML, nodePos->getLine(), pStartJointName);
								}

								pModelSkeleton->m_AimDirBlends.push_back( DirBlend );
							}
						}
					}
				}
			}
			//-----------------------------------------------------------------------


		}
	}

	return 1;
}


//////////////////////////////////////////////////////////////////////////
SkeletonLoader::SkeletonLoader()
	: m_bLoaded(false)
{
}


//////////////////////////////////////////////////////////////////////////
CSkeletonInfo* SkeletonLoader::Load(const char* filename, IPakSystem* pakSystem, ICryXML* xml, const string& tempPath)
{
	std::auto_ptr<TempFilePakExtraction> fileProxyCHR(new TempFilePakExtraction( filename, tempPath.c_str(), pakSystem ));
	m_tempFileName = fileProxyCHR->GetTempName();

	const bool bChr = StringHelpers::EndsWithIgnoreCase(filename, "chr");

	string filenameIK = filename;
	filenameIK.replace(".chr",".chrparams");
	std::auto_ptr<TempFilePakExtraction> fileProxyIK(new TempFilePakExtraction( filenameIK.c_str(), tempPath.c_str(), pakSystem ));;

	if (bChr && m_skeletonInfo.LoadFromChr(m_tempFileName.c_str()))
	{
		LoadIKSetup(fileProxyIK->GetTempName().c_str(), &m_skeletonInfo.m_SkinningInfo, xml);
		m_bLoaded = true;
		return &m_skeletonInfo;
	}
	else if (!bChr && m_skeletonInfo.LoadFromCga(m_tempFileName.c_str()))
	{
		m_bLoaded = true;
		return &m_skeletonInfo;
	}
	else
	{
		m_bLoaded = false;
		return 0;
	}
}

#pragma once

namespace LODGenerator 
{
	class CAutoGeneratorHelper
	{
	public:
		CAutoGeneratorHelper();
		~CAutoGeneratorHelper();
	
	public:
		static bool CheckoutOrExtract(const char * filename);
		static bool CheckoutInSourceControl(const string& filename);
		static CMaterial * CAutoGeneratorHelper::CreateMaterialFromTemplate(const string& matName);
		static void SetLODMaterialId(IStatObj * pStatObj, int matId);
		static string GetDefaultTextureName(const int i, const int nLod, const bool bAlpha, const string& exportPath, const string& fileNameTemplate);
		static bool RasteriseTriangle(Vec2 *tri, byte *buffer, const int width, const int height);

	private:
		static string m_changeId;
	};
}
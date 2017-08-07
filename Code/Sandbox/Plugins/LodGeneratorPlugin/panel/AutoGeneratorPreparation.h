#pragma once

namespace LODGenerator 
{
	class CAutoGeneratorPreparation
	{
	public:
		CAutoGeneratorPreparation();
		~CAutoGeneratorPreparation();

	public:
		bool Tick(float* fProgress);
		bool Generate();

	private:
		bool m_bAwaitingResults;
	};
}
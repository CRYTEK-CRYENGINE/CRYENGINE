#include <random>
std::mt19937 g_rnd;
float cry_random(float start,float end) { return std::uniform_real<float>(start,end)(g_rnd); }
uint cry_random_uint32() { return std::uniform_int<uint>(0,~0)(g_rnd); }
uint cry_srand(uint seed) { g_rnd.seed(seed); return 0; }

//SEnv env,*gEnv=&env;

int g_iLastProfilerId=0; 
ProfilerData g_pd[MAX_PHYS_THREADS+1];
extern "C" CRYPHYSICS_API ProfilerData *GetProfileData(int iThread) { return &g_pd[iThread]; } 
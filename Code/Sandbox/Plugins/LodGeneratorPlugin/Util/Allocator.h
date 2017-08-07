#pragma once

namespace LODGenerator
{
	template <class T> class CAllocator 
	{
	public:
		static inline T* allocate(int number) 
		{
			return new T[number];
		}

		static inline void deallocate(T*& addr) 
		{
			delete[] addr;
			addr = NULL;                    
		}

		static inline void reallocate(T*& addr, int old_number, int new_number) 
		{
			T* new_addr = new T[new_number];
			for(int i=0; i<old_number; i++) 
			{
				new_addr[i] = addr[i];
			}
			delete[] addr;
			addr = new_addr;
		}
	};
}

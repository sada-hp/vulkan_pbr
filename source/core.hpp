#ifdef GRAPI
	#define GRAPI __declspec(dllexport)
#else
	#define GRAPI __declspec(dllimport)
#endif
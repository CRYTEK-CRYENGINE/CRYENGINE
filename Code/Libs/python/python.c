/* Minimal main program -- everything is loaded from the library */

#include "Python.h"
#include "Windows.h"

int main(int argc, char **argv)
{
	CHAR filename[FILENAME_MAX];
	
	int filnameLength = GetModuleFileName(NULL, filename, FILENAME_MAX);
	if (!filnameLength)
		return -1;
	else
	{
		PathRemoveFileSpecA(filename);
		strcat(filename, "\\..\\..\\Editor\\Python\\windows");
		Py_SetPythonHome(filename);
		return Py_Main(argc, argv);
	}
}

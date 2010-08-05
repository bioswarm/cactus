#include "cactusGlobalsPrivate.h"

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
//Functions shared by the test code.
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////

const char *testCommon_getTemporaryNetDisk() {
	system("rm -rf temporaryNetDisk");
	static char cA[] = "temporaryNetDisk";
	return cA;
}

void testCommon_deleteTemporaryNetDisk() {
	int32_t i = system("rm -rf temporaryNetDisk");
	exitOnFailure(i, "Tried to delete the temporary flower disk\n");
}

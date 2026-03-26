#ifdef USE_NANOPB_EXTENSION_NAME
#include "tests/alltypes/alltypes.nanopb.h"
#else
#include "tests/alltypes/alltypes.pb.h"
#endif

int main(int argc, char* argv[]) {
	IntSizes intSizes;
	if (sizeof(intSizes.req_int8) == 1) {
		return 0;
	} else {
		return 1;
	}
}

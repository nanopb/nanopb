#include "tests/alltypes/alltypes.pb.h"

int main(int argc, char* argv[]) {
	IntSizes intSizes;
	if (sizeof(intSizes.req_int8) == 1) {
		return 0;
	} else {
		return 1;
	}
}

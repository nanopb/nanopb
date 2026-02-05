#ifndef FRAMING_TESTDATA_H
#define FRAMING_TESTDATA_H

#include "person.pb.h"

// 192 and 219 varints contain the special SLIP characters for testing.
Person framing_testdata_msg1 = {"Test Person 99", 192, true, "test@person.com",
        5, {{"555-12345678", true, Person_PhoneType_MOBILE},
            {"123456789012345678901234567890", false, Person_PhoneType_HOME},
            {"123456789012345678901234567890", false, Person_PhoneType_HOME},
            {"123456789012345678901234567890", false, Person_PhoneType_HOME},
            {"1234-5678", true, Person_PhoneType_WORK},
        }
};

Person framing_testdata_msg2 = {"T", 0, false, "", 0, {}};

Person framing_testdata_msg3 = {"Test2", 219, true, "test2@person.com",
    1, {{"1", true, Person_PhoneType_WORK}},
};

#endif

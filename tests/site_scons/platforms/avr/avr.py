# Compiler settings for running the tests on a simulated atmega1284.

def set_avr_platform(env):
    native = env.Clone()
    native.Append(LIBS = ["simavr", "libelf"], CFLAGS = "-Wall -Werror -g")
    runner = native.Program("build/run_test", "site_scons/platforms/avr/run_test.c")
    
    env.Replace(EMBEDDED = "AVR")
    env.Replace(CC  = "avr-gcc",
                CXX = "avr-g++")
    env.Replace(TEST_RUNNER = "build/run_test")
    env.Append(CFLAGS = "-mmcu=atmega1284 -Dmain=app_main -Os")
    env.Append(CXXFLAGS = "-mmcu=atmega1284 -Dmain=app_main -Os -Wno-type-limits")
    env.Append(CPPDEFINES = {'PB_CONVERT_DOUBLE_FLOAT': 1, 'UNITTESTS_SHORT_MSGS': 1,
                             '__ASSERT_USE_STDERR': 1, 'MAX_REALLOC_SIZE': '4096',
                             'FUZZTEST_BUFSIZE': 2048})
    env.Append(LINKFLAGS = "-mmcu=atmega1284")
    env.Append(LINKFLAGS = "build/avr_io.o -Wl,-Map,avr.map")
    avr_io = env.Object("build/avr_io.o", "site_scons/platforms/avr/avr_io.c")
    
    # These fake defines just ensure that the needed platform files get built.
    env.Depends("build/common/pb_common.o", runner)
    env.Depends("build/common/pb_common.o", avr_io)
    

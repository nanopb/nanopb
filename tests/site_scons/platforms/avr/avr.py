# Compiler settings for running the tests on a simulated atmega1284.

def set_avr_platform(env):
    native = env.Clone()
    native.Append(LIBS = ["simavr", "libelf"], CFLAGS = "-Wall -Werror -g")
    runner = native.Program("build/run_test", "site_scons/platforms/avr/run_test.c")
    
    env.Replace(EMBEDDED = "1")
    env.Replace(CC  = "avr-gcc",
                CXX = "avr-g++",
                LD  = "avr-gcc")
    env.Replace(TEST_RUNNER = "build/run_test")
    env.Append(CFLAGS = "-mmcu=atmega1284 -Dmain=app_main")
    env.Append(CXXFLAGS = "-mmcu=atmega1284 -Dmain=app_main")
    env.Append(LINKFLAGS = "-mmcu=atmega1284")
    env.Append(LINKFLAGS = "build/avr_io.o")
    avr_io = env.Object("build/avr_io.o", "site_scons/platforms/avr/avr_io.c")
    env.Depends("build/common/pb_common.o", runner)
    env.Depends("build/common/pb_common.o", avr_io)
    

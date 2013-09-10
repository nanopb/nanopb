import subprocess
import sys

def add_nanopb_builders(env):
    '''Add the necessary builder commands for nanopb tests.'''
    
    # Build command for building .pb from .proto using protoc
    def proto_actions(source, target, env, for_signature):
        dirs = ' '.join(['-I' + env.GetBuildPath(d) for d in env['PROTOCPATH']])
        return '$PROTOC $PROTOCFLAGS %s -o%s %s' % (dirs, target[0], source[0])

    proto_file_builder = Builder(generator = proto_actions,
                                 suffix = '.pb',
                                 src_suffix = '.proto')
    env.Append(BUILDERS = {'Proto': proto_file_builder})
    env.SetDefault(PROTOC = 'protoc')

    # Build command for running nanopb generator
    import os.path
    def nanopb_targets(target, source, env):
        basename = os.path.splitext(str(source[0]))[0]
        target.append(basename + '.pb.h')
        return target, source

    nanopb_file_builder = Builder(action = '$NANOPB_GENERATOR $NANOPB_FLAGS $SOURCE',
                                  suffix = '.pb.c',
                                  src_suffix = '.pb',
                                  emitter = nanopb_targets)
    env.Append(BUILDERS = {'Nanopb': nanopb_file_builder})
    env.SetDefault(NANOPB_GENERATOR = 'python ' + env.GetBuildPath("#../generator/nanopb_generator.py"))
    env.SetDefault(NANOPB_FLAGS = '-q')

    # Combined method to run both protoc and nanopb generator
    def run_protoc_and_nanopb(env, source):
        b1 = env.Proto(source)
        b2 = env.Nanopb(source)
        return b1 + b2
    env.AddMethod(run_protoc_and_nanopb, "NanopbProto")

    # Build command that runs a test program and saves the output
    def run_test(target, source, env):
        if len(source) > 1:
            infile = open(str(source[1]))
        else:
            infile = None
        
        pipe = subprocess.Popen(str(source[0]),
                                stdin = infile,
                                stdout = open(str(target[0]), 'w'),
                                stderr = sys.stderr)
        result = pipe.wait()
        if result == 0:
            print '\033[92m[ OK ]\033[0m   Ran ' + str(source[0])
        else:
            print '\033[91m[FAIL]\033[0m   Program ' + str(source[0]) + ' returned ' + str(result)
        return result
        
    run_test_builder = Builder(action = run_test,
                               suffix = '.output')
    env.Append(BUILDERS = {'RunTest': run_test_builder})

    # Build command that decodes a message using protoc
    def decode_actions(source, target, env, for_signature):
        dirs = ' '.join(['-I' + env.GetBuildPath(d) for d in env['PROTOCPATH']])
        return '$PROTOC $PROTOCFLAGS %s --decode=%s %s <%s >%s' % (dirs, env['MESSAGE'], source[1], source[0], target[0])

    decode_builder = Builder(generator = decode_actions,
                             suffix = '.decoded')
    env.Append(BUILDERS = {'Decode': decode_builder})    

    # Build command that asserts that two files be equal
    def compare_files(target, source, env):
        data1 = open(str(source[0]), 'rb').read()
        data2 = open(str(source[1]), 'rb').read()
        if data1 == data2:
            print '\033[92m[ OK ]\033[0m   Files equal: ' + str(source[0]) + ' and ' + str(source[1])
            return 0
        else:
            print '\033[91m[FAIL]\033[0m   Files differ: ' + str(source[0]) + ' and ' + str(source[1])
            return 1

    compare_builder = Builder(action = compare_files,
                              suffix = '.equal')
    env.Append(BUILDERS = {'Compare': compare_builder})



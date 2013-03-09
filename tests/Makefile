CFLAGS=-ansi -Wall -Werror -I .. -g -O0
DEPS=../pb_decode.h ../pb_encode.h ../pb.h person.pb.h \
	callbacks2.pb.h callbacks.pb.h unittests.h unittestproto.pb.h \
	alltypes.pb.h missing_fields.pb.h
TESTS=  decode_unittests encode_unittests \
	test_decode1 test_decode2 test_decode3 test_decode3_buf \
	test_encode1 test_encode2 test_encode3 test_encode3_buf \
	test_decode_callbacks test_encode_callbacks \
	test_missing_fields test_no_messages test_funny_name \
	test_multiple_files test_cxxcompile test_options \
	bc_encode bc_decode

# More strict checks for the core part of nanopb
CC_VERSION=$(shell $(CC) -v 2>&1)
CFLAGS_CORE=
ifneq (,$(findstring gcc,$(CC_VERSION)))
	CFLAGS_CORE=-pedantic -Wextra -Wcast-qual -Wlogical-op -Wconversion
	CFLAGS+=--coverage -fstack-protector-all
	LDFLAGS+=--coverage
endif
ifneq (,$(findstring clang,$(CC_VERSION)))
	CFLAGS_CORE=-pedantic -Wextra -Wcast-qual -Wconversion
endif

# Also use mudflap if it is available
# To enable, run with    make -B USE_MUDFLAP=y
USE_MUDFLAP ?= n
ifeq ($(USE_MUDFLAP),y)
	CFLAGS += -fmudflap
	LDFLAGS += -lmudflap -fmudflap
endif

all: breakpoints $(TESTS) run_unittests

clean:
	rm -f $(TESTS) person.pb* alltypes.pb* *.o *.gcda *.gcno *.pb.h *.pb.c

%.pb.o: %.pb.c %.pb.h
	$(CC) $(CFLAGS) $(CFLAGS_CORE) -c -o $@ $<

%.o: %.c
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

pb_encode.o: ../pb_encode.c $(DEPS)
	$(CC) $(CFLAGS) $(CFLAGS_CORE) -c -o $@ $<
pb_decode.o: ../pb_decode.c $(DEPS)
	$(CC) $(CFLAGS) $(CFLAGS_CORE) -c -o $@ $<

# Test for compilability with c++ compiler

pb_encode.cxx.o: ../pb_encode.c $(DEPS)
	$(CXX) $(CFLAGS) $(CFLAGS_CORE) -c -o $@ $<
pb_decode.cxx.o: ../pb_decode.c $(DEPS)
	$(CXX) $(CFLAGS) $(CFLAGS_CORE) -c -o $@ $<

# Test for PB_BUF_ONLY compilation option

pb_encode.buf.o: ../pb_encode.c $(DEPS)
	$(CC) -DPB_BUFFER_ONLY $(CFLAGS) $(CFLAGS_CORE) -c -o $@ $<
pb_decode.buf.o: ../pb_decode.c $(DEPS)
	$(CC) -DPB_BUFFER_ONLY $(CFLAGS) $(CFLAGS_CORE) -c -o $@ $<
%.buf.o: %.c $(DEPS)
	$(CC) -DPB_BUFFER_ONLY $(CFLAGS) -c -o $@ $<	
test_encode3_buf: test_encode3.buf.o pb_encode.buf.o alltypes.pb.o
	$(CC) $(LDFLAGS) $^ -o $@
test_decode3_buf: test_decode3.buf.o pb_decode.buf.o alltypes.pb.o
	$(CC) $(LDFLAGS) $^ -o $@

test_cxxcompile: pb_encode.cxx.o pb_decode.cxx.o
test_decode1: test_decode1.o pb_decode.o person.pb.o
test_decode2: test_decode2.o pb_decode.o person.pb.o
test_decode3: test_decode3.o pb_decode.o alltypes.pb.o
test_encode1: test_encode1.o pb_encode.o person.pb.o
test_encode2: test_encode2.o pb_encode.o person.pb.o
test_encode3: test_encode3.o pb_encode.o alltypes.pb.o
test_multiple_files: test_multiple_files.o pb_encode.o callbacks2.pb.o callbacks.pb.o
test_decode_callbacks: test_decode_callbacks.o pb_decode.o callbacks.pb.o
test_encode_callbacks: test_encode_callbacks.o pb_encode.o callbacks.pb.o
test_missing_fields: test_missing_fields.o pb_encode.o pb_decode.o missing_fields.pb.o
decode_unittests: decode_unittests.o pb_decode.o unittestproto.pb.o
encode_unittests: encode_unittests.o pb_encode.o unittestproto.pb.o
test_no_messages: no_messages.pb.h no_messages.pb.c no_messages.pb.o
test_funny_name: funny-proto+name.pb.h funny-proto+name.pb.o
bc_encode: bc_alltypes.pb.o pb_encode.o bc_encode.o
bc_decode: bc_alltypes.pb.o pb_decode.o bc_decode.o

%.pb: %.proto
	protoc -I. -I../generator -I/usr/include -o$@ $<

%.pb.c %.pb.h: %.pb ../generator/nanopb_generator.py
	python ../generator/nanopb_generator.py $<

breakpoints: ../*.c *.c
	grep -n 'return false;' $^ | cut -d: -f-2 | xargs -n 1 echo b > $@

coverage: run_unittests
	gcov pb_encode.gcda
	gcov pb_decode.gcda

run_unittests: $(TESTS)
	rm -f *.gcda
	
	./decode_unittests > /dev/null
	./encode_unittests > /dev/null
	
	[ "`./test_encode1 | ./test_decode1`" = \
	"`./test_encode1 | protoc --decode=Person -I. -I../generator -I/usr/include person.proto`" ]

	[ "`./test_encode2 | ./test_decode1`" = \
	"`./test_encode2 | protoc --decode=Person -I. -I../generator -I/usr/include person.proto`" ]

	[ "`./test_encode2 | ./test_decode2`" = \
	"`./test_encode2 | protoc --decode=Person -I. -I../generator -I/usr/include person.proto`" ]
	
	[ "`./test_decode2 < person_with_extra_field.pb`" = \
	"`./test_encode2 | ./test_decode2`" ]
	
	[ "`./test_encode_callbacks | ./test_decode_callbacks`" = \
	"`./test_encode_callbacks | protoc --decode=TestMessage callbacks.proto`" ]

	./test_encode3 | ./test_decode3
	./test_encode3 1 | ./test_decode3 1
	./test_encode3 1 | protoc --decode=AllTypes -I. -I../generator -I/usr/include alltypes.proto >/dev/null
	./test_encode3_buf 1 | ./test_decode3_buf 1
	./bc_encode | ./bc_decode
	
	./test_missing_fields

test_options: options.pb.h options.expected options.pb.o
	cat options.expected | while read -r p; do \
	    if ! grep -q "$$p" $<; then \
	        echo Expected: "$$p"; \
	        exit 1; \
	    fi \
	done

run_fuzztest: test_decode3
	bash -c 'ulimit -c unlimited; I=1; while true; do cat /dev/urandom | ./test_decode3 > /dev/null; I=$$(($$I+1)); echo -en "\r$$I"; done'

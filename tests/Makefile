all:
	scons

clean:
	scons -c

coverage:
	rm -rf build

	# LCOV does not like the newer gcov format
	scons

	# Collect the data
	mkdir build/coverage
	lcov --base-directory . --directory build/ -c -o build/coverage/nanopb.info --ignore-errors inconsistent

	# Remove the test code from results
	lcov -r build/coverage/nanopb.info '*tests*' -o build/coverage/nanopb.info

	# Generate HTML
	genhtml -o build/coverage build/coverage/nanopb.info

	echo "Coverage output available in build/coverage"

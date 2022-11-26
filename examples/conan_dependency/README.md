# About
This example shows how to use Conan to pull in the header files and static libraries
for `nanopb` and incorporate them into a very simple CMake application.

## How To Run

### Before using this example
The `conanfile.py` here imports `0.4.6` for `nanopb` and uses the packaged artifacts
to build a simple application.  You'll likely need to build this yourself, so
checkout the tagged version and run `conan create .` in the base of this repository

### Running line by line
To run though the build one step at a time, use the following commands.
```sh
mkdir build
cd build
conan install ..
conan source ..
conan build ..
conan package ..
```
The `conanfile.py` has been commented to explain the workflow

### Installing to cache
To have everything build at once and install to your local Conan cache
```sh
conan create .
```

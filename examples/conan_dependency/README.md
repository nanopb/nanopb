# About
This example shows how to use Conan to pull in the source and header files for `nanopb`
for when you're needing to compile from source

## How To Run
To run though the build 1 step at a time, use the following commands.
```sh
mkdir build
cd build
conan install ..
conan source ..
conan build ..
conan package ..
```

To have everything build at once and install to your local Conan cache
```sh
conan create .
```

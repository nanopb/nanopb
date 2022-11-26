from conans import ConanFile, CMake

class SimpleProtosConan(ConanFile):
    name = "simple_protos"
    version = "1.0.0"
    description = "An example of importing nanopb as a conan artifact"
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake"
    exports = "*"

    def requirements(self):
        self.requires("nanopb/0.4.6")

    def imports(self):
        # Includes the nanopb headers
        self.copy("*.h")
        # Includes the compiled nanopb libraries
        self.copy("*", src="lib", dst="lib")
        # Includes the protoc plugin
        self.copy("*", src="bin", dst="bin")
        # Includes the python libraries that `bin` reaches out to
        self.copy("*", src="local", dst="local")

    def source(self):
        # To include the packages from nanopb, we need to get their path in cache
        nanopb_package_root = self.deps_cpp_info["nanopb"].rootpath
        python_path=f"PYTHONPATH={nanopb_package_root}/local/lib/python3.10/dist-packages"
        plugin=f"--plugin={nanopb_package_root}/bin/protoc-gen-nanopb" 
        # These next values grab this environments source and proto directories
        output=f"--nanopb_out={self.source_folder}/src"
        proto_flags=f"-I {self.source_folder}/protos simple.proto"

        self.run(f"{python_path} protoc {plugin} {output} {proto_flags}")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        self.copy("simple", dst="bin", src="bin")


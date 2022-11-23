from conan import ConanFile

class SimpleProtosConan(ConanFile):
    name = "simple_protos"
    version = "1.0.0"
    description = "An example of importing nanopb as a conan artifact"
    exports = "*"

    def requirements(self):
        self.requires("nanopb/0.4.7-dev")

    def imports(self):
        self.copy("*.h")
        self.copy("*.c")
        self.copy("*", src="bin", dst="bin")
        self.copy("*", src="local", dst="local")

    def build(self):
        python_path="PYTHONPATH=local/lib/python3.10/dist-packages"
        plugin="--plugin=bin/protoc-gen-nanopb" 
        output="--nanopb_out=src"
        proto_flags="-I protos simple.proto"

        self.run(f"{python_path} protoc {plugin} {output} {proto_flags}")
        self.run("make")

    def package(self):
        self.copy("simple", dst="bin", src="bin")


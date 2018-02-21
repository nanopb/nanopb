from conans import ConanFile

class NanoPbConan(ConanFile):
    name = "nanopb"
    version = "0.3.9"
    license = "zlib"
    url = "https://jpa.kapsi.fi/nanopb/"
    description = "Protocol Buffers with small code size"
    settings = "os", "compiler", "build_type", "arch"
    exports = '*.h', '*.c'

    def package(self):
        self.copy("src/*.h", dst="inc", keep_path=False)
        self.copy("src/*.c", dst="src", keep_path=False)

    def package_id(self):
        self.info.header_only()

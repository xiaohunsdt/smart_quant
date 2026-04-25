from conan import ConanFile
from conan.tools.cmake import cmake_layout


class SmartQuant(ConanFile):
    name = "smart-quant"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("quickfix/1.15.1")
        self.requires("spdlog/1.15.1")
        self.requires("catch2/3.8.1")

    def layout(self):
        cmake_layout(self)

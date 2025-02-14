from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.system.package_manager import Apt, PacMan, Dnf, Brew
from conan.tools.build import check_min_cppstd
import os

class RopesConanFile(ConanFile):

    # Explicitly allow the modification of these settings
    settings = "os", "compiler", "build_type", "arch"


    # Phantom package: it doesn't install anything, it just checks 
    # if the system is openGL ready (skip this check with `-o check_opengl_compatibility=no`)
    opengl_sanity_check = "opengl/system"


    # Dependencies: the packages that will be installed via conan. If you want, you can 
    # use your system packages for sdl and opengl instead of depending on conan: 
    # (use `-o sdl_source=system` or `-o opengl_source=system`)
    conan_packages = {
        "fmt"        : "fmt/10.2.1",
        "mp-units"   : "mp-units/2.2.1",
        "scope-lite" : "scope-lite/0.2.0",
        "structopt"  : "structopt/0.1.3",
        "imgui"      : "imgui/1.90.5",
        "implot"     : "implot/0.16",
        "glfw"       : "glfw/3.3.8",
        "sdl"        : "sdl/2.28.3",
        "sdl_ttf"    : "sdl_ttf/2.22.0",
    }

    options = {
        "check_opengl_compatibility": ["yes", "no"],
        "opengl_source": ["conan", "system"],
        "sdl_source": ["conan", "system"],
    }

    default_options = {
        "check_opengl_compatibility": "yes",
        "opengl_source": "conan",
        "sdl_source": "conan",

        "mp-units/*:std_format": False,
    }


    # Change the required packages based on the user's options
    def configure(self):
        if self.options.check_opengl_compatibility == "yes":
            self.conan_packages["opengl"] = self.opengl_sanity_check
        if self.options.sdl_source == "system":
            self.conan_packages.pop("sdl")
            self.conan_packages.pop("sdl_ttf")
        if self.options.opengl_source == "system":
            self.conan_packages.pop("glfw")


    # Actually add conan packages as project dependencies
    # after they got filtered based on the user's options
    def requirements(self):
        for name, ref in self.conan_packages.items():
            self.requires(ref)


    # Set up the build environment to use CMake generators
    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        
        deps = CMakeDeps(self)
        deps.generate()


    # Set up the build environment to use CMake layouts
    def layout(self):
        cmake_layout(self)

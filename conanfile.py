#!/usr/bin/python
#
# Copyright 2024 Khalil Estell
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from conan import ConanFile
import os


required_conan_version = ">=2.0.14"


class libhal_arm_cortex_conan(ConanFile):
    name = "libhal-armcortex"
    license = "Apache-2.0"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://libhal.github.io/libhal-armcortex"
    description = ("A collection of drivers and libraries for the Cortex M "
                   "series ARM processors using libhal")
    topics = ("arm", "cortex", "cortex-m", "cortex-m0", "cortex-m0plus",
              "cortex-m1", "cortex-m3", "cortex-m4", "cortex-m4f", "cortex-m7",
              "cortex-m23", "cortex-m55", "cortex-m35p", "cortex-m33")
    settings = "compiler", "build_type", "os", "arch"

    python_requires = "libhal-bootstrap/[^1.0.0]"
    python_requires_extend = "libhal-bootstrap.library"

    options = {
        "use_libhal_exceptions": [True, False],
        "use_picolibc": [True, False],
    }

    default_options = {
        "use_libhal_exceptions": True,
        "use_picolibc": True,
    }

    def requirements(self):
        bootstrap = self.python_requires["libhal-bootstrap"]
        bootstrap.module.add_library_requirements(self)

        if self.settings.os == "baremetal" and self.settings.compiler == "gcc":
            if self.options.use_libhal_exceptions:
                self.requires(
                    "libhal-exceptions/[^1.0.0]", transitive_headers=True)
            if self.options.use_picolibc:
                compiler_version = str(self.settings.compiler.version)
                self.requires("prebuilt-picolibc/" + compiler_version)

    def package_info(self):
        self.cpp_info.libs = ["libhal-armcortex"]
        self.cpp_info.set_property("cmake_target_name", "libhal::armcortex")
        self.cpp_info.exelinkflags = []

        if (
            self.settings.os == "baremetal" and
            self.settings.compiler == "gcc" and
            "cortex-" in str(self.settings.arch)
        ):
            linker_path = os.path.join(self.package_folder, "linker_scripts")
            self.cpp_info.exelinkflags.append("-L" + linker_path)

    def package_id(self):
        del self.info.options.use_picolibc
        del self.info.options.use_libhal_exceptions

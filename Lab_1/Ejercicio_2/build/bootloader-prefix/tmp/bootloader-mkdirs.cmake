# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/diegogiordano/esp/idf/esp-idf/components/bootloader/subproject")
  file(MAKE_DIRECTORY "/Users/diegogiordano/esp/idf/esp-idf/components/bootloader/subproject")
endif()
file(MAKE_DIRECTORY
  "/Users/diegogiordano/dev/universidad/embedded_systems/rep_se_202501_Jorge_Gomez/Lab_1/Ejercicio_2/build/bootloader"
  "/Users/diegogiordano/dev/universidad/embedded_systems/rep_se_202501_Jorge_Gomez/Lab_1/Ejercicio_2/build/bootloader-prefix"
  "/Users/diegogiordano/dev/universidad/embedded_systems/rep_se_202501_Jorge_Gomez/Lab_1/Ejercicio_2/build/bootloader-prefix/tmp"
  "/Users/diegogiordano/dev/universidad/embedded_systems/rep_se_202501_Jorge_Gomez/Lab_1/Ejercicio_2/build/bootloader-prefix/src/bootloader-stamp"
  "/Users/diegogiordano/dev/universidad/embedded_systems/rep_se_202501_Jorge_Gomez/Lab_1/Ejercicio_2/build/bootloader-prefix/src"
  "/Users/diegogiordano/dev/universidad/embedded_systems/rep_se_202501_Jorge_Gomez/Lab_1/Ejercicio_2/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/diegogiordano/dev/universidad/embedded_systems/rep_se_202501_Jorge_Gomez/Lab_1/Ejercicio_2/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/diegogiordano/dev/universidad/embedded_systems/rep_se_202501_Jorge_Gomez/Lab_1/Ejercicio_2/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()

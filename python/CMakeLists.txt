cmake_minimum_required(VERSION 3.20)
project(NRForgeNGAPPython VERSION 0.1.0 LANGUAGES CXX)
if(NOT NRFORGE_PYTHON_SDK_PREFIX)
  message(FATAL_ERROR "Provide NRFORGE_PYTHON_SDK_PREFIX: a sealed installed C++ SDK prefix")
endif()
find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 3.0.1 EXACT CONFIG REQUIRED)
find_package(NRForgeNGAP 0.1.0 EXACT CONFIG REQUIRED PATHS "${NRFORGE_PYTHON_SDK_PREFIX}/lib/cmake/NRForgeNGAP" NO_DEFAULT_PATH)
set(generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
execute_process(COMMAND "${Python_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/ngap_sdk/generate_bindings.py"
  --sdk-prefix "${NRFORGE_PYTHON_SDK_PREFIX}" --expected-fingerprint "${NRForgeNGAP_FINGERPRINT}" --output "${generated}" RESULT_VARIABLE generated_status)
if(NOT generated_status EQUAL 0)
  message(FATAL_ERROR "Public SDK shape is unsupported; refusing partial Python bindings")
endif()
include("${generated}/sources.cmake")
pybind11_add_module(_native MODULE NO_EXTRAS ngap_sdk/module.cpp ${NRFORGE_PYTHON_SOURCES})
target_include_directories(_native PRIVATE ngap_sdk "${generated}")
target_link_libraries(_native PRIVATE NRForge::ngap)
target_compile_features(_native PRIVATE cxx_std_20)
set_target_properties(_native PROPERTIES CXX_EXTENSIONS OFF)
if(UNIX AND NOT APPLE)
  target_link_options(_native PRIVATE -Wl,--exclude-libs,ALL)
endif()
target_compile_options(_native PRIVATE -O0 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion)
install(CODE "execute_process(COMMAND \"${Python_EXECUTABLE}\" \"${CMAKE_CURRENT_SOURCE_DIR}/ngap_sdk/generate_bindings.py\"
  --sdk-prefix \"${NRFORGE_PYTHON_SDK_PREFIX}\" --expected-fingerprint \"${NRForgeNGAP_FINGERPRINT}\" --verify-only RESULT_VARIABLE verified)
  if(NOT verified EQUAL 0)
    message(FATAL_ERROR \"SDK public headers changed before Python wheel installation\")
  endif()")

add_custom_command(TARGET _native POST_BUILD
  COMMAND "${Python_EXECUTABLE}" -c "import importlib.util,sys; s=importlib.util.spec_from_file_location('_native',sys.argv[1]); m=importlib.util.module_from_spec(s); s.loader.exec_module(m); assert len(m.messages()) == 131; print('PASS native extension import and complete registry')" "$<TARGET_FILE:_native>"
  VERBATIM)

install(TARGETS _native LIBRARY DESTINATION nrforge_ngap)
install(FILES "${generated}/conversion-schema.json" DESTINATION nrforge_ngap)

add_custom_target(nrforge_python_sdk_verify ALL
  COMMAND "${Python_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/ngap_sdk/generate_bindings.py"
    --sdk-prefix "${NRFORGE_PYTHON_SDK_PREFIX}" --expected-fingerprint "${NRForgeNGAP_FINGERPRINT}" --verify-only
  VERBATIM)
add_dependencies(_native nrforge_python_sdk_verify)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

function(nrforge_verify_sdk_lock)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdk-lock.cmake")
    message(FATAL_ERROR "SDK is not sealed. Run tools/ngap-dispatch/seal_sdk.py immediately after generation")
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}"
    "-DNRFORGE_SOURCE_ROOT=${NRFORGE_SOURCE_ROOT}"
    "-DNRFORGE_GENERATED_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
    -P "${NRFORGE_SOURCE_ROOT}/cmake/VerifyNgapSdk.cmake"
    RESULT_VARIABLE checked)
  if(NOT checked EQUAL 0)
    message(FATAL_ERROR "SDK source/generated fingerprint verification failed")
  endif()
endfunction()

function(nrforge_package_sdk target)
  set(protocol ngap)
  if(ARGC GREATER 1)
    set(protocol "${ARGV1}")
  endif()
  if(NOT protocol MATCHES "^(ngap|f1ap)$")
    message(FATAL_ERROR "Unsupported SDK protocol")
  endif()
  string(TOUPPER "${protocol}" upper_protocol)
  set(package "NRForge${upper_protocol}")
  include("${CMAKE_CURRENT_SOURCE_DIR}/sdk-lock.cmake")
  add_library(NRForge::${protocol} ALIAS ${target})
  set_target_properties(${target} PROPERTIES EXPORT_NAME ${protocol} POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
  target_compile_features(${target} PUBLIC cxx_std_20)
  target_include_directories(${target} PUBLIC
    "$<BUILD_INTERFACE:${NRFORGE_SOURCE_ROOT}/libngap>"
    "$<BUILD_INTERFACE:${NRFORGE_SOURCE_ROOT}/libaper>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nrforge/${protocol}>")
  add_custom_target(${target}_sdk_verify ALL COMMAND "${CMAKE_COMMAND}"
    "-DNRFORGE_SOURCE_ROOT=${NRFORGE_SOURCE_ROOT}"
    "-DNRFORGE_GENERATED_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
    -P "${NRFORGE_SOURCE_ROOT}/cmake/VerifyNgapSdk.cmake" VERBATIM)
  add_dependencies(${target} ${target}_sdk_verify)
  # Static archivers can accept incomplete or empty object members. A real
  # executable must resolve every registration and validate the registry before
  # this archive earns an installable build receipt.
  add_executable(${target}_sdk_link_check "${CMAKE_CURRENT_SOURCE_DIR}/sdk_link_check.cpp")
  set_target_properties(${target}_sdk_link_check PROPERTIES CXX_EXTENSIONS OFF)
  target_link_libraries(${target}_sdk_link_check PRIVATE ${target})
  add_custom_command(TARGET ${target}_sdk_link_check POST_BUILD
    COMMAND "$<TARGET_FILE:${target}_sdk_link_check>"
    COMMAND "${CMAKE_COMMAND}"
    "-DNRFORGE_SOURCE_ROOT=${NRFORGE_SOURCE_ROOT}"
    "-DNRFORGE_GENERATED_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DNRFORGE_SDK_ARCHIVE=$<TARGET_FILE:${target}>"
    "-DNRFORGE_SDK_RECEIPT=${CMAKE_CURRENT_BINARY_DIR}/sdk-build-receipt.cmake"
    -P "${NRFORGE_SOURCE_ROOT}/cmake/WriteNgapSdkReceipt.cmake" VERBATIM)
  # cmake --install does not build targets. Recheck inputs even when callers
  # install directly so modified headers cannot accompany an older archive.
  install(CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\"
    \"-DNRFORGE_SOURCE_ROOT=${NRFORGE_SOURCE_ROOT}\"
    \"-DNRFORGE_GENERATED_ROOT=${CMAKE_CURRENT_SOURCE_DIR}\"
    \"-DNRFORGE_SDK_ARCHIVE=$<TARGET_FILE:${target}>\"
    \"-DNRFORGE_SDK_RECEIPT=${CMAKE_CURRENT_BINARY_DIR}/sdk-build-receipt.cmake\"
    -P \"${NRFORGE_SOURCE_ROOT}/cmake/VerifyNgapSdk.cmake\"
    RESULT_VARIABLE sdk_install_checked)
    if(NOT sdk_install_checked EQUAL 0)
      message(FATAL_ERROR \"SDK changed after build; refusing install\")
    endif()")
  install(TARGETS ${target} EXPORT ${package}Targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/sdk-public/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/nrforge/${protocol}"
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.inc")
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/sdk-provenance.json"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/nrforge-${protocol}")
  set(config_dir "${CMAKE_INSTALL_LIBDIR}/cmake/${package}")
  configure_package_config_file("${NRFORGE_SOURCE_ROOT}/cmake/NRForgeProtocolConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${package}Config.cmake"
    INSTALL_DESTINATION "${config_dir}")
  write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/${package}ConfigVersion.cmake"
    VERSION "${NRFORGE_SDK_VERSION}" COMPATIBILITY ExactVersion)
  install(EXPORT ${package}Targets FILE ${package}Targets.cmake
    NAMESPACE NRForge:: DESTINATION "${config_dir}")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${package}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${package}ConfigVersion.cmake"
    DESTINATION "${config_dir}")
endfunction()

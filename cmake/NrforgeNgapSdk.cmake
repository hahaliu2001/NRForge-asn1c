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
  include("${CMAKE_CURRENT_SOURCE_DIR}/sdk-lock.cmake")
  add_library(NRForge::ngap ALIAS ${target})
  set_target_properties(${target} PROPERTIES EXPORT_NAME ngap POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
  target_compile_features(${target} PUBLIC cxx_std_20)
  target_include_directories(${target} PUBLIC
    "$<BUILD_INTERFACE:${NRFORGE_SOURCE_ROOT}/libngap>"
    "$<BUILD_INTERFACE:${NRFORGE_SOURCE_ROOT}/libaper>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nrforge/ngap>")
  add_custom_target(nrforge_sdk_verify ALL COMMAND "${CMAKE_COMMAND}"
    "-DNRFORGE_SOURCE_ROOT=${NRFORGE_SOURCE_ROOT}"
    "-DNRFORGE_GENERATED_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
    -P "${NRFORGE_SOURCE_ROOT}/cmake/VerifyNgapSdk.cmake" VERBATIM)
  add_dependencies(${target} nrforge_sdk_verify)
  # Static archivers can accept incomplete or empty object members. A real
  # executable must resolve every registration and validate the registry before
  # this archive earns an installable build receipt.
  add_executable(nrforge_sdk_link_check "${CMAKE_CURRENT_SOURCE_DIR}/sdk_link_check.cpp")
  set_target_properties(nrforge_sdk_link_check PROPERTIES CXX_EXTENSIONS OFF)
  target_link_libraries(nrforge_sdk_link_check PRIVATE ${target})
  add_custom_command(TARGET nrforge_sdk_link_check POST_BUILD
    COMMAND "$<TARGET_FILE:nrforge_sdk_link_check>"
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
  install(TARGETS ${target} EXPORT NRForgeNGAPTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/sdk-public/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/nrforge/ngap"
    FILES_MATCHING PATTERN "*.hpp")
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/sdk-provenance.json"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/nrforge-ngap")
  set(config_dir "${CMAKE_INSTALL_LIBDIR}/cmake/NRForgeNGAP")
  configure_package_config_file("${NRFORGE_SOURCE_ROOT}/cmake/NRForgeNGAPConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/NRForgeNGAPConfig.cmake"
    INSTALL_DESTINATION "${config_dir}")
  write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/NRForgeNGAPConfigVersion.cmake"
    VERSION "${NRFORGE_SDK_VERSION}" COMPATIBILITY ExactVersion)
  install(EXPORT NRForgeNGAPTargets FILE NRForgeNGAPTargets.cmake
    NAMESPACE NRForge:: DESTINATION "${config_dir}")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/NRForgeNGAPConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/NRForgeNGAPConfigVersion.cmake"
    DESTINATION "${config_dir}")
endfunction()

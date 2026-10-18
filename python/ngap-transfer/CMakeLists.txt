cmake_minimum_required(VERSION 3.20)
project(NRForgeNGAPTransferPython VERSION 0.2.0 LANGUAGES CXX)
set(NRFORGE_PYTHON_TRANSFERS ON CACHE BOOL "NGAP 0.2 independent roots" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/.." shared-ngap)

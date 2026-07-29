# ESP-IDF project root. Configure with idf.py from an ESP-IDF environment.
# Product face + identity + esprec capture (sibling tig/esprec).

cmake_minimum_required(VERSION 3.16)

# Host-agent screen capture component (tig/esprec) as sibling checkout.
# Layout: parent/esprec/component/esprec  next to  parent/xuss-c/firmware
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../esprec/component")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gcu_c)

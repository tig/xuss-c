# ESP-IDF project root. Configure with idf.py from an ESP-IDF environment.
# Product face + identity + esprec capture (sibling tig/esprec) + SPIFFS First.

cmake_minimum_required(VERSION 3.16)

# Host-agent screen capture component (tig/esprec) as sibling checkout.
# Layout: parent/esprec/component/esprec  next to  parent/xuss-c/firmware
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../esprec/component")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gcu_c)

# SPIFFS image is built out-of-band:
#   py -3 tools/gen_spiffs_image.py --idf-path $IDF_PATH --assets assets --out build/spiffs.bin --size 0xE70000
# Flash storage partition (0x190000) after app flash — see install/README.md.


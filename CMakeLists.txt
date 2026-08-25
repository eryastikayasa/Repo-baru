cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(xiaozhi_esp32s3)

# ESP-SR 2.4.1 ships the selected WakeNet model inside the managed component.
# Build it into the SPIFFS partition labeled "model" so esp_srmodel_init("model")
# can load /srmodel/wn9_hiesp/wn9_index and wn9_data at runtime.
set(SR_MODEL_SOURCE
    "${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__esp-sr/model/target/wn9_hiesp"
)

if(NOT EXISTS "${SR_MODEL_SOURCE}/wn9_index")
    message(FATAL_ERROR
        "ESP-SR WakeNet9 Hi ESP model files not found: ${SR_MODEL_SOURCE}/wn9_index"
    )
endif()

spiffs_create_partition_image(model "${SR_MODEL_SOURCE}" FLASH_IN_PROJECT)

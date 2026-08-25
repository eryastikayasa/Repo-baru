idf_component_register(
    SRCS
        "main.cpp"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp-sr
)

# Force the existing WakeNet lookup in main.cpp to load the model list
# from the SPIFFS "model" partition first.
target_compile_options(${COMPONENT_LIB} PRIVATE
    "-include${CMAKE_CURRENT_LIST_DIR}/wake_model_autoload.h"
)

# ESP-SR 2.4.1 contains the selected model in the managed component.
# Package only wn9_hiesp into the model partition and flash it with the app.
set(SR_MODEL_SOURCE
    "${CMAKE_CURRENT_LIST_DIR}/../managed_components/espressif__esp-sr/model/target/wn9_hiesp"
)

if(NOT EXISTS "${SR_MODEL_SOURCE}/wn9_index")
    message(FATAL_ERROR
        "ESP-SR WakeNet9 Hi ESP model files not found: ${SR_MODEL_SOURCE}/wn9_index"
    )
endif()

spiffs_create_partition_image(model "${SR_MODEL_SOURCE}" FLASH_IN_PROJECT)

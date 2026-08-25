idf_component_register(
    SRCS
        "main.cpp"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp-sr
)

# ESP-SR 2.4.5 is the pinned speech-recognition component used by this project.
# The main branch selects WakeNet9 Hi ESP through sdkconfig.defaults.
# The model partition is named "model" and is 4 MiB in partitions/partitions.csv.
# CI packages the exact upstream WakeNet9 Hi ESP model into srmodels.bin.

set(incs .)

if(COMMAND armino_component_register)
    armino_component_register(INCLUDE_DIRS "${incs}")
else()
# MQTT facade/component header only. Per-chip backend lives under chip_<platform>/.
idf_component_register(
    INCLUDE_DIRS "${incs}"
)
endif()

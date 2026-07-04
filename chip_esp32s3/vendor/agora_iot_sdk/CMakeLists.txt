idf_component_register(INCLUDE_DIRS "." "include")

target_link_libraries(${COMPONENT_LIB} INTERFACE "-L ${CMAKE_CURRENT_SOURCE_DIR}/libs")

add_prebuilt_library(rtsa "${CMAKE_CURRENT_SOURCE_DIR}/libs/libagora-rtc-sdk.a" PRIV_REQUIRES lwip pthread)
add_prebuilt_library(aosl "${CMAKE_CURRENT_SOURCE_DIR}/libs/libaosl.a" PRIV_REQUIRES lwip pthread)

target_link_libraries(${COMPONENT_LIB} INTERFACE rtsa aosl)

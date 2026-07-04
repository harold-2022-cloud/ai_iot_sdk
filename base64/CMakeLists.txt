set(incs .
)
set(srcs "")


set(CURRENT_DIR ./)

file(GLOB C_FILES 
    "${CURRENT_DIR}/*.c"
)
list(APPEND srcs ${C_FILES})

idf_component_register(
	SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	REQUIRES platform_adapter
)
set(DYNAMIC_CMDS_LIST DynTest)

FILE(REMOVE ${CMAKE_SOURCE_DIR}/modules.load)
foreach(lib ${DYNAMIC_CMDS_LIST})
	add_library(cmd_${lib} SHARED cmd_${lib}.cpp)
	target_link_libraries(cmd_${lib} TgBot)
	FILE(APPEND ${CMAKE_SOURCE_DIR}/modules.load libcmd_${lib}\n)
endforeach()

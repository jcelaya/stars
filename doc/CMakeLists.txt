# Create HTML documented code
add_custom_command(OUTPUT HTML/index.html
    COMMAND find ${PROJECT_SOURCE_DIR}/src -name \"*.cpp\" -o -name \"*.hpp\" > ${CMAKE_CURRENT_BINARY_DIR}/sources
    COMMAND find ${PROJECT_SOURCE_DIR}/include -name \"*.hpp\" >> ${CMAKE_CURRENT_BINARY_DIR}/sources
    COMMAND gtags ARGS -f ${CMAKE_CURRENT_BINARY_DIR}/sources ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND htags ARGS -n -s -d ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
add_custom_target(doc ALL DEPENDS HTML/index.html)


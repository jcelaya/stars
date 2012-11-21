# Web UI library
if (Wt_FOUND)
    include_directories(${Wt_INCLUDE_DIR})
    set (STARS_UI_LIBRARIES stars_ui${BIN_SUFFIX} PARENT_SCOPE)
    add_library(stars_ui${BIN_SUFFIX} SHARED WtUI.cpp)
endif (Wt_FOUND)

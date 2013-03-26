# Web UI library
if (Wt_FOUND)
    include_directories(${Wt_INCLUDE_DIR})
    add_library(stars_ui${BIN_SUFFIX} SHARED WtUI.cpp)
endif (Wt_FOUND)

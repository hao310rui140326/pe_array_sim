###############################################################################
# Build file defining build targets for RTL PE Array include files
#
#  Exposed targets: 
#   dla_pe_array    Builds the dla pe array include target
#
#  Options:  
###############################################################################

set (TARGET_NAME "dla_pe_array")

add_library(${TARGET_NAME} INTERFACE)
target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/inc/)
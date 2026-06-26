file(GLOB_RECURSE SOURCES_C *.c)
file(GLOB_RECURSE SOURCES_CPP *.cpp)
#Add sources from ui directory
file(GLOB_RECURSE SRC_UI ${CMAKE_SOURCE_DIR} "ui/*.c")

idf_component_register(
    SRCS ${SOURCES_C} ${SOURCES_CPP}  ${SRC_UI}
    INCLUDE_DIRS "."  "ui")
    

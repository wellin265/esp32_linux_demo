file(GLOB_RECURSE SOURCES_C *.c EasyTracer/*.c)
file(GLOB_RECURSE SOURCES_CPP *.cpp)

idf_component_register(
    SRCS ${SOURCES_C} ${SOURCES_CPP} 
    INCLUDE_DIRS "." "EasyTracer")

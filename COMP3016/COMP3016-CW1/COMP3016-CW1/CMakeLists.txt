find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)

add_executable (COMP3016-CW1 "COMP3016-CW1.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET COMP3016-CW1 PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(COMP3016-CW1 PRIVATE SDL3::SDL3 SDL3_image::SDL3_image)
cmake_minimum_required(VERSION 3.22)

project(COMP3016-CW1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Find SDL3 + SDL3_image from vcpkg ----
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)

# ---- Your game executable ----
add_executable(COMP3016-CW1
    COMP3016-CW1.cpp
)

# Link imported targets from vcpkg
target_link_libraries(COMP3016-CW1 PRIVATE SDL3::SDL3 SDL3_image::SDL3_image)

# ---- Use static MSVC runtime (/MT) so no VC++ redistributable needed ----
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---- Output folder for exe ----
set_target_properties(COMP3016-CW1 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release-bin/$<CONFIG>"
)

# --------------------------------------------------------------
#  Copy assets (.png, data/, assets/) next to the exe each build
# --------------------------------------------------------------

# 1) Copy data/ if it exists
if (EXISTS "${CMAKE_SOURCE_DIR}/data")
  add_custom_command(TARGET COMP3016-CW1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/data"
            "$<TARGET_FILE_DIR:COMP3016-CW1>/data"
  )
endif()

# 2) Copy assets/ if it exists
if (EXISTS "${CMAKE_SOURCE_DIR}/assets")
  add_custom_command(TARGET COMP3016-CW1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:COMP3016-CW1>/assets"
  )
endif()

# 3) Copy any loose PNG files from the project root
file(GLOB ROOT_PNGS "${CMAKE_SOURCE_DIR}/*.png")
if (ROOT_PNGS)
  add_custom_command(TARGET COMP3016-CW1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${ROOT_PNGS} "$<TARGET_FILE_DIR:COMP3016-CW1>"
    COMMAND_EXPAND_LISTS
  )
endif()

# 4) Copy all dependent runtime DLLs beside the exe (CMake ≥ 3.21)
add_custom_command(TARGET COMP3016-CW1 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:COMP3016-CW1> $<TARGET_FILE_DIR:COMP3016-CW1>
  COMMAND_EXPAND_LISTS
)

# --------------------------------------------------------------
#  Optional: 'install' step for ready-to-zip portable folder
# --------------------------------------------------------------
install(TARGETS COMP3016-CW1 RUNTIME DESTINATION .)
if (EXISTS "${CMAKE_SOURCE_DIR}/data")
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/data" DESTINATION .)
endif()
if (EXISTS "${CMAKE_SOURCE_DIR}/assets")
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets" DESTINATION .)
endif()
install(FILES ${ROOT_PNGS} DESTINATION .)

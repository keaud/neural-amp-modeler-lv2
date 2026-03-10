cmake_minimum_required(VERSION 3.10)

project(NeuralAmpModelerLv2 VERSION 0.1.9)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(LIB_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/lib CACHE PATH "The library install dir (default: lib)")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	include_directories(SYSTEM /usr/local/include)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	link_libraries( "$<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:-lstdc++fs>" )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
else()
	message(FATAL_ERROR "Unrecognized Platform!")
endif()

set(NAM_LV2_ID http://github.com/mikeoliphant/neural-amp-modeler-lv2)

add_subdirectory(src)


# create neural_amp_modeler.lv2
add_custom_target(copy_binaries ALL
	${CMAKE_COMMAND} -E copy "$<TARGET_FILE:neural_amp_modeler>" neural_amp_modeler.lv2/
	DEPENDS neural_amp_modeler
)

file(COPY resources/modgui.ttl DESTINATION neural_amp_modeler.lv2)
file(COPY resources/modgui DESTINATION neural_amp_modeler.lv2)

configure_file(resources/manifest.ttl.in neural_amp_modeler.lv2/manifest.ttl)
configure_file(resources/neural_amp_modeler.ttl.in neural_amp_modeler.lv2/neural_amp_modeler.ttl)

# Install to Signal Factory Devices directory
set(DEVICE_ROOT_DIR "/opt/signal/Factory/Devices/NAM")
set(LV2_BUNDLE_DIR "${DEVICE_ROOT_DIR}/contents/lv2/neural_amp_modeler.lv2")

# Install LV2 bundle
install(DIRECTORY ${CMAKE_BINARY_DIR}/neural_amp_modeler.lv2/
    DESTINATION ${LV2_BUNDLE_DIR}
)

# Install device manifest
install(FILES manifest.json
    DESTINATION ${DEVICE_ROOT_DIR}
)

# Install presets
install(DIRECTORY presets/
    DESTINATION ${DEVICE_ROOT_DIR}/presets
    FILES_MATCHING PATTERN "*.json"
)

# Install UI
install(DIRECTORY ui/
    DESTINATION ${DEVICE_ROOT_DIR}/ui
    FILES_MATCHING PATTERN "*.vue" PATTERN "*.jpg" PATTERN "*.png"
)

# Tests
enable_testing()
add_executable(test_pick_processor tests/test_pick_processor.cpp)
target_compile_features(test_pick_processor PRIVATE cxx_std_17)
add_test(NAME PickProcessorTests COMMAND test_pick_processor)

message(STATUS "NAM will install to: ${DEVICE_ROOT_DIR}")

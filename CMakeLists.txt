cmake_minimum_required(VERSION 3.5.0)
if (NOT DEFINED GUI_BASE_DIR)
	if (DEFINED ENV{GUI_BASE_DIR})
		set(GUI_BASE_DIR $ENV{GUI_BASE_DIR})
	else()
		set(GUI_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../plugin-GUI)
	endif()
endif()

get_filename_component(PROJECT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
get_filename_component(PLUGIN_NAME ${PROJECT_FOLDER} NAME)

set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architecture for Mac OS X" FORCE)


project(OE_PLUGIN_${PLUGIN_NAME})
set(CMAKE_SHARED_LIBRARY_PREFIX "")

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
	OEPLUGIN
	"$<$<PLATFORM_ID:Windows>:JUCE_API=__declspec(dllimport)>"
	$<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
	$<$<PLATFORM_ID:Linux>:JUCE_DISABLE_NATIVE_FILECHOOSERS=1>
	$<$<CONFIG:Debug>:DEBUG=1>
	$<$<CONFIG:Debug>:_DEBUG=1>
	$<$<CONFIG:Release>:NDEBUG=1>
	)


set(SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Source)
set(TESTS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Tests)

file(GLOB_RECURSE SRC_FILES LIST_DIRECTORIES false "${SOURCE_PATH}/*.cpp" "${SOURCE_PATH}/*.h")
file(GLOB_RECURSE TESTS_FILES LIST_DIRECTORIES false "${TESTS_PATH}/*.cpp")

set(GUI_COMMONLIB_DIR ${GUI_BASE_DIR}/installed_libs)

set(CONFIGURATION_FOLDER $<$<CONFIG:Debug>:Debug>$<$<NOT:$<CONFIG:Debug>>:Release>)

list(APPEND CMAKE_PREFIX_PATH ${GUI_COMMONLIB_DIR} ${GUI_COMMONLIB_DIR}/${CONFIGURATION_FOLDER})

if (APPLE)
	add_library(${PLUGIN_NAME} MODULE ${SRC_FILES})
else()
	add_library(${PLUGIN_NAME} SHARED ${SRC_FILES})
endif()

add_dependencies(${PLUGIN_NAME} open-ephys)


add_library(${PLUGIN_NAME}_testable SHARED ${SRC_FILES})


target_compile_features(${PLUGIN_NAME} PUBLIC cxx_auto_type cxx_generalized_initializers cxx_std_17)
target_include_directories(${PLUGIN_NAME} PUBLIC ${GUI_BASE_DIR}/JuceLibraryCode ${GUI_BASE_DIR}/JuceLibraryCode/modules ${GUI_BASE_DIR}/Plugins/Headers ${GUI_COMMONLIB_DIR}/include)

target_compile_features(${PLUGIN_NAME}_testable PUBLIC cxx_auto_type cxx_generalized_initializers)
target_include_directories(${PLUGIN_NAME}_testable PUBLIC ${GUI_BASE_DIR}/JuceLibraryCode ${GUI_BASE_DIR}/JuceLibraryCode/modules ${GUI_BASE_DIR}/Plugins/Headers ${GUI_COMMONLIB_DIR}/include)

add_executable(
		${PLUGIN_NAME}_tests
		${TESTS_FILES}
)

target_compile_features(${PLUGIN_NAME}_tests PRIVATE cxx_std_17)


set(GUI_BIN_DIR ${CMAKE_BINARY_DIR}/plugin-GUI/${CONFIGURATION_FOLDER})
set(GUI_TEST_HELPERS_DIR ${GUI_BASE_DIR}/TestHelpers)

if (NOT CMAKE_LIBRARY_ARCHITECTURE)
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(CMAKE_LIBRARY_ARCHITECTURE "x64")
	else()
		set(CMAKE_LIBRARY_ARCHITECTURE "x86")
	endif()
endif()

#Libraries and compiler options
if(MSVC)
	target_link_libraries(${PLUGIN_NAME} ${GUI_BIN_DIR}/open-ephys.lib)
	target_compile_options(${PLUGIN_NAME} PRIVATE /sdl- /W0)
	set(INSTALL_PATH  ${GUI_BIN_DIR}/plugins)	
	install(TARGETS ${PLUGIN_NAME} RUNTIME DESTINATION ${GUI_BIN_DIR}/plugins  CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES})

	set(CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../libs)
elseif(LINUX)
	target_link_libraries(${PLUGIN_NAME} GL X11 Xext Xinerama asound dl freetype pthread rt)
	set_property(TARGET ${PLUGIN_NAME} APPEND_STRING PROPERTY LINK_FLAGS
		"-fvisibility=hidden -fPIC -rdynamic -Wl,-rpath,'$$ORIGIN/../shared'")
	target_compile_options(${PLUGIN_NAME} PRIVATE -fPIC -rdynamic)
	target_compile_options(${PLUGIN_NAME} PRIVATE -O3) #enable optimization for linux debug
	
	set(INSTALL_PATH  ${GUI_BIN_DIR}/plugins)	
	install(TARGETS ${PLUGIN_NAME} LIBRARY DESTINATION ${GUI_BIN_DIR}/plugins)
elseif(APPLE)
	set_target_properties(${PLUGIN_NAME} PROPERTIES BUNDLE TRUE)
	set_property(TARGET ${PLUGIN_NAME} APPEND_STRING PROPERTY LINK_FLAGS
	"-undefined dynamic_lookup -rpath @loader_path/../../../../shared")

    set(INSTALL_PATH  $ENV{HOME}/Library/Application\ Support/open-ephys/plugins-api8)
	install(TARGETS ${PLUGIN_NAME} DESTINATION $ENV{HOME}/Library/Application\ Support/open-ephys/plugins-api8)
	set(CMAKE_PREFIX_PATH /opt/local)
endif()


if(APPLE)
       add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${INSTALL_PATH}/$<TARGET_BUNDLE_DIR_NAME:${PLUGIN_NAME}>) 
       add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_BUNDLE_DIR:${PLUGIN_NAME}> ${INSTALL_PATH}/$<TARGET_BUNDLE_DIR_NAME:${PLUGIN_NAME}>)
elseif(MSVC OR LINUX)
       add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PLUGIN_NAME}> ${INSTALL_PATH})
endif()

if(MSVC)
	add_custom_command(TARGET ${PLUGIN_NAME}_tests POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gui_testable_source> $<TARGET_FILE_DIR:${PLUGIN_NAME}_tests>)
	add_custom_command(TARGET ${PLUGIN_NAME}_tests POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:test_helpers> $<TARGET_FILE_DIR:${PLUGIN_NAME}_tests>)

endif()

add_dependencies(${PLUGIN_NAME}_testable gui_testable_source test_helpers)
add_dependencies(${PLUGIN_NAME}_tests ${PLUGIN_NAME}_testable)

#for testing
find_package(Boost REQUIRED COMPONENTS headers)

target_compile_definitions(${PLUGIN_NAME}_testable PRIVATE -DBUILD_TESTS)
target_link_libraries(${PLUGIN_NAME}_testable PRIVATE gui_testable_source)
target_compile_definitions(${PLUGIN_NAME}_tests PRIVATE -DBUILD_TESTS -DTEST_RUNNER)
target_link_libraries(${PLUGIN_NAME}_tests PRIVATE ${PLUGIN_NAME}_testable gtest_main test_helpers Boost::headers PUBLIC gui_testable_source)
target_include_directories(${PLUGIN_NAME}_tests PRIVATE ${GUI_TEST_HELPERS_DIR}/include ${GUI_BASE_DIR}/Source)
add_test(NAME ${PLUGIN_NAME}_tests  COMMAND ${PLUGIN_NAME}_tests)

#create filters for vs and xcode

foreach( src_file IN ITEMS ${SRC_FILES})
	get_filename_component(src_path "${src_file}" PATH)
	file(RELATIVE_PATH src_path_rel "${SOURCE_PATH}" "${src_path}")
	string(REPLACE "/" "\\" group_name "${src_path_rel}")
	source_group("${group_name}" FILES "${src_file}")
endforeach()

#additional libraries, if needed
#find_package(LIBNAME)
#or
#find_library(LIBNAME_LIBRARIES NAMES libname)
#find_path(LIBNAME_INCLUDE_DIRS includefile.h)
#
#target_link_libraries(${PLUGIN_NAME} ${LIBNAME_LIBRARIES})
#target_include_directories(${PLUGIN_NAME} PRIVATE ${LIBNAME_INCLUDE_DIRS})

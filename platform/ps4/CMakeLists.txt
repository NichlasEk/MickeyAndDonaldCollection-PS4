set(PS4_STACK "" CACHE PATH "Built OpenGNM stack: vulkan-ps4, opengnm, opengnm-psbc, Vulkan-Headers")
foreach(archive vulkan-ps4/libvulkan_ps4.a opengnm/libopengnm.a opengnm-psbc/libpsbc.orbis.a)
  if(NOT EXISTS "${PS4_STACK}/${archive}")
    message(FATAL_ERROR "Missing ${PS4_STACK}/${archive}; set PS4_STACK to a built native stack")
  endif()
endforeach()
find_program(GLSLANG glslangValidator REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders.h
 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/shaders.py ${GLSLANG} ${CMAKE_CURRENT_BINARY_DIR}
 DEPENDS shaders.py frame.vert frame.frag VERBATIM)
add_executable(MickeyDonaldPS4 main.cpp launcher.cpp state_store.cpp video.cpp audio.cpp presentation.cpp ${CMAKE_CURRENT_BINARY_DIR}/shaders.h)
target_include_directories(MickeyDonaldPS4 PRIVATE ${CMAKE_CURRENT_BINARY_DIR} "${PS4_STACK}/Vulkan-Headers/include" "${PS4_STACK}/opengnm/include")
target_link_options(MickeyDonaldPS4 PRIVATE "-Wl,--wrap=sceGnmVideoOutSubmitFlipAndWait")
target_link_libraries(MickeyDonaldPS4 PRIVATE mdc_session
 "-Wl,--start-group" "${PS4_STACK}/vulkan-ps4/libvulkan_ps4.a"
 "${PS4_STACK}/opengnm/libopengnm.a" "${PS4_STACK}/opengnm-psbc/libpsbc.orbis.a" "-Wl,--end-group"
 SceGnmDriver SceVideoOut ScePad SceUserService SceAudioOut)
set_target_properties(MickeyDonaldPS4 PROPERTIES SUFFIX ".elf")

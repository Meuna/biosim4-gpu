# cmake/EmbedKernels.cmake — kernel source embedding helpers
#
# Provides biosim_embed_opencl_source(), which reads a .cl or .h file at
# build time and generates a .c translation unit containing the file content
# as a NUL-terminated C string literal.
#
# Usage:
#   include(EmbedKernels)
#   biosim_embed_opencl_source(my_target path/to/source.cl my_var_name)
#
# This adds an auto-generated embedded_<var_name>.c to my_target's sources.
# The generated file defines:
#   const char my_var_name[] = "...escaped source...";
#
# Declare the symbol in C with:
#   extern const char my_var_name[];

function(biosim_embed_opencl_source target input_file var_name)
    get_filename_component(abs_input "${input_file}" ABSOLUTE)
    set(out_c "${CMAKE_CURRENT_BINARY_DIR}/embedded_${var_name}.c")

    add_custom_command(
        OUTPUT "${out_c}"
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${abs_input}"
            "-DOUTPUT=${out_c}"
            "-DVAR=${var_name}"
            -P "${CMAKE_SOURCE_DIR}/cmake/EmbedKernelsRun.cmake"
        DEPENDS "${abs_input}"
        COMMENT "Embedding ${input_file} as ${var_name}"
        VERBATIM
    )

    target_sources("${target}" PRIVATE "${out_c}")
endfunction()

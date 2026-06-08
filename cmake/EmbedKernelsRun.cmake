# cmake/EmbedKernelsRun.cmake — invoked by add_custom_command via cmake -P
# Required variables (pass as -D on command line):
#   INPUT  — absolute path to the source file to embed
#   OUTPUT — path to the generated .c file
#   VAR    — C identifier for the string constant

if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED VAR)
    message(FATAL_ERROR "EmbedKernelsRun requires INPUT, OUTPUT, and VAR variables")
endif()

cmake_path(GET INPUT FILENAME source_name)

# Read the source line-by-line. file(STRINGS) strips newlines and handles CRLF
# and semicolons in content correctly.
file(STRINGS "${INPUT}" lines)

# Build a sequence of adjacent C string literals, each ≤ 8000 chars, so that
# MSVC's string-literal token limit is never hit. Adjacent string literals are
# concatenated by the compiler at compile time — no runtime cost, no API change.
set(chunk_limit 8000)
set(current_chunk "")
set(current_len 0)
set(literal_lines "")

foreach(raw_line IN LISTS lines)
    # Escape: backslash first, then double-quote. The newline stripped by
    # file(STRINGS) is re-added as a \n escape at the end of each line.
    string(REPLACE "\\" "\\\\" line_safe "${raw_line}")
    string(REPLACE "\"" "\\\"" line_safe "${line_safe}")
    set(line_escaped "${line_safe}\\n")

    string(LENGTH "${line_escaped}" line_len)
    math(EXPR new_len "${current_len} + ${line_len}")

    if(new_len GREATER ${chunk_limit} AND current_len GREATER 0)
        string(APPEND literal_lines "    \"${current_chunk}\"\n")
        set(current_chunk "${line_escaped}")
        string(LENGTH "${current_chunk}" current_len)
    else()
        string(APPEND current_chunk "${line_escaped}")
        set(current_len ${new_len})
    endif()
endforeach()

if(current_len GREATER 0)
    string(APPEND literal_lines "    \"${current_chunk}\"\n")
endif()

file(WRITE "${OUTPUT}"
    "/* auto-generated from ${source_name} — do not edit */\n"
    "const char ${VAR}[] =\n"
    "${literal_lines};\n"
)

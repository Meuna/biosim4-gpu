# cmake/EmbedKernelsRun.cmake — invoked by add_custom_command via cmake -P
# Required variables (pass as -D on command line):
#   INPUT  — absolute path to the source file to embed
#   OUTPUT — path to the generated .c file
#   VAR    — C identifier for the string constant

cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED VAR)
    message(FATAL_ERROR "EmbedKernelsRun requires INPUT, OUTPUT, and VAR variables")
endif()

file(READ "${INPUT}" raw)

# Escape in this order: backslash first, then double-quote, then newlines
string(REPLACE "\\" "\\\\" safe "${raw}")
string(REPLACE "\"" "\\\"" safe "${safe}")
string(REPLACE "\n" "\\n" safe "${safe}")

cmake_path(GET INPUT FILENAME source_name)

file(WRITE "${OUTPUT}"
    "/* auto-generated from ${source_name} — do not edit */\n"
    "const char ${VAR}[] = \"${safe}\";\n"
)

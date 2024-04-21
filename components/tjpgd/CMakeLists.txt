idf_component_register(SRCS "tjpgd.c"
                       INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Werror -Wextra -Wsign-compare -Wunreachable-code)

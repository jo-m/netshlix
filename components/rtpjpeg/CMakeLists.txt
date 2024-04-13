idf_component_register(SRCS "rtp.c" "rtp_jpeg.c" "rfc2435.c"
                       INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Werror -Wextra -Wshadow -Wsign-compare -Wunreachable-code)

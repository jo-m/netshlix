idf_component_register(SRCS "smpte_bars.c" "main.c" "wifi.c" "dns.c" "rtp_udp.c" "jpeg.c"
                       INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Werror -Wextra -Wshadow -Wsign-compare -Wunreachable-code -fstack-usage)

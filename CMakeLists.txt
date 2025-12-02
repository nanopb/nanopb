file(GLOB NANOPB_SRCS "pb_*.c")

idf_component_register(
        SRCS ${NANOPB_SRCS}
        INCLUDE_DIRS "."
)

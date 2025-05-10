# Search for all source files in the current directory
# and save the names to the DIR_Fonts_SRCS variable
aux_source_directory(. DIR_Fonts_SRCS)

# Generate link library
add_library(Fonts ${DIR_Fonts_SRCS})
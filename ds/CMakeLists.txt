# =============================================================================
# DataScript Code Generator (ds)
# =============================================================================

add_executable(ds
    main.cc
    logger.cc
    compiler_options.cc
    compiler.cc
)

target_link_libraries(ds PRIVATE datascript)

# Apply strict warnings to the CLI tool
target_set_strict_warnings(ds)

# Set target properties
set_target_properties(ds PROPERTIES
    FOLDER "Tools"
)

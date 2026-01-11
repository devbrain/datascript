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
neutrino_target_warnings(ds)

# Set target properties
set_target_properties(ds PROPERTIES
    FOLDER "Tools"
)

# Put ds in a separate directory when not a top-level project
neutrino_is_top_level(_datascript_is_top_level)
if(NOT _datascript_is_top_level)
    set_target_properties(ds PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tools"
    )
endif()

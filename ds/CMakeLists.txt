add_executable(ds
    main.cc
    logger.cc
    compiler_options.cc
    compiler.cc
)
target_link_libraries(ds PRIVATE datascript)


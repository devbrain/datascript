//
// Created by igor on 19/11/2025.
//
// Main entry point for doctest test runner.
// All test cases are defined in separate test_*.cc files.
//

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>

// Note: Test auto-registration works fine with static libraries.
// Each test_*.cc file that's compiled into this executable will
// have its TEST_CASE macros automatically registered at static
// initialization time.
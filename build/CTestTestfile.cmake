# CMake generated Testfile for 
# Source directory: /Users/hiro/Projetct/GitHub/agens
# Build directory: /Users/hiro/Projetct/GitHub/agens/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(unit "/Users/hiro/Projetct/GitHub/agens/build/unit_tests")
set_tests_properties(unit PROPERTIES  _BACKTRACE_TRIPLES "/Users/hiro/Projetct/GitHub/agens/CMakeLists.txt;54;add_test;/Users/hiro/Projetct/GitHub/agens/CMakeLists.txt;0;")
add_test(cli_help "/Users/hiro/Projetct/GitHub/agens/build/agens" "--help")
set_tests_properties(cli_help PROPERTIES  _BACKTRACE_TRIPLES "/Users/hiro/Projetct/GitHub/agens/CMakeLists.txt;55;add_test;/Users/hiro/Projetct/GitHub/agens/CMakeLists.txt;0;")
add_test(integration "bash" "/Users/hiro/Projetct/GitHub/agens/tests/integration.sh" "/Users/hiro/Projetct/GitHub/agens/build/agens")
set_tests_properties(integration PROPERTIES  _BACKTRACE_TRIPLES "/Users/hiro/Projetct/GitHub/agens/CMakeLists.txt;56;add_test;/Users/hiro/Projetct/GitHub/agens/CMakeLists.txt;0;")

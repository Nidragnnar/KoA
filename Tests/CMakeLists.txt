include(GoogleTest)

add_executable(EngineTests EngineTests/TestesEngine.cpp)
target_link_libraries(EngineTests PRIVATE KingdomEngine GTest::gtest_main)
configurar_alvo(EngineTests)
gtest_discover_tests(EngineTests DISCOVERY_MODE PRE_TEST)

add_executable(GameTests GameTests/TestesJogo.cpp GameTests/TestesSave.cpp GameTests/TestesVida.cpp GameTests/TestesReino.cpp GameTests/TestesCombate.cpp GameTests/TestesEras.cpp GameTests/TestesFinal.cpp GameTests/TestesCronicas.cpp GameTests/TestesEstacoes.cpp GameTests/TestesRegioes.cpp GameTests/TestesConselho.cpp GameTests/TestesDefesa.cpp GameTests/TestesGuilda.cpp GameTests/TestesCotidiano.cpp GameTests/TestesCastelo.cpp GameTests/TestesFundacoes.cpp ${CMAKE_SOURCE_DIR}/Tools/Balanceamento/JogadorAutomatico.cpp Pilha.cpp)
target_link_libraries(GameTests PRIVATE KingdomGame GTest::gtest_main dbghelp)
target_compile_definitions(GameTests PRIVATE PASTA_DATA="${CMAKE_SOURCE_DIR}/Data" PASTA_FIXTURES="${CMAKE_SOURCE_DIR}/Tests/fixtures")
configurar_alvo(GameTests)
gtest_discover_tests(GameTests DISCOVERY_MODE PRE_TEST)

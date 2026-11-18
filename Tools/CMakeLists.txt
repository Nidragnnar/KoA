# Gerador da pixel art provisória (era das cinzas): grava Assets/Sprites/atlas.png + atlas.json
add_executable(AssetGen AssetGen/main.cpp AssetGen/Desenho.cpp AssetGen/Sprites.cpp AssetGen/Eras.cpp)
target_link_libraries(AssetGen PRIVATE stb nlohmann_json::nlohmann_json)
configurar_alvo(AssetGen)

# Simulação de balanceamento (joga N dias com a lógica real e mede o ritmo das eras)
add_executable(Balanceamento Balanceamento/main.cpp Balanceamento/JogadorAutomatico.cpp)
target_link_libraries(Balanceamento PRIVATE KingdomGame)
configurar_alvo(Balanceamento)

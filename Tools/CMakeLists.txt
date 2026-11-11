# Gerador da pixel art provisória (era das cinzas): grava Assets/Sprites/atlas.png + atlas.json
add_executable(AssetGen AssetGen/main.cpp AssetGen/Desenho.cpp AssetGen/Sprites.cpp)
target_link_libraries(AssetGen PRIVATE stb nlohmann_json::nlohmann_json)
configurar_alvo(AssetGen)

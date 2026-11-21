# Regras do jogo (sem SDL nem OpenGL): testadas sem janela
add_library(KingdomGame STATIC
    Inventory/Inventory.cpp
    Data/GameData.cpp
    Kingdom/Kingdom.cpp
    Kingdom/Clock.cpp
    Kingdom/Estacoes.cpp
    Kingdom/Conselho.cpp
    Kingdom/Defesa.cpp
    Kingdom/Guilda.cpp
    Kingdom/Cotidiano.cpp Kingdom/Castelo.cpp Kingdom/Povo.cpp Kingdom/Clima.cpp Kingdom/Faccoes.cpp Kingdom/Mercado.cpp Kingdom/Linhagem.cpp Kingdom/MundoVivo.cpp
    NPC/Npc.cpp
    Economy/Economia.cpp
    Combat/Combate.cpp
    Buildings/Building.cpp
    Player/Player.cpp
    World/World.cpp
    World/WorldCombate.cpp
    World/WorldMoradores.cpp
    World/WorldSave.cpp
    World/WorldCronicas.cpp
    World/WorldRegioes.cpp
    World/WorldConselho.cpp
    World/WorldDefesa.cpp
    World/WorldGuilda.cpp
    World/WorldCotidiano.cpp
    World/WorldCastelo.cpp
    World/WorldPovo.cpp
    World/WorldFaccoes.cpp
    World/WorldMercado.cpp
    World/WorldLinhagem.cpp
    World/WorldMundo.cpp
    World/Regioes.cpp
    Save/SaveSystem.cpp
    Lore/Lore.cpp)
target_link_libraries(KingdomGame PUBLIC KingdomEngineCore)
configurar_alvo(KingdomGame)

# O jogo em si
add_executable(KingdomOfAshes WIN32
    Main.cpp
    App/Contexto.cpp
    Scenes/TitleScene.cpp
    Scenes/WorldScene.cpp
    Render/WorldRenderer.cpp
    UI/Brasao.cpp
    UI/Hud.cpp
    UI/Menus.cpp
    Demo/Roteiro.cpp
    icone.rc)
target_link_libraries(KingdomOfAshes PRIVATE KingdomGame KingdomEngine)
configurar_alvo(KingdomOfAshes)
add_custom_command(TARGET KingdomOfAshes POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:KingdomOfAshes>)

# Parte pura da engine (sem SDL nem OpenGL): testada sem janela
add_library(KingdomEngineCore STATIC
    Core/Logger.cpp
    Core/Time.cpp
    Platform/FileSystem.cpp
    World/TileMap.cpp
    World/Pathfinding.cpp
    Physics/Collision.cpp
    Renderer/Animation.cpp
    Renderer/ParticleSystem.cpp
    Audio/Sintetizador.cpp)
target_link_libraries(KingdomEngineCore PUBLIC glm::glm nlohmann_json::nlohmann_json EnTT::EnTT)
configurar_alvo(KingdomEngineCore)

# Camada de plataforma e renderização (SDL3 + OpenGL 3.3)
add_library(KingdomEngine STATIC
    Core/Application.cpp
    Audio/AudioDevice.cpp
    Core/SceneManager.cpp
    Platform/Window.cpp
    Platform/Input.cpp
    Renderer/Shader.cpp
    Renderer/Texture.cpp
    Renderer/Renderer2D.cpp
    Renderer/Camera2D.cpp
    Renderer/RenderTarget.cpp
    Renderer/Lighting.cpp
    Resources/ResourceManager.cpp
    UI/BitmapFont.cpp
    Debug/DebugUI.cpp
    Debug/Screenshot.cpp)
target_link_libraries(KingdomEngine PUBLIC KingdomEngineCore SDL3::SDL3 glad stb imgui opengl32)
configurar_alvo(KingdomEngine)

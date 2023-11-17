#include "screens.h"

#include <iostream>
#include <memory>
#include <glm/glm.hpp>
#include <filesystem>
#include <stdexcept>

#include "../window/Camera.h"
#include "../window/Events.h"
#include "../window/input.h"
#include "../graphics/Shader.h"
#include "../graphics/Batch2D.h"
#include "../assets/Assets.h"
#include "../world/Level.h"
#include "../world/World.h"
#include "../objects/Player.h"
#include "../voxels/ChunksController.h"
#include "../voxels/Chunks.h"
#include "../voxels/Chunk.h"
#include "world_render.h"
#include "hud.h"
#include "gui/GUI.h"
#include "gui/panels.h"
#include "gui/controls.h"
#include "../engine.h"
#include "../files/engine_files.h"
#include "../util/stringutil.h"

using std::string;
using std::wstring;
using glm::vec3;
using glm::vec4;
using std::shared_ptr;
using std::filesystem::path;
using std::filesystem::u8path;
using std::filesystem::directory_iterator;
using namespace gui;

                                            // Replace this ugly piece of code
                                            // With some normal pages system
Panel* create_main_menu_panel(Engine* engine, shared_ptr<UINode>* newWorldPanel) {
    Panel* panel = new Panel(vec2(400, 200), vec4(5.0f), 1.0f);
    panel->color(vec4(0.0f));
	panel->setCoord(vec2(10, 10));

    {
        auto button = new Button(L"New World", vec4(12.0f, 10.0f, 12.0f, 10.0f));
        button->listenAction([engine, panel, newWorldPanel](GUI*) {
            panel->visible(false);
            (*newWorldPanel)->visible(true);
        });
        panel->add(shared_ptr<UINode>(button));
    }
    Panel* worldsPanel = new Panel(vec2(390, 200), vec4(5.0f));
    worldsPanel->color(vec4(0.1f));
    for (auto const& entry : directory_iterator(enginefs::get_worlds_folder())) {
        string name = entry.path().filename();
        Button* button = new Button(util::str2wstr_utf8(name), 
                                    vec4(10.0f, 8.0f, 10.0f, 8.0f));
        button->color(vec4(0.5f));
        button->listenAction([engine, panel, name](GUI*) {
            EngineSettings& settings = engine->getSettings();

            auto folder = enginefs::get_worlds_folder()/name;
            World* world = new World(name, folder, 42, settings);
            auto screen = new LevelScreen(engine, world->load(settings));
            engine->setScreen(shared_ptr<Screen>(screen));
        });
        worldsPanel->add(shared_ptr<UINode>(button));
    }
    panel->add(shared_ptr<UINode>(worldsPanel));
    
    {
        Button* button = new Button(L"Quit", vec4(12.0f, 10.0f, 12.0f, 10.0f));
        button->listenAction([](GUI*) {
            Window::setShouldClose(true);
        });
        panel->add(shared_ptr<UINode>(button));
    }
    return panel;
}

Panel* create_new_world_panel(Engine* engine, shared_ptr<UINode>* mainPanel) {
    Panel* panel = new Panel(vec2(400, 200), vec4(5.0f), 1.0f);
    panel->color(vec4(0.0f));
	panel->setCoord(vec2(10, 10));

    TextBox* worldNameInput;
    {
        Label* label = new Label(L"World Name");
        panel->add(shared_ptr<UINode>(label));

        TextBox* input = new TextBox(L"New World", vec4(6.0f));
        panel->add(shared_ptr<UINode>(input));
        worldNameInput = input;
    }

    TextBox* seedInput;
    {
        Label* label = new Label(L"Seed");
        panel->add(shared_ptr<UINode>(label));

        uint64_t randseed = rand() ^ (rand() << 8) ^ 
                        (rand() << 16) ^ (rand() << 24) ^
                        ((uint64_t)rand() << 32) ^ ((uint64_t)rand() << 40) ^
                        ((uint64_t)rand() << 56);

        TextBox* input = new TextBox(std::to_wstring(randseed), vec4(6.0f));
        panel->add(shared_ptr<UINode>(input));
        seedInput = input;
    }

    {
        Button* button = new Button(L"Create World", vec4(10.0f));
        button->margin(vec4(0, 20, 0, 0));
        vec4 basecolor = worldNameInput->color();   
        button->listenAction([=](GUI*) {
            wstring name = worldNameInput->text();
            string nameutf8 = util::wstr2str_utf8(name);

            // Basic validation
            if (!util::is_valid_filename(name) || 
                 enginefs::is_world_name_used(nameutf8)) {
                // blink red two times
                panel->listenInterval(0.1f, [worldNameInput, basecolor]() {
                    static bool flag = true;
                    if (flag) {
                        worldNameInput->color(vec4(0.3f, 0.0f, 0.0f, 0.5f));
                    } else {
                        worldNameInput->color(basecolor);
                    }
                    flag = !flag;
                }, 4);
                return;
            }

            wstring seedstr = seedInput->text();
            uint64_t seed;
            if (util::is_integer(seedstr)) {
                try {
                    seed = std::stoull(seedstr);
                } catch (const std::out_of_range& err) {
                    std::hash<wstring> hash;
                    seed = hash(seedstr);
                }
            } else {
                std::hash<wstring> hash;
                seed = hash(seedstr);
            }
            std::cout << "world seed: " << seed << std::endl;
            
            EngineSettings& settings = engine->getSettings();

            auto folder = enginefs::get_worlds_folder()/u8path(nameutf8);
            std::filesystem::create_directories(folder);
            World* world = new World(nameutf8, folder, seed, settings);
            auto screen = new LevelScreen(engine, world->load(settings));
            engine->setScreen(shared_ptr<Screen>(screen));
        });
        panel->add(shared_ptr<UINode>(button));
    }

    {
        Button* button = new Button(L"Back", vec4(10.0f));
        button->listenAction([panel, mainPanel](GUI*) {
            panel->visible(false);
            (*mainPanel)->visible(true);
        });
        panel->add(shared_ptr<UINode>(button));
    }

    return panel;
}

MenuScreen::MenuScreen(Engine* engine_) : Screen(engine_) {
    GUI* gui = engine->getGUI();
    panel = shared_ptr<UINode>(create_main_menu_panel(engine, &newWorldPanel));
    newWorldPanel = shared_ptr<UINode>(create_new_world_panel(engine, &panel));
    newWorldPanel->visible(false);
    gui->add(panel);
    gui->add(newWorldPanel);

    batch = new Batch2D(1024);
    uicamera = new Camera(vec3(), Window::height);
	uicamera->perspective = false;
	uicamera->flipped = true;
}

MenuScreen::~MenuScreen() {
    GUI* gui = engine->getGUI();
    gui->remove(newWorldPanel);
    gui->remove(panel);
    delete batch;
    delete uicamera;
}

void MenuScreen::update(float delta) {
}

void MenuScreen::draw(float delta) {
    panel->setCoord((Window::size() - panel->size()) / 2.0f);
    newWorldPanel->setCoord((Window::size() - newWorldPanel->size()) / 2.0f);
    
    Window::clear();
    Window::setBgColor(vec3(0.2f, 0.2f, 0.2f));

    uicamera->fov = Window::height;
	Shader* uishader = engine->getAssets()->getShader("ui");
	uishader->use();
	uishader->uniformMatrix("u_projview", uicamera->getProjView());

    batch->begin();
    batch->texture(engine->getAssets()->getTexture("menubg"));
    batch->rect(0, 0, 
                Window::width, Window::height, 0, 0, 0, 
                UVRegion(0, 0, Window::width/64, Window::height/64), 
                false, false, vec4(1.0f));
    batch->render();
}

LevelScreen::LevelScreen(Engine* engine, Level* level) 
    : Screen(engine), 
      level(level) {
    worldRenderer = new WorldRenderer(level, engine->getAssets());
    hud = new HudRenderer(engine, level);
}

LevelScreen::~LevelScreen() {
    delete hud;
    delete worldRenderer;

	std::cout << "-- writing world" << std::endl;
    World* world = level->world;
	world->write(level, !engine->getSettings().debug.generatorTestMode);

    delete level;
	delete world;
}

void LevelScreen::updateHotkeys() {
    if (Events::jpressed(keycode::O)) {
        occlusion = !occlusion;
    }
    if (Events::jpressed(keycode::F3)) {
        level->player->debug = !level->player->debug;
    }
    if (Events::jpressed(keycode::F5)) {
        for (uint i = 0; i < level->chunks->volume; i++) {
            shared_ptr<Chunk> chunk = level->chunks->chunks[i];
            if (chunk != nullptr && chunk->isReady()) {
                chunk->setModified(true);
            }
        }
    }
}

void LevelScreen::update(float delta) {
    gui::GUI* gui = engine->getGUI();
    EngineSettings& settings = engine->getSettings();

    bool inputLocked = hud->isPause() || 
                       hud->isInventoryOpen() || 
                       gui->isFocusCaught();
    if (!inputLocked) {
        updateHotkeys();
    }
    level->updatePlayer(delta, !inputLocked, hud->isPause(), !inputLocked);
    level->update();
    level->chunksController->update(settings.chunks.loadSpeed);
}

void LevelScreen::draw(float delta) {
    EngineSettings& settings = engine->getSettings();
    Camera* camera = level->player->camera;

    float fogFactor = 18.0f / (float)settings.chunks.loadDistance;
    worldRenderer->draw(camera, occlusion, fogFactor, settings.graphics.fogCurve);
    hud->draw();
    if (level->player->debug) {
        hud->drawDebug( 1 / delta, occlusion);
    }
}

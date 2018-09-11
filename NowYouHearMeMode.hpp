#pragma once

#include "Mode.hpp"

#include "MeshBuffer.hpp"
#include "GL.hpp"
#include "Scene.hpp"
#include "Sound.hpp"
#include "WalkMesh.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>

namespace NowYouHearMe
{
    // The 'Now You Hear Me Mode' is an implementation of
    // the design document fround below:
    // http://graphics.cs.cmu.edu/courses/15-466-f18/game1-designs/suannc/
    // Code is modified from the base code:
    // https://github.com/ixchow/15-466-f18-base1
    struct NowYouHearMeMode : public Mode
    {
        NowYouHearMeMode();
        virtual ~NowYouHearMeMode();

        //handle_event is called when new mouse or keyboard events are received:
        // (note that this might be many times per frame or never)
        //The function should return 'true' if it handled the event.
        virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;

        //update is called at the start of a new frame, after events are handled:
        virtual void update(float elapsed) override;

        //draw is called after update:
        virtual void draw(glm::uvec2 const &drawable_size) override;

        //starts up a 'quit/resume' pause menu:
        void show_pause_menu();

        struct {
            bool forward = false;
            bool backward = false;
            bool left = false;
            bool right = false;
        } controls;

        bool mouse_captured = false;

        Scene scene;
        Scene::Camera *camera = nullptr;

        Scene::Object *monster = nullptr;

        std::shared_ptr< Sound::PlayingSample > monster_growl;

        WalkMesh *walk_mesh = nullptr;
        WalkMesh::WalkPoint player_walk_point;
    };
};
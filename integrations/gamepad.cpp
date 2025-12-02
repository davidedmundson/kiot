// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "core.h"
#include <QCoreApplication>
#include <QSocketNotifier>
#include <QTimer>
#include <SDL2/SDL.h>

class GamepadWatcher : public QObject
{
    Q_OBJECT
public:
    explicit GamepadWatcher(QObject *parent = nullptr)
        : QObject(parent)
    {
        m_sensor = new BinarySensor(this);
        m_sensor->setId("gamepad_connected");
        m_sensor->setName("Gamepad Connected");

        if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
            qWarning() << "SDL_Init failed:" << SDL_GetError();
            m_sensor->setState(false);
            return;
        }

        // Opprett en QSocketNotifier på SDL eventfd
        int sdl_fd = SDL_GetEventState(SDL_CONTROLLERDEVICEADDED);
        (void)sdl_fd; // SDL har ikke direkte fd, vi bruker poll via timer men med minimal CPU
        startEventLoop();
        updateState();
    }

    ~GamepadWatcher()
    {
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    }

private:
    void startEventLoop()
    {
        m_timer = new QTimer(this);
        m_timer->setInterval(50); // lavt intervall bare for å hente SDL events
        connect(m_timer, &QTimer::timeout, this, [this]() {
            SDL_Event e;
            bool changed = false;
            while (SDL_PollEvent(&e)) {
                switch (e.type) {
                case SDL_CONTROLLERDEVICEADDED:
                case SDL_CONTROLLERDEVICEREMOVED:
                    changed = true;
                    break;
                default:
                    break;
                }
            }
            if (changed)
                updateState();
        });
        m_timer->start();
    }

    void updateState()
    {
        int num = SDL_NumJoysticks();
        bool connected = false;
        for (int i = 0; i < num; ++i) {
            if (SDL_IsGameController(i)) {
                connected = true;
                break;
            }
        }
        m_sensor->setState(connected);
    }

private:
    BinarySensor *m_sensor;
    QTimer *m_timer;
};

void setupGamepadWatcher()
{
    new GamepadWatcher(qApp);
}

REGISTER_INTEGRATION("Gamepad", setupGamepadWatcher, true)

#include "gamepad.moc"

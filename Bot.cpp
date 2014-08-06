#include "Common.h"
#include "ScreenArea.h"
#include "Keyboard.h"
#include "WindowFinder.h"
#include "Bot.h"
#include "ScreenGrabber.h"
#include "State.h"
#include "Util.h"
#include <thread>
#include <tchar.h>
#include <iostream>
#include <map>
#include <string>

Bot::Bot(int ship)
    : m_Finder(_T("Continuum")),
      m_Window(0),
      m_Keyboard(),
      m_LastEnemy(timeGetTime()),
      m_State(new AggressiveState(*this)),
      m_ShipNum(ship)
{}

ScreenAreaPtr& Bot::GetRadar() {
    return m_Radar;
}

ScreenAreaPtr& Bot::GetShip() {
    return m_Ship;
}

ScreenAreaPtr& Bot::GetPlayer() {
    return m_Player;
}

std::shared_ptr<ScreenGrabber> Bot::GetGrabber() {
    return m_Grabber;
}

HWND Bot::SelectWindow() {
    const WindowFinder::Matches& matches = m_Finder.GetMatches();
    std::map<int, const WindowFinder::Match*> select_map;

    int i = 1;
    for (const WindowFinder::Match& kv : matches) {
        select_map[i] = &kv;
        tcout << i++ << " " << kv.second << " (" << kv.first << ")" << std::endl;
    }

    if (i == 1)
        throw std::runtime_error("No windows found.");

    tcout << "> ";

    tstring input;
    tcin >> input;

    int selection = _tstoi(input.c_str());

    if (selection < 1 || selection >= i)
        throw std::runtime_error("Error with window selection.");

    tcout << "Running bot on window " << select_map[selection]->second << "." << std::endl;

    return select_map[selection]->first;
}

void Bot::SelectShip() {
    static bool selected;

    if (selected == true) return;

    selected = true;
    tcout << "Ship number: ";

    std::string input;
    std::cin >> input;
    
    if (input.length() != 1 || input[0] < 0x31 || input[0] > 0x38) {
        tcout << "Defaulting to ship 8." << std::endl;
        return;
    }

    m_ShipNum = input[0] - 0x30;
}

void Bot::Update() {
    m_Grabber->Update();
    m_Radar->Update();
    m_Ship->Update();
    m_Player->Update();

    if (!Util::InShip(m_Grabber)) {
        m_Keyboard.Up(VK_CONTROL);
        m_Keyboard.Send(VK_ESCAPE);
        m_Keyboard.Send(0x30 + m_ShipNum);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    m_State->Update();
}

void Bot::GrabRadar() {
    int radarstart = 0;
    int radarend = 0;
    int radary = 0;

    while (radarend == 0) {
        m_Grabber->Update();
        for (int y = static_cast<int>(m_Grabber->GetHeight() / 1.5); y < m_Grabber->GetHeight(); y++) {
            for (int x = static_cast<int>(m_Grabber->GetWidth() / 1.5); x < m_Grabber->GetWidth(); x++) {
                Pixel pix = m_Grabber->GetPixel(x, y);

                if (radarstart == 0 && pix == Colors::RadarBorder)
                    radarstart = x;

                if (radarstart != 0 && pix != Colors::RadarBorder) {
                    radarend = x - 1;
                    if (radarend - radarstart < 104) {
                        radarstart = 0;
                        radarend = 0;
                        break;
                    }
                    radarstart++;
                    radary = y + 1;
                    x = m_Grabber->GetWidth();
                    y = m_Grabber->GetHeight();
                    break;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        tcout << "Finding radar location." << std::endl;
    }

    int radarwidth = radarend - radarstart;

    tcout << "Creating radar screen with width of " << radarwidth << " at " << radarstart << ", " << radary << "." << std::endl;

    m_Radar = m_Grabber->GetArea(radarstart, radary, radarwidth, radarwidth);

    if (!m_Radar.get()) {
        tcerr << "Resolution (" << m_Grabber->GetWidth() << "x" << m_Grabber->GetHeight() << ") not supported." << std::endl;
        std::abort();
    }
}

int Bot::Run() {
    bool ready = false;

    while (!ready) {
        try {
            if (!m_Window)
                m_Window = SelectWindow();

            SelectShip();

            m_Grabber = std::shared_ptr<ScreenGrabber>(new ScreenGrabber(m_Window));
            m_Ship = m_Grabber->GetArea(m_Grabber->GetWidth() / 2 - 18, m_Grabber->GetHeight() / 2 - 18, 36, 36);

            GrabRadar();

            m_Player = m_Radar->GetArea(m_Radar->GetWidth() / 2 - 1, m_Radar->GetWidth() / 2 - 1, 4, 4);

            int width = m_Grabber->GetWidth();

            m_EnergyArea[0] = m_Grabber->GetArea(width - 78, 0, 16, 21);
            m_EnergyArea[1] = m_Grabber->GetArea(width - 62, 0, 16, 21);
            m_EnergyArea[2] = m_Grabber->GetArea(width - 46, 0, 16, 21);
            m_EnergyArea[3] = m_Grabber->GetArea(width - 30, 0, 16, 21);

            ready = true;
        } catch (const std::exception& e) {
            tcerr << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }

    while (true)
        Update();

    return 0;
}
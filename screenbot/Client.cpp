#include "Client.h"

#include "Common.h"
#include "ScreenArea.h"
#include "ScreenGrabber.h"
#include "Util.h"
#include "RotationStore.h"
#include "Level.h"

#include <iostream>
#include <iostream>
#include <thread>
#include <unordered_map>

ScreenClient::ScreenClient(HWND hwnd, Config& config)
    : m_Window(hwnd),
      m_Config(config),
      m_LastBomb(0),
      m_LastBullet(0),
      m_CurrentBulletDelay(0),
      m_ConfigLoaded(false),
      m_Rotations(nullptr),
      m_MapZoom(9),
      m_EmpEnd(0),
      m_Thrusting(false)
{ 
    m_Screen = std::make_shared<ScreenGrabber>(m_Window);
    m_Ship = m_Screen->GetArea(m_Screen->GetWidth() / 2 - 18, m_Screen->GetHeight() / 2 - 18, 36, 36);

    GrabRadar();

    m_Player = m_Radar->GetArea(m_Radar->GetWidth() / 2 - 1, m_Radar->GetWidth() / 2 - 1, 4, 4);

    int width = m_Screen->GetWidth();

    m_EnergyArea[0] = m_Screen->GetArea(width - 94, 0, 16, 21);
    m_EnergyArea[1] = m_Screen->GetArea(width - 78, 0, 16, 21);
    m_EnergyArea[2] = m_Screen->GetArea(width - 62, 0, 16, 21);
    m_EnergyArea[3] = m_Screen->GetArea(width - 46, 0, 16, 21);
    m_EnergyArea[4] = m_Screen->GetArea(width - 30, 0, 16, 21);
    m_PlayerWindow.SetScreenArea(m_Screen->GetArea(3, 3, 172, m_Screen->GetHeight() - 50));
}


void ScreenClient::Update(DWORD dt) {
    m_Screen->Update();
    m_Radar->Update();
    m_Ship->Update();
    m_Player->Update();
    m_PlayerWindow.Update(dt);

    Scan();

    if (!m_ConfigLoaded) {
        m_BombDelay = m_Config.Get<int>(_T("BombTime"));
        m_BulletDelay = m_Config.Get<int>(_T("BulletDelay")) * 10;
        m_ScaleDelay = m_Config.Get<bool>(_T("ScaleDelay"));
        m_FireBombs = m_Config.Get<bool>(_T("FireBombs"));
        m_FireGuns = m_Config.Get<bool>(_T("FireGuns"));
        m_MapZoom = m_Config.Get<int>("MapZoom");
        m_IgnoreCarriers = m_Config.Get<bool>("IgnoreCarriers");
        m_CurrentBulletDelay = m_BulletDelay;
        m_ConfigLoaded = true;
        m_Rotations = new Ships::RotationStore(m_Config);
    }
}

std::string ScreenClient::GetName() {
    return m_Config.Get<std::string>("Name");
    try {
        return m_PlayerWindow.GetPlayer(0)->GetName();
    } catch (...) {}
    return "";
}

int ScreenClient::GetFreq() {
    try {
        return m_PlayerWindow.GetPlayer(0)->GetFreq();
    } catch (...) {}
    return -1;
}

PlayerList ScreenClient::GetFreqPlayers(int freq) {
    return m_PlayerWindow.GetFrequency(freq);
}

PlayerList ScreenClient::GetPlayers() {
    return m_PlayerWindow.GetPlayers();
}

bool ScreenClient::OnSoloFreq() {
    int freq = GetFreq();
    return GetFreqPlayers(freq).size() <= 1;
}

PlayerPtr ScreenClient::GetSelectedPlayer() {
    return m_PlayerWindow.GetSelectedPlayer();
}

void ScreenClient::MoveTicker(Direction dir) {
    if (dir == Direction::Left || dir == Direction::Right) return;

    int key = dir == Direction::Down ? VK_NEXT : VK_PRIOR;

    m_Keyboard.ToggleDown();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m_Keyboard.Send(key);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m_Keyboard.ToggleDown();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void ScreenClient::Bomb() {
    if (!m_FireBombs) return;
    if (Emped()) {
        m_Keyboard.Up(VK_TAB);
        return;
    }
    if (m_BombDelay > 0 && (timeGetTime() < m_LastBomb + m_BombDelay)) return;

    if (m_Thrusting)
        SetThrust(false);

    if (m_BombDelay == 0) {
        m_Keyboard.Down(VK_TAB);
    } else {
        m_Keyboard.ToggleDown();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        m_Keyboard.Down(VK_TAB);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m_Keyboard.Up(VK_TAB);
        m_Keyboard.ToggleDown();
        m_LastBomb = timeGetTime();
    }
}

void ScreenClient::Gun(GunState state, int energy_percent) {
    if (!m_FireGuns) state = GunState::Off;

    if (Emped()) {
        m_Keyboard.Up(VK_CONTROL);
        return;
    }

    if (m_ScaleDelay)
        m_CurrentBulletDelay = static_cast<int>(std::ceil(m_BulletDelay * (1.0f + (100.0f - energy_percent) / 100)));

    if (state == GunState::Tap && m_BulletDelay == 0) state = GunState::Constant;
    if (state == GunState::Tap && timeGetTime() < m_LastBullet + m_CurrentBulletDelay) return;

    if (m_Thrusting)
        SetThrust(false);

    switch (state) {
        case GunState::Constant:
            m_Keyboard.Down(VK_CONTROL);
        break;
        case GunState::Off:
            m_Keyboard.Up(VK_CONTROL);
            return;
        case GunState::Tap:
            m_Keyboard.Down(VK_CONTROL);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            m_Keyboard.Up(VK_CONTROL);
        break;
    }
    
    m_LastBullet = timeGetTime();
}

void ScreenClient::Burst() {
    m_Keyboard.ToggleDown();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    m_Keyboard.Down(VK_SHIFT);
    std::this_thread::sleep_for(std::chrono::milliseconds(105));
    m_Keyboard.Send(VK_DELETE);
    m_Keyboard.Up(VK_SHIFT);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    m_Keyboard.ToggleDown();
}

void ScreenClient::Repel() {
    m_Keyboard.ToggleDown();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    m_Keyboard.Down(VK_SHIFT);
    std::this_thread::sleep_for(std::chrono::milliseconds(105));
    m_Keyboard.Send(VK_CONTROL);
    m_Keyboard.Up(VK_SHIFT);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    m_Keyboard.ToggleDown();
}

void ScreenClient::Decoy() {
	m_Keyboard.ToggleDown();
	std::this_thread::sleep_for(std::chrono::milliseconds(30));

    if (m_Thrusting)
        SetThrust(false);

	m_Keyboard.Send(VK_F5);

	m_Keyboard.ToggleDown();
}

void ScreenClient::SetThrust(bool on) {
    m_Thrusting = on;

    if (on)
        m_Keyboard.Down(VK_SHIFT);
    else
        m_Keyboard.Up(VK_SHIFT);
}

void ScreenClient::SetXRadar(bool on) {
    int count = 0;
    /* Try to toggle xradar, timeout after 50 tries */

    try {
        while (Util::XRadarOn(m_Screen) != on && count < 50) {
            if (m_Thrusting)
                SetThrust(false);

            m_Keyboard.Down(VK_END);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            m_Keyboard.Up(VK_END);
            count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            m_Screen->Update();
        }
    } catch (...) {}
}

void ScreenClient::Warp() {
    SetXRadar(false);
    if (m_Thrusting)
        SetThrust(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    m_Keyboard.Send(VK_INSERT);
}


void ScreenClient::Up(bool val) {
    val ? m_Keyboard.Down(VK_UP) : m_Keyboard.Up(VK_UP);
}

void ScreenClient::Down(bool val) {
    val ? m_Keyboard.Down(VK_DOWN) : m_Keyboard.Up(VK_DOWN);
}

void ScreenClient::Left(bool val) {
    val ? m_Keyboard.Down(VK_LEFT) : m_Keyboard.Up(VK_LEFT);
}

void ScreenClient::Right(bool val) {
    val ? m_Keyboard.Down(VK_RIGHT) : m_Keyboard.Up(VK_RIGHT);
}

void ScreenClient::Attach() {
    if (m_Thrusting)
        SetThrust(false);

    m_Keyboard.Send(VK_F7);
}

bool ScreenClient::InShip() const {
    return Util::InShip(m_Screen);
}

void ScreenClient::EnterShip(int num) {
    if (m_Thrusting)
        SetThrust(false);

    m_Keyboard.Up(VK_CONTROL);
    m_Keyboard.Send(VK_ESCAPE);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    m_Keyboard.Send(0x30 + num);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

std::vector<Vec2> ScreenClient::GetEnemies(Vec2 real_pos, const Level& level) {
    // TODO: Fix ball carrier and flags dropped looking exactly the same
    std::vector<Pixel> ign_colors = { Colors::EnemyColor[0], Colors::EnemyColor[1] };

    if (!m_IgnoreCarriers)
        ign_colors.push_back(Colors::EnemyBallColor);

    auto IsEnemy = [&](Pixel pix) -> bool {
        for (Pixel& check : ign_colors)
            if (pix == check) return true;
        return false;
    };

    std::vector<Vec2> enemies;

    for (int y = 0; y < m_Radar->GetWidth(); y++) {
        for (int x = 0; x < m_Radar->GetWidth(); x++) {
            Pixel pix = m_Radar->GetPixel(x, y);
            if (pix == Colors::EnemyColor[0] || pix == Colors::EnemyColor[1] || pix == Colors::EnemyBallColor) {
                int count = 0;
                try {
                    Pixel pixel;

                    // right
                    pixel = m_Radar->GetPixel(x + 1, y);
                    if (IsEnemy(pixel)) count++;

                    // bottom-right
                    pixel = m_Radar->GetPixel(x + 1, y + 1);
                    if (IsEnemy(pixel)) count++;

                    // bottom
                    pixel = m_Radar->GetPixel(x, y + 1);
                    if (IsEnemy(pixel)) count++;
                } catch (std::exception&) {}

                if (count >= 3) {
                    Vec2 coord(static_cast<float>(x), static_cast<float>(y));
                    if (!Util::InSafe(m_Radar, coord))
                        enemies.push_back(this->GetRealPosition(real_pos, coord, level));
                }
            }
        }
    }

    if (enemies.size() == 0)
        throw std::runtime_error("No enemies near.");
    return enemies;
}

Vec2 ScreenClient::GetClosestEnemy(Vec2 real_pos, const Level& level, int* dx, int* dy, double* dist) {
    std::vector<Vec2> enemies = GetEnemies(real_pos, level);
    *dist = std::numeric_limits<double>::max();
    Vec2 closest = enemies.at(0);

    for (unsigned int i = 0; i < enemies.size(); i++) {
        int cdx, cdy;
        double cdist;

        Util::GetDistance(enemies.at(i), real_pos, &cdx, &cdy, &cdist);

        if (cdist < *dist) {
            *dist = cdist;
            *dx = cdx;
            *dy = cdy;
            closest = enemies.at(i);
        }
    }

    return closest;
}

Vec2 ScreenClient::GetRealPosition(Vec2 bot_pos, Vec2 target, const Level& level) {
    return Util::FindTargetPos(bot_pos, target, m_Screen, m_Radar, level, m_MapZoom);
}

int ScreenClient::GetEnergy()  {
    return Util::GetEnergy(m_EnergyArea);
}

int ScreenClient::GetRotation() {
    u64 val = 0;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++)
            val = m_Ship->GetPixel(16 + j, 16 + i) + val;
    }

    if (m_Rotations)
        return m_Rotations->GetRotation(val);

    return -1;
}

bool ScreenClient::InSafe(Vec2 real_pos, const Level& level) {
    return level.GetTileID(static_cast<int>(real_pos.x), static_cast<int>(real_pos.y)) == 171;
}

void ScreenClient::ReleaseKeys() {
    m_Keyboard.ReleaseAll();
}

void ScreenClient::ToggleKeys() {
    m_Keyboard.ToggleDown();
}

void ScreenClient::GrabRadar() {
    int radarstart = 0;
    int radarend = 0;
    int radary = 0;

    while (radarend == 0) {
        m_Screen->Update();
        for (int y = static_cast<int>(m_Screen->GetHeight() / 1.5); y < m_Screen->GetHeight(); y++) {
            for (int x = static_cast<int>(m_Screen->GetWidth() / 1.5); x < m_Screen->GetWidth(); x++) {
                Pixel pix = m_Screen->GetPixel(x, y);

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
                    x = m_Screen->GetWidth();
                    y = m_Screen->GetHeight();
                    break;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        tcout << "Finding radar location. (Screen dimensions: " << m_Screen->GetWidth() << "x" << m_Screen->GetHeight() << ")" << std::endl;
        tcout << "Saving screen to screen.bmp" << std::endl;
        m_Screen->Save("screen.bmp");
    }

    int radarwidth = radarend - radarstart;

    tcout << "Creating radar screen with width of " << radarwidth << " at " << radarstart << ", " << radary << "." << std::endl;

    m_Radar = m_Screen->GetArea(radarstart, radary, radarwidth, radarwidth);

    if (!m_Radar.get()) {
        tcerr << "Resolution (" << m_Screen->GetWidth() << "x" << m_Screen->GetHeight() << ") not supported." << std::endl;
        std::abort();
    }
}

bool ScreenClient::Emped() {
    return m_EmpEnd > timeGetTime();
}

void ScreenClient::SendString(const std::string& str) {
    m_Keyboard.ReleaseAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    for (char c : str) {
        short code = VkKeyScan(c);
        u8 state = (code >> 8) & 0xFF;
        bool shift = state & 1;

        if (shift)
            m_Keyboard.Down(VK_LSHIFT);
        m_Keyboard.Down(code);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        m_Keyboard.Up(code);
        if (shift)
            m_Keyboard.Up(VK_LSHIFT);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    m_Keyboard.Send(VK_RETURN);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
}

void ScreenClient::UseMacro(short num) {
    if (m_Thrusting)
        SetThrust(false);

    m_Keyboard.Send(VK_F1 + num - 1);
}

void ScreenClient::EMPPixelHandler(ScreenGrabberPtr screen, int x, int y) {
    static const std::vector<Vec2> dirs = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
    const Pixel emp_color(148, 148, 214, 0);

    // Only check near the ship
    if (std::abs(x - (screen->GetWidth() / 2)) > 100) return;
    if (std::abs(y - (screen->GetHeight() / 2)) > 100) return;

    for (const auto& dir : dirs) {
        if (x + dir.x < 0 || x + dir.x > screen->GetWidth() - 1) continue;
        if (y + dir.y < 0 || y + dir.y > screen->GetHeight() - 1) continue;
        Pixel pixel = screen->GetPixel(int(x + dir.x), int(y + dir.y));

        if (pixel != emp_color)
            return;
    }

    m_EmpEnd = timeGetTime() + 100;
}

void ScreenClient::Scan() {
    const int width = m_Screen->GetWidth();
    const int height = m_Screen->GetHeight();

    typedef std::function<void(ScreenGrabberPtr, int, int)> PixelHandler;
    std::unordered_map<Pixel, PixelHandler> handlers;
    
    handlers[Pixel(255, 255, 255, 0)] = std::bind(&ScreenClient::EMPPixelHandler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Pixel pixel = m_Screen->GetPixel(x, y);

            auto found = handlers.find(pixel);
            if (found != handlers.end())
                found->second(m_Screen, x, y);
        }
    }
}

std::vector<Vec2> ScreenClient::FindMines(Vec2 bot_pixel_pos) {
    const int width = m_Screen->GetWidth();
    const int height = m_Screen->GetHeight();

    const int mine_width = 14;
    const int mine_height = 16;
    const int start_x = 180;
    const int start_y = 75;
    const int end_x = width - 28;
    const int end_y = height - mine_height - 1;    
    const Pixel black(0, 0, 0, 0);

    // how many non-black pixels are in each row of a mine
    const int mine_counts[] = {3, 6, 9, 13, 14, 14, 14, 14, 14, 14, 14, 12, 12, 10, 7, 2};

    std::vector<Vec2> mines;

    for (int y = start_y; y < end_y; ++y) {
        for (int x = start_x; x < end_x; ++x) {
            if (m_Screen->GetPixel(x, y) != black) continue;
            if (m_Screen->GetPixel(x + 1, y) == black) continue;

            // Start checking if it's a mine from 5 pixels back
            x -= 5; 
            
            bool possible = true;
            for (int i = 0; i < mine_height; ++i) {
                int count = 0;

                for (int j = 0; j < mine_width; ++j) {
                    Pixel pixel = m_Screen->GetPixel(x + j, y + i);
                    if (pixel != black) count++;
                }

                int diff = std::abs(count - mine_counts[i]);
                if (diff > 2) {
                    possible = false;
                    break;
                }
            }

            x += 5;

            if (possible) {
                // It's probably a mine if we made it this far
                // Calculate how far away the mine is from the player ship (center of screen)
                Vec2 diff(x + 1 - (width / 2.0f), y + 7 - (height / 2.0f));

                // Add the mine to the list of mines by calculating its position based on the bot's pixel position in the level.
                double mine_x = std::floor((bot_pixel_pos.x + diff.x) / 16.0f);
                double mine_y = std::floor((bot_pixel_pos.y + diff.y) / 16.0f);
                mines.emplace_back(mine_x, mine_y);
            }
        }
    }

    return mines;
}

void ScreenClient::SelectPlayer(const std::string& name) {
    PlayerPtr target = m_PlayerWindow.Find(name);

    if (!target.get()) {
        tcerr << "ScreenClient::SelectPlayer : "<< "Could not find " << name << " in player window. Scrolling up." << std::endl;;

        // Page up the whole way in case it is off screen
        m_Keyboard.ReleaseAll();

        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            m_Keyboard.Down(VK_LSHIFT);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            m_Keyboard.Down(VK_PRIOR);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            m_Keyboard.Up(VK_LSHIFT);
            m_Keyboard.Up(VK_PRIOR);
        }
        return;
    }

    if (target == m_PlayerWindow.GetSelectedPlayer()) return;

    Direction dir = m_PlayerWindow.GetDirection(target);

    // hopefully this doesn't happen
    if (dir == Direction::None) return;

    MoveTicker(dir);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    m_Screen->Update();
    m_PlayerWindow.Update(151);
    // Keep going until it's selected
    SelectPlayer(name);
}

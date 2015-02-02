#include "MemorySensor.h"

#include "Memory.h"

#include <iostream>

namespace Memory {

std::ostream& operator<<(std::ostream& os, SensorError error) {
    static const std::vector<std::string> strs = { "None", "DebugPrivileges", "Pid", "Handle", "BaseAddress" };
    os << strs[(int)error];
    return os;
}

template<typename Map, typename Vec>
void MapToVector(const Map& m, Vec& v) {
    for (typename Map::const_iterator it = m.begin(); it != m.end(); ++it )
        v.push_back(it->second);
}

MemorySensor::MemorySensor()
    : m_ProcessHandle(nullptr),
    m_ContBaseAddr(0),
    m_MenuBaseAddr(0),
    m_PositionAddr(0),
    m_Pid(0),
    m_Initialied(false),
    m_Position(0, 0),
    m_Velocity(0, 0),
    m_Freq(9999),
    m_Name("")
{

}

SensorError MemorySensor::Initialize(HWND window) {
    if (!Memory::GetDebugPrivileges())
        return SensorError::DebugPrivileges;

    if (GetWindowThreadProcessId(window, &m_Pid) == 0)
        return SensorError::Pid;

    if (!(m_ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_Pid)))
        return SensorError::Handle;

    m_ContBaseAddr = Memory::GetModuleBase("Continuum.exe", m_Pid);
    m_MenuBaseAddr = Memory::GetModuleBase("menu040.dll", m_Pid);

    if (m_ContBaseAddr == 0 || m_MenuBaseAddr == 0)
        return SensorError::BaseAddress;

    DetectName();
    DetectPosition();

    m_Initialied = true;

    return SensorError::None;
}

PlayerList MemorySensor::GetPlayers() {
    PlayerList players;

    MapToVector(m_Players, players);

    return players;
}

void MemorySensor::DetectName() {
    unsigned short pindex = Memory::GetU32(m_ProcessHandle, m_MenuBaseAddr + 0x47FA0) & 0xFFFF;
    const int ProfileStructLength = 2860;
    uintptr_t addr = m_MenuBaseAddr;

    addr = Memory::GetU32(m_ProcessHandle, addr + 0x47A38) + 0x15;
    if (addr == 0) return;

    addr += pindex * ProfileStructLength;

    std::string name = Memory::GetString(m_ProcessHandle, addr, 23);
    m_Name = name.substr(0, strlen(name.c_str()));
}

// Detect the position address by searching for a unique identifier
void MemorySensor::DetectPosition() {
    const unsigned int PosStructID = 0x4AC7C0;

    std::vector<uintptr_t> found = Memory::FindU32(m_ProcessHandle, PosStructID);
    uintptr_t address = 0;

    if (found.size() == 1) {
        address = found.at(0) + 0x1C;
    } else {
        // More than one address with the identifier was found, narrow down the results.
        for (uintptr_t addr : found) {
            bool okay = true;
            for (int i = 1; i <= 3; ++i) {
                unsigned int value = Memory::GetU32(m_ProcessHandle, addr + i * 4);
                if (value != 0) {
                    okay = false;
                    break;
                }
            }
            if (okay) {
                address = addr;
                break;
            }
        }
    }

    if (address == 0) return;

    m_PositionAddr = address;

    m_Position.x = Memory::GetU32(m_ProcessHandle, address);
    m_Position.y = Memory::GetU32(m_ProcessHandle, address + 4);
}

void MemorySensor::DetectFreq() {
    uintptr_t addr = Memory::GetU32(m_ProcessHandle, m_ContBaseAddr + 0xC1AFC) + 0x30;
    const unsigned int FreqOffset = 0x12664;
    addr += FreqOffset;
    m_Freq = Memory::GetU32(m_ProcessHandle, addr) & 0xFFFF;
}

void MemorySensor::DetectPlayers() {
    uintptr_t addr = Memory::GetU32(m_ProcessHandle, m_ContBaseAddr + 0xC1AFC); // Starting address

    addr += 0x127EC;

    std::vector<PlayerData> players;

    uintptr_t count_addr = addr + 0x1884;
    uintptr_t players_addr = addr + 0x884;

    unsigned short count = Memory::GetU32(m_ProcessHandle, count_addr) & 0xFFFF;

    const unsigned char NameOffset = 0x6D;
    const unsigned char FreqOffset = 0x58;
    const unsigned char RotOffset = 0x3C;
    const unsigned char PosOffset = 0x04;
    const unsigned char SpeedOffset = 0x10;
    const unsigned char IDOffset = 0x18;

    for (unsigned short i = 0; i < count; ++i) {
        uintptr_t player_addr = Memory::GetU32(m_ProcessHandle, players_addr + (i * 4));
        if (player_addr == 0) continue;

        unsigned short x = Memory::GetU32(m_ProcessHandle, player_addr + PosOffset) / 1000;
        unsigned short y = Memory::GetU32(m_ProcessHandle, player_addr + PosOffset + 4) / 1000;
        int xspeed = Memory::GetU32(m_ProcessHandle, player_addr + SpeedOffset);
        int yspeed = Memory::GetU32(m_ProcessHandle, player_addr + SpeedOffset + 4);
        unsigned short rot = Memory::GetU32(m_ProcessHandle, player_addr + RotOffset) / 1000;
        unsigned short freq = Memory::GetU32(m_ProcessHandle, player_addr + FreqOffset) & 0xFFFF;
        unsigned short pid = Memory::GetU32(m_ProcessHandle, player_addr + IDOffset);
        std::string name = Memory::GetString(m_ProcessHandle, player_addr + NameOffset, 23);
        name = name.substr(0, strlen(name.c_str())); // trim nulls

        if (freq > 9999) freq = 9999;

        PlayerPtr player;

        auto found = m_Players.find(pid);
        if (found == m_Players.end())
            player = std::make_shared<Player>();
         else
            player = found->second;

        m_Players[pid] = player;

        player->SetName(name);
        player->SetFreq(freq);
        player->SetRotation(rot);
        player->SetPosition(Vec2(x, y));
        player->SetVelocity(Vec2(xspeed, yspeed));
        player->SetPid(pid);
    }
}

void MemorySensor::Update(unsigned long dt) {
    if (!m_Initialied) return;

    if (m_PositionAddr != 0) {
        m_Position.x = Memory::GetU32(m_ProcessHandle, m_PositionAddr);
        m_Position.y = Memory::GetU32(m_ProcessHandle, m_PositionAddr + 4);
        m_Velocity.x = Memory::GetU32(m_ProcessHandle, m_PositionAddr + 8);
        m_Velocity.y = Memory::GetU32(m_ProcessHandle, m_PositionAddr + 12);
    }
    DetectFreq();
    DetectPlayers();
}

} // ns Memory

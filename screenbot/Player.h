#ifndef PLAYER_H_
#define PLAYER_H_

#include "Vector.h"

#include <string>
#include <memory>
#include <vector>

enum class Ship { Warbird, Javelin, Spider, Leviathan, Terrier, Weasel, Lancaster, Shark, Spectator };

class Player {
private:
    std::string m_Name;
    unsigned short m_Freq;
    unsigned short m_Rotation;
    Vec2 m_Position;
    Vec2 m_Velocity;
    unsigned short m_Pid;
    Ship m_Ship;
    bool m_InArena;

public:
    Player() : m_InArena(true) { }
    Player(const std::string& name) : m_Name(name), m_InArena(true) { }
    Player(const std::string& name, unsigned short freq) : m_Name(name), m_Freq(freq), m_InArena(true) { }

    const std::string& GetName() const { return m_Name; }
    unsigned short GetFreq() const { return m_Freq; }
    unsigned short GetRotation() const { return m_Rotation; }
    // Pixels
    Vec2 GetPosition() const { return m_Position; }
    // Pixels
    Vec2 GetVelocity() const { return m_Velocity; }
    unsigned short GetPid() const { return m_Pid; }
    Ship GetShip() const { return m_Ship; }
    bool InArena() const { return m_InArena; }

    void SetName(const std::string& name) { m_Name = name; }
    void SetFreq(unsigned short freq) { m_Freq = freq; }
    void SetRotation(unsigned short rot) { m_Rotation = rot; }
    // Pixels
    void SetPosition(int x, int y) { m_Position = Vec2(x, y); }
    // Pixels
    void SetPosition(Vec2 pos) { m_Position = pos; }
    // Pixels
    void SetVelocity(int xspeed, int yspeed) { m_Velocity = Vec2(xspeed, yspeed); }
    // Pixels
    void SetVelocity(Vec2 vel) { m_Velocity = vel; }
    void SetPid(unsigned short pid) { m_Pid = pid; }
    void SetShip(Ship ship) { m_Ship = ship; }
    void SetInArena(bool inarena) { m_InArena = inarena; }

};

typedef std::shared_ptr<Player> PlayerPtr;
typedef std::vector<PlayerPtr> PlayerList;

#endif

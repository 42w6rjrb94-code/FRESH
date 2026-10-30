#pragma once
#include "memory.hpp"
#include "offsets.hpp"
#include <vector>
#include <string>
#include <cmath>

struct Vec2 { float x=0,y=0; };
struct Vec3 { float x=0,y=0,z=0; };
enum class AttackState : int { Idle=0,Startup=1,Active=2,Recovery=3 };

struct BHPlayer {
    uintptr_t   addr=0;
    float       hp=200,maxHp=200;
    int         teamID=0;
    std::string name;
    Vec3        worldPos;
    Vec2        velocity;
    bool        isGrounded=false;
    bool        isLocal=false;
    bool        facingRight=false;
    AttackState atkState=AttackState::Idle;
    int         atkFrame=0;
    float       dodgeCD=0.f;
    uintptr_t   weaponPtr=0;
    int         weaponType=-1;
    float       distToLocal=0.f;
};

struct BHWeapon {
    uintptr_t addr=0;
    Vec3      pos;
    int       type=0;
    float     distToLocal=0.f;
};

class Scene {
    Memory&   mem;
    uintptr_t airBase       = 0;
    uintptr_t pointerArray  = 0;
    uintptr_t indexStore    = 0;
    bool      caveReady     = false;
    bool      avmReady      = false;
    uintptr_t mainInstance  = 0;

    // ── Code cave (AOB fallback) ─────────────────────────────────────────
    bool SetupCodeCave() {
        const char* pattern = "\xF2\x0F\x10\x8B\x40\x01\x00\x00";
        const char* mask    = "xxxxxxxx";
        uintptr_t targetAddr = mem.FindPatternSafe(pattern, mask);
        if (!targetAddr) return false;

        uintptr_t caveAddr = mem.AllocNear(targetAddr);
        if (!caveAddr) return false;

        pointerArray         = caveAddr + 0x100;
        indexStore           = caveAddr + 0x80;
        uintptr_t returnAddr = targetAddr + 8;

        std::vector<uint8_t> sc = {
            0x50,0x51,0x52,
            0x48,0xB8,0,0,0,0,0,0,0,0,
            0x8B,0x08,
            0x83,0xF9,0x32,
            0x7C,0x02,0x31,0xC9,
            0x48,0xBA,0,0,0,0,0,0,0,0,
            0x48,0x89,0x1C,0xCA,
            0xFF,0xC1,
            0x89,0x08,
            0x5A,0x59,0x58,
            0xF2,0x0F,0x10,0x8B,0x40,0x01,0x00,0x00,
            0xFF,0x25,0x00,0x00,0x00,0x00,
            0,0,0,0,0,0,0,0
        };
        memcpy(&sc[5],  &indexStore,   8);
        memcpy(&sc[24], &pointerArray, 8);
        memcpy(&sc[57], &returnAddr,   8);
        mem.WriteBytes(caveAddr, sc);

        std::vector<uint8_t> patch={0xE9,0,0,0,0,0x90,0x90,0x90};
        DWORD rel=(DWORD)(caveAddr-targetAddr-5);
        memcpy(&patch[1],&rel,4);
        DWORD oldProt;
        VirtualProtectEx(mem.handle,(LPVOID)targetAddr,8,
            PAGE_EXECUTE_READWRITE,&oldProt);
        mem.WriteBytes(targetAddr,patch);
        VirtualProtectEx(mem.handle,(LPVOID)targetAddr,8,oldProt,&oldProt);
        return true;
    }

    // ── AVM2 traversal — trouve Main via Adobe Air.dll ──────────────────
    bool SetupAVM2() {
        airBase = mem.GetModuleBase("Adobe AIR.dll");
        if (!airBase) {
            std::cout << "[AVM2] Adobe AIR.dll non trouve\n";
            return false;
        }
        std::cout << "[AVM2] airBase=0x" << std::hex << airBase << "\n";

        uintptr_t gcHeap = mem.Read<uintptr_t>(airBase + AirOff::GC_HEAP_SLOT_RVA);
        if (!gcHeap) { std::cout << "[AVM2] gcHeap null\n"; return false; }
        std::cout << "[AVM2] gcHeap=0x" << std::hex << gcHeap << "\n";

        // Trouve AvmCore via collectors
        uintptr_t avmCore = 0;
        uintptr_t collectorsBase = gcHeap + AirOff::GC_HEAP_COLLECTORS;
        int colCount = mem.Read<int>(collectorsBase + AirOff::LIST_COUNT);
        uintptr_t colEntries = mem.Read<uintptr_t>(collectorsBase + AirOff::LIST_ENTRIES);
        std::cout << "[AVM2] collectors count=" << std::dec << colCount << "\n";

        for (int i = 0; i < colCount && i < 64; i++) {
            uintptr_t gc = mem.Read<uintptr_t>(colEntries + i * 8);
            if (!gc) continue;
            uintptr_t core = mem.Read<uintptr_t>(gc + AirOff::GC_TO_AVM_CORE);
            if (core > 0x10000 && core < 0x7FFFFFFFFFFF) {
                avmCore = core;
                std::cout << "[AVM2] avmCore=0x" << std::hex << avmCore << "\n";
                break;
            }
        }
        if (!avmCore) { std::cout << "[AVM2] avmCore non trouve\n"; return false; }

        // Traverse live pools → cherche Main instance
        uintptr_t node = mem.Read<uintptr_t>(avmCore + AirOff::CORE_LIVE_POOLS);
        int poolIdx = 0;
        while (node && poolIdx < 256) {
            uintptr_t weakPool = mem.Read<uintptr_t>(node + AirOff::NODE_WEAK_POOL);
            if (weakPool) {
                uintptr_t poolObj = mem.Read<uintptr_t>(weakPool + AirOff::WEAK_REF_OBJECT);
                if (poolObj) {
                    // Tente chaîne Main sur les scripts du pool
                    uintptr_t scriptsList = poolObj + AirOff::POOL_SCRIPTS;
                    int sCount = mem.Read<int>(scriptsList + AirOff::LIST_COUNT);
                    uintptr_t sEntries = mem.Read<uintptr_t>(scriptsList + AirOff::LIST_ENTRIES);

                    for (int s = 0; s < sCount && s < 256; s++) {
                        uintptr_t scriptEnv = mem.Read<uintptr_t>(sEntries + s * 8);
                        if (!scriptEnv) continue;
                        // Le scope global du script devrait contenir Main
                        uintptr_t scope = mem.Read<uintptr_t>(scriptEnv + 0x28);
                        if (!scope) continue;
                        uintptr_t candidate = mem.Read<uintptr_t>(scope + 0x10);
                        if (!candidate) continue;

                        // Valide : candidate + 0x100 → GameManager ?
                        uintptr_t gm = mem.Read<uintptr_t>(candidate + GameOff::MAIN_TO_GM);
                        if (!gm) continue;
                        uintptr_t el = mem.Read<uintptr_t>(gm + GameOff::GM_TO_ENTITYLIST);
                        if (!el) continue;
                        int ec = mem.Read<int>(el + 0x08);
                        if (ec > 0 && ec < 8) {
                            mainInstance = candidate;
                            std::cout << "[AVM2] Main=0x" << std::hex
                                      << mainInstance << " GM=0x" << gm
                                      << " entities=" << std::dec << ec << "\n";
                            return true;
                        }
                    }
                }
            }
            node = mem.Read<uintptr_t>(node + AirOff::NODE_NEXT);
            poolIdx++;
        }
        std::cout << "[AVM2] Main non trouve via pools\n";
        return false;
    }

    // ── Lecture entités via chaîne AVM2 ─────────────────────────────────
    std::vector<BHPlayer> GetPlayersAVM2() {
        std::vector<BHPlayer> out;
        uintptr_t gm = mem.Read<uintptr_t>(mainInstance + GameOff::MAIN_TO_GM);
        if (!gm) return out;

        uintptr_t localEntity = mem.Read<uintptr_t>(gm + GameOff::GM_TO_LOCAL);
        uintptr_t entityList  = mem.Read<uintptr_t>(gm + GameOff::GM_TO_ENTITYLIST);
        if (!entityList) return out;

        int count = mem.Read<int>(entityList + 0x08);
        uintptr_t entries = mem.Read<uintptr_t>(entityList + 0x10);
        if (count <= 0 || count > 8 || !entries) return out;

        for (int i = 0; i < count; i++) {
            uintptr_t entity = mem.Read<uintptr_t>(entries + i * 8);
            if (!entity) continue;
            uintptr_t dc = mem.Read<uintptr_t>(entity + GameOff::ENTITY_TO_DISPLAY);
            if (!dc) continue;

            double posX = mem.Read<double>(dc + GameOff::DISPLAY_X);
            double posY = mem.Read<double>(dc + GameOff::DISPLAY_Y);
            if (posX < -50000.0 || posX > 50000.0) continue;

            BHPlayer p;
            p.addr     = dc;
            p.worldPos = {(float)posX, (float)posY, 0.f};
            p.isLocal  = (entity == localEntity);
            p.teamID   = i;
            p.name     = p.isLocal ? "You" : "Enemy";
            p.hp = p.maxHp = 200.f;
            out.push_back(p);
        }

        Vec3 lp{};
        for (auto& p : out) if (p.isLocal) { lp=p.worldPos; break; }
        for (auto& p : out) {
            float dx=p.worldPos.x-lp.x, dy=p.worldPos.y-lp.y;
            p.distToLocal=std::sqrtf(dx*dx+dy*dy);
        }
        return out;
    }

    // ── Lecture entités via code cave (fallback) ─────────────────────────
    std::vector<BHPlayer> GetPlayersCave() {
        std::vector<BHPlayer> out;
        std::vector<uintptr_t> dps;

        for (int i = 0; i < 50; i++) {
            uintptr_t dp = mem.Read<uintptr_t>(pointerArray + i * 8);
            if (!dp) continue;
            bool dup=false;
            for (auto e:dps) if(e==dp){dup=true;break;}
            if (!dup) dps.push_back(dp);
        }

        for (size_t i = 0; i < dps.size(); i++) {
            uintptr_t dp = dps[i];
            // UKC : DisplayCache+0x140=Y, +0x148=X
            double posY = mem.Read<double>(dp + Off::PosY);
            double posX = mem.Read<double>(dp + Off::PosX);
            if (posX < -50000.0 || posX > 50000.0 ||
                posY < -50000.0 || posY > 50000.0) continue;

            BHPlayer p;
            p.addr      = dp;
            p.worldPos  = {(float)posX,(float)posY,0.f};
            p.atkState  = (AttackState)mem.Read<int>(dp+Off::AttackState);
            p.atkFrame  = mem.Read<int>(dp+Off::AttackFrame);
            p.isLocal   = (i==0);
            p.teamID    = (int)i;
            p.name      = p.isLocal ? "You" : "Enemy";
            p.hp = p.maxHp = 200.f;
            out.push_back(p);
        }

        Vec3 lp{};
        for (auto& p:out) if(p.isLocal){lp=p.worldPos;break;}
        for (auto& p:out){
            float dx=p.worldPos.x-lp.x,dy=p.worldPos.y-lp.y;
            p.distToLocal=std::sqrtf(dx*dx+dy*dy);
        }
        return out;
    }

public:
    Scene(Memory& m) : mem(m) {
        avmReady  = SetupAVM2();
        caveReady = SetupCodeCave();
    }

    bool IsReady() const { return caveReady || avmReady; }

    std::vector<BHPlayer> GetPlayers(uintptr_t) {
        if (avmReady && mainInstance)
            return GetPlayersAVM2();
        if (caveReady)
            return GetPlayersCave();
        return {};
    }

    std::vector<BHWeapon> GetGroundWeapons(Vec3) { return {}; }
};

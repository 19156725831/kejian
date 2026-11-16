#include "raylib.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vector>

// ============================================================
// 1. 数据结构定义
// ============================================================

struct WorldConfig {
    float gravity = 10.0f;        
    float groundAngleDeg = 0.0f;
    float groundFriction = 0.3f;
} world;

enum EntityType { TYPE_BOX, TYPE_ANCHOR, TYPE_SPRING_HOOK };
enum ItemType { ITEM_SPRING, ITEM_BOX }; 

struct ItemDef {
    const char* name;
    ItemType type;
};

struct ForceVisual {
    float magnitude;
    Vector2 dir; 
    Color color;
};

struct FlashEffect {
    Vector2 pos;
    float timer;
};

struct Entity {
    int id;
    EntityType type;
    Vector2 pos;     
    Vector2 vel;     
    float mass;
    float width, height; 
    float radius;        
    float angle;         
    bool isStatic;       
    Color color;
    bool isDragging;
    
    float pushForce[3];     
    float pushAngleDeg[3];  
    
    std::vector<ForceVisual> forces; 
};

struct Spring {
    int idA;
    int idB;
    float k;
    float restLength;
    float damping; 
};

struct InputBox {
    Rectangle rect;
    char text[32];
    bool isActive;
    float* valuePtr;
    const char* label;
};

// ============================================================
// 2. 辅助函数
// ============================================================

float DistanceToSegment(Vector2 p, Vector2 a, Vector2 b) {
    float l2 = (b.x-a.x)*(b.x-a.x) + (b.y-a.y)*(b.y-a.y);
    if (l2 == 0.0f) return sqrtf((p.x-a.x)*(p.x-a.x) + (p.y-a.y)*(p.y-a.y));
    float t = ((p.x-a.x)*(b.x-a.x) + (p.y-a.y)*(b.y-a.y)) / l2;
    t = fmaxf(0, fminf(1, t));
    Vector2 proj = { a.x + t*(b.x-a.x), a.y + t*(b.y-a.y) };
    return sqrtf((p.x-proj.x)*(p.x-proj.x) + (p.y-proj.y)*(p.y-proj.y));
}

void DrawSpringWithAngle(Font font, Vector2 start, Vector2 end, float thick, Color color, bool showAngle) {
    Vector2 dir = { end.x - start.x, end.y - start.y };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (len < 0.1f) return;
    float ux = dir.x / len, uy = dir.y / len;
    float px = -uy, py = ux;
    int coils = (int)(len / 15.0f); 
    if (coils < 4) coils = 4;
    if (coils > 40) coils = 40;
    float coilWidth = 8.0f; 
    Vector2 prev = start;
    for (int i = 1; i <= coils; i++) {
        float t = (float)i / coils;
        Vector2 cur = { start.x + dir.x * t, start.y + dir.y * t };
        if (i < coils) {
            float offset = (i % 2 == 0 ? 1.0f : -1.0f) * coilWidth;
            cur.x += px * offset;
            cur.y += py * offset;
        }
        DrawLineEx(prev, cur, thick, color);
        prev = cur;
    }

    if (showAngle) {
        float dx = start.x - end.x; 
        float dy = end.y - start.y; 
        float angleRad = atan2f(dy, dx); 
        float angleDeg = angleRad * RAD2DEG;
        
        Vector2 midPos = { start.x + (end.x - start.x) * 0.5f, start.y + (end.y - start.y) * 0.5f };
        char text[32];
        snprintf(text, sizeof(text), "%.1f°", angleDeg);
        
        Vector2 textSize = MeasureTextEx(font, text, 16, 1);
        DrawRectangle(midPos.x - textSize.x/2 - 4, midPos.y - textSize.y/2 - 2, textSize.x + 8, textSize.y + 4, Fade(BLACK, 0.7f));
        DrawTextEx(font, text, (Vector2){midPos.x - textSize.x/2, midPos.y - textSize.y/2}, 16, 1, YELLOW);
    }
}

void DrawForceDot(Font font, Vector2 center, ForceVisual f) {
    if (f.magnitude < 0.01f) return;
    float finalAngle = atan2f(f.dir.y, f.dir.x);
    float lineLen = 30.0f + fminf(f.magnitude * 2.0f, 30.0f); 
    Vector2 dotPos = { center.x + cosf(finalAngle) * lineLen, center.y + sinf(finalAngle) * lineLen };
    
    DrawLineEx(center, dotPos, 2.0f, Fade(f.color, 0.6f));
    DrawCircleV(dotPos, 6.0f, f.color);
    DrawCircleLines(dotPos.x, dotPos.y, 6.0f, WHITE);
    
    char text[32];
    snprintf(text, sizeof(text), "%.1fN", f.magnitude);
    Vector2 textPos = { dotPos.x + cosf(finalAngle) * 10.0f, dotPos.y + sinf(finalAngle) * 10.0f };
    if (cosf(finalAngle) < -0.3f) textPos.x -= MeasureTextEx(font, text, 16, 1).x;
    if (sinf(finalAngle) < -0.3f) textPos.y -= 16.0f;
    DrawTextEx(font, text, textPos, 16, 1, f.color);
}

InputBox* activeInput = nullptr;
void UpdateInputBox(InputBox& box, Vector2 mouse) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(mouse, box.rect)) {
            if (activeInput != &box) {
                if (activeInput && activeInput->valuePtr) {
                    *(activeInput->valuePtr) = atof(activeInput->text);
                    // 【新增】防归零兜底
                    if (strcmp(activeInput->label, "质量") == 0 && *(activeInput->valuePtr) < 0.01f) *(activeInput->valuePtr) = 0.01f;
                }
                activeInput = &box;
                box.isActive = true;
                box.text[0] = '\0'; 
            }
        } else {
            if (box.isActive) { 
                if (box.valuePtr) {
                    *(box.valuePtr) = atof(box.text);
                    if (strcmp(box.label, "质量") == 0 && *(box.valuePtr) < 0.01f) *(box.valuePtr) = 0.01f;
                }
                box.isActive = false;
                if (activeInput == &box) activeInput = nullptr;
            }
        }
    }
    if (box.isActive) {
        int key = GetCharPressed();
        while (key > 0) {
            if ((key >= 32) && (key <= 125) && (strlen(box.text) < 30)) {
                int len = strlen(box.text);
                box.text[len] = (char)key; box.text[len+1] = '\0';
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            int len = strlen(box.text); if (len > 0) box.text[len-1] = '\0';
        }
        if (IsKeyPressed(KEY_ENTER)) {
            if (box.valuePtr) {
                *(box.valuePtr) = atof(box.text);
                if (strcmp(box.label, "质量") == 0 && *(box.valuePtr) < 0.01f) *(box.valuePtr) = 0.01f;
            }
            box.isActive = false; if (activeInput == &box) activeInput = nullptr;
        }
    } else {
        if (box.valuePtr) {
            char temp[32]; 
            // 【修改】质量显示两位小数
            if (strcmp(box.label, "质量") == 0) {
                snprintf(temp, sizeof(temp), "%.2f", *(box.valuePtr));
            } else if (strstr(box.label, "推力") || strstr(box.label, "方向") || strstr(box.label, "角度")) {
                snprintf(temp, sizeof(temp), "%.0f", *(box.valuePtr));
            } else {
                snprintf(temp, sizeof(temp), "%.2f", *(box.valuePtr));
            }
            if (strcmp(box.text, temp) != 0) strcpy(box.text, temp);
        }
    }
}

void DrawInputBoxSide(Font font, InputBox& box) {
    if (!box.valuePtr) return;
    DrawTextEx(font, box.label, (Vector2){box.rect.x - 120.0f, box.rect.y + 4.0f}, 16, 1, YELLOW); 
    DrawRectangleRec(box.rect, box.isActive ? DARKGRAY : BLACK);
    DrawRectangleLinesEx(box.rect, 2, box.isActive ? SKYBLUE : GRAY);
    DrawTextEx(font, box.text, (Vector2){box.rect.x + 8.0f, box.rect.y + 2.0f}, 16, 1, WHITE);
}

// ============================================================
// 3. 主程序
// ============================================================
const float PPM = 80.0f;

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 768, "物理实验");
    SetTargetFPS(60);

    Font font = GetFontDefault(); 
    const char* uiText = "0123456789.,:%/mkNθ°μ-+ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz|物品生成环境参数全局地面角度动摩擦因数选中物体质量劲度系数原长阻尼推力方向方块弹簧拖拽到中间松开点击输入修改按回车确认删除全场未选中重力支持摩擦弹力"; 
    const char* paths[] = {"simkai.ttf", "simhei.ttf", "msyh.ttc", "C:/Windows/Fonts/simhei.ttf"};
    for (int i = 0; i < 4; i++) {
        if (FileExists(paths[i])) {
            int codepointCount = 0;
            int *codepoints = LoadCodepoints(uiText, &codepointCount);
            Font tempFont = LoadFontEx(paths[i], 24, codepoints, codepointCount);
            if (tempFont.texture.id > 0) { font = tempFont; UnloadCodepoints(codepoints); break; }
            UnloadCodepoints(codepoints);
        }
    }

    std::vector<Entity> entities;
    std::vector<Spring> springs;
    std::vector<FlashEffect> flashes;
    int nextId = 1;

    int selectedEntityId = -1;
    int selectedSpringIdx = -1;

    std::vector<ItemDef> itemList = {
        { "弹簧", ITEM_SPRING },
        { "方块", ITEM_BOX }
    };
    int selectedItemIndex = 0; 

    bool isDraggingItem = false;
    Vector2 dragStartPos = {0, 0};

    InputBox inAngle = { {0, 0, 80.0f, 25.0f}, "0", false, &world.groundAngleDeg, "地面角度" };
    InputBox inFric = { {0, 0, 80.0f, 25.0f}, "0.3", false, &world.groundFriction, "摩擦因数" };
    
    InputBox selMass = { {0, 0, 80.0f, 25.0f}, "", false, nullptr, "质量" };
    InputBox selForce[3], selAngle[3];
    for(int i=0; i<3; i++) {
        selForce[i] = { {0, 0, 80.0f, 25.0f}, "", false, nullptr, "推力" };
        selAngle[i] = { {0, 0, 80.0f, 25.0f}, "", false, nullptr, "方向" };
    }
    
    InputBox selK = { {0, 0, 80.0f, 25.0f}, "", false, nullptr, "劲度(k)" };
    InputBox selL0 = { {0, 0, 80.0f, 25.0f}, "", false, nullptr, "原长(m)" };
    InputBox selDamp = { {0, 0, 80.0f, 25.0f}, "", false, nullptr, "阻尼(c)" };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        float scrW = (float)GetScreenWidth();
        float scrH = (float)GetScreenHeight();
        float UI_RIGHT_WIDTH = 280.0f;
        float playAreaRight = scrW - UI_RIGHT_WIDTH;
        Vector2 origin = { playAreaRight / 2.0f, scrH - 80.0f }; 

        inAngle.rect = { playAreaRight + 160.0f, 230.0f, 80.0f, 25.0f };
        inFric.rect = { playAreaRight + 160.0f, 265.0f, 80.0f, 25.0f };
        
        selMass.rect = { playAreaRight + 160.0f, 340.0f, 80.0f, 25.0f };
        for(int i=0; i<3; i++) {
            selForce[i].rect = { playAreaRight + 160.0f, 375.0f + i*60.0f, 80.0f, 25.0f };
            selAngle[i].rect = { playAreaRight + 160.0f, 400.0f + i*60.0f, 80.0f, 25.0f };
        }

        selK.rect = { playAreaRight + 160.0f, 340.0f, 80.0f, 25.0f };
        selL0.rect = { playAreaRight + 160.0f, 375.0f, 80.0f, 25.0f };
        selDamp.rect = { playAreaRight + 160.0f, 410.0f, 80.0f, 25.0f };
        
        Rectangle btnDelete = { playAreaRight + 30.0f, scrH - 50.0f, 220.0f, 35.0f };

        Vector2 mouse = GetMousePosition();
        Vector2 mouseWorld = { (mouse.x - origin.x) / PPM, -(mouse.y - origin.y) / PPM };

        for (int i = flashes.size() - 1; i >= 0; i--) {
            flashes[i].timer -= dt;
            if (flashes[i].timer <= 0.0f) flashes.erase(flashes.begin() + i);
        }

        // --- 1. 物品库选择与拖拽 ---
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && mouse.x > playAreaRight) {
            for (int i = 0; i < itemList.size(); i++) {
                Rectangle rowRect = { playAreaRight + 20.0f, 40.0f + i * 45.0f, 240.0f, 40.0f };
                if (CheckCollisionPointRec(mouse, rowRect)) {
                    selectedItemIndex = i;
                    isDraggingItem = true;
                    dragStartPos = mouse;
                    break;
                }
            }
        }

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            if (isDraggingItem && mouse.x < playAreaRight) {
                if (mouse.y > origin.y) mouseWorld.y = 0.4f; 
                
                ItemType type = itemList[selectedItemIndex].type;
                
                if (type == ITEM_BOX) {
                    Entity e = { nextId++, TYPE_BOX, mouseWorld, {0,0}, 1.0f, 0.8f, 0.8f, 0, 0, false, ORANGE, false };
                    for(int i=0; i<3; i++) { e.pushForce[i] = 0.0f; e.pushAngleDeg[i] = 0.0f; }
                    entities.push_back(e);
                    selectedEntityId = entities.back().id; selectedSpringIdx = -1;
                } 
                else if (type == ITEM_SPRING) {
                    Entity* target = nullptr; float minD = 2.0f;
                    for (auto& e : entities) {
                        if (e.type != TYPE_BOX) continue;
                        float d = sqrtf((e.pos.x - mouseWorld.x)*(e.pos.x - mouseWorld.x) + (e.pos.y - mouseWorld.y)*(e.pos.y - mouseWorld.y));
                        if (d < minD) { minD = d; target = &e; }
                    }
                    if (target) {
                        Vector2 topW = { target->pos.x, target->pos.y + 2.0f };
                        Entity a = { nextId++, TYPE_ANCHOR, topW, {0,0}, 99999.0f, 0, 0, 0.15f, 0, true, RED, false };
                        for(int i=0;i<3;i++){ a.pushForce[i]=0; a.pushAngleDeg[i]=0; }
                        entities.push_back(a);
                        springs.push_back({entities.back().id, target->id, 1.0f, 2.0f, 0.5f}); 
                    } else {
                        Vector2 topW = { mouseWorld.x, mouseWorld.y + 1.0f };
                        Vector2 bottomW = { mouseWorld.x, mouseWorld.y - 1.0f };
                        Entity a = { nextId++, TYPE_ANCHOR, topW, {0,0}, 99999.0f, 0, 0, 0.15f, 0, true, RED, false };
                        Entity b = { nextId++, TYPE_SPRING_HOOK, bottomW, {0,0}, 0.0f, 0, 0, 0.15f, 0, false, GREEN, false };
                        for(int i=0;i<3;i++){ a.pushForce[i]=0; a.pushAngleDeg[i]=0; b.pushForce[i]=0; b.pushAngleDeg[i]=0; }
                        entities.push_back(a);
                        entities.push_back(b);
                        springs.push_back({entities[entities.size()-2].id, entities.back().id, 1.0f, 2.0f, 0.5f});
                    }
                    selectedSpringIdx = springs.size() - 1; selectedEntityId = -1;
                }
            }
            isDraggingItem = false;
        }

        // --- 2. 场景交互 ---
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && mouse.x < playAreaRight) {
            bool hit = false;
            for (int i = entities.size() - 1; i >= 0; i--) {
                Entity& e = entities[i];
                Vector2 eScreen = { origin.x + e.pos.x * PPM, origin.y - e.pos.y * PPM };
                bool isHit = false;
                if (e.type == TYPE_BOX) isHit = CheckCollisionPointRec(mouse, (Rectangle){eScreen.x - e.width*PPM/2, eScreen.y - e.height*PPM/2, e.width*PPM, e.height*PPM});
                else isHit = CheckCollisionPointCircle(mouse, eScreen, e.radius*PPM + 15); 
                
                if (isHit) {
                    selectedEntityId = e.id; selectedSpringIdx = -1; e.isDragging = true; hit = true; break;
                }
            }
            if (!hit) {
                for (int i = 0; i < springs.size(); i++) {
                    Entity* a = nullptr; Entity* b = nullptr;
                    for (auto& e : entities) {
                        if (e.id == springs[i].idA) a = &e;
                        if (e.id == springs[i].idB) b = &e;
                    }
                    if (a && b) {
                        Vector2 p1 = { origin.x + a->pos.x*PPM, origin.y - a->pos.y*PPM };
                        Vector2 p2 = { origin.x + b->pos.x*PPM, origin.y - b->pos.y*PPM };
                        if (DistanceToSegment(mouse, p1, p2) < 10.0f) {
                            selectedSpringIdx = i; selectedEntityId = -1; break;
                        }
                    }
                }
            }
        }

        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            for (auto& e : entities) {
                if (e.isDragging) { e.pos = mouseWorld; e.vel = {0,0}; }
            }
        }

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            std::vector<int> toRemove;
            for (auto& e : entities) {
                if (e.isDragging && (e.type == TYPE_ANCHOR || e.type == TYPE_SPRING_HOOK)) {
                    Entity* target = nullptr; float minD = 2.0f; 
                    for (auto& other : entities) {
                        if (other.id == e.id || other.type != TYPE_BOX) continue;
                        float d = sqrtf((other.pos.x - e.pos.x)*(other.pos.x - e.pos.x) + (other.pos.y - e.pos.y)*(other.pos.y - e.pos.y));
                        if (d < minD) { minD = d; target = &other; }
                    }
                    if (target) {
                        for (auto& sp : springs) {
                            if (sp.idA == e.id) sp.idA = target->id;
                            if (sp.idB == e.id) sp.idB = target->id;
                        }
                        flashes.push_back({ target->pos, 0.3f });
                        toRemove.push_back(e.id);
                    }
                }
            }
            for (int id : toRemove) {
                for (int i = 0; i < entities.size(); i++) {
                    if (entities[i].id == id) { entities.erase(entities.begin() + i); break; }
                }
            }
            for (auto& e : entities) e.isDragging = false;
        }

        // --- 3. UI 更新 ---
        UpdateInputBox(inAngle, mouse);
        UpdateInputBox(inFric, mouse);

        if (selectedEntityId != -1) {
            Entity* selE = nullptr;
            for (auto& e : entities) if (e.id == selectedEntityId) selE = &e;
            if (selE) {
                if (selE->type == TYPE_BOX) {
                    selMass.valuePtr = &selE->mass; 
                    UpdateInputBox(selMass, mouse);
                    for(int i=0; i<3; i++) {
                        selForce[i].valuePtr = &selE->pushForce[i];
                        selAngle[i].valuePtr = &selE->pushAngleDeg[i];
                        UpdateInputBox(selForce[i], mouse);
                        UpdateInputBox(selAngle[i], mouse);
                    }
                }
            }
            selK.valuePtr = nullptr; selL0.valuePtr = nullptr; selDamp.valuePtr = nullptr;
        } else if (selectedSpringIdx != -1 && selectedSpringIdx < springs.size()) {
            selK.valuePtr = &springs[selectedSpringIdx].k;
            selL0.valuePtr = &springs[selectedSpringIdx].restLength;
            selDamp.valuePtr = &springs[selectedSpringIdx].damping;
            UpdateInputBox(selK, mouse);
            UpdateInputBox(selL0, mouse);
            UpdateInputBox(selDamp, mouse);

            selMass.valuePtr = nullptr;
            for(int i=0; i<3; i++) { selForce[i].valuePtr = nullptr; selAngle[i].valuePtr = nullptr; }
        } else {
            if (selMass.isActive) { selMass.isActive = false; activeInput = nullptr; }
            selMass.valuePtr = nullptr;
            for(int i=0; i<3; i++) {
                if (selForce[i].isActive) { selForce[i].isActive = false; activeInput = nullptr; }
                if (selAngle[i].isActive) { selAngle[i].isActive = false; activeInput = nullptr; }
                selForce[i].valuePtr = nullptr; selAngle[i].valuePtr = nullptr;
            }
            if (selK.isActive) { selK.isActive = false; activeInput = nullptr; }
            if (selL0.isActive) { selL0.isActive = false; activeInput = nullptr; }
            if (selDamp.isActive) { selDamp.isActive = false; activeInput = nullptr; }
            selK.valuePtr = nullptr; selL0.valuePtr = nullptr; selDamp.valuePtr = nullptr;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, btnDelete)) {
            if (selectedEntityId != -1) {
                for (int i = springs.size() - 1; i >= 0; i--) {
                    if (springs[i].idA == selectedEntityId || springs[i].idB == selectedEntityId) {
                        springs.erase(springs.begin() + i);
                    }
                }
                for (int i = 0; i < entities.size(); i++) {
                    if (entities[i].id == selectedEntityId) {
                        entities.erase(entities.begin() + i);
                        break;
                    }
                }
                selectedEntityId = -1;
            } else if (selectedSpringIdx != -1 && selectedSpringIdx < springs.size()) {
                springs.erase(springs.begin() + selectedSpringIdx);
                selectedSpringIdx = -1;
            }
        }

        // ============================================================
        // 4. 物理计算
        // ============================================================
        float rad = world.groundAngleDeg * DEG2RAD;
        Vector2 n = { -sinf(rad), cosf(rad) }; 
        Vector2 t = { cosf(rad), sinf(rad) };  

        for (size_t i = 0; i < entities.size(); i++) {
            Entity& e = entities[i];
            e.forces.clear(); 
            if (e.isStatic || e.isDragging) continue;
            if (e.mass <= 0.001f) continue; 
            
            // 【新增】物理引擎兜底：确保质量至少为 0.01
            if (e.mass < 0.01f) e.mass = 0.01f;

            Vector2 totalActiveForce = { 0, 0 }; 

            totalActiveForce.y -= world.gravity * e.mass;
            e.forces.push_back({ e.mass * world.gravity, {0, 1}, RED });

            for (int k = 0; k < 3; k++) {
                if (e.pushForce[k] != 0.0f) {
                    float pRad = e.pushAngleDeg[k] * DEG2RAD;
                    Vector2 forceVec = { e.pushForce[k] * cosf(pRad), e.pushForce[k] * sinf(pRad) };
                    totalActiveForce.x += forceVec.x;
                    totalActiveForce.y += forceVec.y;
                    e.forces.push_back({ e.pushForce[k], { forceVec.x, -forceVec.y }, ORANGE });
                }
            }

            for (auto& sp : springs) {
                if (sp.idA != e.id && sp.idB != e.id) continue;
                Entity* other = nullptr;
                bool isA = (sp.idA == e.id);
                for (auto& o : entities) if (o.id == (isA ? sp.idB : sp.idA)) other = &o;
                
                if (other) {
                    Vector2 d = { other->pos.x - e.pos.x, other->pos.y - e.pos.y };
                    float len = sqrtf(d.x*d.x + d.y*d.y); if (len < 0.1f) len = 0.1f;
                    float extension = len - sp.restLength;
                    Vector2 dir = { d.x / len, d.y / len }; 
                    
                    Vector2 relVel = { other->vel.x - e.vel.x, other->vel.y - e.vel.y };
                    float vRel = relVel.x * dir.x + relVel.y * dir.y;

                    float fSpring = sp.k * extension + sp.damping * vRel;
                    fSpring = fmaxf(-5000.0f, fminf(5000.0f, fSpring));

                    totalActiveForce.x += dir.x * fSpring;
                    totalActiveForce.y += dir.y * fSpring;
                    e.forces.push_back({ fabsf(fSpring), { dir.x, -dir.y }, PURPLE });
                }
            }

            float normalDist = e.pos.x * n.x + e.pos.y * n.y;
            float halfHeight = (e.type == TYPE_BOX) ? (e.height / 2.0f) : e.radius;
            
            if (normalDist <= halfHeight + 0.01f) { 
                if (normalDist < halfHeight) {
                    float penetration = normalDist - halfHeight;
                    e.pos.x -= n.x * penetration;
                    e.pos.y -= n.y * penetration;
                }

                float vn = e.vel.x * n.x + e.vel.y * n.y;
                if (vn < 0) {
                    e.vel.x -= vn * n.x;
                    e.vel.y -= vn * n.y;
                    vn = 0;
                }

                float Fn_down = -(totalActiveForce.x * n.x + totalActiveForce.y * n.y); 
                float Ft = totalActiveForce.x * t.x + totalActiveForce.y * t.y;

                float N = fmaxf(0, Fn_down);
                
                if (N > 0.01f) e.forces.push_back({ N, { n.x, -n.y }, GREEN }); 

                float maxFriction = world.groundFriction * N;
                float vt = e.vel.x * t.x + e.vel.y * t.y;
                float friction = 0;

                if (fabsf(vt) < 0.05f) {
                    if (fabsf(Ft) <= maxFriction) {
                        friction = -Ft;
                        vt = 0; e.vel.x = 0; e.vel.y = 0; 
                    } else {
                        friction = -copysignf(maxFriction, Ft);
                    }
                } else {
                    friction = -copysignf(maxFriction, vt);
                }

                if (fabsf(friction) > 0.01f) {
                    Vector2 worldFric = { friction * t.x, friction * t.y };
                    e.forces.push_back({ fabsf(friction), { worldFric.x, -worldFric.y }, YELLOW });
                }

                if (e.vel.x != 0 || e.vel.y != 0 || fabsf(Ft) > maxFriction) {
                    totalActiveForce.x += friction * t.x;
                    totalActiveForce.y += friction * t.y;
                    e.vel.x += totalActiveForce.x / e.mass * dt;
                    e.vel.y += totalActiveForce.y / e.mass * dt;

                    float newVt = e.vel.x * t.x + e.vel.y * t.y;
                    if (vt * newVt < 0) { e.vel.x = 0; e.vel.y = 0; }
                }

                if (e.type == TYPE_BOX) e.angle = world.groundAngleDeg; 
            } else {
                e.vel.x += totalActiveForce.x / e.mass * dt;
                e.vel.y += totalActiveForce.y / e.mass * dt;
                if (e.type == TYPE_BOX) e.angle = 0; 
            }

            e.pos.x += e.vel.x * dt;
            e.pos.y += e.vel.y * dt;

            float rightBound = (playAreaRight - origin.x)/PPM - (e.type == TYPE_BOX ? e.width/2 : e.radius);
            float leftBound = -origin.x/PPM + (e.type == TYPE_BOX ? e.width/2 : e.radius);
            
            if (e.pos.x > rightBound + 2.0f) { e.pos.x = leftBound - 1.0f; }
            if (e.pos.x < leftBound - 2.0f) { e.pos.x = rightBound + 1.0f; }
        }

        for (size_t i = 0; i < entities.size(); i++) {
            if (entities[i].type != TYPE_BOX || entities[i].isDragging || entities[i].mass <= 0.001f) continue;
            for (size_t j = i + 1; j < entities.size(); j++) {
                if (entities[j].type != TYPE_BOX || entities[j].isDragging || entities[j].mass <= 0.001f) continue;
                
                Vector2 diff = { entities[j].pos.x - entities[i].pos.x, entities[j].pos.y - entities[i].pos.y };
                float d = sqrtf(diff.x*diff.x + diff.y*diff.y);
                float minDist = entities[i].width/2 + entities[j].width/2;
                
                if (d < minDist && d > 0.001f) {
                    float overlap = minDist - d;
                    Vector2 normal = { diff.x / d, diff.y / d };
                    float totalMass = entities[i].mass + entities[j].mass;
                    entities[i].pos.x -= normal.x * overlap * (entities[j].mass / totalMass);
                    entities[i].pos.y -= normal.y * overlap * (entities[j].mass / totalMass);
                    entities[j].pos.x += normal.x * overlap * (entities[i].mass / totalMass);
                    entities[j].pos.y += normal.y * overlap * (entities[i].mass / totalMass);
                }
            }
        }

        // ============================================================
        // 5. 渲染
        // ============================================================
        BeginDrawing();
        ClearBackground((Color){ 15, 23, 42, 255 });

        BeginScissorMode(0, 0, (int)playAreaRight, (int)scrH);
            
            float tanA = tanf(world.groundAngleDeg * DEG2RAD);
            float worldLeft = (0 - origin.x) / PPM;
            float worldRight = (playAreaRight - origin.x) / PPM;
            
            Vector2 gL = { 0, origin.y - worldLeft * tanA * PPM };
            Vector2 gR = { playAreaRight, origin.y - worldRight * tanA * PPM };
            
            DrawTriangle((Vector2){0, scrH}, gL, gR, (Color){ 30, 41, 59, 255 });
            DrawTriangle((Vector2){0, scrH}, gR, (Vector2){playAreaRight, scrH}, (Color){ 30, 41, 59, 255 });
            DrawLineEx(gL, gR, 6.0f, LIGHTGRAY);

            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                for (auto& e : entities) {
                    if (e.isDragging && (e.type == TYPE_ANCHOR || e.type == TYPE_SPRING_HOOK)) {
                        for (auto& box : entities) {
                            if (box.type == TYPE_BOX) {
                                float d = sqrtf((box.pos.x - e.pos.x)*(box.pos.x - e.pos.x) + (box.pos.y - e.pos.y)*(box.pos.y - e.pos.y));
                                if (d < 2.0f) {
                                    Vector2 bScreen = { origin.x + box.pos.x * PPM, origin.y - box.pos.y * PPM };
                                    DrawRectangleLinesEx((Rectangle){bScreen.x - box.width*PPM/2 - 5, bScreen.y - box.height*PPM/2 - 5, box.width*PPM + 10, box.height*PPM + 10}, 3.0f, GREEN);
                                }
                            }
                        }
                    }
                }
            }

            for (auto& e : entities) {
                Vector2 screenPos = { origin.x + e.pos.x * PPM, origin.y - e.pos.y * PPM };
                bool isSelected = (e.id == selectedEntityId);
                
                if (e.type == TYPE_BOX) {
                    DrawRectanglePro((Rectangle){screenPos.x, screenPos.y, e.width*PPM, e.height*PPM}, (Vector2){e.width*PPM/2, e.height*PPM/2}, -e.angle, e.color);
                    if (isSelected) {
                        rlPushMatrix(); rlTranslatef(screenPos.x, screenPos.y, 0); rlRotatef(-e.angle, 0, 0, 1);
                        DrawRectangleLinesEx((Rectangle){-e.width*PPM/2, -e.height*PPM/2, e.width*PPM, e.height*PPM}, 2, WHITE);
                        rlPopMatrix();
                    }
                    DrawTextEx(font, TextFormat("%.2fkg", e.mass), (Vector2){screenPos.x - 20, screenPos.y - 10}, 20, 2, WHITE);
                    for (auto& f : e.forces) {
                        DrawForceDot(font, screenPos, f);
                    }
                } 
                else if (e.type == TYPE_ANCHOR) {
                    DrawCircleV(screenPos, e.radius*PPM, e.color);
                    DrawCircleLines(screenPos.x, screenPos.y, e.radius*PPM+2, isSelected ? WHITE : LIGHTGRAY);
                } else if (e.type == TYPE_SPRING_HOOK) {
                    DrawCircleLines(screenPos.x, screenPos.y, e.radius*PPM+4, GREEN);
                    DrawCircleV(screenPos, e.radius*PPM, Fade(GREEN, 0.3f));
                }
            }

            for (int i = 0; i < springs.size(); i++) {
                Entity* a = nullptr; Entity* b = nullptr;
                for (auto& e : entities) {
                    if (e.id == springs[i].idA) a = &e;
                    if (e.id == springs[i].idB) b = &e;
                }
                if (a && b) {
                    Vector2 p1 = { origin.x + a->pos.x * PPM, origin.y - a->pos.y * PPM }; 
                    Vector2 p2 = { origin.x + b->pos.x * PPM, origin.y - b->pos.y * PPM }; 
                    DrawSpringWithAngle(font, p1, p2, (i == selectedSpringIdx) ? 5.0f : 3.0f, (i == selectedSpringIdx) ? WHITE : PURPLE, true);
                }
            }

            for (auto& f : flashes) {
                Vector2 p = { origin.x + f.pos.x * PPM, origin.y - f.pos.y * PPM };
                float progress = 1.0f - (f.timer / 0.3f);
                float radius = progress * 60.0f;
                float alpha = f.timer / 0.3f;
                DrawCircleLines(p.x, p.y, radius, Fade(WHITE, alpha));
                DrawCircleLines(p.x, p.y, radius * 0.8f, Fade(YELLOW, alpha));
            }

            if (isDraggingItem) {
                if (itemList[selectedItemIndex].type == ITEM_BOX) {
                    DrawRectanglePro((Rectangle){mouse.x, mouse.y, 0.8f*PPM, 0.8f*PPM}, (Vector2){0.8f*PPM/2, 0.8f*PPM/2}, 0, Fade(ORANGE, 0.6f));
                } else {
                    DrawCircleV(mouse, 10, Fade(PURPLE, 0.6f));
                }
            }
        EndScissorMode();

        // 右侧 UI
        DrawRectangle((int)playAreaRight, 0, (int)UI_RIGHT_WIDTH, (int)scrH, (Color){ 25, 35, 55, 255 });
        
        DrawTextEx(font, "1. 物品生成", (Vector2){playAreaRight + 20, 15}, 22, 2, SKYBLUE);
        for (int i = 0; i < itemList.size(); i++) {
            Rectangle rowRect = { playAreaRight + 20.0f, 40.0f + i * 45.0f, 240.0f, 40.0f };
            bool isSelected = (i == selectedItemIndex);
            DrawRectangleRec(rowRect, isSelected ? DARKGRAY : BLACK);
            DrawRectangleLinesEx(rowRect, 2, isSelected ? WHITE : GRAY);
            if (itemList[i].type == ITEM_BOX) {
                DrawRectangle(rowRect.x + 10, rowRect.y + 10, 20, 20, ORANGE);
            } else {
                DrawSpringWithAngle(font, {rowRect.x + 10.0f, rowRect.y + 20.0f}, {rowRect.x + 40.0f, rowRect.y + 20.0f}, 2.0f, PURPLE, false);
            }
            DrawTextEx(font, itemList[i].name, (Vector2){rowRect.x + 50.0f, rowRect.y + 10.0f}, 20, 1, WHITE);
        }

        DrawLine((int)playAreaRight, 200, (int)scrW, 200, GRAY);
        DrawTextEx(font, "2. 环境参数", (Vector2){playAreaRight + 20, 215}, 22, 2, SKYBLUE);
        DrawInputBoxSide(font, inAngle);
        DrawInputBoxSide(font, inFric);

        DrawLine((int)playAreaRight, 310, (int)scrW, 310, GRAY);
        DrawTextEx(font, "3. 选中物体", (Vector2){playAreaRight + 20, 325}, 22, 2, SKYBLUE);
        
        if (selMass.valuePtr) {
            DrawInputBoxSide(font, selMass);
            for(int i=0; i<3; i++) {
                if (selForce[i].valuePtr) {
                    DrawInputBoxSide(font, selForce[i]);
                    DrawInputBoxSide(font, selAngle[i]);
                }
            }
        } else if (selK.valuePtr) {
            DrawInputBoxSide(font, selK);
            DrawInputBoxSide(font, selL0);
            DrawInputBoxSide(font, selDamp);
        } else {
            DrawTextEx(font, "未选中物体", (Vector2){playAreaRight + 30, 360}, 18, 2, LIGHTGRAY);
            DrawTextEx(font, "点击场景中的方块或弹簧", (Vector2){playAreaRight + 30, 390}, 16, 2, DARKGRAY);
        }

        DrawRectangleRec(btnDelete, CheckCollisionPointRec(mouse, btnDelete) ? RED : MAROON);
        DrawTextEx(font, "删除选中", (Vector2){btnDelete.x + 70, btnDelete.y + 8}, 20, 2, WHITE);

        EndDrawing();
    }
    CloseWindow();
    return 0;
}
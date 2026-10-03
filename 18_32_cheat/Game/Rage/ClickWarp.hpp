#pragma once

struct IDirect3DDevice9;

class ClickWarp
{
public:
    // Свободный прицел: курсор ходит по экрану, СКМ телепортирует игрока
    // (или его машину) в мировую точку под прицелом.
    static void Update(IDirect3DDevice9* device);
};

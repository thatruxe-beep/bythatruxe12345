# 18:32 cheat

`18:32 cheat` — модульный проект для GTA:SA 1.0 US с интерфейсом ImGui. Меню открывается клавишей **Insert**; безопасная выгрузка находится в `Profile → Important → Unload cheat`.

> Windows не разрешает символ `:` в имени файла, поэтому собранная библиотека называется `18_32_cheat.dll`. В интерфейсе используется полное название **18:32 cheat**.

## Модули

### Rage

- Rapid Fire
- Infinite Ammo
- Anti Collision с выбором Vehicles / Peds / Objects
- God Mode

### Legit

- No Spread
- Fast Crosshair

### Visual

**Wallhack:** 2D Box, HP, Armor, Skeleton, Tracer.

**Effects:** Night Mode, Custom Time.

### Misc

- **Player:** No Fall Damage, Heal HP
- **Movement:** Fast Run, Fast Rotation, Aspect Ratio
- **Vehicle:** Repair Vehicle, Flip, Speedhack, Auto Engine, Auto Unlock, No Bike Fall

### Profile

- управление конфигами;
- Accent Color;
- Keybinds List;
- DPI Scale;
- Language.

## Локальная сборка

Требования:

- Visual Studio 2022;
- конфигурация `Release | Win32`;
- Plugin-SDK;
- GTA:SA 1.0 US.

Plugin-SDK загружается ZIP-архивом и собирается автоматически при первой сборке. Устанавливать Git или вручную задавать путь к SDK не требуется.

Запустите Developer PowerShell for Visual Studio и выполните:

```powershell
.\build.ps1
```

Скрипт загрузит Plugin-SDK, соберёт `Plugin_SA`, а затем создаст `build\18_32_cheat.dll`. При обычной сборке через `18_32_cheat.slnx` проект также автоматически подготовит Plugin-SDK в `.deps\plugin-sdk`.

## Структура

```text
18_32_cheat/
├── Core/
├── Game/
│   ├── Rage/
│   ├── Legit/
│   ├── Visuals/
│   └── Misc/
├── Hooks/
├── Menu/Tabs/
└── Utils/
thirdparty/
```

## Лицензия

Проект распространяется по GNU GPL v3. Полный текст находится в файле [LICENSE](LICENSE).

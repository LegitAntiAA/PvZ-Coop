
# PvZ Co-op (stabledecompile fork)

English | [Русский](#русский)

## What is this

A fork of the **stabledecompile** Plants vs. Zombies project that adds **full online co-op** to the classic game.
Play the entire campaign, every minigame, survival and custom modes together over a LAN or a VPN (e.g. **Radmin VPN**, Hamachi, ZeroTier).

### The concept

- **Host-authoritative server-client architecture.** The host runs the *only* real simulation. Clients are smart renderers: they receive full-state snapshots (~15–30 Hz, adaptive) and send back raw input events. This makes desyncs impossible by design — the client never simulates gameplay.
- **Up to 4 players** (1 host + 3 clients). Player colors are relative: you always see yourself uncolored, other players appear **blue / red / green** on your HUD (seed packets, cursors, held-plant silhouettes).
- **Shared economy.** Sun money is shared; the host's profile (level, coins, store purchases, challenge records) is the authority for everyone.
- **Shared seed picking.** The seed bank is split between players: `bank size / player count` picks each, no duplicates across players. In the bank your packets come first, other players' packets are tinted with their color, show their cooldowns and cannot be clicked by you.
- **Everything is synchronized**: pausing (host-only), win/lose, scene transitions, minigames, the store, wave banners, sounds, cursors (visible even in the menu, with nickname + ping).

### New game modes (all playable solo or in co-op)

1. **Endless** — infinite waves; zombies get stronger over time.
2. **Endless T** — the same, plus a day/night cycle and random fog/storm events.
3. **Endless S** — the same as endless, but every 10 waves you re-pick your seeds.
4. **Endless ST** — Endless T + re-picking.
5. **Tower Defense** — a random road is drawn over the lawn and zombies can only walk along it.
6. **PvP** — one team plays zombies (spawning them with a "brains" currency earned over time and by eating plants), the other plays plants. Teams can be 2v2, 1v3, 3v1 — chosen in the lobby before the game. Cursors are hidden in this mode.

### How to play (Radmin VPN)

1. Download and install [Radmin VPN](https://www.radmin-vpn.com/) (free).
2. One player creates a network (e.g. `pvzcoop`), everyone else joins it.
3. The host launches the game, enters their nickname in the **PvZ CO-OP panel** (left side of the main menu) and presses **HOST GAME**.
4. Other players press **CONNECT** and enter the host's **Radmin VPN IP** (`26.x.x.x`, shown in Radmin VPN) and the port (`27713`).
5. The player list appears on the left with nicknames and pings. The host starts any game through the normal menu — everyone is pulled in automatically.
6. Before a PvP game everyone picks a team (PLANTS / ZOMBIES button in the panel).

### Configuration (`coop.ini`, next to the exe)

```ini
nickname=Player
unlock_all_minigames=0      ; 1 = every minigame/mode unlocked without adventure progress
enable_coop=1
base_snapshot_rate=15       ; starting sync rate (15..30), adapts up automatically
window_width=0              ; 0 = default 1066, e.g. 1366x768 or 1920x1080
window_height=0
borderless_fullscreen=1     ; fullscreen = borderless desktop at native resolution
```

### Building

- Visual Studio 2022+ (v143/v145 toolset), C++20.
- Open `PlantsVsZombies.sln`, pick `ReleaseGOTY` + `Win32`/`x64`, build.
- You must own the original game: copy its folder (with `main.pak` and `properties/`) into the workspace as `Plants Vs. Zombies/` before building — the post-build step copies the game assets.

### Roadmap / status

- [x] TCP transport, framing, threads, ping, adaptive snapshot rate
- [x] Full-state snapshot sync (plants, zombies, projectiles, coins, mowers, grid items, seed packets)
- [x] Ghost entities with interpolation on clients
- [x] Lobby panel in the main menu (nick, IP, port, host/connect, player list, PvP team select)
- [x] Host-only pause / mode selection; clients' play buttons blocked
- [x] Remote cursors with nick + ping (menu and board), held-plant silhouettes in player colors
- [x] Configurable window size, borderless fullscreen
- [x] Six new modes (4 endless variants, tower defense, PvP)
- [ ] Deep per-minigame state sync polish (conveyor/scary-potter edge cases)
- [ ] Playtesting & desync hunting

---

## Русский

## Что это

Форк проекта **stabledecompile** (Plants vs. Zombies), добавляющий в классическую игру **полноценный онлайн-кооператив**.
Проходите кампанию, все мини-игры, выживания и новые режимы вместе по локальной сети или через VPN (например **Radmin VPN**, Hamachi, ZeroTier).

### Задумка

- **Архитектура «сервер-клиент», авторитет у хоста.** Симуляция — только на хосте. Клиенты — «умные рендереры»: получают полные снапшоты состояния (~15–30 Гц, адаптивно) и отправляют обратно события ввода. Рассинхрон исключён самой архитектурой — клиент не симулирует игру.
- **До 4 игроков** (1 хост + 3 клиента). Цвета относительные: себя ты видишь обычным, остальные игроки на твоём HUD — **синий / красный / зелёный** (пакеты семян, курсоры, силуэты растений в руках).
- **Общая экономика.** Солнце общее; профиль хоста (уровень, монеты, покупки, рекорды) — авторитет для всех.
- **Общий выбор растений.** Банк семян делится между игроками: `размер банка / число игроков` выборов каждому, дубликаты запрещены. В банке сначала твои пакеты, чужие подсвечены их цветом, видны их кулдауны, но нажать их нельзя.
- **Синхронизировано всё**: пауза (только хост), победы/поражения, переходы между сценами, мини-игры, магазин, баннеры волн, звуки, курсоры (видны даже в меню, с ником и пингом).

### Новые режимы (играются и соло, и в коопе)

1. **Endless** — бесконечные волны; зомби постепенно сильнее.
2. **Endless T** — то же + смена дня и ночи со временем, случайные туман/гроза.
3. **Endless S** — то же, но каждые 10 волн можно заново выбрать растения.
4. **Endless ST** — Endless T + перевыбор растений.
5. **Tower Defense** — по газону рисуется случайная дорога, зомби могут идти только по ней.
6. **PvP** — одна команда играет за зомби (спавнит их за «мозги» — валюту, которая копится со временем и за съеденные растения), вторая — за растения. Составы 2х2, 1х3, 3х1 — выбираются в лобби перед игрой. Курсоры в этом режиме скрыты.

### Как играть (Radmin VPN)

1. Скачайте и установите [Radmin VPN](https://www.radmin-vpn.com/) (бесплатно).
2. Один игрок создаёт сеть (например `pvzcoop`), остальные подключаются.
3. Хост запускает игру, вводит ник в **панели PvZ CO-OP** (слева в главном меню) и жмёт **HOST GAME**.
4. Остальные жмут **CONNECT** и вводят **IP хоста из Radmin VPN** (`26.x.x.x`, виден в Radmin VPN) и порт (`27713`).
5. Слева появляется список игроков с никами и пингом. Хост запускает любую игру через обычное меню — всех перекидывает автоматически.
6. Перед игрой в PvP все выбирают команду (кнопки PLANTS / ZOMBIES в панели).

### Конфигурация (`coop.ini`, рядом с exe)

```ini
nickname=Player
unlock_all_minigames=0      ; 1 = все мини-игры/режимы открыты без прохождения приключения
enable_coop=1
base_snapshot_rate=15       ; стартовая частота синхронизации (15..30), растёт автоматически
window_width=0              ; 0 = по умолчанию 1066, можно 1366x768 или 1920x1080
window_height=0
borderless_fullscreen=1     ; полный экран = безрамочный на нативном разрешении
```

### Сборка

- Visual Studio 2022+ (toolset v143/v145), C++20.
- Откройте `PlantsVsZombies.sln`, выберите `ReleaseGOTY` + `Win32`/`x64`, соберите.
- Нужно owning-копия оригинальной игры: скопируйте папку игры (с `main.pak` и `properties/`) в папку проекта как `Plants Vs. Zombies/` перед сборкой — пост-билд скопирует ресурсы.

### Статус

- [x] TCP-транспорт, фрейминг, потоки, пинг, адаптивная частота снапшотов
- [x] Полные снапшоты состояния (растения, зомби, снаряды, солнца, косилки, объекты сетки, пакеты семян)
- [x] Ghost-сущности с интерполяцией на клиентах
- [x] Панель лобби в главном меню (ник, IP, порт, хост/коннект, список игроков, выбор команд PvP)
- [x] Пауза и запуск игр — только хост; кнопки игр у клиентов заблокированы
- [x] Чужие курсоры с ником + пингом (меню и поле), силуэты растений в цвете игрока
- [x] Настраиваемый размер окна, безрамочный полный экран
- [x] Шесть новых режимов (4 варианта endless, tower defense, PvP)
- [ ] Тонкая полировка синхронизации отдельных мини-игр
- [ ] Плейтесты и вылов рассинхронов

---

# DISCLAIMER

This project does not condone piracy

This project does not include any IP from PopCap outside of their open source game engine, this will only output the executable for a decompiled, fan version of PvZ

If you want to play this game using this project

If you compile in GOTY, then you need the original game files by purchasing Plants Vs. Zombies: Game of the Year Edition on [Steam](https://store.steampowered.com/app/3590/Plants_vs_Zombies_GOTY_Edition/)!

If you compile in 2009, then you need the original game files by owning Plants Vs. Zombies!

# Setting Up the Mod

### 1. Workspace Setup

You need to download Visual Studio 2022 ([Download](https://my.visualstudio.com/Downloads?q=visual%20studio%202022&wt.mc_id=o~msft~vscom~older-downloads))
and download the C++ Build Tools.

With your *legally owned copy* of Plants Vs. Zombies, **Copy the game's folder** where the `PlantsVsZombies.exe` is located and **paste it inside the stabledecompile folder** (worksplace)

#### Note:
The game's folder should have the `properties/` folder and the `main.pak` file. In the Steam version, the Plants Vs. Zombies folder has a launcher and the important files are stored in a subfolder.

Only `Plants Vs Zombies` and `Plants Vs. Zombies` folder names are valid for the build script so you might want to rename them

### 2. Choosing Configuration and Platform

For development, use the `Debug` configuration. For deploying your mod to the general public, use `Release`.

||PvZ OG|PvZ GOTY|
|---|---|---|
|Development|Debug|DebugGOTY|
|Deployment|Release|ReleaseGOTY|

### 3. Building the Executable

Build the solution. Output goes to `build/<Configuration>_<Platform>/bin/`. The co-op panel is visible in the main menu.

---

# Original stabledecompile credits

### Lead Programmer
- InLiothixie

### Artists
- Andreko
-  ReatExists
-  Nostalgic2137
-  Fruko
-  Unnamed 

### Special Thanks
- YourLocalMoon
- Electr0Gunner 
- Exter 
- Adnini 
- bayant81_0613
- CharneleX
- Drenco 
- BoneL

# Acknowledgements
- [@patoke](https://github.com/Patoke) for reverse-engineering GOTY achievements, Disco, ect.
- [@rspforhp](https://www.github.com/octokatherine) for their amazing work decompiling the 0.9.9 version of PvZ
- [@ruslan831](https://github.com/ruslan831) for archiving the [0.9.9 decompilation of PvZ](https://github.com/ruslan831/PlantsVsZombies-decompilation)
- [@PortAudio](https://github.com/PortAudio/portaudio) for the amazing portable audio I/O library
- [@PopLib](https://github.com/teampopwork/PopLib) for the SDL3 inputs and other graphics function
- [@FFmpeg](https://www.ffmpeg.org/) for the amazing video audio library.
- PopCap for creating the amazing PvZ franchise (and making their game engine public)
- All the contributors which have worked or are actively working in this amazing project

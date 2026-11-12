# PvZ Co-op

English | [Русский](#русский)

A host-authoritative online co-op fork of **Plants vs. Zombies / stabledecompile**. Play Adventure, mini-games, Survival and six custom modes with up to four people. The host owns the simulation and progression; clients render authoritative snapshots and send input. Presentation, UI transitions and input lifetime barriers are synchronized separately from gameplay state.

## Playing together

**Online rooms, without a VPN:** open **ONLINE ROOMS** and create a room or select one to join. The native PvZ browser connects to the mod's shared service automatically and provides refresh, password entry, room status and connection feedback. Rooms support a name, optional password and a total capacity of 2–4 players. The directory shows the same mod build; the gameplay handshake also rejects a different build. A reliable, ordered WebRTC DataChannel carries gameplay directly between computers; Cloudflare handles discovery/signaling.

Players use the ready-made archive. The mod owner operates one shared backend; its address and public STUN are built into the mod. The [operator guide (Russian)](docs/ONLINE_SETUP_RU.md) covers Free Cloudflare deployment and maintenance. STUN without TURN cannot connect every NAT/firewall combination; LAN/VPN remains available when direct connectivity fails. Free services have usage limits.

**LAN / VPN:** enter a nickname and press **HOST GAME**. Other players enter the host's IPv4 address and port `27713`, then press **CONNECT**. For Radmin VPN, use the host's `26.x.x.x` address. The host starts games through the normal menu.

Distribute the **same compiled build** to everyone. Each game build generates a new identity containing its counter, source revision and build timestamp. Rebuilding a copy independently produces a different identity.

## Co-op controls and rules

| Action | Control / behavior |
|---|---|
| Chat | Enter to type/send, Escape to cancel; UTF-8 nicknames and messages |
| Point ping | Middle mouse button; sound and 0.5-second cooldown |
| Draw | Hold Ctrl and move the mouse; the stroke starts fading from its end after 8 seconds |
| Plant packet ping | Alt + left click; flashes the same packet for everyone and announces its name / remaining recharge time |
| Seed selection | Picks appear in the native top bank, tinted by player; fixed cards count toward the host's total quota |
| Mystery plants | The rectangular **?** button fills your available slots with hidden random plants; press it again to clear the set and return to manual picking |
| Start after picking | Every participant presses the ready button; it shows ready/total and red/green state |
| Difficulty | Host chooses 1–10 in the lobby. PvE increases wave pressure or the mini-game's own objectives/timers, with stock zombie HP; Endless retains its native health progression. PvP uses gradual comeback assistance from difficulty2 |
| Join an ongoing game | Read-only spectator: chat, middle-button ping, Alt packet ping and Ctrl drawing; the spectator's cursor is visible to others. Becomes a participant after the current level ends |
| Kick | Host uses the player-row button in the lobby or the player list while paused; an active departing player's packets are reassigned fairly |
| Personal Zombatar | Each player saves their own native-editor appearance. It uploads automatically to the host on joining; active host/client appearances can occasionally appear on zombies |

Sun, progression, store purchases and profile name belong to the host. Your personal Zombatar and visual settings remain your own. Other players' packets are visible but cannot be used. Spectators cannot plant or perform gameplay actions. Chat includes join/leave/kick messages. The GitHub icon in the top-left menu opens this repository.

Mystery cards stay locked and appear as **?** to everyone until gameplay starts, including their owner. They do not mark ordinary picker plants as taken or restrict anyone else's manual picks. Ordinary plants do not repeat within one random set; an Imitator may copy one of them. Fixed Crazy Dave cards remain visible. If anyone uses mystery plants, the usual missing-sunflower warning is skipped. Random cards cost 5% less at difficulty 1, rising linearly to 25% at 10; the discount stays with the card if its owner leaves.

Difficulty also applies to playable mini-games and custom modes: wave-based games gain more/faster waves, while puzzles and other families adjust their own goals, resources or timers. Finite-ammo modes keep their native ammunition budget. In PvP, difficulty 1 has no comeback bonuses. Higher settings gradually help the trailing side: plants receive lower prices and faster firing/sun production; zombies receive lower prices, movement speed and extra health on newly spawned units. Existing zombies are never healed by a balance change. Combined mystery/comeback discounts are capped at 35%.

### Custom modes

- **Endless / Endless T / Endless S / Endless ST:** endless stages; T adds actual day/night gameplay and weather, S adds seed repicking every ten waves. Endless retains its own progressive zombie-health scaling.
- **Tower Defense:** zombies follow a generated road; the roster includes strong enemies and Gargantuars.
- **PvP:** select plants or zombies after opening the mode, before the level loads. Every participant confirms readiness. Zombies see a waiting screen during plant selection, then their own compact zombie palette and brain balance. They start with 25 shared brains and receive 25 every 10 seconds; plants retain the native 50 starting sun. Plant tools are restricted to the plant team. Bungees target the clicked cell, and Diggers surface and walk back without scoring a house entry. Plants win at 100 zombie kills; zombies win by reaching the house objective.

## Configuration

`coop.ini` next to the executable:

```ini
nickname=Player
unlock_all_minigames=0
# 1 unlocks all mini-games/custom modes and their menu buttons

enable_coop=1
base_snapshot_rate=15
window_width=0
window_height=0
borderless_fullscreen=1
```

Default window size is 1066×600; set both dimensions for another size. The default configuration is resolved beside the EXE, independently of the launcher's working folder. The host's effective mini-game unlock setting is mirrored to clients. Window settings, co-op colors/tints and the personal Zombatar persist across restarts; the expanded settings dialog includes a live preview.

## Building

1. Use Visual Studio C++ Build Tools (v145, or v143 with an explicit toolset override), C++20, and the Windows SDK. Win32 ReleaseGOTY is the currently verified target; x64 is configured but not covered by the current validation.
2. For online rooms, build the pinned native dependency with `powershell -NoProfile -ExecutionPolicy Bypass -File tools/BuildOnlineDeps.ps1 -Platform Win32`. See [dependency notices](third_party/ONLINE_DEPENDENCIES.md). LAN/TCP can run without the WebRTC DLL.
3. Build `PlantsVsZombies.sln`, `ReleaseGOTY | Win32`. The post-build step copies `datachannel.dll`, when built, and its license notices beside the game.
4. Supply your legally owned original `main.pak` and `properties/` files. Original game assets are not included in releases. Local build output is `build/ReleaseGOTY_Win32/bin/PlantsVsZombies.exe`.

## Verification and roadmap

The [delivery board](docs/agent/WORKBOARD.md) records the checks and remaining work. Native transport checks passed 30/30 and signaling checks passed 9/9. A two-process game through the public Cloudflare service passed on one PC; connectivity between different external networks remains unverified. Connection errors explain the failed stage and known cause, with possible NAT/firewall restrictions identified as possibilities. [Smoke documentation](tests/CoopSmoke/README.md) describes the test tools.

- [x] Host-authoritative TCP, snapshots, ordered input and adaptive snapshot rate
- [x] Shared seed selection, relative player colors, custom modes
- [x] Native WebRTC transport and the shared Cloudflare service
- [x] Native PvZ room browser, refreshed mod UI and settings previews
- [ ] Complete the current play-test regression matrix and real-network NAT testing
- [ ] Lua modding API — planned, not implemented
- [ ] More custom mini-games — planned, not implemented
- [ ] New custom Adventure modes — planned, not implemented

---

## Русский

Кооперативный форк **Plants vs. Zombies / stabledecompile**: Adventure, мини-игры, выживание и шесть дополнительных режимов для компании до четырёх человек. Хост рассчитывает игру и хранит общий прогресс. Клиенты получают состояние и отправляют ввод; анимации и переходы интерфейса передаются отдельными механизмами.

### Подключение

**Без VPN:** нажмите **ONLINE ROOMS** и создайте комнату либо подключитесь к существующей. Нативный браузер в стиле PvZ автоматически соединяется с общим сервисом мода; в нём есть обновление списка, ввод пароля, состояние комнат и понятные сообщения подключения. Можно задать имя, пароль и вместимость 2–4 игрока. Каталог показывает комнаты той же сборки; версия также проверяется при игровом подключении. Игровые данные идут напрямую по WebRTC, Cloudflare обслуживает каталог и установку соединения.

Игрокам нужен готовый архив мода. Один общий сервер обслуживает владелец мода; его адрес и публичный STUN встроены в клиент. [Инструкция оператора](docs/ONLINE_SETUP_RU.md) описывает бесплатное развёртывание и обслуживание Cloudflare. У бесплатного тарифа есть лимиты. Без TURN некоторые NAT/корпоративные сети не позволяют прямое соединение; в таком случае доступно подключение по LAN/VPN.

**LAN / VPN:** хост вводит ник и нажимает **HOST GAME**. Остальные вводят его IPv4 и порт `27713`, затем **CONNECT**. В Radmin VPN используется адрес хоста вида `26.x.x.x`. Режим запускает хост через обычное меню.

Всем нужна **одна и та же готовая сборка**. При каждом билде автоматически меняется идентификатор: счётчик, ревизия исходников и время сборки. Самостоятельно пересобранная копия получает другой идентификатор.

### Управление и правила

- **Enter** — открыть чат / отправить, **Escape** — отменить. Поддерживаются русские ники и сообщения.
- **Колёсико мыши** — звуковой пинг, перезарядка 0,5 секунды.
- **Зажать Ctrl и двигать мышью** — рисовать. Через 8 секунд линия начинает исчезать от конца к началу.
- **Alt + ЛКМ по пакету растения** — подсветить именно этот пакет у всех, отправить его название и оставшийся кулдаун в чат. Указатель над банком также привязан к пакету, независимо от его позиции у другого игрока.
- При выборе растения переходят в штатный верхний банк и окрашиваются цветом владельца. Обязательные карты входят в общую квоту хоста. Для старта **все участники нажимают готовность**; кнопка показывает число готовых и меняет цвет.
- Прямоугольная кнопка **?** заполняет свободные личные слоты случайными скрытыми растениями. Повторное нажатие очищает набор и возвращает ручной выбор.
- Хост выбирает **сложность 1–10** в лобби. В PvE растут давление волн либо собственные цели/таймеры мини-игры; обычное HP зомби сохраняется, у Endless остаётся его штатное усиление здоровья. В PvP со сложности2 появляется плавная помощь отстающей стороне.
- Подключившийся во время игры становится **наблюдателем**: доступен чат, пинг колёсиком, Alt-пинг пакета и рисование через Ctrl; его курсор виден остальным. Посадка и другие игровые действия недоступны. После завершения текущего уровня он становится участником.
- Хост может кикнуть игрока кнопкой возле имени в лобби или в списке слева во время паузы. Пакеты ушедшего активного игрока распределяются между оставшимися участниками.
- Каждый игрок сохраняет собственный **зомботар** в штатном редакторе. При входе он автоматически отправляется хосту; на зомби иногда появляются головы активных хоста и клиентов. Внешность клиента не заменяется внешностью хоста. Одежда сохраняется для портрета редактора, а игровая форма зомби остаётся штатной.

Прогресс, магазин, имя профиля и общая валюта принадлежат хосту. Личная внешность и настройки отображения остаются собственными у каждого игрока. Чужие пакеты видны, но недоступны для использования. В чате есть сообщения о входе, выходе и кике. Значок GitHub в левом верхнем углу меню открывает репозиторий.

Случайный набор заблокирован и скрыт **знаками вопроса для всех**, включая владельца, до начала игры. Он не помечает растения в обычном выборе как занятые и не мешает другим выбирать их вручную. Обычные растения внутри набора не повторяются; Имитатор может скопировать уже выпавшее растение. Обязательные карты Дейва остаются видимыми. Если хотя бы один участник выбрал случайный набор, предупреждение об отсутствии подсолнуха пропускается. Скидка на случайные карты линейно растёт с 5% на сложности 1 до 25% на 10 и сохраняется при передаче карт ушедшего игрока.

Сложность действует и в доступных мини-играх и дополнительных режимах: волновые получают больше волн и более быстрый темп, остальные семейства меняют собственные цели, ресурсы или таймеры. В режимах с ограниченными боеприпасами их штатный запас сохраняется. В PvP на сложности 1 усилений нет; со 2 помощь отстающей команде нарастает плавно. У растений снижаются цены, ускоряются стрельба и выдача солнца; у зомби снижаются цены, растут скорость и здоровье новых зомби. Уже появившиеся зомби от пересчёта не лечатся. Общая скидка со случайным набором ограничена 35%.

### Дополнительные режимы

- **Endless / Endless T / Endless S / Endless ST:** бесконечные этапы; T — настоящая смена дня/ночи и погода, S — перевыбор растений каждые десять волн. У Endless сохраняется собственное постепенное усиление HP зомби.
- **Tower Defense:** движение по случайной дороге; усиленный состав врагов, включая гаргантюа.
- **PvP:** команда выбирается после открытия режима, перед загрузкой уровня. Во время выбора растений зомби видят экран ожидания, затем — только собственную панель и мозги. Старт — 25 общих мозгов, пассивный доход — 25 каждые 10 секунд; у растений остаётся штатный старт с 50 солнца. Инструменты растений недоступны зомби. Тарзанка выбирает нажатую клетку, шахтёр вылезает и идёт обратно без ложного прохода в дом. Цель растений — 100 убийств, цель зомби — добраться до дома.

Параметры `coop.ini` приведены выше. По умолчанию файл находится рядом с EXE, независимо от рабочей папки запуска. Размер окна, цвета/тонировка кооператива и личный зомботар сохраняются после перезапуска; увеличенное окно настроек показывает предпросмотр. `unlock_all_minigames=1` открывает мини-игры, дополнительные режимы и кнопки меню; эффективная настройка хоста передаётся клиентам. Инструкция сборки и расположение результата также приведены выше.

### Проверки и планы

[Доска задач и фактических проверок](docs/agent/WORKBOARD.md). Нативные проверки транспорта прошли 30/30, проверки сигналинга — 9/9. Двухпроцессная игра через публичный Cloudflare-сервис проверена на одном ПК; соединение между разными внешними сетями ещё не проверялось. Ошибки подключения объясняют этап и известную причину; ограничения NAT или брандмауэра обозначаются как возможная причина, если точный диагноз недоступен.

Нативный браузер комнат в стиле PvZ, обновлённый интерфейс мода и предпросмотр настроек уже реализованы. Пока **не реализуются**, только запланированы: Lua modding API, дополнительные мини-игры и новые Adventure-режимы.

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

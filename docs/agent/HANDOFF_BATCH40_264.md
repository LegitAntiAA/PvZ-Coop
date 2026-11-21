# Продолжение Fusion: точка передачи 264, 2026-11-14

## Актуальное завершение265 — 2026-11-14 17:20 UTC

**Текущая партия40 завершена в ограниченных native factory/co-op сценариях: 106/281 = прежние66 + новые40.** Эта запись и [ACCEPTANCE_BATCH40_264.md](ACCEPTANCE_BATCH40_264.md) заменяют прежний статус WIP/0ниже; остальной исходный handoff сохранён как история.

Root зарегистрировал модули/тесты, завершил общий wire/lifecycle/render/input glue и исправления механизмов. Release**60k protocol50/FNS15**, SHA256 `A9731560E12E3725B1A163A620202BB39FFBD348B21E5B9FA648B00748D87A64`; финальные unit **34540/0/0**. На этом же EXE Passed: Cold12 **980** common snapshots, Shooter13 **1428**, Fire15 **527**, каждый с0encode/validation refusals и реальными host/client кадрами; Classic full-intro Adventure8Soak **1034** common. Все игровые процессы закрыты. Все три исполнителя завершили работу и заморожены; Root — владелец интеграции.

Точные артефакты,40ID, выполненные проверки и ограничения — в acceptance-документе и `work/fusion/batch40-264/acceptance-final.json`. **Paid acquisition/все рецепты40 не проверены**, источник получения1020/1096/1161, particles198/199, прочие Doom death-paths и полная source NPC/кампания/магазин/альманах остаются отдельно. Прежние66 не перепроверялись поштучно60k. Старая неудачная Shooter60f сводка исправлена наFAIL из-за88encode rejects; финальные60k чистые. `git diff --check` сохраняет унаследованные whitespace-ошибки, сборка/игровые проверки Passed.

**Next:** следующая независимая партия30–40 по общим уже доказанным механизмам, с пакетной графикой и групповыми проверками. Не пересобирать завершённую партию ради старых записейниже. Незакоммиченные изменения сохранены, публикации не было. Не использовать59j для текущей логики.

## Исходный checkpoint264 — историческая запись

Это актуальный checkpoint, он имеет приоритет над старыми записями SESSION/WORKBOARD. Пользователь попросил перейти в новый чат из-за большого контекста. Все три исполнителя подтвердили полную заморозку файлов. Новых сборок/игр сейчас нет. Продолжать после запроса пользователя в новом чате.

## Цель и приоритеты пользователя

Полноценный дополнительный Fusion-режим внутри существующего native PvZ-Coop: растения, зомби, рецепты/скрещивание, приключение, мини-игры, альманах, магазин и необходимые ресурсы/анимации с кооп-синхронизацией. Classic должен сохранять поведение. Не делать второй независимый движок/интерфейс рядом с Classic.

- Самое свежее требование: работать максимально быстро большими партиями примерно30–40 растений, общими механизмами, пакетной конвертацией и групповыми проверками. Не возвращаться к многочасовым циклам по1 растению. Тяжёлые зависимости можно переносить в следующую партию с доказанными заменами; не считать подмены/алиасы готовой логикой.
- Использовать и расширять родные Plant/Projectile/Zombie/Board, chooser, seed bank, shovel, солнца, сетевые функции. Сохранить раздельный/случайный выбор, сложности, свои карточки первыми независимо отhost/client, Ready X/Y, чат/пинги. Проверки Fusion в общем коде разрешены.
- UI в стиле существующего PvZ/мода. Цифры1/2/P1 в банке убирались. Переключатель фона — отдельная кнопка справа сверху во время игры, личный выбор каждого игрока с сохранением. Предыдущие изменения фона/intro/UI подробно описаны в SESSION/FUSION; без причины не переделывать.
- Альманах/магазин и остальные Fusion-разделы ещё требуют завершения; брать переведённые строки из analysis, проверять реальные поля. Полный порт не завершён.
- Отчёты по-русски в чат и Telegram MCP: важные события и около10мин активной работы; честные счётчики. Routine Telegram разрешён пользователем. Только Root отправляет общий отчёт.

## Проект и безопасное продолжение

- Репозиторий `C:\PvZ\stabledecompile-master`; branch `codex/fusion-coop-20261113`, HEAD `5bfba0618627223a7884fd11a247be583712a3e4`. Огромный dirty/untracked tree — накопленная работа, сохранять. Никаких commits/push/releases в этой сессии. Публикацию не предполагать; при отдельном разрешении push только через авторизованный gh от пользователя.
- Fusion: `C:\PvZ\fusion\mod`; ресурсы: `assetripper\extracted\AssetRipper_export_20261112_215914\ExportedProject\Assets`; дамп `il2cppdumper`; первичный анализ `analysis`. Ghidra MCP открыт на `GameAssembly.dll`, доступен также dnSpy MCP. Сначала искать сохранённое доказательство, не читать весь дамп заново.
- Root единственный запускает сборки/игры. Скрытые owned SDL-процессы, максимум3 одновременно, парные проверки последовательно. Не использовать foreground/global input/CUA. Compilation/pure tests не заменяют реальные host/client кадры.
- Диск/ОЗУ уже освобождены. Logitech agent ранее перезапущен с разрешения пользователя; повтор не нужен. Старое заблокированное удаление кэша не повторять. `Z:\PvZ-Coop-Codex-Rescue-20261113-084934` только читать.
- Подходящие навыки ранее применялись: karpathy, UI, imagegen. Не начинать бессмысленное повторное обследование/документирование. При новом использовании skill читать его инструкцию.

## Что действительно проверено

- Прежняя игровая приёмка:66/281 растений в оговорённых native Day5/ground-NPC/co-op сценариях; не полный Fusion. Последняя новая партия264 принята0/40. Не увеличивать счётчик за код/графику/таблицы.
- Source NPC0 и2: квалифицированный базовый Day5/core сценарий2/94. Не заявлять полное соответствие всей Unity AI, MC/shrink/всех статусов/сложностей/loot/кампании.
- Последняя успешно собранная программа **59j, protocol49/FNS14**, SHA256 `C0949899C82FDA3379090D1564AE863ABC466600FE18D7C21CF363BB1B5B4AE0`. Текущий EXE всё ещё этот. Последний выполненный unit suite81: Passed31229/0/0.
- Source zombie59j regression: Passed564common/429postbirth/22BMP, cleanup/fulltail true: `work/fusion/sourcezombie59j-regression/run_b183d9b87c9f4393abc261e54a340555`.
- Controlled initial walk2: Passed560/426/22BMP: `sourcezombie59j-walk2/run_c864262e7ddf47969bc07d2003f97974`. Два исходных originSpeed заменены1.5 только до первого Update, RNG сохранён/залогирован; это не естественное RNG-покрытие.
- Classic59j Adventure8Soak/full intro1066/800: Passed1033common, first1CA96D8E, артефакты сохранены: `classic59j/run_2a494dd455dc4704b4ecc09e07e852b0`. Не повторять Classic после каждой мелкой правки.
- Ранее source59i один запуск провалился только из-за захвата кадра при flashCounter==0: native счётчик проходит в минус. Fixture исправлен на<=0, regression59j выше прошёл. Исходные failed summaries сохранены; подробности SESSION264.

## Актуальная партия40 и владельцы

Источник истины `work/fusion/new-batch40-scope.json`, FUSION-BATCH40-264 IN_PROGRESS. Все ниже — WIP, не принятые растения.

| Группа | Замороженный исполнитель | IDs |
|---|---|---|
| Cold12 | fusion_content |1036,1037,1038,1040,1023,1042,1043,1044,1045,1046,1171,1054|
| Shooter13 | fusion_architect |1020,1152,1107,1109,1161,1165,1167,1168,1096,1183,1191,929,1192|
| Fire15 | fusion_mechanics |20,22,1091,1052,1053,1055,1059,1061,1062,1063,1064,1173,1080,1179,1182|

Отложенные заменены:1085→1020,1104→1152,1170→1096,1174→929,30→1091,1041→1023,1178→1080,1172→1054. Причины в scope.deferred: air/homing/meteor, world placement, SpriteSkin, hostile projectile/companions, source NPC spawn, poison. NPC armor262 также отложен ради растений.

Все80 моделей актуальных40 (runtime+preview) прошли строгую проверку источников/текстур; `work/fusion/batch40-264/art-ready.json`. Подготовка `prepare_art.py`, установлены `assets/fusion/pilot/batch-ID` и `portrait-batch-ID`;40 записей добавлены в NativeVisual. Это только графика, не runtime-приёмка. Новые projectile-модели ещё НЕ экспортированы/зарегистрированы!

Общий конвертер/renderer расширены для animated sortingOrder, доказанно отсутствующих Sprite/GameObject bindings и полной quaternion-иерархии для bounds/anchors. Добавлен отдельный известный Animator parameter attackSpeed. Native-проверка этих новых изменений ещё не выполнена.

### Файлы исполнителей

Cold: существующие `FusionBasePlantRules`, `FusionNativeBasePlants`, `FusionBatchActionRules`, `FusionNativeBatchActions`, `FusionColdRules`, `FusionNativeCold` .h/.cpp + существующие BasePlant/BatchAction/Cold tests. NativeBasePlants.h не менялся. Добавлены также Caltrop1062/63/64 и1192 swallow callback по согласованным API.

Shooter: НОВЫЕ `FusionShooterBatchRules.h/.cpp`, `FusionNativeShooterBatch.h/.cpp`, `tests/NetProtocolTests/FusionShooterBatchTests.h/.cpp`; в существующем `FusionSplitShooterRules.cpp` добавлены3 emission1107. Entry `RunFusionShooterBatchTests()` → mChecks/mFailures.

Fire: существующие `FusionFireRules.h/.cpp`, `FusionNativeFire.h/.cpp`; НОВЫЕ `FusionFireActionBatchRules.h/.cpp`, `FusionNativeFireActionBatch.h/.cpp`, `tests/NetProtocolTests/FusionFireActionBatchTests.h/.cpp`. Entry `RunFusionFireActionBatchTests()`. Umbrella не меняли.

Все исполнители: C++ build/unit/runtime **NOT EXECUTED**, самостоятельный review не закончен. Только часть source-readback/static checks. Не считать enabled/profile готовностью. Хэши текущей передачи `work/fusion/batch40-264/handoff-hashes.json`.

## Уже внесённая Root-интеграция (не собрана)

- Исходники уже **protocol50/FNS15**, EXE по-прежнему49/14. Никогда не запускать59j для проверки новой партии!
- NativePlantData append: batchShotCount U32, batchRageTicks U16, batchBagCount U16, batchMode U8. Rage mirror native counter, source bag — оставшиеся одинаковые false0..20 плюс один implicit true, не bitmask.
- NativeZombieActionData.doomOnDeath bool сериализован, но **Root canonical death consumption hook ещё НЕ написан**. Иначе1045 только ставит тег.
- NativeContext temporaryLightDeadlines host vector<int> +temporaryLightLevel U16; count в FNS frame/capture/client bind. UpdateNativeFireLight вызван после host Board.mMainCounter++ перед actor updates. Лизы аддитивны и переживают смерть Blover; новых клиентских часов нет.
- Host fireContacts map<fullProjectileUID,set<fullZombieUID>> для source12 OnEnter episodes, prune по projectile. Не FNS clock.
- Добавлены carrier aliases новой партии; base chooser20/22. Shooter12→PEASHOOTER,1192→CHOMPER; Cold1036/1044→PUFF,1037/1023/1043/1046→FUME; Fire-carriers согласованы с агентом.1091 получил Defender pumpkin profile4000/225/CD3000 и init делегирует Defender перед собственным таймером.
- Общие finder/max-duration делегируют Base/ShooterBatch; native init Fire→Shooter перед прежними семьями; NativePlant phase Fire после Batch и ShooterPhase; dispatch новых shot events; UpdateNormal/UpdateLobMotion/DoImpact/GetRect новых projectiles; source24 land hook только actual null-impact.
- Native Plant Update/DoSpecial/MouseDown manual1043 и death callback Fire+Batch подключены. Zombie invulnerable/targetable hooks подключены в обычных native путях;1173 bitable Jala exception. Для typed SourceZombie target helper и остальных путей ещё проверить общую согласованность.
- NativeVisual вызывает новые pose/center helpers,1161 options.attackSpeed берётся из rate1/1.5/2 (НЕ умножать на animationSpeed). Поднятая base idle2/слои1161 ещё требуют завершения.
- Straight rows21/22/24/97 добавлены с source colliders;15 и97 поддерживают split1/2. Все новые Cold carriers PEA. NativeProjectileHit использует настоящий COIN_SUN для25, SMALLSUN для5.
- Central projectile metadata частично расширена для новых семей и Fire states0..3/small marker. Остаточные несовместимости ниже.
- **Новые .cpp/.h и два новых test entry ещё НЕ зарегистрированы** в SexyAppBase.vcxproj/NetProtocolTests.vcxproj/TestMain.cpp. Сначала закончить glue/регистрацию, затем build.

## Конкретно незавершённое — первые действия нового чата

1. Прочитать текущие небольшие header API и проверить Root glue в реальном diff. Не продолжать старое NPC-исследование. Сохранить40 как одну партию; сделать общую сборку/групповые сценарии после исправлений.
2. Register NEW Shooter/FireAction modules/tests; скорректировать pure wire goldens/offsets для FNS15 (+9bytes/plant,+1/zombieAction,+2roomLight) и старые invalid Cold тесты:401/1401 теперь допустимы, negatives501/1501.
3. Doom tag: consume once на первом canonical death у реального Zombie, до удаления метаданных; host only; флаг сбросить ДО рекурсивного blast. Вызвать existing ApplyNativeBatchDoomAt(...1800,setPit=false,iceDoom=false) — НЕ убивать дружеские растения/делать кратер. Включить все реальные native/source death entry paths без двойного эффекта.
4. Fire gaps подтверждены автором: idleSeconds не увеличивается у nonCaltrop Fire после нового early-return — исправить единый phase;1179 moving runner должен освобождать клетку в GetPlantsOnLawn и восстанавливаться на ghost через RestoreNativeFireActionPresentation; shared HealNativeCherryNutFromCircle(...amount100,radius1.5) ещё нужен для1179 Explode10 (НЕ для28 bounce). Добавить FusionFireActionMaximumDuration в общую envelope validation.
5. Metadata/combination gaps: old batch+kind0+hot branch ещё содержит !effectVariant и отвергает ThreeTorch smallEFV1 в Three-motion. Source small marker — effectVariant1 ТОЛЬКОkind0hot;13 сохраняет0..6цвета. Small damage:30yellow,55orange,70/95red; обычные hot40;60/80;80/100/120/140. Поддержать Rage special15 для97, а также stamped unheated15/97 после IceTorch: сохранить направление/phase/motion, не терять stamp. Проверить NativeShooterBatch recognition/motion/validator и Torch composition целиком; Root часть этого ещё не внесена.
6. Shooter known WIP:1161 at101/301 пересчитывает mShootingCounter, но НЕmStateCountdown; исправить общий rephase. batchMode0..6=startup/elevating/shoot/loop/rest/raisedidle/armed; начальный подъём автоматический безврага. Динамическая base idle2 после подъёма.46 и вращающийся102 пока AABB вместо точного rotated-box; Iron knockback source maxX mapping требует проверки; Doom skill double-sin не сохраняет source F32 порядок.13 не считать ready.
7. Cold gaps: два Plant.cpp Fume guard IsNativeBasePlant(this,7) заменить на IsNativeBaseMushroom(this) в соответствующих FUME branches.1171 после native prep20s требует source child0/child1 visual toggle (ещё не сделан).1042 shotCount0..14 валидируется новым FusionBatchPlantStateValid; Root уже выбирает его для nonShooter. SourceIceDoom freeze500/slow1500, обычные400/1400 не менять.
8. Export/register всех новых projectile моделей.6точных bindings: `work/fusion/shooters264-projectile-bindings.json` (11 ProjectileIronPea;90 PuffPea_iron;46 IronStarBullet;92 Puff_potato;95 SunSpark;102 Cactus_doom), префикс Resources/bullet/prefabs/. **90 Awake заменяет sprite на11**, scale/collider90 сохраняются: нужен source sprite override, статический prefab90 недостаточен. Cold21PuffSnow/22PuffBlack/24IceDoomBullet/97 проверить точный loader path по сохранённой primary.97 Awake XYscale1.7 вместоprefab1.5062028, нужен корректный runtime draw multiplier. Fire12/28 точныеpaths/geometry в fire264/projectile-assets.json. Никакого SpriteSkin-фреймворка1174 в этой партии.
9. Затем единый ReleaseGOTY/Win32 build (Root, CL /MP4 /FS), relevant NetProtocolTests и реальные групповые host/client smoke с кадрами. Новых grouped fixtures264 ещё нет; не дробить на40 однорастительных релизов. Контроли nativeNORMAL не равны полной sourceZombie parity. Проверить Classic один раз после совокупных общих изменений, не после каждой мелочи.

## Primary evidence / быстрые ориентиры

Cold `work/fusion/coldhybrids263/*`, `work/fusion/batch40-264/cold-primary-1.json`, `cold-primary-2.json`, `cold-projectile-primary.json`, `cold-area-primary.json`, `cold-midbomb-asm.json`, `doom-asm.json`, `source-dependencies-primary.json`, `poison-primary.json`. Последний включает JalaSquash492A40/4929F0 и полный GameAPP.LoadBullet38DB50.

Shooter `work/fusion/shooters264-*`; Fire `work/fusion/fire264/{assets.json,projectile-assets.json,source-*.json}`. Часть последних доказательств Fire была сообщена агентом, но не сохранена им передSTOP; при необходимости точечно перепроверить VA: TempLight324C70/315DA0;1080AnimBlow493270;1091Start485070/Attribute484EC0; KnockBack386370;JalaedExplode386030;ThreeSpike43E150/43E0C0;Bullet.OnEnter422EC0/OnStay422FE0;CreateBullet.cctor441CC0;BombCherry.Start440160/Explode43F9C0. Не перечитывать весь Fusion.

Старые SESSION/WORKBOARD/FUSION сохраняют историю/остальные подсистемы. Этот checkpoint фиксирует текущую незавершённую партию, её реальные ограничения и конкретный следующий шаг.

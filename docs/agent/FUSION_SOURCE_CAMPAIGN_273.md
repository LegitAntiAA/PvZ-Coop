# TASK273-CAMPAIGN-READY — минимальный контракт настоящей Adventure

Статический разбор текущих файлов 2026-11-21. Изменён только этот новый отчёт. Сборки, C++ tests, игры и Git mutation **Not executed**. Это предложение реализации, не приёмка кампании.

## Что можно брать первым

**Source Adventure 1–3 не требуют ни одного нового NPC.** Их полные ordinary/final allow-lists покрываются существующими source factories 0/2/4. Это позволяет делать первый полноценный вертикальный срез 1→2→3, параллельно закрывая следующие NPC. Сейчас даже эти уровни не готовы целиком: существующий adapter запускает Classic расписание, Classic tutorial/unlocks/award на native Board.

Фактически зарегистрированы 15 source kinds: `0,2,4,8,11,12,13,14,15,18,41,42,104,109,111`. Основание: `FusionZombieRules.cpp:12` (11 profiles), `SpecialZombie269Rules.cpp:10` (ещё4), `FusionNativeZombies.cpp:145` (carrier map), `:224` (factory). Profile/carrier сам по себе не заменяет resource preflight или ранее ограниченную runtime-приёмку.

Первые9 уровней имеют **Day scene0, 10 колонок, все5 рядов grass, sun500, отдельный money3000**. Cycle>1 добавляет ровно10 волн, не10 за каждый следующий цикл.

| Source level | Волн cycle1 / later | Полный source allow-list | Недостающий NPC | Новый base kind после trophy | Дополнительный level gate |
|---|---:|---|---|---|---|
|1|10 /20|0|нет|2|source Tutor1 на cycle1, если tutor включён|
|2|10 /20|0,2|нет|3|общий scheduler/reward/progress|
|3|10 /20|0,2,4|нет|нет|общий scheduler/reward/progress|
|4|20 /30|0,2,3|3 PolevaulterZombie|4|source Tutor4 на cycle1|
|5|20 /30|0,2,4,100|100 PeaShooterZombie / PeaShooterZ|5|не превращать native layout5 в bowling|
|6|20 /30|0,2,4,21|21 DollDiamond / DollZombie|нет|21 final-only, всё равно обязателен|
|7|30 /40|0,2,4,21,104|21|нет|source Tutor7 на cycle1|
|8|30 /40|4,8,106,108|106 BucketNutZombie;108 IronPeaZombie|нет|source Tutor8 на cycle1|
|9|30 /40|0,2,3,4,21,104,106,107|3,21,106,107 CherryNutZombie|6|не выдавать native note вместо trophy|

**Точный недостающий верхнеуровневый NPC-набор первых9: `{3,21,100,106,107,108}`.** Для каждого ещё нужны собственные AI/проектильные/armor/death/FX зависимости, source art и FNS admission; наличие модели или общий native carrier этого не закрывает. Транзитивная готовность этих шести здесь не доказана; нельзя объявлять её готовой или подменять отсутствующие типы. Source21 не имеет обычного weight: он гарантированно добавляется в конце последней волны уровней6/7/9, поэтому gate нужен до начала уровня.

Чистое сравнение таблиц даёт ещё source levels `10,11,19,24,28,37,39` без новых верхнеуровневых NPC. Это **только совпадение множества ID**: Night/Pool/Fog/Roof, water variants, terrain, tutorials и world settings отдельно не закрыты. Последовательная первая кампания всё равно останавливается перед4, пока нет3.

## Состояние и вход: отдельная source identity, прежний native Board

Добавить явный opt-in source-session в `NativeContext`, например `SourceAdventure272State`, а не оживлять отдельный `FusionRuntime` и не переименовывать прежние50 native уровней. Минимум identity: `sourceLevel1..45`, `cycle>=1`, `sourceDifficulty0..5`, replay flag, tutorial preference, completed-level bitset; host-only scheduler/health-cache/death bookkeeping. Existing `NativeContext::level`/`Board::mLevel` не являются достаточным контрактом: сейчас инициализация берёт Classic profile level.

Предлагаемые узкие интерфейсы:

- `CanStartSourceAdventure272(descriptor, error)` — level/cycle/source-data version, все NPC включая final-only, eligible cards/recipes, terrain/resources, tutorial/reward/save capabilities. При отказе ничего не запускать, не расходовать RNG и не уменьшать allow-list.
- `InitializeSourceAdventure272(Board*, descriptor)` — до chooser/wave setup; native projection только для геометрии/фона, затем source rows/sun/money/eligible/maxWave/clock. Source1 не должен проходить native one-row/sod/tutorial1 setup.
- `UpdateSourceAdventure272(Board*) -> handled` — host один раз на существующем gameplay tick; client только принимает state. `handled=true` полностью исключает native wave loop для source-session, включая случаи pause/tutorial/award.
- `OnSourceAdventureZombieDied272(Zombie*)`, `HandleSourceAdventureTrophyClick272(slot,id,token)` — идемпотентные death и authenticated click на реальных native actors; единственная точка source progress commit.

Входные guards сейчас: `FusionApp.cpp:60 CanStartFusionClassicCampaign` и вызовы в `LawnApp.cpp:1388/1450/1622/1650/6389`. **Оставить public legacy disabled.** Добавить отдельный строгий source-start путь/descriptor, который разрешается только после полной готовности конкретного source level. `IsFusionAdventure()` сейчас слишком широк: true для Fusion+ADVENTURE, в том числе legacy smoke. Все новые hooks проверяют именно source-session, сохраняя Classic и dormant adapter.

## Source scheduler: точные обязательные правила

Чистые функции `FusionCampaign272Rules.cpp` уже покрывают45 layouts/allow-lists, wave counts, source budget, ordered weighted pick и final append. Их надо вызвать из нового host loop; они не scheduler.

1. `Board::UpdateZombieSpawning` (`Board.cpp:6900`) — ранний source dispatch до native huge-wave/tutorial/challenge/countdown логики. Либо dispatch в единственной точке вызова `UpdateGame` около`:7776`, но не обе сразу. Сохранить native actor update, collision, plant/projectile/animation, pauses и snapshot transport.
2. На sourceD2 multiplier5; baseline coop difficulty1 использует sourceD2. Source difficulty и существующая mod difficulty1..10 — **разные параметры**, кооперативная политика применяется явно один раз. Формула budget `max(1,floor(wave*multiplier/3))`, wave%10==0 ×2; multipliers D0..5=`1,3,5,7,10,10`.
3. Для каждого budget pick host draw `[1,total]`, порядок dictionary из JSON, затем `TryResolveFusionSourceAdventurePick272`. Если не хватает budget на выбранный тип, source испускает0 и вычитает стоимость **выбранного типа**; это единственный разрешённый алгоритмический fallback. Неготовый NPC/resource не причина выпустить0.
4. На финальной волне после weighted fill добавить все allow-listed kinds в исходном порядке. Не добавлять stock FLAG, native composition, substitution14/18 или guessed boss. Source1D2 даёт counts `[1,3,5,6,8,10,11,13,15,33]`, всего105 обычных0.
5. Row sampling: source integer RNG rejection sampling без повторения ряда до исчерпания5, затем очистка used set; новый set для каждой волны. `PickRowForNewZombie` нельзя брать как алгоритм: он имеет stock eligibility/weights и иной RNG consumption. Native `AddZombieInRow` оставить как allocation/initialization primitive через `SpawnNativeSourceZombie`, source kind писать в metadata, не в `ZombieType` массив.
6. Начальная задержка15s, после TutorOver5s, после волны30s; spawn только countdown<0. Внутри волны интервал0. Перед каждой десятой — warning один раз, spawn после hugeTimer>5s; countdown/hold продолжаются во время warning. Final banner — когда волна фактически становится maxWave. Native warning750ticks/threshold5 не эквивалентен.
7. Host health-cache обновляется при elapsed>1s и сбрасывает elapsed в0; работает также во время source tutorial, до tutorial-gate волнового clock. Считает `int(source bodyFloat)+armor1+armor2` всех non-MC записей, в том числе dying corpses до удаления. Не брать `TotalZombiesHealthInWave`: там другой фильтр, только предыдущая волна и shield×0.2. Значение source bodyFloat берётся из `NativeSourceZombieData::health`, native liveness mirror может отличаться.
8. Threshold RNG3.5..4.0 семплируется каждый active source update, включая wave0. Early condition: currentHP<latestWaveHP/threshold && wave>0 && hold>4.2; countdown=min(countdown,random(.95,1.05)*1.8), hold=0. На spawn сбросить latestWaveHP, суммировать начальное HP+armor batch, прибавить к cachedHP; **не сбрасывать hold на spawn**. Pause clock при tutor/iceDoom/!allowSummon/boss/scaryPot; обычная game pause останавливает весь gameplay update.

Текущая factory сохраняет native random birthX. Для source scheduler нужен отдельный source spawn placement: recovered sentinel9.9 означает source lastColumnX+2.6, GetLandY(actualX,row). Переиспользовать existing `positioned/shadowX` request и Board cell geometry, но проверить source-root→native-shadow anchor: нельзя приравнять shadow к source root без доказательства. Не переносить legacy14 warp и не менять обычные native factory births. Не обещать исходный Unity RNG stream: native allocation тоже потребляет RNG; source weighted/row algorithms и host authority обязательны, побитовое Unity seed parity пока не установлено.

## Tutorial, солнце, выбор и награда

Source1 Tutor1 уже recovered: 3/4/3s вступительные сообщения, затем ожидание реальных kinds1→1000→1032, без проверки владельца/клетки. После1000 выдать три бесплатные source0 mission cards в(4,2),(5,2),(6,2); TutorOver задаёт5s. Переиспользовать native Advice, `CreateNativeDroppedCard269`, существующие recipe/placement/command, Coin full-generation metadata и FNS; сообщения взять из переведённых строк. В `Coin::Update` передавать source-tutorial признак в уже имеющийся `UpdateNativeDroppedCard269(Coin*,bool)`: сейчас default=false, без него карты исчезнут через15s вопреки учебному сценарию. Native `TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER`/level2 и его one-row restrictions для source-session подавить. Tutors4/7/8 подтверждены `unique-level-primary.c` (`SetUniqueLevel`); Tutor1 contract не покрывает их сценарии.

Source1–9 ordinary preplants отсутствуют: saved primary `waves-first-board-glove.json`, `Board.SetPrePlants 0x18031E130`, activity0 возвращает без preplants для любого уровня кроме37 (0x25); предварительные special flags0x1e0/0x1f2 в обычном Day не устанавливаются. Tutor mission cards — отдельный последующий сценарий, не preplants.

Native Sun/Coin визуал и сбор оставить. Day sky timing по recovered source: first7.5s, затем7s; текущий `UpdateSunSpawning` (`Board.cpp:6833`) рандомный/растущий и не подходит без source branch. В старом `levels-first-campaign.json` skySunValue=null: подтвердить actual source item amount по уже имеющимся primary/prefab либо явно записать принятую native adaptation; не заявлять source economy parity по совпадению текстуры. Initial500/3000 задавать после stock init так, чтобы native150/50 не перезаписали.

Unlock — **completed source flags**, не номер текущего уровня/Classic seed alias. До первой победы source0/1, обе normal+extra версии (4 identities), после completed1 добавить2 (6), после completed2 добавить3 (8), completed3 ничего, completed4 добавить4 (10), completed5 добавить5 (12), completed9 добавить6 (14). Replay выбранного более высокого level сам ничего не открывает. Использовать `TryGetFusionSourceBaseUnlock272`; заменить source ветку `InitializeNativeBoardContext` (`FusionNativeGameplay.cpp:495`), где сейчас `GetAwardSeedForLevel(level)`+Classic counterparts. Реальные native chooser/SeedPacket ordering/owner/recharge/Mystery/ready, shovel, split/random selection и difficulty UI переиспользовать. Existing native bank policy10 сознательно отличается source14 (`FusionNative.h:83`); для уровней1–3 разницы нет, для6+ требуется согласованное расширение native capacity до14 либо явно утверждённая mod policy, не скрытая source parity.

Победа: source death non-MC уменьшает enemyCount ровно один раз; когда wave>=max и enemyCount<=0 и !over/!IZ — установить over, создать trophy у последнего source ROOT+(0,1) с viewport clamp[.2,.8]. **Не ждать удаления трупа**; health-cache и enemyCount имеют разную семантику. Сейчас `UpdateNativeSourceZombieHealth` (`FusionNativeZombies.cpp:299`) вызывает native death/DropLoot, а `Zombie::TrySpawnLevelAward` (`Zombie.cpp:8759`) может поставить WON и RemoveAllZombies. Source-session должен перехватить award до этих stock effects; не выдавать seed/note/shovel по carrier level.

Trophy — настоящий native Coin/input/render primitive с source reward marker/epoch/token; click host authenticated/idempotent, client не пишет profile. Commit только на click: completed[currentSourceLevel]=true; advance только если выигран текущий frontier; next source level, на45 nextcycle+level1. Save, затем normal reward animation~1.25s+GoBack delay3s; свежий проход auto-next, replay возвращается в source selector. Existing `UpdatePlayerProfileForFinishingLevel` (`LawnApp.cpp:2956`) сейчас native50+1/wrap — для source-session заменить source commit и не вызывать второй раз. Classic profile level/achievement/unlocks не менять.

Для полного gameplay admission остаются отдельные source mower/defeat и loot policy (текущая zombie death прямо помечена native loot adaptation). Не выдавать этот wave/victory contract за доказательство их parity. Переиспользовать существующие native mower/lose scene/network terminal state там, где поведение явно принято, с guards по source-session.

## Синхронизация и минимальная приёмка следующего изменения

Actor UID/token, HP/armor, source clips/animations, plants/projectiles/coins идут существующим `NetBoardSync`/FNS. Новый source descriptor и authoritative view-state добавить туда же с версией/валидацией: level/cycle/sourceDifficulty/replay, completed bitset, source wave/max, tutor stage/message/card IDs, over/trophy/commit phase; countdown/sign/huge phase, если клиент их показывает. Нельзя полагаться на старые U16 `max(0,mZombieCountDown)` (`NetBoardSync.cpp:1818`): отрицательный source boundary там теряется. Host private caches/RNG client не пересчитывает. Resume/save требует полного host scheduler state, либо явного запрета mid-level resume до реализации; late join обязан принять текущий source epoch и всё видимое состояние.

Предлагаемый первый acceptance scope: **source1–3, cycle1, sourceD2, existing coop policies**, sourceTutor1+skip preference, source2/3 без nativeTutor; public unlock только после завершения всей цепи. Проверки Root: exact105 typed0 level1 без FLAG; actual client-owned plant/fusion/dropped-card progression; host/client pause/late join; reward lastkill до corpse removal, до click ни progress ни next; один trophy click от клиента ровно один commit/unlock; replay out-of-order не двигает frontier; next2→3 и blocked4 без substitution; split/Mystery/own-first packets сохранены. Pure rules отдельно уже есть; конфигурация/компиляция не acceptance. До этих проверок **0 новых runtime-verified source levels**.

## Source evidence

- `assets/fusion/catalog/source-levels272.json`:45levels,35unlocks,49 ordered weighted entries; binary SHA256 `b19cfc30b2061514689a93191a979e76335dd02f5c12e550e1bb62de9578b94f`. Его `sourceEvidence` содержит hash каждого сохранённого primary body; таблица совпадает с `generated/FusionSourceLevels272.inc`.
- `work/fusion/stage272/modes/integration.md`, `InitZombieList$$InitAdvWave.c`, `InitZombieList$$AdvantureZombieTypeSpawn.c`, `InitZombieList$$PickZombie.c`, `unique-level-primary.c`, `unlock-primary.c`: source configs/generation/unlocks; это не native slot adapters.
- `work/fusion/recovered/waves-contract.json`: точные scheduler/health/RNG/warning/trophy boundaries и105NPC; связанные saved primary bodies перечислены внутри. `levels-first-campaign.json`: source1–3 economy/eligibility/tutorial, старые null/gaps не объявлять закрытыми без нового evidence.
- `work/fusion/recovered/tutorial-contract.json`: Tutor1, миссионные карты, Source functions/addresses и physics; `levels-ghidra-batch3.json/7.json` — source scene, Tutor dispatch, PrizeMgr Click/AdvantureSettings/GoBack.
- `assets/fusion/catalog/zombies.json`: названия/классы6 missing kinds; runtime readiness устанавливается C++ factory/profile/resource contracts, не старым catalog.runtimeImplemented.

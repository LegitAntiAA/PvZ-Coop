# Fusion — партия40-266, ограниченная приёмка — 2026-11-14

**DONE в указанных native/co-op сценариях:106+40=146/281.** В партии35 новых поддержанных ID и5 ранее частично реализованных ID с закрытием проверяемых пробелов. Это не146 полностью перенесённых растений со всеми способами получения и всеми исходными NPC. Прежние106 поштучно на61i не перепроверялись. Общий перенос Fusion продолжается; следующая40 —267.

Итоговый ReleaseGOTY/Win32 **61i, protocol51/FNS16**, SHA256 `0BD788F213B6DBDFAAC6C85227E25F635C82B19D8F93FF5B68442C57F745DB41`. Build Passed; NetProtocolTests unit88 **39918/0failures/0skipped**. Все финальные игровые прогоны выполнены этим EXE, последовательно скрытыми owned SDL host/client парами. Foreground/global input не использовался; после проверки процессов0.

| Проверка | ID | Общие снимки | Реальные BMP | Результат |
|---|---:|---:|---:|---|
| Shooter,4 активности, включая повтор света |15|332|32|Passed|
| Action,20 растений вместе |20|1333|8|Passed|
| Economy,5 растений и3 платных действия |5|2645|12|Passed|
| Classic Adventure8Soak, full intro |—|1037|8|Passed|

У Fusion групп0 snapshot encode/validation отказов, CASE/COMPLETE на обоих peers, реальные кадры и owned cleanup. Машинный итог `work/fusion/batch40-266/acceptance-final.json`; повторный аудит `audit_acceptance.py` проверил SHA, unit,40 уникальных ID, CASE, кадры и отсутствие отказов. Первая версия аудита ошибочно ожидала5 Classic кадров на роль по прежнему прогону; текущий фактический сценарий сохраняет4 сцены на роль. Аудит исправлен на точные scene1..4 и размеры, повтор Passed; игровые результаты не изменялись.

## Состав40

- Shooter15:1117,1118,1119,1149,1126,1079,1081,1159,1160,1011,1135,1005,901,907,933.
- Action20:1002,1009,1015,1060,1070,1071,1072,1074,1075,1087,1145,1146,1163,1170,1190,1057,1155,1048,1189,1039. Последние5 — прежние частичные реализации.
- Economy5:1031,1184,1136,1137,1188.

## Интеграция и наблюдаемое поведение

Расширены существующие native Plant/Projectile/Zombie, семейные таймеры/урон/производство/слои; новые Action266/Economy/Poison и MONEY-команда зарегистрированы в проектах и wire.80 plant model packages проверены по исходным prefab/texture SHA и anchors (70 новых каталогов,10 прежних сохранены);14 новых projectile packages добавлены общим способом. Classic guards сохранены. Chomper1011 использует собственный source visual contract через общий профиль без ослабления strict loader.

В Shooter подтверждены source lobbers и дочерние снаряды, поглощение1011 и обратный4/5, источник света1079/1081, GoldCabbage supervolley, Doom-ветви заданного сценария. Native NORMAL-цель1160 имеет начальные Bucket/Door10000; на обоих peers тело/шлем/щит потеряли по160 (две порции80), финальные HP совпали. Это проверка компонентов, не source NPC parity. Реальное улучшение клиента1134→1135 списало1000 MONEY, сохранило происхождение и получило свежие UID/token.

Action подтверждает20-bit maskfffff на обоих peers после естественных7000 тиков: AoE/status/гарлик/rock/crush заданных NORMAL-контролей, producer/death/world-clone пути. Начальные низкие HP1009/1163/1190 и начальное Gold skill явно setup-only. После setup fixture не меняет HP/позиции/счётчики акторов.

Economy подтверждает естественный рост1031 за12001 тик и SUN15→25,1184 SUN15,1136/1137 MONEY25,1188 первые501/следующие1001 тиков заряда. Три реальные клиентские Money-клика:1136→1137 fee4000,1188 skill fee1000,1145→1146 fee1000; баланс10000→6000→5000→4000, реальные возвраты8/2/2 SUN25. Три SUN50 родились в2721/2731/2741 — ровно10 тиков. Выбор Money пережил>=2 applied snapshots перед каждым кликом. Исходные money10000/частично повреждённые растения — явные initial fixtures; это не проверка заработка всей суммы в кампании.

## Найденные проблемы и границы

Исправлены реальные Money cursor empty-echo reset и Gold coroutine activation-tick off-by-one. Исправлены ошибочные fixtures: unreachable Star-цель, collateral Star hit при проверке брони, privateNoParents происхождение world gifts и чтение несинхронизируемого client StateCountdown. Production валидация не ослаблялась; все неудачные прогоны сохранены. Host clock proof использует реальные birth/UID/token и client metadata; cadence SUN50 — BoardTick−CoinAge. Независимый source review подтвердил порядок command→Plant→Coin→MainCounter.

Ограничения: factory Day5/NORMAL controls,4 платных клиентских Money действия, а не все рецепты/способы получения40. Источники1159/1160 без нормального рецепта остаются world-gift fixture; equipment acquisition отдельно. Source Garg44/46 crush, air NPC и все специальные MC/Travel ветви не проверены. Начальные fade API negatives Passed, реальный terminal UI переход здесь Not executed. Exact source particles198/199/poison tint и некоторые FX остаются pending. Source NPC/все кампании/мини-игры/альманах/магазин не повышены в статусе. Рандомный/раздельный выбор, все сложности и пинги полностью повторно не проходились; используемый lobby/Ready/own-first/chat/full intro прошёл.

Targeted diff-check шести файлов последних исправлений Passed; прежние whole-tree whitespace проблемы не очищались. Dirty tree сохранён, commit/push/release не выполнялись.

Артефакты финального61i:

- Shooter: `work/fusion/batch40-266/shooter/run_af1cf035e357433a976a6022a2945fb3`.
- Action: `work/fusion/batch40-266/action/run_258dc4b7244c4bf4b5d2055dc161824a`.
- Economy: `work/fusion/batch40-266/economy/run_d0410cd3cd6f4493833f3545952d26ad`.
- Classic: `work/fusion/batch40-266/classic61i/run_b1cf21a6ed0747738b3372e6f9e1f9a7`.

Root просмотрел реальные host/client Fusion кадры и клиентский Classic. Авторские модули и независимые проверки: engineer1_shooter, engineer2_fire, engineer3_projectile_art; Root — вся интеграция, сборки, игровые запуски и приёмка.

# Batch40-267 — принята в ограниченных сценариях, 2026-11-15 03:42 MSK

Приняты ровно40 уникальных растений; общий проверенный счёт186/281. Полный перенос Fusion продолжается. Эта приёмка покрывает перечисленные групповые сценарии, а не все приобретения, Travel, source NPC AI, кампанию и визуальные эффекты.

Сборка62m ReleaseGOTY Win32, protocol52/FNS17. SHA256 `60F94A984C5CD29C20E8CCEAA51E4D94B80DCF6CD42DE0E54ED318E4A0D1AAA3`. Unit:41929 Passed,0 Failed,0 Skipped. Все игровые результаты получены на этом EXE в скрытых owned SDL host/client процессах; максимум2 одновременно,0 оставшихся процессов. Сохранены68 Fusion и8 Classic BMP; Root просмотрел реальные пары всех групп и Classic. Audit и ручной review Passed.

|Группа|Растений|Общие снимки|BMP|
|---|---:|---:|---:|
|potstar62m-g1|1|539|10|
|shooter62m|16|793|32|
|umbrella62m-g0|17|1978|8|
|umbrella62m-g1|3|115|8|
|potstar62m-g0|3|427|10|
|Classic Adventure8Soak full intro|—|1035|8|

Во всех Fusion группах0 encode/validation refusals. Проверены реальные native callbacks, урон/броня/MC, фазовые состояния и полёт, платные SDL14+13 и16 с ценой/перезарядкой/новой идентичностью, естественное поедание и замена, pot traits, Umbrella/Obsidian/Butter,90s Lily1→1033, SeaShroom growth и перенос SeaLantern со свежим readback. Состав40 зафиксирован в scope.json.

Независимый просмотр нашёл общую проблему Pool6: фиксированные5-row guards, Day100px projectile geometry и input PixelToGridY делил на100. Исправлены общие пути и128 граничных unit-проверок Day/Pool/Fog. Расширенный G3 на62m доказал1058 entry/edge/adjacent, настоящий платный16 вrow5 и два отдельных лечения1073 от16 и естественной смерти919. Обе стороны подтвердили разные события; Root увидел реальные Pool кадры. Fixture не меняла actorHP/таймеры/координаты после setup.

Ограничения: Lily runtime проверен для stored1→1033, не для всех типов и blocked cycles; hostile58 и1186 проверены native control actors, не всей source Catapult/air AI; полная Pool кампания/mowers и Lily+pot+body тройной слой не приняты.1078 sourceFX40 и attached88 exact latejoin phase остаются отдельной работой. Старые failed прогоны сохранены. Нет commit/push/release.

Артефакты: work/fusion/batch40-267/acceptance-final.json, review-62m.json и verified-62m. Последняя папка — архив EXE/tests/evidence, не отдельная запускаемая сборка с DLL/pak.

Следующая партия268: единый набор40 в разработке общими механизмами. Новые файлы ещё не зарегистрированы, native/game проверки268 не выполнены. Приёмка267 не означает завершение всей задачи.

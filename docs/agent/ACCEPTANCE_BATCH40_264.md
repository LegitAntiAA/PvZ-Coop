# Fusion: приёмка партии40, checkpoint265 — 2026-11-14

**FUSION-BATCH40-264: DONE в описанных ниже ограниченных native/co-op сценариях.** Прежние66 + новые40 = **106/281** растений с ограниченной игровой приёмкой. Это не показатель полного переноса всех способов получения, кампании или исходной игры. Прежние66 отдельно на60k не перепроверялись.

## Сборка и результат

- ReleaseGOTY Win32 **60k**, protocol**50**, FNS**15**. Новая логика проверялась этим EXE;59j не использовался.
- EXE: `build/ReleaseGOTY_Win32/bin/PlantsVsZombies.exe`.
- SHA256: `A9731560E12E3725B1A163A620202BB39FFBD348B21E5B9FA648B00748D87A64`.
- Release build: **Passed**. Финальные NetProtocolTests: **34540 проверок, failures0, skipped0**.
- Все три семейства и Classic проверены скрытыми owned SDL host/client процессами, последовательно; foreground/global input не использовался. После прогонов игровых процессов нет.

| Проверка на60k | Растений | Seed | Общих снимков | Реальных BMP | Статус |
| --- | ---: | ---: | ---: | ---: | --- |
| Cold, 3 активности4+4+4 | 12 | 424243 | 980 | 28 | Passed |
| Shooter, 3 активности5+6+2 | 13 | 424242 | 1428 | 26 | Passed |
| Fire, 1 активность | 15 | 424242 | 527 | 8 | Passed |
| Classic Adventure8Soak, полный intro | — | 424242 | 1034 | 10 | Passed |

У каждого Fusion-прогона **0 отказов snapshot encode/validation**, все ожидаемые CASE/COMPLETE получены на обоих peers, owned cleanup подтверждён. Сообщения о disconnected при штатном завершении не являются отказами валидации. Classic: host1066x600/client800x600, первый общий hash `1E2BAEC0`.

Root просмотрел реальные host/client кадры Fusion: снаряды/горячие спрайты, Gatling301, Doom-взрыв, гипноз, оба состояния света1171; также клиентский кадр Classic. Снимки и BMP сохранены, прежние неудачные прогоны не удалены.

## Точный состав40

| Семейство | ID |
| --- | --- |
| Cold12 | 1036,1037,1038,1040,1023,1042,1043,1044,1045,1046,1171,1054 |
| Shooter13 | 1020,1152,1107,1109,1161,1165,1167,1168,1096,1183,1191,929,1192 |
| Fire15 | 20,22,1091,1052,1053,1055,1059,1061,1062,1063,1064,1173,1080,1179,1182 |

Названия, sourceClass и восемь отложенных зависимостей сохранены в `work/fusion/new-batch40-scope.json`. Все40 помечены DONE только с qualification `bounded native factory/co-op`; отложенные растения в40 не включены.

## Что интегрировано

- Production/test проекты зарегистрировали ShooterBatchRules/NativeShooterBatch, FireActionBatchRules/NativeFireActionBatch, новые тесты и три групповых native fixtures. FNS15 передаёт shot/rage/bag/mode, Doom death-tag и свет; обновлены wire goldens, ancestry/roundtrip/atomic rejection проверки.
- Родные Plant/Projectile/Zombie/Board остаются владельцами gameplay. Общий emitter использует канонические тип и motion; поддержаны Cold/Three/Rage stamp, горячие состояния и15 новых моделей снарядов. Native renderer принимает source additive SortingOrder относительно первого ключа.
- Host-authoritative ActivatePlant использует полную идентичность растения и существующую очередь намерений; реальный клик клиента по1043 проверен. Shared world placement разрешает подтверждённые внутренние spawn-пути без фиктивных родителей, сохраняя проверки платного размещения.
- Исправлены общие admission-механизмы: animation speed Fire, временные пределы многофазной стрельбы и source-контактный прямоугольник. Поведение групп теперь проходит без маскировки сетевых отказов.
- Doom death-tag потребляется однократно до рекурсивного взрыва. Свет1171 передаёт host-состояние в клиентскую анимацию. Fire runner1179 сохраняется в native lifecycle/snapshot и исключается из клеточного поиска/таргетинга.
- Нативный UI сохранён. Групповые fixtures проходили общий lobby/Ready/intro/chat и проверку порядка собственных карточек. Отдельная полная матрица случайного/раздельного выбора, всех сложностей и пингов в этой партии не выполнялась.

## Границы приёмки и оставшаяся работа

- Начальные растения создаются native factory на Day5; после setup fixtures не меняют HP/счётчики/позиции акторов. Fire использует начальные границы счётчиков50/100. Получение всех40 через оплату/рецепты/UI **Not executed**.
- Для1020/1096/1161 нет входящего рецепта; полное source-получение/разблокировка остаётся отдельно. Прямое создание в мире не доказывает завершённость получения.
- NORMAL-зомби используются как управляемые начальные цели. Это не приёмка source AI, брони, кампании, мини-игр, альманаха или магазина. Старый NPC-контракт/отложенная броня262 не повышены в статусе.
- Source particles198/199 отложены. Горячие FX берут общий native projectile age; точная фаза перезапуска FX при конверсии ещё не воспроизведена.
- Doom hooks для burned/mowed/typed death интегрированы; фактическое доказательство однократного взрыва здесь — смерть в NORMAL mind-controlled duel, остальные входы отдельно **Not executed**.
- Seed424242 в раннем Cold-прогоне не дал успешной ветки случайного гипноза; этот Failed-прогон сохранён. Положительная проверка использует424243 без переопределения RNG. Это не статистическая проверка вероятности.
- `git diff --check`: **Failed (exit2)** на уже имевшихся пробелах в Plant.cpp/Zombie.cpp и EOF пустых строках старых agent-документов; посторонний текст не очищался. Сборка и применимые тесты Passed.

## Доказательства и повторение

Корень: `C:\PvZ\stabledecompile-master`. Итоговый машинный аудит: `work/fusion/batch40-264/acceptance-final.json`; SHA guard: `verified-build.json`; unit log: `unit60k-final-tests.log` в той же папке.

- Cold: `work/fusion/batch40-264/cold/run_06049e3f1b0a48c894c8f3c34d66db28`.
- Shooter: `work/fusion/batch40-264/shooter/run_44f761fca7fd4fd1bda3837e1588cea1`.
- Fire: `work/fusion/batch40-264/fire/run_f7d5640f436d404583b293a00b2cf3b5`.
- Classic: `work/fusion/batch40-264/classic60k/run_0701a85a8b6447a584e309d7a65d1941`.

Для групп используется `tests/CoopSmoke/RunFusionBatch40Smoke.ps1 -Family Cold -Seed 424243 -TimeoutSeconds 480`; Shooter/Fire — соответствующий Family и Seed424242. Перед повтором проверить SHA/protocol/FNS guard и конкретные параметры самого runner. Classic — существующий `RunCoopSmoke.ps1 -Scenario Adventure8Soak -SkipIntro 0 -MinimumSnapshots 8 -TimeoutSeconds 180 -Background -HostWindowWidth 1066 -ClientWindowWidth 800` с отдельным ArtifactsRoot.

Root выполнил интеграцию, сборки и игровые запуски. Реальные исполнители engineer1_shooter, engineer2_fire, engineer3_projectile_art завершили свои модули/fixtures/review и заморозили файлы. Существующая незакоммиченная работа сохранена. Commit/push/release не выполнялись.

Следующий шаг полного проекта: выбрать следующую независимую партию30–40 из оставшихся растений с уже доказанными общими механизмами; пакетно подготовить код/графику, затем группами проверить новый EXE. Не возвращаться к исследованию всей игры или считать каталожные строки готовыми. Пробелы получения и тяжёлые зависимости вести отдельно, с явными границами подсчёта.

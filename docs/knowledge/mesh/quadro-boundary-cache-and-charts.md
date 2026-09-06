# Quadros: кэш тела, коррекция параметров и UV-адаптер

Продолжение реализации: [Подушка-2 — готовая квадро-сетка](pillow-2-cad-quadro.md)
и [Фен — согласование допусков и уточнение общих рёбер](hairdryer-cad-boundary-reconciliation.md).
Результаты 73/85 ниже относятся к прежнему строгому режиму; новый явно включаемый
режим подготовки проходит 85/85, сохраняя кэш CAD topology.

Дата: 2026-09-05. Статус: `Внедрено` — независимый boundary слой; legacy meshing не переключён.

## Жизненный цикл snapshot

`CSolid` владеет ленивым `quadro::BoundaryCache`:

```cpp
auto topology = solid.GetQuadroTopologySnapshot();
auto prepared = solid.PrepareQuadroBoundary(options);
auto input = quadro::BuildFaceBoundaryInput(prepared, faceId);
```

Topology создаётся при первом запросе и переиспользуется для неизменённого CAD body. Повторный запрос не обходит faces и не копирует BRep: сравниваются identity, location и orientation корневого `TopoDS_Shape` через `IsEqual`. Отдельно кэшируется последняя дискретизация со всеми её параметрами. Изменение плотности/шага перестраивает только дискретизацию. Ошибочный результат тоже кэшируется до изменения входа, чтобы повторный запрос не запускал ту же неуспешную работу.

`ReBuldMesh` пересоздаёт CSurfaceFace через InitSurfaces, поэтому кэш принципиально не привязан к адресам этих объектов. Его источник — `CSolid::m_Shape`, а не подготовленные ломаные временных граней. Изменение сетки, триангуляции, материала или текстуры не является изменением CAD topology.

Замена shape, transform с новым shape/location и reversal обнаруживаются автоматически при следующем запросе. Для будущих **in-place** изменений существующих TShape, кривых или tolerances обязательно вызвать `InvalidateQuadroBoundary()` перед изменением. Одна проверка IsEqual не обнаруживает изменения содержимого объекта при сохранённой identity. В текущих проверенных путях изменения тела заменяют shape; прямые in-place CAD-редакторы должны соблюдать этот контракт. Если редактируется разделяемый TShape, нужны copy-on-write или инвалидизация всех его владельцев.

Clear инвалидирует кэш. Сохранившие shared_ptr потребители продолжают видеть прежний неизменяемый snapshot; следующему запросу выдаётся новая версия. Snapshot не сериализуется в .dom3d: после загрузки он лениво строится один раз для загруженного тела. API кэша предназначен для потока-владельца CSolid; синхронизация конкурентных изменений CAD не добавлена.

## Коррекция соответствия параметров

В предыдущем запуске фена все 68 превышений `CadRepresentationMismatch` относились к внутренним узлам edges. Концы уже укладывались в CAD-бюджеты. Добавлена проверяемая локальная подгонка параметра вдоль **сохранённой pcurve данного occurrence**:

1. Вычислить nominal UV и residual относительно неизменяемого master XYZ.
2. Если residual превышает прежний бюджет, искать ближайшее положение на этой pcurve в ограниченной ячейке параметра вокруг nominal sample.
3. Для соседних samples интервалы не пересекаются, концы ребра фиксированы; порядок и ветвь seam не меняются.
4. Принять параметр только с последующей проверкой того же CAD budget. Оставшееся превышение остаётся ошибкой.

Master XYZ, CAD tolerances и исходные pcurves не изменяются. В JSON сохраняются `nominalParameter`, `nominalResidual`, итоговые `parameter`/`residual` и `parameterCorrected`. Опция `allowLocalParameterCorrection=false` оставляет прежний строгий режим. Это ограниченный parameter mapper, а не универсальный healing или доказательство глобальной биективности произвольной параметризации.

## Контракт UV-адаптера

`BuildFaceBoundaryInput` возвращает данные для **одного ограниченного UV chart**: loops с CAD WireId/ролью outer-hole, UV instances и их происхождением OccurrenceId/sample/NodeId. Он не вызывает quadrangulator.

- Контуры собираются по CAD wire order. XYZ-nearest joining не используется.
- Повторные seam occurrences сохраняются с разными UV instances и общими пространственными NodeIds.
- Угол polygon использует начало следующего occurrence; исходные endpoints остаются в snapshot. Промежуточные точки соединения проверяются на поверхности относительно локального бюджета CAD-вершины.
- Целочисленные сдвиги периодов согласуют непрерывное представление соседних occurrences.
- Outer определяется только CAD topology. Для hole выбирается единственный допустимый периодический образ внутри outer.
- Проверяются нулевые UV-сегменты, самопересечения, пересечения и вложенность holes. Пространственная ориентация face хранится отдельно; outer выдаётся против часовой стрелки, holes — по часовой.
- Non-zero winding без chart cut, degenerated pole, неоднозначный hole image или превышение бюджета проверки получают явную ошибку.

Обработка одной face не скрывает диагностируемые ошибки другой. Но face с несогласованным master/occurrence не получает ready. `ContainsUV` работает только для готового дискретного chart и исключает holes.

Это валидация дискретных контуров с проверками по пробам, не математическая сертификация точного покрытия криволинейной CAD-области. Полюсные patches, разрезы noncontractible wires, глобальная декомпозиция и solver составных сторон остаются отдельными последующими задачами.

## Код и проверки

- [QuadroBoundary.h](../../../src/solid/QuadroBoundary.h), [QuadroBoundary.cpp](../../../src/solid/QuadroBoundary.cpp) — кэш, master и mapper.
- [Solid.h](../../../src/solid/Solid.h), [Solid.cpp](../../../src/solid/Solid.cpp) — владение и API тела.
- [QuadroFaceBoundary.h](../../../src/solid/QuadroFaceBoundary.h), [QuadroFaceBoundary.cpp](../../../src/solid/QuadroFaceBoundary.cpp) — адаптер UV-областей.
- [QuadroBoundaryTests.cpp](../../../tests/QuadroBoundaryTests.cpp) — кэш, параметризация с несовпадающей скоростью, неизменность master, швы, holes и явный отказ полюса.
- [CatalogImportTests.cpp](../../../tests/CatalogImportTests.cpp), режим `--test-quadro-body-cache` — реальный ReBuldMesh на двух плотностях, повторные запросы, замена тела и инвалидизация.

```powershell
ctest --test-dir build -C Release -R 'QuadroBoundarySnapshot|QuadroBodyBoundaryCache' --output-on-failure
build/Release/CatalogImportTests.exe --snapshot-project-boundary output/pillow-diagnostics/pillow-2.dom3d 'Extrude Solid' output/boundary-stage2/pillow.json
build/Release/CatalogImportTests.exe --snapshot-project-boundary output/hairdryer-diagnostics/hairdryer.dom3d 'Imported STEP 4' output/boundary-stage2/hairdryer.json
```

В JSON дополнительно выводятся per-face `charts`, их loops/причины отказа. `chartsValidated` означает, что все faces прошли ограниченный single-chart adapter; `meshingConnected` остаётся false.

## Результаты текущего этапа

| Проверка | Подушка | Фен, Imported STEP 4 |
|---|---:|---:|
| CAD topology valid | Да | Да |
| Замкнутые wires по общим NodeIds | 18/18 | 88/88 |
| Готовые single-chart inputs | 18/18 | 73/85 |
| Превышения CAD residual после mapper | 0 | 59 (ранее 68) |

Mapper скорректировал параметры 64 samples фена, устранив 9 превышений без перемещения master XYZ и без изменения budgets. Для 59 оставшихся добавлена независимая диагностическая проекция на исходную поверхность, вне ограничения trim. В 54 случаях найденная OCCT проекция также имеет расстояние выше выбранного бюджета; в 5 — ниже. Максимальное отношение найденного расстояния к бюджету около 1.12802. Это свидетельство расхождения представлений при текущей строгой политике, а не доказательство невалидности CAD по всем правилам OCCT или гарантированного глобального минимума проекции. Поле `surfaceProjectionDistance` не используется для ослабления допуска или принятия boundary.

Грани фена, пока не прошедшие adapter: 2, 8, 9, 18, 21, 28, 29, 36, 46, 47, 49, 53. У F47 остаётся пересечение сегментов UV-контура; у остальных — остаточные `CadRepresentationMismatch`. Четыре ложных сообщения на F25/F30/F31/F37 устранены численно устойчивой проверкой ориентаций почти коллинеарных сегментов и отсечением по bounding boxes.

F15 теперь имеет корректные outer/hole loops: точка (U=4,V=15) входит в область, а (U=7.854,V=15.45) попадает в исключённое отверстие. Также проходят F50/F52 (ранее пустые), seam faces F51/F82/F83 и F84. Это результат нового boundary слоя, не готовая исправленная квадро-сетка.

[Validate-QuadroBoundaryReports.py](../../../tools/Validate-QuadroBoundaryReports.py) проверяет реальные отчёты: identity, ориентированный порядок, provenance, сохранение hole F15, корректные charts и отказ F47:

```powershell
python tools/Validate-QuadroBoundaryReports.py output/boundary-stage2
```

Сборки Release `Dom3D_Pro`, `QuadroBoundaryTests`, `CatalogImportTests` успешны. CTest: `QuadroBoundarySnapshot`, `QuadroBodyBoundaryCache`, `ToolsMesh3DFillContour`, `ExtrudeSlQuadro` — 4/4 passed. Тест на CSolid подтвердил один Capture после повторных ReBuldMesh на двух плотностях и запросов разных SamplingOptions. Legacy meshing фена даёт побайтно тот же лог, что baseline до внедрения нового слоя.

## Связанные материалы

- [Архитектура](quadro-boundary-architecture.md).
- [Первый этап реализации](quadro-boundary-implementation.md).
- [Исходные ошибки подушки и фена](quadro-boundary-failures.md).

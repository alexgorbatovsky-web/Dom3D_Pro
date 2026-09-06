# Quadros: реализация CAD boundary snapshot и master-дискретизации

Статус: `Внедрено` — независимый модуль подготовки и его тестовый запуск; подключение к meshing ещё не выполнено.  
Дата: 2026-09-05.

Эта статья фиксирует первый этап. Последующее развитие — [кэш тела, mapper и single-chart UV-адаптер](quadro-boundary-cache-and-charts.md); ниже сохранены исходные результаты 68 превышений и ограничения того этапа.

## Что находится в коде

[QuadroBoundary.h](../../../src/solid/QuadroBoundary.h) и [QuadroBoundary.cpp](../../../src/solid/QuadroBoundary.cpp) реализуют отдельный слой `quadro::BoundarySnapshot`. Модуль входит в исходники приложения и не зависит от CNet, SurfaceFace или quadrangulator.

```cpp
auto topology = quadro::BoundarySnapshot::Capture(cadFaces, bodyRevision);
// topology: CAD IDs, oriented occurrences, wires, pcurves, tolerances.
auto boundary = quadro::BoundarySnapshot::Prepare(*topology, samplingOptions);
// boundary: a new snapshot, master nodes and occurrence-specific UV views.
```

Capture не запускает дискретизацию или meshing. Все faces копируются одной OCCT-операцией, чтобы сохранить общие vertices/edges и изолировать snapshot от последующих изменений CAD. Геометрические handles наружу не выдаются; API возвращает `shared_ptr<const BoundarySnapshot>` и const-данные. EvaluatePCurve позволяет вычислять сохранённую кривую без доступа на запись.

Prepare создаёт новый snapshot. Общие CAD-вершины имеют единые NodeIds, общие edges — одну последовательность параметров и пространственных узлов. Reversed occurrences используют обратный порядок; у seam общие пространственные узлы и отдельные UV-представления. Замкнутые рёбра имеют единую фазу, у degenerated edges сохраняется UV-путь с одним пространственным якорем.

Поддержаны ограничения минимального числа сегментов и группы равенства counts отдельных edges. Уточнение монотонное и ограничено бюджетом. Требования длины/отклонения проверяются до публикации результата. `matchesBoundary` проверяет NodeIds, порядок и координаты результата относительно master; он не исправляет чужую границу и не переносит ряд донора в snapshot.

ID имеет смысл внутри своего snapshot. BodyRevision передаётся вызывающей стороной; numeric IDs не являются persistent naming между импортами или произвольными перестановками входных faces. Независимость пространственной master-дискретизации от порядка faces проверяется отдельно.

## Проверки и результаты на реальных моделях

Команда [CatalogImportTests](../../../tests/CatalogImportTests.cpp) загружает проект и вызывает только Capture/Prepare, без ReBuldMesh:

```powershell
build/Release/CatalogImportTests.exe --snapshot-project-boundary output/pillow-diagnostics/pillow-2.dom3d 'Extrude Solid' output/boundary-snapshot/pillow.json
build/Release/CatalogImportTests.exe --snapshot-project-boundary output/hairdryer-diagnostics/hairdryer.dom3d 'Imported STEP 4' output/boundary-snapshot/hairdryer.json
```

Каталог отчёта должен существовать. Отчёт пишется атомарно; при отказе готовности сохраняются диагностические данные. Параметры этого CLI: chordTolerance=0.01, maxSegmentLength=5, minimumSegments=8, maximumSegments=32768; это параметры нового sampler, не legacy normalized density.

| Модель | Faces | CAD vertices | CAD edges | Occurrences | Wires | Пространственные nodes | Topology valid | Discretization ready |
|---|---:|---:|---:|---:|---:|---:|---|---|
| Подушка-2 / Extrude Solid | 18 | 24 | 40 | 80 | 18 | 1120 | Да | Да |
| Фен / Imported STEP 4 | 85 | 134 | 220 | 440 | 88 | 4018 | Да | Нет — 68 проверок residual выше бюджета |

В обоих файлах все wires замкнуты по IDs и по endpoint NodeIds. У фена сохранены 8 seam occurrences (две стороны каждого из четырёх швов). Это подтверждает устранение неоднозначности представления в новом слое, но не исправление старых сеток: существующий meshing пока использует прежние границы.

68 замечаний фена — `CadRepresentationMismatch` на 12 faces. Максимальный residual около 0.00777558; максимальное отношение residual/budget около 1.14660. Это проверки master XYZ относительно `surface(pcurve(t))` даже при установленном CAD SameParameter. Допуск не расширяется автоматически ради успешного результата. Требуется отдельное исследование согласованности параметров и семантики CAD tolerance на этих рёбрах перед подключением к meshing.

Для конечной точки бюджет равен максимуму применимых vertex/edge/face tolerances + numericalTolerance; для внутреннего master sample — максимуму edge/face + numericalTolerance. В JSON сохранены значения допусков, residual и budget. Эта явно выбранная строгая политика не объявляется универсальной моделью всех погрешностей импорта. Новый отказ не равнозначен доказательству невалидности исходного CAD body.

## Автоматические проверки

[QuadroBoundaryTests.cpp](../../../tests/QuadroBoundaryTests.cpp), CTest `QuadroBoundarySnapshot`:

- 8 общих vertices / 12 edges / 24 occurrences коробки;
- непрерывность wires по identity до и после дискретизации;
- общие NodeIds и разные UV на швах цилиндра, тора и сферы;
- сохранение замкнутых и вырожденных рёбер;
- фиксация узлов при reversed occurrences;
- отклонение подменённого конца границы на 0.3;
- согласование counts и останов по лимиту;
- независимость пространственной дискретизации от порядка faces;
- изоляция от последующего изменения исходного CAD;
- отсутствие слияния совпадающих, но разных CAD bodies и разных размещений TShape;
- расхождение конца кривой на 0.003 внутри tolerance 0.01 допускается, вне tolerance 1e-7 — отклоняется;
- пустой body, null face и открытый wire не получают успешный статус.

Сборка Release `Dom3D_Pro`, `QuadroBoundaryTests` и `CatalogImportTests` успешна. `QuadroBoundarySnapshot`, `ToolsMesh3DFillContour`, `ExtrudeSlQuadro`: 3/3 passed. Вывод legacy meshing фена на density 0.5 побайтно совпал с диагностическим baseline до добавления модуля.

## Явные границы реализации

- Не изменены quadrangulator и действующие ветки согласования сеток. Новая команда не вызывает построение сеток.
- `discretizationReady` относится к master/occurrence samples, а не к готовому входу quadrangulator. JSON явно содержит `meshingConnected=false`, `chartsValidated=false`.
- Сохранены сырые CAD pcurves и переходы периодов между occurrences. Декомпозиция на charts, искусственные разрезы и доказательство покрытия области ещё не реализованы. Особенно важно не подать эти данные обратно в старый XYZ Join/PutOnSurface и не потерять seam.
- Рёбра без SameParameter/SameRange отклоняются: общий монотонный parameter mapper ещё не реализован. Геометрический healing не выполняется.
- Для counts реализованы равенства отдельных рёбер; solver сумм сегментов составных сторон и требования конкретных face builders — следующий слой.
- Chord error измеряется в четвертях каждого интервала; это адаптивная проверка по пробам, не математически гарантированная верхняя оценка для произвольного сплайна. У конечных сегментов отдельно учитывается бюджет CAD-якоря. Полная сертификация кривой требует более строгого sampler.
- UV residuals проверены в master samples. Межузловая UV-метрика, особые charts полюсов и качество будущих клеток пока не проверяются.
- В legacy UI новый слой ещё не включён. Следующий этап — разобрать CAD representation mismatches фена, затем написать face-boundary adapter и проверки chart domain перед изменением технологии квадро-сетки.

## Связанные материалы

- [Архитектура](quadro-boundary-architecture.md).
- [Ошибка и принятые выводы](quadro-boundary-failures.md).
- [Диагностика подушки](pillow-2-quadro-diagnostics.md).
- [Диагностика фена](hairdryer-quadro-diagnostics.md).

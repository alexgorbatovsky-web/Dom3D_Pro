"""Append user-facing C++ strings to the Dom3D XML language catalogs.

Existing hand-written entries are never replaced.  Auto-generated IDs are
stable (SHA-1 of the English source text), so the script can be rerun after UI
changes.  Russian translation is optional and uses the public Google
Translate endpoint; failed requests keep the English fallback and are
reported at the end.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import re
import time
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LANGUAGES = ROOT / "Languages"
STRING = re.compile(r'"(?:\\.|[^"\\])*"')
PLACEHOLDER = re.compile(r"%[1-9][0-9]*")
LOWERCASE_UI = {
    "none", "small", "medium", "large", "custom", "shaded", "wired",
    "wireframe", "default", "document", "architecture", "furniture",
    "surfaces", "solid", "curves", "sketch", "assemblies",
}
BUILTIN_UI = {
    "OK", "Cancel", "Apply", "Close", "Yes", "No", "Save", "Open",
    "Delete", "Reset", "Restore Defaults", "Discard", "Help", "Abort",
    "Retry", "Ignore", "Yes to All", "No to All",
}
RUSSIAN_OVERRIDES = {
    "OK": "OK",
    "Cancel": "Отмена",
    "Apply": "Применить",
    "Close": "Закрыть",
    "Yes": "Да",
    "No": "Нет",
    "Save": "Сохранить",
    "Open": "Открыть",
    "Delete": "Удалить",
    "Reset": "Сбросить",
    "Restore Defaults": "Восстановить значения по умолчанию",
    "Discard": "Не сохранять",
    "Help": "Справка",
    "Abort": "Прервать",
    "Retry": "Повторить",
    "Ignore": "Игнорировать",
    "Yes to All": "Да для всех",
    "No to All": "Нет для всех",
    "Kitchen Nika-260": "Кухня Ника-260",
    "Corner Kitchen": "Кухня угловая",
    "Left Run Length": "Длина левого крыла",
    "Right Run Length": "Длина правого крыла",
    "Kitchen Width": "Ширина кухни",
    "Base Height": "Высота нижних шкафов",
    "Base Depth": "Глубина нижних шкафов",
    "Upper Height": "Высота верхних шкафов",
    "Upper Depth": "Глубина верхних шкафов",
    "Worktop to Upper Gap": "Расстояние до верхних шкафов",
    "Worktop Thickness": "Толщина столешницы",
    "Worktop Front Radius": "Радиус передней кромки",
    "Leg Height": "Высота опор",
    "Worktop Material": "Материал столешницы",
    "Hardware Material": "Материал фурнитуры",
    "Lower Door 1 Angle": "Угол нижней двери 1",
    "Lower Door 4 Left Angle": "Угол левой нижней двери 4",
    "Lower Door 4 Right Angle": "Угол правой нижней двери 4",
    "Upper Door 1 Angle": "Угол верхней двери 1",
    "Upper Door 2 Angle": "Угол верхней двери 2",
    "Upper Door 3 Angle": "Угол верхней двери 3",
    "Upper Door 4 Left Angle": "Угол левой верхней двери 4",
    "Upper Door 4 Right Angle": "Угол правой верхней двери 4",
    "MDF Profile Milano": "МДФ профиль Милано",
    "Chipboard Panel": "Панель ДСП",
    "Knob": "Кнопка",
    "&New": "&Создать",
    "&Open...": "&Открыть...",
    "Open &Recent": "&Недавние файлы",
    "&Save": "&Сохранить",
    "Save &As...": "Сохранить &как...",
    "&Preferences...": "&Настройки...",
    "&Hot Keys...": "Горячие &клавиши...",
    "&Import...": "&Импорт...",
    "&Catalog...": "&Каталог...",
    "&Export...": "&Экспорт...",
    "E&xit": "В&ыход",
    "&Undo": "&Отменить",
    "&Redo": "&Повторить",
    "Architecture": "Архитектура",
    "Furniture": "Мебель",
    "Surfaces": "Поверхности",
    "Solid": "Твердотельное моделирование",
    "Curves": "Кривые",
    "Sketch": "Эскиз",
    "Assemblies": "Сборки",
    "Ready": "Готово",
    "Small": "Мелкая",
    "Medium": "Средняя",
    "Large": "Крупная",
    "Custom": "Пользовательская",
    "Add Ref image": "Добавить опорное изображение",
    "Zebra Analysis": "Анализ отражений (зебра)",
    "Solid Display": "Отображение тел",
    "Wired / Shaded": "Каркас / Заливка",
    "Orbit Mode": "Режим вращения камеры",
    "Transparent Solid Surfaces": "Прозрачные поверхности тел",
    "Search command...": "Поиск команды...",
    "Search commands...": "Поиск команд...",
    "Options of program": "Настройки программы",
    "Project saved": "Проект сохранён",
    "Project opened": "Проект открыт",
    "New project": "Новый проект",
    "Tool Options": "Параметры инструмента",
    "Property Panel": "Панель свойств",
    "Materials Library": "Библиотека материалов",
    "Scene Tree": "Дерево сцены",
}
STYLE_MARKERS = (
    "background:", "background-color:", "border:", "border-",
    "padding:", "padding-", "margin:", "margin-", "color:",
    "font-", "min-width:", "width:", "height:", "spacing:",
    "image:", "outline:", "rgba(", "QWidget", "QLabel", "QLineEdit",
    "QListWidget", "QPushButton", "QToolButton", "QTabBar", "QMenu",
    "QComboBox", "QTreeWidget", "QDialog",
)


def decode_cpp_string(token: str) -> str | None:
    try:
        return json.loads(token)
    except (ValueError, json.JSONDecodeError):
        return None


def cpp_strings(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8", errors="ignore")
    matches = list(STRING.finditer(text))
    result: list[str] = []
    index = 0
    while index < len(matches):
        current = matches[index]
        value = decode_cpp_string(current.group())
        if value is None:
            index += 1
            continue
        end = current.end()
        next_index = index + 1
        while next_index < len(matches):
            gap = text[end:matches[next_index].start()]
            gap = re.sub(r"/\*.*?\*/", "", gap, flags=re.S)
            gap = re.sub(r"//[^\n]*", "", gap)
            if gap.strip():
                break
            following = decode_cpp_string(matches[next_index].group())
            if following is None:
                break
            value += following
            end = matches[next_index].end()
            next_index += 1
        result.append(value)
        index = next_index
    return result


def is_user_facing(value: str) -> bool:
    value = value.strip()
    if len(value) < 2 or len(value) > 800 or not re.search(r"[A-Za-z]", value):
        return False
    if any(marker in value for marker in STYLE_MARKERS):
        return False
    if value.startswith((":/", "qrc:/", "http://", "https://", "#")):
        return False
    if value in {"true", "false", "UTF-8", "English.xml", "*.xml"}:
        return False
    if re.fullmatch(r"[A-Za-z0-9_./%+-]+\.(?:h|hpp|cpp|cxx|png|svg|ico|pdn|obj)", value):
        return False
    if re.fullmatch(r"[a-z][a-z0-9_.\-/]*", value) and value not in LOWERCASE_UI:
        return False
    if re.fullmatch(r"(?:\\[nsrt]|\[[^]]+\]|\(\?[^)]+\)|[.*+?^$|\\]+)+", value):
        return False
    if value.startswith(("ui/", "view/", "files/", "tools/", "material/")):
        return False
    return True


def collect_sources() -> list[str]:
    values: set[str] = set(BUILTIN_UI)
    for path in sorted((ROOT / "src").rglob("*")):
        if path.suffix.lower() not in {".cpp", ".h", ".hpp"}:
            continue
        for value in cpp_strings(path):
            normalized = value.strip()
            if is_user_facing(normalized):
                values.add(normalized)
    return sorted(values, key=lambda item: (item.casefold(), item))


def read_catalog(path: Path) -> tuple[ET.ElementTree, ET.Element, dict[str, str]]:
    tree = ET.parse(path)
    root = tree.getroot()
    values: dict[str, str] = {}
    for item in root.findall("TextItem"):
        identifier = item.findtext("ID", "").strip()
        if identifier:
            values[identifier] = item.findtext("Text", "")
    return tree, root, values


def append_item(root: ET.Element, identifier: str, value: str) -> None:
    item = ET.SubElement(root, "TextItem")
    ET.SubElement(item, "ID").text = identifier
    ET.SubElement(item, "Text").text = value


def auto_id(source: str) -> str:
    digest = hashlib.sha1(source.encode("utf-8")).hexdigest()[:14].upper()
    return f"Auto_{digest}"


def translate_one(source: str) -> str:
    query = urllib.parse.urlencode({
        "client": "gtx", "sl": "en", "tl": "ru", "dt": "t", "q": source,
    })
    url = "https://translate.googleapis.com/translate_a/single?" + query
    for attempt in range(4):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": "Dom3D localization tool"})
            with urllib.request.urlopen(request, timeout=30) as response:
                payload = json.loads(response.read().decode("utf-8"))
            translated = "".join(part[0] for part in payload[0] if part and part[0])
            if translated and sorted(PLACEHOLDER.findall(translated)) == sorted(PLACEHOLDER.findall(source)):
                return translated
        except Exception:
            if attempt == 3:
                break
            time.sleep(0.5 * (attempt + 1))
    return source


def write_catalog(tree: ET.ElementTree, path: Path) -> None:
    ET.indent(tree, space="  ")
    tree.write(path, encoding="UTF-8", xml_declaration=True, short_empty_elements=False)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--translate-russian", action="store_true")
    parser.add_argument("--workers", type=int, default=10)
    args = parser.parse_args()

    english_tree, english_root, english = read_catalog(LANGUAGES / "English.xml")
    russian_tree, russian_root, russian = read_catalog(LANGUAGES / "Russian.xml")
    english_sources = set(english.values())
    pending = [source for source in collect_sources() if source not in english_sources]

    translations: dict[str, str] = {}
    if args.translate_russian and pending:
        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.workers)) as pool:
            results = pool.map(translate_one, pending)
            translations = dict(zip(pending, results))

    for source in pending:
        identifier = auto_id(source)
        append_item(english_root, identifier, source)
        english[identifier] = source
        if identifier not in russian:
            append_item(russian_root, identifier, translations.get(source, source))
            russian[identifier] = translations.get(source, source)

    # Curated common UI terminology wins only for generated entries.  The
    # hand-maintained IDs at the beginning of the catalogs remain untouched.
    russian_elements = {
        item.findtext("ID", "").strip(): item.find("Text")
        for item in russian_root.findall("TextItem")
    }
    for identifier, source in english.items():
        if identifier.startswith("Auto_") and source in RUSSIAN_OVERRIDES:
            text_element = russian_elements.get(identifier)
            if text_element is not None:
                text_element.text = RUSSIAN_OVERRIDES[source]
                russian[identifier] = text_element.text

    # Backfill any IDs that existed on only one side without overwriting text.
    for identifier, source in english.items():
        if identifier not in russian:
            append_item(russian_root, identifier, translations.get(source, source))
    for identifier, source in russian.items():
        if identifier not in english:
            append_item(english_root, identifier, source)

    write_catalog(english_tree, LANGUAGES / "English.xml")
    write_catalog(russian_tree, LANGUAGES / "Russian.xml")
    untranslated = sum(
        1 for source in pending if translations.get(source, source) == source
    ) if args.translate_russian else len(pending)
    print(f"sources={len(english)} added={len(pending)} untranslated={untranslated}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

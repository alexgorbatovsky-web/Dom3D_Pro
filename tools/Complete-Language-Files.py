# -*- coding: utf-8 -*-
"""Complete Dom3D Pro XML language files from the English reference.

Existing translations are preserved. Missing entries are translated locally
with Argos Translate models and written in the exact English ID order.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import xml.etree.ElementTree as ET

import ctranslate2

os.environ.setdefault("ARGOS_CHUNK_TYPE", "MINISBD")
os.environ.setdefault("ARGOS_COMPUTE_TYPE", "int8")

from argostranslate import package  # noqa: E402


ROOT = Path(__file__).resolve().parents[1]
LANGUAGE_DIR = ROOT / "Languages"
CACHE_DIR = ROOT / "tmp" / "language-translation-cache"

LANGUAGES = {
    "ar": "Arabic.xml",
    "zh": "Chinese.xml",
    "fr": "French.xml",
    "de": "German.xml",
    "hi": "Hindi.xml",
    "it": "Itaian.xml",  # Existing filename is intentionally retained.
    "ja": "Japanese.xml",
    "ko": "Korean.xml",
    "pl": "Polish.xml",
    "es": "Spanish.xml",
    "tr": "Turkish.xml",
    "uk": "Ukrainian.xml",
}

# Core commands are reviewed translations. They override machine output so
# the most visible menus remain concise and use conventional CAD wording.
CORE_TRANSLATIONS = {
    "ar": {"User &Manual": "&دليل المستخدم", "&Save": "&حفظ", "Save &As...": "حفظ &باسم...", "&Open...": "&فتح...", "&Delete Selected": "&حذف المحدد", "&Delete selected": "&حذف المحدد", "&New": "&جديد", "&Import...": "&استيراد...", "&Export...": "&تصدير...", "&Preferences...": "&تفضيلات...", "&Undo": "&تراجع", "&Redo": "&إعادة", "E&xit": "&خروج", "&Hot Keys...": "&اختصارات لوحة المفاتيح...", "&About Dom-3D...": "&حول Dom-3D...", "&Catalog...": "&الكتالوج...", "&Support service": "&الدعم الفني", "Open &Recent": "فتح &الأخيرة", "&Select object": "&تحديد كائن", "&Greeting Box...": "&نافذة الترحيب..."},
    "zh": {"User &Manual": "用户&手册", "&Save": "&保存", "Save &As...": "另存&为...", "&Open...": "&打开...", "&Delete Selected": "&删除所选", "&Delete selected": "&删除所选", "&New": "&新建", "&Import...": "&导入...", "&Export...": "&导出...", "&Preferences...": "&首选项...", "&Undo": "&撤销", "&Redo": "&重做", "E&xit": "退&出", "&Hot Keys...": "&快捷键...", "&About Dom-3D...": "&关于 Dom-3D...", "&Catalog...": "&目录...", "&Support service": "&技术支持", "Open &Recent": "打开&最近文件", "&Select object": "&选择对象", "&Greeting Box...": "&欢迎窗口..."},
    "fr": {"User &Manual": "&Manuel utilisateur", "&Save": "&Enregistrer", "Save &As...": "Enregistrer &sous...", "&Open...": "&Ouvrir...", "&Delete Selected": "&Supprimer la sélection", "&Delete selected": "&Supprimer la sélection", "&New": "&Nouveau", "&Import...": "&Importer...", "&Export...": "&Exporter...", "&Preferences...": "&Préférences...", "&Undo": "&Annuler", "&Redo": "&Rétablir", "E&xit": "&Quitter", "&Hot Keys...": "&Raccourcis clavier...", "&About Dom-3D...": "&À propos de Dom-3D...", "&Catalog...": "&Catalogue...", "&Support service": "&Assistance", "Open &Recent": "Ouvrir les fichiers &récents", "&Select object": "&Sélectionner un objet", "&Greeting Box...": "&Fenêtre de bienvenue..."},
    "de": {"User &Manual": "&Benutzerhandbuch", "&Save": "&Speichern", "Save &As...": "Speichern &unter...", "&Open...": "&Öffnen...", "&Delete Selected": "Auswahl &löschen", "&Delete selected": "Auswahl &löschen", "&New": "&Neu", "&Import...": "&Importieren...", "&Export...": "&Exportieren...", "&Preferences...": "&Einstellungen...", "&Undo": "&Rückgängig", "&Redo": "&Wiederholen", "E&xit": "&Beenden", "&Hot Keys...": "&Tastenkürzel...", "&About Dom-3D...": "&Über Dom-3D...", "&Catalog...": "&Katalog...", "&Support service": "&Kundendienst", "Open &Recent": "&Zuletzt geöffnet", "&Select object": "Objekt &auswählen", "&Greeting Box...": "&Begrüßungsfenster..."},
    "hi": {"User &Manual": "&उपयोगकर्ता पुस्तिका", "&Save": "&सहेजें", "Save &As...": "इस रूप में &सहेजें...", "&Open...": "&खोलें...", "&Delete Selected": "&चयनित हटाएँ", "&Delete selected": "&चयनित हटाएँ", "&New": "&नया", "&Import...": "&आयात...", "&Export...": "&निर्यात...", "&Preferences...": "&प्राथमिकताएँ...", "&Undo": "&पूर्ववत", "&Redo": "&फिर करें", "E&xit": "&बाहर निकलें", "&Hot Keys...": "&कुंजीपटल शॉर्टकट...", "&About Dom-3D...": "Dom-3D के &बारे में...", "&Catalog...": "&कैटलॉग...", "&Support service": "&सहायता सेवा", "Open &Recent": "&हाल की फ़ाइलें खोलें", "&Select object": "&वस्तु चुनें", "&Greeting Box...": "&स्वागत विंडो..."},
    "it": {"User &Manual": "&Manuale utente", "&Save": "&Salva", "Save &As...": "Salva &con nome...", "&Open...": "&Apri...", "&Delete Selected": "&Elimina selezionati", "&Delete selected": "&Elimina selezionati", "&New": "&Nuovo", "&Import...": "&Importa...", "&Export...": "&Esporta...", "&Preferences...": "&Preferenze...", "&Undo": "&Annulla", "&Redo": "&Ripeti", "E&xit": "&Esci", "&Hot Keys...": "&Tasti di scelta rapida...", "&About Dom-3D...": "&Informazioni su Dom-3D...", "&Catalog...": "&Catalogo...", "&Support service": "&Assistenza", "Open &Recent": "Apri &recenti", "&Select object": "&Seleziona oggetto", "&Greeting Box...": "&Finestra di benvenuto..."},
    "ja": {"User &Manual": "&ユーザーマニュアル", "&Save": "&保存", "Save &As...": "名前を付けて&保存...", "&Open...": "&開く...", "&Delete Selected": "&選択項目を削除", "&Delete selected": "&選択項目を削除", "&New": "&新規", "&Import...": "&インポート...", "&Export...": "&エクスポート...", "&Preferences...": "&設定...", "&Undo": "&元に戻す", "&Redo": "&やり直し", "E&xit": "&終了", "&Hot Keys...": "&ショートカットキー...", "&About Dom-3D...": "Dom-3Dに&ついて...", "&Catalog...": "&カタログ...", "&Support service": "&サポート", "Open &Recent": "&最近使ったファイル", "&Select object": "&オブジェクトを選択", "&Greeting Box...": "&ようこそ..."},
    "ko": {"User &Manual": "&사용자 설명서", "&Save": "&저장", "Save &As...": "다른 이름으로 &저장...", "&Open...": "&열기...", "&Delete Selected": "&선택 항목 삭제", "&Delete selected": "&선택 항목 삭제", "&New": "&새로 만들기", "&Import...": "&가져오기...", "&Export...": "&내보내기...", "&Preferences...": "&환경 설정...", "&Undo": "&실행 취소", "&Redo": "&다시 실행", "E&xit": "&끝내기", "&Hot Keys...": "&단축키...", "&About Dom-3D...": "Dom-3D &정보...", "&Catalog...": "&카탈로그...", "&Support service": "&기술 지원", "Open &Recent": "&최근 파일 열기", "&Select object": "&객체 선택", "&Greeting Box...": "&환영 창..."},
    "pl": {"User &Manual": "&Podręcznik użytkownika", "&Save": "&Zapisz", "Save &As...": "Zapisz &jako...", "&Open...": "&Otwórz...", "&Delete Selected": "&Usuń zaznaczone", "&Delete selected": "&Usuń zaznaczone", "&New": "&Nowy", "&Import...": "&Importuj...", "&Export...": "&Eksportuj...", "&Preferences...": "&Preferencje...", "&Undo": "&Cofnij", "&Redo": "&Ponów", "E&xit": "&Zakończ", "&Hot Keys...": "&Skróty klawiszowe...", "&About Dom-3D...": "&O programie Dom-3D...", "&Catalog...": "&Katalog...", "&Support service": "&Pomoc techniczna", "Open &Recent": "Otwórz &ostatnie", "&Select object": "&Wybierz obiekt", "&Greeting Box...": "&Okno powitalne..."},
    "es": {"User &Manual": "&Manual de usuario", "&Save": "&Guardar", "Save &As...": "Guardar &como...", "&Open...": "&Abrir...", "&Delete Selected": "&Eliminar seleccionados", "&Delete selected": "&Eliminar seleccionados", "&New": "&Nuevo", "&Import...": "&Importar...", "&Export...": "&Exportar...", "&Preferences...": "&Preferencias...", "&Undo": "&Deshacer", "&Redo": "&Rehacer", "E&xit": "&Salir", "&Hot Keys...": "&Atajos de teclado...", "&About Dom-3D...": "&Acerca de Dom-3D...", "&Catalog...": "&Catálogo...", "&Support service": "&Soporte técnico", "Open &Recent": "Abrir &recientes", "&Select object": "&Seleccionar objeto", "&Greeting Box...": "&Ventana de bienvenida..."},
    "tr": {"User &Manual": "&Kullanıcı kılavuzu", "&Save": "&Kaydet", "Save &As...": "&Farklı kaydet...", "&Open...": "&Aç...", "&Delete Selected": "&Seçileni sil", "&Delete selected": "&Seçileni sil", "&New": "&Yeni", "&Import...": "&İçe aktar...", "&Export...": "&Dışa aktar...", "&Preferences...": "&Tercihler...", "&Undo": "&Geri al", "&Redo": "&Yinele", "E&xit": "&Çıkış", "&Hot Keys...": "&Klavye kısayolları...", "&About Dom-3D...": "Dom-3D &hakkında...", "&Catalog...": "&Katalog...", "&Support service": "&Teknik destek", "Open &Recent": "&Son kullanılanları aç", "&Select object": "&Nesne seç", "&Greeting Box...": "&Karşılama penceresi..."},
    "uk": {"User &Manual": "&Посібник користувача", "&Save": "&Зберегти", "Save &As...": "Зберегти &як...", "&Open...": "&Відкрити...", "&Delete Selected": "&Видалити вибране", "&Delete selected": "&Видалити вибране", "&New": "&Новий", "&Import...": "&Імпорт...", "&Export...": "&Експорт...", "&Preferences...": "&Налаштування...", "&Undo": "&Скасувати", "&Redo": "&Повторити", "E&xit": "&Вийти", "&Hot Keys...": "&Гарячі клавіші...", "&About Dom-3D...": "&Про Dom-3D...", "&Catalog...": "&Каталог...", "&Support service": "&Технічна підтримка", "Open &Recent": "Відкрити &останні", "&Select object": "&Вибрати об’єкт", "&Greeting Box...": "&Вітальне вікно..."},
}

ADDITIONAL_REVIEWED_TRANSLATIONS = {
    "ar": {"Other Params...": "معلمات أخرى...", "Other Parameters": "معلمات أخرى", "Open facade, construction, hardware and material parameters": "فتح معلمات الواجهة والبنية والتجهيزات والمواد"},
    "zh": {"Other Params...": "其他参数...", "Other Parameters": "其他参数", "Open facade, construction, hardware and material parameters": "打开立面、结构、五金件和材料参数"},
    "fr": {"Other Params...": "Autres paramètres…", "Other Parameters": "Autres paramètres", "Open facade, construction, hardware and material parameters": "Ouvrir les paramètres de façade, de construction, de quincaillerie et de matériaux"},
    "de": {"Other Params...": "Weitere Parameter…", "Other Parameters": "Weitere Parameter", "Open facade, construction, hardware and material parameters": "Parameter für Front, Konstruktion, Beschläge und Materialien öffnen"},
    "hi": {"Other Params...": "अन्य पैरामीटर...", "Other Parameters": "अन्य पैरामीटर", "Open facade, construction, hardware and material parameters": "अग्रभाग, संरचना, हार्डवेयर और सामग्री के पैरामीटर खोलें"},
    "it": {"Other Params...": "Altri parametri...", "Other Parameters": "Altri parametri", "Open facade, construction, hardware and material parameters": "Apri i parametri di facciata, struttura, ferramenta e materiali"},
    "ja": {"Other Params...": "その他のパラメータ...", "Other Parameters": "その他のパラメータ", "Open facade, construction, hardware and material parameters": "ファサード、構造、金具、マテリアルのパラメータを開く"},
    "ko": {"Other Params...": "기타 매개변수...", "Other Parameters": "기타 매개변수", "Open facade, construction, hardware and material parameters": "파사드, 구조, 하드웨어 및 재료 매개변수 열기"},
    "pl": {"Other Params...": "Inne parametry...", "Other Parameters": "Inne parametry", "Open facade, construction, hardware and material parameters": "Otwórz parametry frontu, konstrukcji, okuć i materiałów"},
    "es": {"Other Params...": "Otros parámetros...", "Other Parameters": "Otros parámetros", "Open facade, construction, hardware and material parameters": "Abrir los parámetros de fachada, construcción, herrajes y materiales"},
    "tr": {"Other Params...": "Diğer parametreler...", "Other Parameters": "Diğer parametreler", "Open facade, construction, hardware and material parameters": "Cephe, yapı, donanım ve malzeme parametrelerini aç"},
    "uk": {"Other Params...": "Інші параметри...", "Other Parameters": "Інші параметри", "Open facade, construction, hardware and material parameters": "Відкрити параметри фасаду, конструкції, фурнітури та матеріалів"},
}

SCENE_PRINT_SOURCES = (
    "Print Scene...",
    "Export Scene to PDF...",
    "Print Scene",
    "Could not capture the 3D scene.",
    "Print Preview — 3D Scene",
    "Export Scene to PDF",
    "Scene PDF saved",
    "Dom3D Scene",
)
SCENE_PRINT_REVIEWED = {
    "ar": dict(zip(SCENE_PRINT_SOURCES, ("طباعة المشهد...", "تصدير المشهد إلى PDF...", "طباعة المشهد", "تعذر التقاط المشهد ثلاثي الأبعاد.", "معاينة الطباعة — مشهد ثلاثي الأبعاد", "تصدير المشهد إلى PDF", "تم حفظ ملف PDF للمشهد", "مشهد Dom3D"))),
    "zh": dict(zip(SCENE_PRINT_SOURCES, ("打印场景...", "将场景导出为 PDF...", "打印场景", "无法捕获 3D 场景。", "打印预览 — 3D 场景", "将场景导出为 PDF", "场景 PDF 已保存", "Dom3D 场景"))),
    "fr": dict(zip(SCENE_PRINT_SOURCES, ("Imprimer la scène…", "Exporter la scène en PDF…", "Imprimer la scène", "Impossible de capturer la scène 3D.", "Aperçu avant impression — Scène 3D", "Exporter la scène en PDF", "PDF de la scène enregistré", "Scène Dom3D"))),
    "de": dict(zip(SCENE_PRINT_SOURCES, ("Szene drucken…", "Szene als PDF exportieren…", "Szene drucken", "Die 3D-Szene konnte nicht erfasst werden.", "Druckvorschau — 3D-Szene", "Szene als PDF exportieren", "Szenen-PDF gespeichert", "Dom3D-Szene"))),
    "hi": dict(zip(SCENE_PRINT_SOURCES, ("दृश्य प्रिंट करें...", "दृश्य को PDF में निर्यात करें...", "दृश्य प्रिंट करें", "3D दृश्य कैप्चर नहीं किया जा सका।", "प्रिंट पूर्वावलोकन — 3D दृश्य", "दृश्य को PDF में निर्यात करें", "दृश्य PDF सहेजा गया", "Dom3D दृश्य"))),
    "it": dict(zip(SCENE_PRINT_SOURCES, ("Stampa scena...", "Esporta scena in PDF...", "Stampa scena", "Impossibile acquisire la scena 3D.", "Anteprima di stampa — Scena 3D", "Esporta scena in PDF", "PDF della scena salvato", "Scena Dom3D"))),
    "ja": dict(zip(SCENE_PRINT_SOURCES, ("シーンを印刷...", "シーンをPDFにエクスポート...", "シーンを印刷", "3Dシーンをキャプチャできませんでした。", "印刷プレビュー — 3Dシーン", "シーンをPDFにエクスポート", "シーンのPDFを保存しました", "Dom3Dシーン"))),
    "ko": dict(zip(SCENE_PRINT_SOURCES, ("장면 인쇄...", "장면을 PDF로 내보내기...", "장면 인쇄", "3D 장면을 캡처할 수 없습니다.", "인쇄 미리 보기 — 3D 장면", "장면을 PDF로 내보내기", "장면 PDF가 저장되었습니다", "Dom3D 장면"))),
    "pl": dict(zip(SCENE_PRINT_SOURCES, ("Drukuj scenę...", "Eksportuj scenę do PDF...", "Drukuj scenę", "Nie udało się przechwycić sceny 3D.", "Podgląd wydruku — Scena 3D", "Eksportuj scenę do PDF", "Zapisano PDF sceny", "Scena Dom3D"))),
    "es": dict(zip(SCENE_PRINT_SOURCES, ("Imprimir escena...", "Exportar escena a PDF...", "Imprimir escena", "No se pudo capturar la escena 3D.", "Vista previa de impresión — Escena 3D", "Exportar escena a PDF", "PDF de la escena guardado", "Escena Dom3D"))),
    "tr": dict(zip(SCENE_PRINT_SOURCES, ("Sahneyi yazdır...", "Sahneyi PDF'ye aktar...", "Sahneyi yazdır", "3D sahne yakalanamadı.", "Baskı önizleme — 3D sahne", "Sahneyi PDF'ye aktar", "Sahne PDF'si kaydedildi", "Dom3D sahnesi"))),
    "uk": dict(zip(SCENE_PRINT_SOURCES, ("Друк сцени...", "Експорт сцени в PDF...", "Друк сцени", "Не вдалося отримати зображення 3D-сцени.", "Попередній перегляд — 3D-сцена", "Експорт сцени в PDF", "PDF сцени збережено", "Сцена Dom3D"))),
}

FORMAT_TOKEN = (
    r"%[-+0#]*\d*(?:\.\d+)?(?:hh|h|ll|l|j|z|t|L)?"
    r"[diuoxXfFeEgGaAcspn]"
)
PROTECTED_RE = re.compile(
    rf"https?://[^\s]+|{FORMAT_TOKEN}|%L?\d+|\{{[^{{}}]+\}}|<[^<>]+>"
)
PLACEHOLDER_RE = re.compile(rf"{FORMAT_TOKEN}|%L?\d+|\{{[^{{}}]+\}}")
MARKER_RE = re.compile(r"XPH([A-Z])X")
PRESERVED_EXISTING_IDS = {
    "LanguageName", "MenuFile", "MenuView", "MenuTools", "MenuEdit",
    "MenuHelp", "MenuLanguage", "HelpTopics", "LanguageChangedStatus",
}


def read_items(path: Path) -> list[tuple[str, str]]:
    root = ET.parse(path).getroot()
    result = []
    for item in root.findall("TextItem"):
        item_id = item.findtext("ID")
        text = item.findtext("Text")
        if item_id is None or text is None:
            raise ValueError(f"Malformed TextItem in {path}")
        result.append((item_id, text))
    return result


def shield(text: str) -> tuple[str, list[str]]:
    values: list[str] = []

    def replace(match: re.Match[str]) -> str:
        index = len(values)
        if index >= 26:
            raise ValueError(f"Too many protected tokens in: {text}")
        values.append(match.group(0))
        return f"XPH{chr(ord('A') + index)}X"

    return PROTECTED_RE.sub(replace, text), values


def unshield(text: str, values: list[str]) -> str:
    found = MARKER_RE.findall(text)
    expected = [chr(ord("A") + index) for index in range(len(values))]
    if found != expected:
        raise ValueError(f"Translation damaged protected tokens: {found} != {expected}")
    for index, value in enumerate(values):
        text = text.replace(f"XP{chr(ord('A') + index)}X", value)
        text = text.replace(f"XPH{chr(ord('A') + index)}X", value)
    return text


def translate_with_split_tokens(source: str, model, translator) -> str:
    """Fallback that translates text fragments and interleaves exact tokens."""
    source = source.replace("&", "")
    tokens = PROTECTED_RE.findall(source)
    parts = PROTECTED_RE.split(source)
    translated_parts: list[str] = []
    for part in parts:
        if not part:
            translated_parts.append("")
            continue
        leading = part[: len(part) - len(part.lstrip())]
        trailing = part[len(part.rstrip()) :]
        core = part.strip()
        if not core:
            translated_parts.append(part)
            continue
        encoded = model.tokenizer.encode(core)
        target_prefix = [[model.target_prefix]] if model.target_prefix else None
        result = translator.translate_batch(
            [encoded], target_prefix=target_prefix, replace_unknowns=True,
            beam_size=2, num_hypotheses=1,
            max_decoding_length=max(12, min(96, len(encoded) * 4 + 8)),
        )[0]
        value = model.tokenizer.decode(result.hypotheses[0]).strip()
        if model.target_prefix and value.startswith(model.target_prefix):
            value = value[len(model.target_prefix) :].lstrip()
        translated_parts.append(leading + value + trailing)
    output = translated_parts[0]
    for index, token in enumerate(tokens):
        output += token + translated_parts[index + 1]
    return output


def preserve_mnemonic(source: str, translated: str) -> str:
    source_count = source.count("&")
    translated_count = translated.count("&")
    while translated_count < source_count:
        match = re.search(r"\w", translated)
        if not match:
            break
        translated = translated[: match.start()] + "&" + translated[match.start() :]
        translated_count += 1
    return translated


def should_retry_unchanged(source: str, target: str) -> bool:
    if source != target:
        return False
    if "&" in source:
        return True
    words = re.findall(r"[A-Za-z]{2,}", source)
    return (len(words) >= 4 and "\\" not in source and "==" not in source
            and not source.startswith("<"))


def is_excessive(source: str, target: str) -> bool:
    return len(target) > max(120, len(source) * 4 + 30)


def is_technical_literal(source: str) -> bool:
    return ("\\" in source or "==" in source or "||" in source
            or source.startswith("{") or source.startswith("</"))


def normalize_positional_boundaries(source: str, text: str) -> str:
    # A translated word must not become part of a Qt placeholder ("%2G...").
    # Real printf tokens are protected separately before translation.
    positional = [
        token for token in PLACEHOLDER_RE.findall(source)
        if re.fullmatch(r"%L?\d+", token)
    ]
    for token in positional:
        text = re.sub(re.escape(token) + r"(?=[A-Za-z])", token + " ", text)
    return text


def ensure_model(language_code: str):
    installed = [
        value
        for value in package.get_installed_packages()
        if value.from_code == "en" and value.to_code == language_code
    ]
    if installed:
        return installed[-1]
    package.update_package_index()
    available = [
        value
        for value in package.get_available_packages()
        if value.from_code == "en" and value.to_code == language_code
    ]
    if not available:
        raise RuntimeError(f"Argos model en -> {language_code} is unavailable")
    selected = available[-1]
    print(f"Downloading en -> {language_code} model {selected.package_version}...", flush=True)
    package.install_from_path(selected.download())
    return next(
        value
        for value in package.get_installed_packages()
        if value.from_code == "en" and value.to_code == language_code
    )


def translate_missing(language_code: str, texts: list[str]) -> dict[str, str]:
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    cache_path = CACHE_DIR / f"en-{language_code}.json"
    cache: dict[str, str] = {}
    if cache_path.exists():
        cache = json.loads(cache_path.read_text(encoding="utf-8"))
    cache = {
        source: target for source, target in cache.items()
        if sorted(PLACEHOLDER_RE.findall(source))
        == sorted(PLACEHOLDER_RE.findall(target))
        and "XPH" not in target
        and "&quot;" not in target
        and "&amp;" not in target
        and not should_retry_unchanged(source, target)
        and not is_excessive(source, target)
    }
    missing = [text for text in texts if text not in cache]
    if not missing:
        return cache

    model = ensure_model(language_code)
    translator = ctranslate2.Translator(
        str(model.package_path / "model"),
        device="cpu",
        compute_type="int8",
        inter_threads=2,
        intra_threads=0,
    )
    batch_size = 96
    for start in range(0, len(missing), batch_size):
        originals = missing[start : start + batch_size]
        prepared: list[str] = []
        protected: list[list[str]] = []
        for text in originals:
            value, tokens = shield(text.replace("&", ""))
            prepared.append(value)
            protected.append(tokens)
        tokenized = [model.tokenizer.encode(value) for value in prepared]
        target_prefix = None
        if model.target_prefix:
            target_prefix = [[model.target_prefix]] * len(tokenized)
        results = translator.translate_batch(
            tokenized,
            target_prefix=target_prefix,
            replace_unknowns=True,
            max_batch_size=32,
            batch_type="examples",
            beam_size=2,
            num_hypotheses=1,
            max_decoding_length=128,
        )
        for source, tokens, result in zip(originals, protected, results):
            value = model.tokenizer.decode(result.hypotheses[0]).strip()
            if model.target_prefix and value.startswith(model.target_prefix):
                value = value[len(model.target_prefix) :].lstrip()
            try:
                value = unshield(value, tokens)
                if (sorted(PLACEHOLDER_RE.findall(source))
                        != sorted(PLACEHOLDER_RE.findall(value))):
                    raise ValueError("Placeholder changed after unshielding")
                if "XPH" in value:
                    raise ValueError("Unresolved protection marker")
            except ValueError:
                value = translate_with_split_tokens(source, model, translator)
            if should_retry_unchanged(source, value):
                value = translate_with_split_tokens(source, model, translator)
            if is_excessive(source, value):
                value = (source if is_technical_literal(source)
                         else translate_with_split_tokens(source, model, translator))
            if is_excessive(source, value):
                value = source
            value = value.replace("&quot;", '"').replace("&amp;", "&")
            value = preserve_mnemonic(source, value)
            value = normalize_positional_boundaries(source, value)
            if not value:
                raise ValueError(f"Empty translation for {source!r}")
            cache[source] = value
        cache_path.write_text(
            json.dumps(cache, ensure_ascii=False, indent=2), encoding="utf-8"
        )
        done = min(start + batch_size, len(missing))
        print(f"  {language_code}: {done}/{len(missing)}", flush=True)
    return cache


def write_language(path: Path, english: list[tuple[str, str]], translations: dict[str, str]):
    root = ET.Element("ClassArray.TextItem")
    for item_id, source in english:
        item = ET.SubElement(root, "TextItem")
        ET.SubElement(item, "ID").text = item_id
        ET.SubElement(item, "Text").text = translations[source]
    ET.indent(root, space="  ")
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def validate(path: Path, english: list[tuple[str, str]]):
    localized = read_items(path)
    english_ids = [item_id for item_id, _ in english]
    localized_ids = [item_id for item_id, _ in localized]
    if localized_ids != english_ids:
        raise ValueError(f"ID order mismatch in {path}")
    if len(localized_ids) != len(set(localized_ids)):
        raise ValueError(f"Duplicate IDs in {path}")
    for (item_id, source), (_, target) in zip(english, localized):
        if not target.strip():
            raise ValueError(f"Empty translation {item_id} in {path}")
        if sorted(PLACEHOLDER_RE.findall(source)) != sorted(PLACEHOLDER_RE.findall(target)):
            raise ValueError(f"Placeholder mismatch for {item_id} in {path}")
        if "XPH" in target:
            raise ValueError(f"Unresolved protection marker for {item_id} in {path}")


def process(language_code: str, filename: str, english: list[tuple[str, str]]):
    path = LANGUAGE_DIR / filename
    existing_items = dict(read_items(path))
    missing_sources = [
        source for item_id, source in english
        if item_id not in existing_items
    ]
    print(f"{filename}: translating {len(missing_sources)} missing entries", flush=True)
    machine = translate_missing(language_code, missing_sources)
    reviewed = {
        **CORE_TRANSLATIONS.get(language_code, {}),
        **ADDITIONAL_REVIEWED_TRANSLATIONS.get(language_code, {}),
        **SCENE_PRINT_REVIEWED.get(language_code, {}),
    }
    translations = {
        source: (reviewed[source] if source in reviewed
                 else existing_items[item_id]
                 if item_id in existing_items
                 else machine.get(source, source))
        for item_id, source in english
    }
    write_language(path, english, translations)
    validate(path, english)
    print(f"{filename}: complete ({len(english)} entries)", flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--languages",
        nargs="*",
        choices=sorted(LANGUAGES),
        default=list(LANGUAGES),
    )
    args = parser.parse_args()
    english = read_items(LANGUAGE_DIR / "English.xml")
    if len(english) < 2509:
        raise ValueError(f"English reference is unexpectedly incomplete: {len(english)}")
    for language_code in args.languages:
        process(language_code, LANGUAGES[language_code], english)


if __name__ == "__main__":
    main()

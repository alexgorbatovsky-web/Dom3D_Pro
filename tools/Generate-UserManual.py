from pathlib import Path
import math

from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "output" / "pdf" / "User Manual Dom3D Pro.pdf"
ASSETS = ROOT / "docs" / "manual" / "assets"

PAGE_W, PAGE_H = A4
MARGIN_X = 48
TOP = PAGE_H - 52
BOTTOM = 46
NAVY = colors.HexColor("#07152B")
BLUE = colors.HexColor("#1677FF")
CYAN = colors.HexColor("#58A6FF")
INK = colors.HexColor("#172033")
MUTED = colors.HexColor("#5D687A")
PALE = colors.HexColor("#EFF5FF")
LINE = colors.HexColor("#D6DEEA")
GREEN = colors.HexColor("#1D9B64")
ORANGE = colors.HexColor("#E68A19")


def register_fonts():
    fonts = Path("C:/Windows/Fonts")
    pdfmetrics.registerFont(TTFont("Manual", str(fonts / "arial.ttf")))
    pdfmetrics.registerFont(TTFont("ManualBold", str(fonts / "arialbd.ttf")))
    pdfmetrics.registerFont(TTFont("ManualItalic", str(fonts / "ariali.ttf")))


def wrap(text, font, size, width):
    words = text.split()
    lines = []
    line = ""
    for word in words:
        candidate = word if not line else f"{line} {word}"
        if pdfmetrics.stringWidth(candidate, font, size) <= width:
            line = candidate
        else:
            if line:
                lines.append(line)
            line = word
    if line:
        lines.append(line)
    return lines


class Manual:
    def __init__(self, filename):
        self.c = canvas.Canvas(str(filename), pagesize=A4, pageCompression=1)
        self.c.setTitle("Руководство пользователя Dom3D Pro")
        self.c.setAuthor("Dom3D Pro")
        self.c.setSubject("3D-моделирование, чертежи, печать и PDF")
        self.page = 0
        self.y = TOP

    def new_page(self, title, section="РУКОВОДСТВО ПОЛЬЗОВАТЕЛЯ"):
        if self.page:
            self.c.showPage()
        self.page += 1
        self.c.setFillColor(NAVY)
        self.c.rect(0, PAGE_H - 34, PAGE_W, 34, stroke=0, fill=1)
        self.c.setFillColor(CYAN)
        self.c.setFont("ManualBold", 8)
        self.c.drawString(MARGIN_X, PAGE_H - 22, section)
        self.c.setFillColor(INK)
        self.c.setFont("ManualBold", 23)
        self.c.drawString(MARGIN_X, PAGE_H - 72, title)
        self.c.setStrokeColor(BLUE)
        self.c.setLineWidth(2)
        self.c.line(MARGIN_X, PAGE_H - 83, MARGIN_X + 52, PAGE_H - 83)
        self.y = PAGE_H - 108
        bookmark = f"page-{self.page}"
        self.c.bookmarkPage(bookmark)
        self.c.addOutlineEntry(title, bookmark, level=0, closed=False)

    def footer(self):
        self.c.setStrokeColor(LINE)
        self.c.setLineWidth(0.5)
        self.c.line(MARGIN_X, 33, PAGE_W - MARGIN_X, 33)
        self.c.setFont("Manual", 8)
        self.c.setFillColor(MUTED)
        self.c.drawString(MARGIN_X, 20, "Dom3D Pro")
        self.c.drawRightString(PAGE_W - MARGIN_X, 20, str(self.page))

    def heading(self, text, size=14, color=INK, gap=7):
        self.c.setFillColor(color)
        self.c.setFont("ManualBold", size)
        self.c.drawString(MARGIN_X, self.y, text)
        self.y -= size + gap

    def paragraph(self, text, width=None, size=10.2, leading=14.2, color=INK,
                  x=MARGIN_X, gap=8, font="Manual"):
        width = width or PAGE_W - 2 * MARGIN_X
        self.c.setFont(font, size)
        self.c.setFillColor(color)
        for line in wrap(text, font, size, width):
            self.c.drawString(x, self.y, line)
            self.y -= leading
        self.y -= gap

    def bullets(self, items, width=None, size=9.8, leading=13.6, gap=4):
        width = width or PAGE_W - 2 * MARGIN_X - 18
        for item in items:
            self.c.setFillColor(BLUE)
            self.c.circle(MARGIN_X + 3, self.y + 3, 2.2, stroke=0, fill=1)
            self.c.setFillColor(INK)
            self.c.setFont("Manual", size)
            lines = wrap(item, "Manual", size, width)
            for index, line in enumerate(lines):
                self.c.drawString(MARGIN_X + 15, self.y, line)
                self.y -= leading
            self.y -= gap

    def callout(self, title, text, tone="blue"):
        palette = {
            "blue": (PALE, BLUE),
            "green": (colors.HexColor("#EAF8F2"), GREEN),
            "orange": (colors.HexColor("#FFF4E5"), ORANGE),
        }
        fill, accent = palette[tone]
        lines = wrap(text, "Manual", 9.4, PAGE_W - 2 * MARGIN_X - 34)
        h = 32 + len(lines) * 12.5
        top = self.y
        self.c.setFillColor(fill)
        self.c.setStrokeColor(accent)
        self.c.roundRect(MARGIN_X, top - h, PAGE_W - 2 * MARGIN_X, h, 7,
                         stroke=1, fill=1)
        self.c.setFillColor(accent)
        self.c.setFont("ManualBold", 9.7)
        self.c.drawString(MARGIN_X + 14, top - 18, title)
        self.c.setFillColor(INK)
        self.c.setFont("Manual", 9.4)
        ty = top - 34
        for line in lines:
            self.c.drawString(MARGIN_X + 14, ty, line)
            ty -= 12.5
        self.y = top - h - 10

    def key_row(self, items):
        x = MARGIN_X
        for key, label in items:
            kw = max(34, pdfmetrics.stringWidth(key, "ManualBold", 9) + 18)
            self.c.setFillColor(NAVY)
            self.c.roundRect(x, self.y - 4, kw, 23, 5, stroke=0, fill=1)
            self.c.setFillColor(colors.white)
            self.c.setFont("ManualBold", 9)
            self.c.drawCentredString(x + kw / 2, self.y + 3, key)
            x += kw + 7
            self.c.setFillColor(MUTED)
            self.c.setFont("Manual", 8.5)
            self.c.drawString(x, self.y + 3, label)
            x += pdfmetrics.stringWidth(label, "Manual", 8.5) + 19
        self.y -= 33

    def image(self, name, max_w, max_h, caption=None, x=None):
        path = ASSETS / name
        with Image.open(path) as im:
            iw, ih = im.size
        scale = min(max_w / iw, max_h / ih)
        w, h = iw * scale, ih * scale
        x = x if x is not None else (PAGE_W - w) / 2
        y = self.y - h
        self.c.setFillColor(colors.white)
        self.c.setStrokeColor(LINE)
        self.c.roundRect(x - 4, y - 4, w + 8, h + 8, 5, stroke=1, fill=1)
        self.c.drawImage(str(path), x, y, width=w, height=h,
                         preserveAspectRatio=True, mask="auto")
        self.y = y - 10
        if caption:
            self.c.setFillColor(MUTED)
            self.c.setFont("ManualItalic", 8.3)
            self.c.drawCentredString(PAGE_W / 2, self.y, caption)
            self.y -= 14

    def two_columns(self, left_title, left_items, right_title, right_items):
        gap = 18
        col_w = (PAGE_W - 2 * MARGIN_X - gap) / 2
        top = self.y
        heights = []
        for items in (left_items, right_items):
            count = sum(len(wrap(x, "Manual", 9.1, col_w - 30)) for x in items)
            heights.append(45 + count * 12.3 + len(items) * 6)
        h = max(heights)
        for column, (title, items) in enumerate(((left_title, left_items),
                                                  (right_title, right_items))):
            x = MARGIN_X + column * (col_w + gap)
            self.c.setFillColor(colors.HexColor("#F7F9FC"))
            self.c.setStrokeColor(LINE)
            self.c.roundRect(x, top - h, col_w, h, 7, stroke=1, fill=1)
            self.c.setFillColor(BLUE)
            self.c.setFont("ManualBold", 11)
            self.c.drawString(x + 13, top - 21, title)
            ty = top - 41
            for item in items:
                self.c.setFillColor(BLUE)
                self.c.circle(x + 15, ty + 3, 1.8, stroke=0, fill=1)
                self.c.setFillColor(INK)
                self.c.setFont("Manual", 9.1)
                for line in wrap(item, "Manual", 9.1, col_w - 32):
                    self.c.drawString(x + 24, ty, line)
                    ty -= 12.3
                ty -= 6
        self.y = top - h - 12

    def end_page(self):
        self.footer()

    def cover(self):
        self.page = 1
        self.c.setFillColor(NAVY)
        self.c.rect(0, 0, PAGE_W, PAGE_H, stroke=0, fill=1)
        self.c.setStrokeColor(colors.HexColor("#17365F"))
        self.c.setLineWidth(0.7)
        for offset in range(-400, 900, 32):
            self.c.line(offset, 0, offset + 540, PAGE_H)
        self.c.setFillColor(BLUE)
        self.c.roundRect(54, PAGE_H - 176, 82, 82, 13, stroke=0, fill=1)
        self.c.setStrokeColor(colors.white)
        self.c.setLineWidth(2.2)
        self.c.rect(77, PAGE_H - 151, 36, 36, stroke=1, fill=0)
        self.c.line(77, PAGE_H - 133, 95, PAGE_H - 120)
        self.c.line(113, PAGE_H - 133, 95, PAGE_H - 120)
        self.c.line(95, PAGE_H - 120, 95, PAGE_H - 105)
        self.c.setFillColor(colors.white)
        self.c.setFont("ManualBold", 35)
        self.c.drawString(54, PAGE_H - 245, "Dom3D")
        self.c.setFillColor(CYAN)
        self.c.drawString(183, PAGE_H - 245, "Pro")
        self.c.setFillColor(colors.white)
        self.c.setFont("ManualBold", 23)
        self.c.drawString(54, PAGE_H - 292, "Руководство пользователя")
        self.c.setFillColor(colors.HexColor("#BBD3F8"))
        self.c.setFont("Manual", 13)
        self.c.drawString(54, PAGE_H - 325,
                          "3D-моделирование  |  CAD / CAM / CAE  |  Drafting")
        self.c.setStrokeColor(BLUE)
        self.c.setLineWidth(3)
        self.c.line(54, 130, PAGE_W - 54, 130)
        self.c.setFillColor(colors.white)
        self.c.setFont("Manual", 10)
        self.c.drawString(54, 103, "Редакция 1.0")
        self.c.drawRightString(PAGE_W - 54, 103, "2026")
        self.c.setFillColor(colors.HexColor("#8DA8CC"))
        self.c.setFont("Manual", 8.5)
        self.c.drawString(54, 76,
                          "Для актуальной версии Dom3D Pro. Открывается клавишей F1.")

    def finish(self):
        self.end_page()
        self.c.save()


def build_manual():
    register_fonts()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    m = Manual(OUTPUT)
    m.cover()

    m.new_page("Содержание", "БЫСТРЫЙ СТАРТ")
    contents = [
        ("1", "Главное окно и организация модулей", "3"),
        ("2", "Проекты: создание, открытие, сохранение", "4"),
        ("3", "Управление сценой и выбор объектов", "5"),
        ("4", "Перемещение, вращение, масштаб и Gizmo", "6"),
        ("5", "Architecture и Furniture", "7"),
        ("6", "Solid: создание и редактирование тел", "8"),
        ("7", "Surfaces и Curves", "9"),
        ("8", "Sketch и Assemblies", "10"),
        ("9", "Mesh 3D и сетка Quadro", "11"),
        ("10", "Материалы, отображение и рендер", "12"),
        ("11", "Drafting: листы и инструменты", "13"),
        ("12", "Drafting: виды модели, размеры и текст", "14"),
        ("13", "Предпросмотр, печать, плоттер и PDF", "15"),
        ("14", "Настройки, автосохранение и горячие клавиши", "16"),
        ("15", "Форматы, восстановление и диагностика", "17"),
        ("16", "Краткая памятка", "18"),
    ]
    y = m.y
    for number, title, page in contents:
        m.c.setFillColor(BLUE)
        m.c.setFont("ManualBold", 9.5)
        m.c.drawString(MARGIN_X, y, number.zfill(2))
        m.c.setFillColor(INK)
        m.c.setFont("Manual", 10.1)
        m.c.drawString(MARGIN_X + 29, y, title)
        m.c.setStrokeColor(LINE)
        m.c.setDash(1, 2)
        start = MARGIN_X + 35 + pdfmetrics.stringWidth(title, "Manual", 10.1)
        if start < PAGE_W - MARGIN_X - 25:
            m.c.line(start + 5, y + 2, PAGE_W - MARGIN_X - 22, y + 2)
        m.c.setDash()
        m.c.setFont("ManualBold", 9.5)
        m.c.drawRightString(PAGE_W - MARGIN_X, y, page)
        y -= 30
    m.y = y
    m.callout("Совет", "Начните с разделов 1-4, затем переходите к нужному модулю. В любой момент нажмите F1, чтобы снова открыть это руководство.", "green")
    m.end_page()

    m.new_page("Главное окно", "1  |  ИНТЕРФЕЙС")
    m.paragraph("Рабочее пространство Dom3D Pro разделено на постоянные панели и модульную область. При смене вкладки меняются только профильные инструменты; дерево сцены, свойства и материалы остаются доступны.")
    m.image("drafting-workspace.png", PAGE_W - 2 * MARGIN_X, 275,
            "Интерфейс Dom3D Pro на примере модуля Drafting")
    m.two_columns("Постоянные области", [
        "Главное меню: проект, вид, инструменты, рендер и справка.",
        "Главная панель: режим выбора, оси, сетка, прозрачность и стиль отображения.",
        "Scene Tree: видимость, выбор и иерархия объектов документа.",
        "Status Bar: подсказки, ход операции и сообщения об ошибках.",
    ], "Модули", [
        "Architecture, Furniture, Surfaces, Solid и Curves.",
        "Mesh 3D для полигональной обработки и Low Poly.",
        "Sketch и Assemblies для профилей и сборок.",
        "Drafting для листов, видов, размеров, печати и PDF.",
    ])
    m.end_page()

    m.new_page("Проекты и сохранение", "2  |  ОСНОВНОЙ ПРОЦЕСС")
    m.heading("Рекомендуемая последовательность")
    m.bullets([
        "Создайте новый проект через File > New или откройте существующий файл .dom3d.",
        "Сформируйте геометрию в подходящем модуле. Следите за сообщениями в строке состояния.",
        "Сохраните проект сочетанием Ctrl+S. При первом сохранении укажите имя и папку.",
        "Для обмена используйте File > Import и File > Export. Исходный .dom3d хранит структуру проекта, параметры и листы Drafting.",
    ])
    m.key_row([("Ctrl+N", "новый"), ("Ctrl+O", "открыть"), ("Ctrl+S", "сохранить")])
    m.heading("Автосохранение")
    m.paragraph("В Preferences можно включить Auto Save и задать Time - интервал автоматического сохранения. Автосохранение дополняет обычное сохранение, но не отменяет его перед крупными изменениями или импортом.")
    m.callout("Надёжная работа", "Перед булевыми операциями, перестроением сложной сетки и импортом больших STEP-файлов сохраните проект вручную. Используйте понятные версии имени: Chair_01.dom3d, Chair_02.dom3d.", "orange")
    m.heading("Отмена и повтор")
    m.paragraph("Команды Edit > Undo и Edit > Redo возвращают предыдущее состояние документа. Название ожидаемой операции отображается непосредственно в меню.")
    m.key_row([("Ctrl+Z", "отменить"), ("Ctrl+Y", "повторить"), ("Delete", "удалить")])
    m.end_page()

    m.new_page("Сцена и выбор объектов", "3  |  НАВИГАЦИЯ")
    m.heading("Управление камерой")
    m.two_columns("Мышь", [
        "Колесо - приблизить или отдалить сцену.",
        "Средняя кнопка с движением - панорамирование.",
        "Режим Orbit - вращение камеры вокруг модели.",
        "All Scene вписывает видимые объекты в окно.",
    ], "Режимы вида", [
        "CAD и Architectural orbit задают разное поведение вращения.",
        "Orthographic отключает перспективное уменьшение.",
        "Wired / Shaded переключает каркас и заливку.",
        "XY Plane View ориентирует камеру по рабочей плоскости.",
    ])
    m.heading("Выбор")
    m.paragraph("На главной панели включите тип сущности: Obj, Face, Edge или Pt. Щелчок выбирает ближайшую допустимую сущность, а рамка позволяет выбрать группу. Выбранные элементы подсвечиваются и появляются в свойствах.")
    m.bullets([
        "Obj - целый объект или параметрическое тело.",
        "Face - грань твердого тела; применяется для выдавливания, смещения и уклона.",
        "Edge - кромка; применяется для фаски, скругления и извлечения кривой.",
        "Pt - вершина или управляющая точка.",
    ])
    m.callout("Если объект не выбирается", "Проверьте тип выбора, видимость слоя и объекта в Scene Tree, а также не перекрывает ли нужную геометрию другая поверхность.", "blue")
    m.end_page()

    m.new_page("Преобразования и Gizmo", "4  |  РЕДАКТИРОВАНИЕ")
    m.heading("Перемещение, вращение и масштаб")
    m.paragraph("Выберите один или несколько объектов, затем активируйте Move, Rotate или Scale. Для точного результата используйте диалоговые команды: числовые значения надёжнее свободного движения мышью.")
    m.two_columns("Интерактивно", [
        "Стрелки Gizmo перемещают вдоль X, Y и Z.",
        "Кольца вращают вокруг соответствующей оси.",
        "Центральный маркер перемещает свободно.",
        "Маркер масштаба изменяет размер относительно базовой точки.",
    ], "Точно", [
        "Precise Move задаёт вектор или две точки.",
        "Precise Rotate задаёт ось, центр и угол.",
        "Precise Scale задаёт коэффициенты либо требуемый размер.",
        "P2P переносит выбранную точку объекта в целевую точку.",
    ])
    m.heading("Копирование, зеркало и группы")
    m.bullets([
        "Linked Clone создаёт связанную копию, когда изменения должны повторяться.",
        "Mirror отражает объект относительно выбранной плоскости.",
        "Create Group объединяет элементы в иерархию; Ungroup снимает объединение.",
        "Assemblies предназначены для функциональных сборок и параметрической мебели.",
    ])
    m.callout("Базовая точка", "При масштабировании и вращении заранее выбирайте осмысленную базовую точку: угол детали, центр отверстия или монтажную ось.", "green")
    m.end_page()

    m.new_page("Architecture и Furniture", "5  |  ПАРАМЕТРИЧЕСКИЕ ОБЪЕКТЫ")
    m.heading("Architecture")
    m.paragraph("Модуль содержит базовые архитектурные объекты Room, Window и Door. После размещения изменяйте размеры и параметры в панели свойств. Проёмы и зависимые поверхности перестраиваются вместе с исходным объектом.")
    m.heading("Furniture")
    m.paragraph("В библиотеке доступны стулья, столы, шкафы, витрины, ящики, фасады и кухонные секции. Эти элементы создаются как параметрические объекты: удобнее менять ширину, высоту, глубину и материалы, чем редактировать каждую грань вручную.")
    m.bullets([
        "Chair / Chair Simple - быстрые варианты стула.",
        "Cabinet, Advanced Cabinet и Showcase - корпусная мебель.",
        "Drawer Box, Single Drawer и Single Facade - элементы наполнения.",
        "Kitchen Nika 260 и Kitchen Corner - кухонные сборки.",
    ])
    m.heading("Работа со свойствами")
    m.paragraph("Выберите объект, измените параметр и примените изменения. Если результат не подходит, используйте Cancel или Undo. Для повторяющихся изделий сначала настройте один экземпляр, затем создайте копии.")
    m.callout("Материалы", "Материал можно назначать выбранному объекту или отдельной грани. Для фасадов и столешниц удобно создавать документные копии материалов и сохранять их вместе с проектом.", "blue")
    m.end_page()

    m.new_page("Solid", "6  |  ТВЁРДОТЕЛЬНОЕ МОДЕЛИРОВАНИЕ")
    m.image("solid-model.png", PAGE_W - 2 * MARGIN_X, 215,
            "Твердое тело и его контурные рёбра")
    m.two_columns("Создание", [
        "Box, Cylinder, Sphere, Torus, Prism и Polyhedron.",
        "Extrude из замкнутого эскиза или выбранной грани.",
        "Sweep по траектории и Sweep Two Rails.",
        "Frame, Wire, Beam и Thick Solid.",
    ], "Редактирование", [
        "Boolean: объединение, вычитание и пересечение.",
        "Fillet и Chamfer для рёбер.",
        "Extrude Face, Offset Face и Draft.",
        "Shell, Sheet Bend и Trim инструментами плоскости, эскиза или поверхности.",
    ])
    m.heading("Boss / Pocket")
    m.paragraph("Создайте замкнутый эскиз на грани тела и выберите операцию Boss для выступа либо Pocket для кармана. Задайте глубину и угол уклона. Эскиз должен лежать на актуальной грани перестроенного тела.")
    m.callout("Проверка операции", "Если выступ или карман не строится, обновите сцену, проверьте замкнутость профиля и принадлежность эскиза текущей поверхности. После перестроения исходная грань может быть заменена новой.", "orange")
    m.end_page()

    m.new_page("Surfaces и Curves", "7  |  ПОВЕРХНОСТИ И КРИВЫЕ")
    m.heading("Surfaces")
    m.bullets([
        "Plane создаёт опорную плоскость.",
        "Ruled соединяет две направляющие линейчатой поверхностью.",
        "Loft строит форму по последовательности сечений.",
        "Sweep Two Rails ведёт профиль по двум рельсам.",
        "Four Splines создаёт поверхность по четырём граничным кривым.",
        "Join, Reverse Normals и Surface of Revolution завершают обработку.",
    ])
    m.heading("Curves")
    m.two_columns("Создать", [
        "Polyline, B-Spline, Draw Spline, Bezier и NURBS.",
        "Пересечение плоскостей и поверхностей.",
        "Проекция кривой на поверхность.",
        "Извлечение кромки поверхности.",
    ], "Изменить", [
        "Edit Point и параметры NURBS.",
        "Fillet, Join, Split и Extend.",
        "Trim by Plane и Simplify by Point.",
        "Reverse для смены направления.",
    ])
    m.callout("Качество поверхности", "Для устойчивого Loft и Sweep используйте согласованное направление кривых и сопоставимое число характерных точек. При скручивании сначала выполните Reverse у одной из направляющих.", "green")
    m.end_page()

    m.new_page("Sketch и Assemblies", "8  |  ПРОФИЛИ И СБОРКИ")
    m.heading("Sketch")
    m.paragraph("Эскиз задаёт плоский профиль для выдавливания, выреза, вращения и других операций. Создавайте его на рабочей плоскости или на плоской грани тела. Для твердотельной операции внешний контур должен быть замкнут.")
    m.bullets([
        "Выберите плоскость или грань и нажмите New Sketch.",
        "Постройте линии, дуги и замкнутые контуры.",
        "Проверьте отсутствие случайных разрывов и дублирующихся сегментов.",
        "Завершите редактирование и выберите Solid Extrude, Boss / Pocket либо другую операцию.",
    ])
    m.heading("Assemblies")
    m.paragraph("Сборки хранят взаимосвязанные детали как единый функциональный объект. Используйте Scene Tree для контроля состава и видимости. Для локальной правки выбирайте нужный дочерний элемент, для перемещения изделия - корневую сборку.")
    m.callout("Эскиз после перестроения", "Если операция зависит от грани, а тело было перестроено, убедитесь, что ссылка эскиза указывает на новую актуальную грань. Это особенно важно для Pocket и Boss.", "orange")
    m.end_page()

    m.new_page("Mesh 3D и Quadro", "9  |  ПОЛИГОНАЛЬНАЯ СЕТКА")
    m.image("quadro-openings.png", PAGE_W - 2 * MARGIN_X, 245,
            "Равномерная сетка Quadro вокруг проёмов")
    m.paragraph("Solid Low Poly строит полигональное представление тела. Параметр Density (normalized) управляет плотностью, Mesh Quadro формирует преимущественно четырёхугольную сетку, а Mesh Quadro Hole SLX добавляет устойчивый воротник вокруг отверстий.")
    m.image("quadro-collar.png", 320, 185,
            "Воротник локализует переход сетки у малого отверстия")
    m.callout("Выбор плотности", "Оценивайте не только число полигонов, но и равномерность шага. На торцах и вокруг отверстий избегайте длинных узких граней. Для малых отверстий включайте Hole SLX.", "green")
    m.end_page()

    m.new_page("Материалы и визуализация", "10  |  ВНЕШНИЙ ВИД")
    m.heading("Материалы")
    m.paragraph("Materials Library содержит готовые шейдеры. Выберите объект или грань, затем примените материал. Команды контекстного меню позволяют применить, редактировать, дублировать в документ и удалить документный материал.")
    m.heading("Отображение сцены")
    m.bullets([
        "Color, Weight и Style меняют способ отображения линий и поверхностей.",
        "Draw Edges и Open Edges помогают анализировать границы и разрывы.",
        "Transparent Solid Surfaces показывает внутреннюю геометрию.",
        "Zebra Analysis помогает оценивать плавность сопряжения поверхностей.",
        "Lighting и Background Color настраивают читаемость модели.",
    ])
    m.heading("Рендер")
    m.two_columns("Blender Cycles", [
        "Фотореалистичный внешний рендер.",
        "Подходит для материалов, света и презентаций.",
        "Результат можно открыть через View Last Render.",
    ], "Dom3D Native Raytrace", [
        "Встроенный быстрый трассировщик.",
        "Удобен для предварительной оценки сцены.",
        "Последний результат сохраняется для повторного просмотра.",
    ])
    m.callout("Перед рендером", "Проверьте нормали поверхностей, материалы, положение камеры, фон и освещение. Скрытые в Scene Tree объекты не должны случайно участвовать в кадре.", "blue")
    m.end_page()

    m.new_page("Drafting: листы", "11  |  ЧЕРТЕЖИ")
    m.image("drawing-parameters.png", 300, 225,
            "Параметры нового чертежа")
    m.paragraph("Кнопка Добавить чертёж создаёт новый лист. Задайте имя, формат A4-A0, книжную или альбомную ориентацию, тип основной надписи и масштаб. Каждый лист отображается отдельной вкладкой.")
    m.bullets([
        "Белая область - бумага; светло-серая область вокруг - монтажный стол.",
        "Вписать лист показывает бумагу целиком.",
        "Средняя кнопка перемещает лист в двух направлениях; колесо масштабирует.",
        "Удалить лист удаляет текущий чертёж вместе с его элементами.",
        "Правая вертикальная панель содержит собственные инструменты Drafting.",
    ])
    m.callout("Масштаб листа", "Масштаб в параметрах чертежа применяется к размерным значениям и основной надписи. Размер бумаги при печати остаётся физически точным.", "blue")
    m.end_page()

    m.new_page("Drafting: элементы и виды", "12  |  ЧЕРТЕЖИ")
    m.image("text-dialog.png", 300, 250,
            "Текст: содержимое, шрифт, высота и поворот")
    m.two_columns("Нарисовать", [
        "Линия, прямоугольник и эллипс.",
        "Текст с выбором шрифта, высоты и угла.",
        "Размер с привязкой к опорным точкам элементов.",
        "Выбор, перемещение и удаление элементов.",
    ], "Вид модели", [
        "Front, Top, Right и Isometric.",
        "Точная векторная проекция Open Cascade.",
        "Опциональные скрытые линии.",
        "Вид можно двигать и удалить; он удерживается внутри печатной области.",
    ])
    m.heading("Ассоциативные размеры")
    m.paragraph("Начальная и конечная точки размера должны привязаться к элементам чертежа или вершинам вида модели. При перемещении исходного элемента размер следует за ним; при удалении источника зависимый размер удаляется.")
    m.key_row([("Delete", "удалить выбранное"), ("MMB", "панорама"), ("Wheel", "масштаб")])
    m.callout("Размещение видов", "Сначала создайте основные ортогональные виды, выровняйте их, затем добавляйте размеры и поясняющий текст. Изометрический вид обычно размещают в свободном углу листа.", "green")
    m.end_page()

    m.new_page("Печать, плоттер и PDF", "13  |  ВЫВОД ЧЕРТЕЖА")
    m.image("print-preview.png", PAGE_W - 2 * MARGIN_X, 255,
            "Просмотр перед печатью")
    m.heading("Порядок вывода")
    m.bullets([
        "Нажмите Просмотр печати и убедитесь, что рамка, штамп, виды и размеры полностью находятся на листе.",
        "Для физического устройства выберите Печать / плоттер, затем формат бумаги, ориентацию и драйвер.",
        "Для электронного документа нажмите PDF и укажите имя файла.",
        "После экспорта откройте PDF и проверьте читаемость текста и отсутствие обрезанных видов.",
    ])
    m.image("drafting-pdf.png", 320, 180,
            "Векторный PDF сохраняет тонкие линии и размеры")
    m.callout("Важно для плоттера", "Формат и ориентация драйвера должны совпадать с параметрами листа Drafting. Отключите автоматическое растягивание драйвера, если требуется печать в точном масштабе.", "orange")
    m.end_page()

    m.new_page("Настройки и горячие клавиши", "14  |  ПЕРСОНАЛИЗАЦИЯ")
    m.heading("Preferences")
    m.bullets([
        "Auto Save включает периодическое сохранение, Time задаёт интервал.",
        "Параметры вида управляют сеткой, плоскостью XY, осями и фоном.",
        "Режим орбиты CAD или Architectural выбирается под тип работы.",
        "Язык интерфейса меняется через Help > Language.",
    ])
    m.heading("Основные сочетания")
    rows = [
        ("F1", "Открыть руководство пользователя"),
        ("Ctrl+N / Ctrl+O", "Новый проект / открыть проект"),
        ("Ctrl+S", "Сохранить проект"),
        ("Ctrl+I / Ctrl+E", "Импорт / экспорт"),
        ("Ctrl+Z / Ctrl+Y", "Отмена / повтор"),
        ("Ctrl+A", "Выбрать все видимые объекты"),
        ("Delete", "Удалить выбранные объекты или элементы Drafting"),
    ]
    y = m.y
    for key, desc in rows:
        m.c.setFillColor(PALE)
        m.c.setStrokeColor(LINE)
        m.c.roundRect(MARGIN_X, y - 9, 118, 28, 5, stroke=1, fill=1)
        m.c.setFillColor(BLUE)
        m.c.setFont("ManualBold", 9.5)
        m.c.drawCentredString(MARGIN_X + 59, y, key)
        m.c.setFillColor(INK)
        m.c.setFont("Manual", 10)
        m.c.drawString(MARGIN_X + 135, y, desc)
        y -= 39
    m.y = y - 4
    m.callout("Настройка клавиш", "Откройте File > Hot Keys, чтобы посмотреть назначенные команды и адаптировать сочетания под свой рабочий процесс.", "blue")
    m.end_page()

    m.new_page("Форматы и диагностика", "15  |  ОБМЕН ДАННЫМИ")
    m.heading("Основной формат")
    m.paragraph(".dom3d - рабочий формат проекта Dom3D Pro. Он хранит объекты, параметры, структуру, настройки вида и данные Drafting. Для продолжения редактирования всегда сохраняйте исходный проект именно в этом формате.")
    m.heading("Импорт и экспорт")
    m.two_columns("CAD", [
        "STEP / STP для обмена твердыми телами.",
        "IGES для кривых и поверхностей.",
        "Проверяйте единицы измерения после импорта.",
    ], "Полигональные данные", [
        "OBJ для сеток и материалов.",
        "Low Poly перед экспортом снижает сложность.",
        "Проверяйте нормали и открытые кромки.",
    ])
    m.heading("Если результат неожиданен")
    m.bullets([
        "Смотрите последнюю строку состояния и текст предупреждения - он часто содержит конкретную причину.",
        "Выполните View > Update Scene и повторно выберите исходную грань или профиль.",
        "Проверьте видимость и тип выбора, замкнутость эскиза, ориентацию кривых и валидность тела.",
        "Сохраните отдельную копию проекта перед воспроизведением ошибки.",
    ])
    m.callout("Обращение в поддержку", "Приложите файл .dom3d, последовательность действий, скриншот и значения параметров. Чем меньше тестовый файл, тем быстрее можно найти причину.", "green")
    m.end_page()

    m.new_page("Краткая памятка", "16  |  ЕЖЕДНЕВНАЯ РАБОТА")
    m.two_columns("Перед моделированием", [
        "Выберите модуль и режим сущности.",
        "Определите рабочую плоскость и единицы.",
        "Сохраните новый проект.",
        "Настройте автосохранение.",
    ], "Перед сложной операцией", [
        "Проверьте выбранные грани и контуры.",
        "Убедитесь, что профиль замкнут.",
        "Сохраните контрольную версию.",
        "Следите за строкой состояния.",
    ])
    m.two_columns("Перед чертежом", [
        "Уточните формат, ориентацию и масштаб.",
        "Разместите ортогональные виды.",
        "Добавьте размеры с привязкой.",
        "Проверьте штамп и тексты.",
    ], "Перед печатью", [
        "Откройте предварительный просмотр.",
        "Проверьте границы печатной области.",
        "Согласуйте формат драйвера.",
        "Проверьте готовый PDF.",
    ])
    m.heading("Главное правило")
    m.callout("Сначала геометрия - потом оформление", "Стройте устойчивую параметрическую модель, проверяйте перестроение, затем создавайте виды, размеры и выводите чертёж. Так изменения в проекте остаются предсказуемыми.", "blue")
    m.heading("Справка")
    m.paragraph("Нажмите F1 из любого модуля Dom3D Pro. Руководство откроется в системной программе просмотра PDF.")
    m.finish()
    print(OUTPUT)


if __name__ == "__main__":
    build_manual()

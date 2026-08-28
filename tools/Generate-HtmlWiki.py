#!/usr/bin/env python3
"""Build a dependency-free local HTML Wiki from docs/knowledge Markdown."""

from __future__ import annotations

import argparse
import html
import json
import re
from pathlib import Path
from urllib.parse import quote


HEADING_RE = re.compile(r"^(#{1,6})\s+(.+?)\s*$")
LIST_RE = re.compile(r"^\s*([-*+] |\d+[.)] )(.+)$")
TABLE_SEPARATOR_RE = re.compile(r"^\s*\|?\s*:?-{3,}:?\s*(?:\|\s*:?-{3,}:?\s*)+\|?\s*$")


def page_id(relative_path: Path) -> str:
    without_suffix = relative_path.with_suffix("").as_posix()
    return "home" if without_suffix.lower() == "readme" else without_suffix.lower()


def title_from_markdown(text: str, fallback: str) -> str:
    for line in text.splitlines():
        match = HEADING_RE.match(line)
        if match and len(match.group(1)) == 1:
            return match.group(2).strip()
    return fallback


def anchor_id(text: str) -> str:
    value = re.sub(r"[^\w\-\u0400-\u04ff]+", "-", text.lower(), flags=re.UNICODE)
    return value.strip("-") or "section"


class MarkdownRenderer:
    def __init__(self, source: Path, knowledge_root: Path, project_root: Path) -> None:
        self.source = source
        self.knowledge_root = knowledge_root
        self.project_root = project_root

    def rewrite_link(self, target: str) -> tuple[str, bool]:
        target = target.strip()
        if re.match(r"^(?:https?://|mailto:)", target, flags=re.IGNORECASE):
            return target, True
        if target.startswith("#"):
            return target, False

        path_part, separator, fragment = target.partition("#")
        resolved = (self.source.parent / path_part).resolve()
        try:
            relative_knowledge = resolved.relative_to(self.knowledge_root.resolve())
            if relative_knowledge.suffix.lower() == ".md":
                wiki_id = page_id(relative_knowledge)
                suffix = "#" + quote(fragment) if separator and fragment else ""
                return f"#/{quote(wiki_id)}{suffix}", False
        except ValueError:
            pass

        try:
            relative_project = resolved.relative_to(self.project_root.resolve()).as_posix()
            suffix = "#" + quote(fragment) if separator and fragment else ""
            return "/" + quote(relative_project) + suffix, True
        except ValueError:
            return target, True

    def inline(self, text: str) -> str:
        tokens: list[str] = []

        def token(value: str) -> str:
            tokens.append(value)
            return f"\x00{len(tokens) - 1}\x00"

        def code_replace(match: re.Match[str]) -> str:
            return token(f"<code>{html.escape(match.group(1))}</code>")

        def image_replace(match: re.Match[str]) -> str:
            alt = html.escape(match.group(1), quote=True)
            src, _ = self.rewrite_link(match.group(2))
            return token(
                f'<img class="wiki-image" src="{html.escape(src, quote=True)}" '
                f'alt="{alt}" loading="lazy">'
            )

        def link_replace(match: re.Match[str]) -> str:
            label = html.escape(match.group(1))
            href, external = self.rewrite_link(match.group(2))
            attributes = ' target="_blank" rel="noreferrer"' if external else ""
            return token(f'<a href="{html.escape(href, quote=True)}"{attributes}>{label}</a>')

        value = re.sub(r"`([^`]+)`", code_replace, text)
        value = re.sub(r"!\[([^\]]*)\]\(([^)]+)\)", image_replace, value)
        value = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", link_replace, value)
        value = html.escape(value)
        value = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", value)
        value = re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", r"<em>\1</em>", value)
        value = re.sub(r"~~([^~]+)~~", r"<del>\1</del>", value)
        value = re.sub(
            r"^\s*\[([ xX])\]\s+",
            lambda match: '<span class="task done">✓</span> ' if match.group(1).lower() == "x"
            else '<span class="task">○</span> ',
            value,
        )
        for index, replacement in enumerate(tokens):
            value = value.replace(f"\x00{index}\x00", replacement)
        return value

    @staticmethod
    def table_cells(line: str) -> list[str]:
        stripped = line.strip().strip("|")
        return [cell.strip() for cell in stripped.split("|")]

    def render(self, markdown: str) -> str:
        lines = markdown.replace("\r\n", "\n").replace("\r", "\n").split("\n")
        output: list[str] = []
        paragraph: list[str] = []
        list_kind: str | None = None
        index = 0

        def flush_paragraph() -> None:
            if paragraph:
                output.append("<p>" + self.inline(" ".join(part.strip() for part in paragraph)) + "</p>")
                paragraph.clear()

        def close_list() -> None:
            nonlocal list_kind
            if list_kind:
                output.append(f"</{list_kind}>")
                list_kind = None

        while index < len(lines):
            line = lines[index]
            stripped = line.strip()

            if stripped.startswith("```"):
                flush_paragraph()
                close_list()
                language = stripped[3:].strip().lower()
                code_lines: list[str] = []
                index += 1
                while index < len(lines) and not lines[index].strip().startswith("```"):
                    code_lines.append(lines[index])
                    index += 1
                code = html.escape("\n".join(code_lines))
                if language == "mermaid":
                    output.append(
                        '<section class="diagram-source"><span>Диаграмма</span>'
                        f'<pre><code>{code}</code></pre></section>'
                    )
                else:
                    language_class = f" language-{html.escape(language)}" if language else ""
                    output.append(
                        '<div class="code-block"><button class="copy-code" type="button">Копировать</button>'
                        f'<pre><code class="{language_class.strip()}">{code}</code></pre></div>'
                    )
                index += 1
                continue

            heading = HEADING_RE.match(line)
            if heading:
                flush_paragraph()
                close_list()
                level = len(heading.group(1))
                heading_text = heading.group(2).strip()
                output.append(
                    f'<h{level} id="{anchor_id(heading_text)}">{self.inline(heading_text)}</h{level}>'
                )
                index += 1
                continue

            if index + 1 < len(lines) and "|" in line and TABLE_SEPARATOR_RE.match(lines[index + 1]):
                flush_paragraph()
                close_list()
                headers = self.table_cells(line)
                index += 2
                rows: list[list[str]] = []
                while index < len(lines) and "|" in lines[index] and lines[index].strip():
                    rows.append(self.table_cells(lines[index]))
                    index += 1
                output.append('<div class="table-wrap"><table><thead><tr>')
                output.extend(f"<th>{self.inline(cell)}</th>" for cell in headers)
                output.append("</tr></thead><tbody>")
                for row in rows:
                    output.append("<tr>")
                    output.extend(f"<td>{self.inline(cell)}</td>" for cell in row)
                    output.append("</tr>")
                output.append("</tbody></table></div>")
                continue

            list_match = LIST_RE.match(line)
            if list_match:
                flush_paragraph()
                marker = list_match.group(1).strip()
                new_kind = "ol" if marker[0].isdigit() else "ul"
                if list_kind != new_kind:
                    close_list()
                    list_kind = new_kind
                    output.append(f"<{list_kind}>")
                output.append(f"<li>{self.inline(list_match.group(2))}</li>")
                index += 1
                continue

            if stripped.startswith(">"):
                flush_paragraph()
                close_list()
                quote_lines: list[str] = []
                while index < len(lines) and lines[index].strip().startswith(">"):
                    quote_lines.append(lines[index].strip()[1:].strip())
                    index += 1
                output.append("<blockquote>" + self.inline(" ".join(quote_lines)) + "</blockquote>")
                continue

            if not stripped:
                flush_paragraph()
                close_list()
                index += 1
                continue

            if stripped in {"---", "***", "___"}:
                flush_paragraph()
                close_list()
                output.append("<hr>")
                index += 1
                continue

            paragraph.append(line)
            index += 1

        flush_paragraph()
        close_list()
        return "\n".join(output)


STYLE = r"""
:root{color-scheme:dark;--bg:#101416;--panel:#171d20;--panel2:#1e262a;--text:#e8ece9;--muted:#98a5a1;--line:#2b363a;--brand:#7ee2b8;--brand2:#e7b86d;--code:#0b0e10;--shadow:0 18px 55px rgba(0,0,0,.3)}
:root[data-theme="light"]{color-scheme:light;--bg:#f4f1ea;--panel:#fffdf8;--panel2:#ebe7de;--text:#202523;--muted:#68736e;--line:#d7d2c7;--brand:#087b61;--brand2:#9a5b12;--code:#f0ede6;--shadow:0 18px 55px rgba(44,38,25,.12)}
*{box-sizing:border-box}html,body{height:100%;margin:0}body{font-family:Inter,"Segoe UI",system-ui,sans-serif;background:var(--bg);color:var(--text);overflow:hidden}.app{height:100%;display:grid;grid-template-columns:310px 1fr}.sidebar{background:var(--panel);border-right:1px solid var(--line);display:flex;flex-direction:column;min-width:0}.brand{padding:26px 24px 19px;border-bottom:1px solid var(--line)}.brand-mark{display:inline-flex;align-items:center;gap:10px;color:var(--brand);font-size:12px;font-weight:800;letter-spacing:.16em;text-transform:uppercase}.brand-mark::before{content:"";width:11px;height:11px;border:2px solid currentColor;transform:rotate(45deg);box-shadow:5px 5px 0 color-mix(in srgb,var(--brand) 38%,transparent)}.brand h1{font-size:24px;line-height:1.05;margin:17px 0 7px}.brand p{margin:0;color:var(--muted);font-size:13px}.search-wrap{padding:16px 18px 10px}.search{display:flex;align-items:center;gap:8px;border:1px solid var(--line);background:var(--bg);border-radius:10px;padding:9px 11px}.search:focus-within{border-color:var(--brand);box-shadow:0 0 0 3px color-mix(in srgb,var(--brand) 15%,transparent)}.search input{border:0;outline:0;background:transparent;color:var(--text);width:100%;font:inherit}.search kbd{font-size:11px;color:var(--muted);border:1px solid var(--line);border-radius:4px;padding:1px 5px}.nav{overflow:auto;padding:6px 12px 20px}.nav-section{margin-top:12px}.nav-label{padding:8px 10px 5px;color:var(--muted);font-size:10px;font-weight:800;letter-spacing:.14em;text-transform:uppercase}.nav a{display:block;padding:9px 11px;border-radius:8px;color:var(--muted);text-decoration:none;font-size:13px;line-height:1.3;margin:2px 0}.nav a:hover{color:var(--text);background:var(--panel2)}.nav a.active{color:var(--text);background:color-mix(in srgb,var(--brand) 13%,var(--panel2));box-shadow:inset 3px 0 var(--brand)}.sidebar-footer{margin-top:auto;border-top:1px solid var(--line);padding:13px 18px;display:flex;align-items:center;justify-content:space-between;color:var(--muted);font-size:11px}.icon-button{border:1px solid var(--line);background:var(--panel2);color:var(--text);border-radius:8px;padding:7px 9px;cursor:pointer}.main{overflow:auto;position:relative}.topbar{position:sticky;top:0;z-index:5;display:flex;align-items:center;justify-content:space-between;padding:12px 24px;background:color-mix(in srgb,var(--bg) 86%,transparent);backdrop-filter:blur(14px);border-bottom:1px solid color-mix(in srgb,var(--line) 75%,transparent)}.breadcrumb{font-size:12px;color:var(--muted)}.menu-button{display:none}.content{width:min(960px,calc(100% - 64px));margin:36px auto 100px}.page{display:none}.page.active{display:block;animation:enter .18s ease-out}@keyframes enter{from{opacity:.3;transform:translateY(4px)}to{opacity:1;transform:none}}article h1{font-size:clamp(34px,5vw,54px);line-height:1.04;letter-spacing:-.035em;margin:0 0 28px;max-width:850px}article h2{font-size:26px;margin:48px 0 17px;padding-top:8px;border-top:1px solid var(--line)}article h3{font-size:19px;margin:31px 0 12px;color:var(--brand2)}article h4{font-size:15px;margin:24px 0 9px}article p,article li{font-size:16px;line-height:1.72}article p{margin:0 0 18px}article ul,article ol{padding-left:25px;margin:0 0 22px}article li{padding-left:5px;margin:4px 0}article a{color:var(--brand);text-decoration-thickness:1px;text-underline-offset:3px}article strong{font-weight:750;color:color-mix(in srgb,var(--text) 90%,var(--brand))}article code{font-family:"Cascadia Code",Consolas,monospace;font-size:.88em;background:var(--code);border:1px solid var(--line);border-radius:5px;padding:2px 5px}.wiki-image{display:block;max-width:100%;height:auto;margin:22px auto;border:1px solid var(--line);border-radius:12px;background:#050708;box-shadow:var(--shadow)}.code-block{position:relative;margin:22px 0}.code-block pre,.diagram-source pre{overflow:auto;background:var(--code);border:1px solid var(--line);border-radius:12px;padding:20px;box-shadow:var(--shadow)}.code-block code,.diagram-source code{border:0;padding:0;background:none;font-size:13px;line-height:1.55}.copy-code{position:absolute;right:10px;top:10px;border:1px solid var(--line);background:var(--panel2);color:var(--muted);border-radius:7px;padding:6px 9px;cursor:pointer}.diagram-source{margin:24px 0}.diagram-source>span{display:inline-block;color:var(--brand2);font-size:11px;font-weight:800;letter-spacing:.12em;text-transform:uppercase;margin-bottom:6px}.table-wrap{overflow:auto;margin:22px 0;border:1px solid var(--line);border-radius:12px}table{width:100%;border-collapse:collapse;font-size:13px}th,td{text-align:left;padding:11px 13px;border-bottom:1px solid var(--line);vertical-align:top}th{position:sticky;top:0;background:var(--panel2);font-size:11px;text-transform:uppercase;letter-spacing:.06em}tr:last-child td{border-bottom:0}blockquote{margin:22px 0;padding:14px 18px;border-left:3px solid var(--brand);background:color-mix(in srgb,var(--brand) 7%,var(--panel));border-radius:0 9px 9px 0;color:var(--muted);line-height:1.6}.task{color:var(--muted)}.task.done{color:var(--brand)}hr{border:0;border-top:1px solid var(--line);margin:32px 0}.empty{display:none;padding:20px;color:var(--muted);text-align:center}.empty.visible{display:block}
@media(max-width:820px){body{overflow:auto}.app{display:block}.sidebar{position:fixed;z-index:20;inset:0 auto 0 0;width:min(86vw,330px);transform:translateX(-102%);transition:transform .2s;box-shadow:var(--shadow)}body.nav-open .sidebar{transform:none}.main{overflow:visible}.menu-button{display:inline-block}.content{width:min(100% - 34px,760px);margin-top:24px}.topbar{padding:10px 16px}article h1{font-size:36px}article h2{font-size:23px}}
"""


SCRIPT = r"""
const pages=[...document.querySelectorAll('.page')];const links=[...document.querySelectorAll('[data-page-link]')];const breadcrumb=document.querySelector('#breadcrumb');
function currentId(){const raw=decodeURIComponent(location.hash.replace(/^#\/?/,''));return raw.split('#')[0]||'home'}
function showPage(){let id=currentId();let page=pages.find(p=>p.dataset.page===id)||pages.find(p=>p.dataset.page==='home');if(!page)return;pages.forEach(p=>p.classList.toggle('active',p===page));links.forEach(a=>a.classList.toggle('active',a.dataset.pageLink===page.dataset.page));breadcrumb.textContent=page.dataset.title;document.title=page.dataset.title+' · Dom3D Pro Wiki';document.querySelector('.main').scrollTo(0,0);document.body.classList.remove('nav-open')}
addEventListener('hashchange',showPage);showPage();
const search=document.querySelector('#search');const empty=document.querySelector('#empty');search.addEventListener('input',()=>{const q=search.value.trim().toLocaleLowerCase('ru');let count=0;links.forEach(a=>{const page=pages.find(p=>p.dataset.page===a.dataset.pageLink);const ok=!q||(a.textContent+' '+(page?.innerText||'')).toLocaleLowerCase('ru').includes(q);a.hidden=!ok;if(ok)count++});empty.classList.toggle('visible',count===0)});
addEventListener('keydown',e=>{if(e.key==='/'&&!/input|textarea/i.test(document.activeElement.tagName)){e.preventDefault();search.focus()}if(e.key==='Escape'){search.value='';search.dispatchEvent(new Event('input'));search.blur();document.body.classList.remove('nav-open')}});
document.querySelector('#theme').addEventListener('click',()=>{const root=document.documentElement;const next=root.dataset.theme==='light'?'dark':'light';root.dataset.theme=next;localStorage.setItem('wiki-theme',next)});document.documentElement.dataset.theme=localStorage.getItem('wiki-theme')||'dark';
document.querySelector('#menu').addEventListener('click',()=>document.body.classList.toggle('nav-open'));
document.querySelectorAll('.copy-code').forEach(button=>button.addEventListener('click',async()=>{await navigator.clipboard.writeText(button.parentElement.querySelector('code').innerText);button.textContent='Скопировано';setTimeout(()=>button.textContent='Копировать',1200)}));
"""


def build(project_root: Path) -> Path:
    knowledge_root = project_root / "docs" / "knowledge"
    output_root = project_root / "docs" / "wiki-html"
    output_root.mkdir(parents=True, exist_ok=True)

    pages = []
    for source in sorted(knowledge_root.rglob("*.md")):
        if source.name.startswith("_"):
            continue
        relative = source.relative_to(knowledge_root)
        markdown = source.read_text(encoding="utf-8-sig")
        title = title_from_markdown(markdown, source.stem)
        rendered = MarkdownRenderer(source, knowledge_root, project_root).render(markdown)
        category = relative.parent.as_posix()
        if category == ".":
            category = "Главное"
        pages.append({"id": page_id(relative), "title": title, "category": category, "html": rendered})

    pages.sort(key=lambda item: (0 if item["id"] == "home" else 1, item["category"], item["title"]))
    categories: dict[str, list[dict[str, str]]] = {}
    for page in pages:
        categories.setdefault(page["category"], []).append(page)

    nav_parts = []
    for category, category_pages in categories.items():
        label = {
            "Главное": "Обзор",
            "architecture": "Архитектура",
            "codebase": "Кодовая база",
            "generated": "Автоматический индекс",
            "mesh/trimming": "Сетка и тримминг",
        }.get(category, category.replace("/", " · "))
        nav_parts.append(f'<section class="nav-section"><div class="nav-label">{html.escape(label)}</div>')
        for page in category_pages:
            nav_parts.append(
                f'<a href="#/{quote(page["id"])}" data-page-link="{html.escape(page["id"], quote=True)}">'
                f'{html.escape(page["title"])}</a>'
            )
        nav_parts.append("</section>")

    articles = "\n".join(
        f'<article class="page" data-page="{html.escape(page["id"], quote=True)}" '
        f'data-title="{html.escape(page["title"], quote=True)}">{page["html"]}</article>'
        for page in pages
    )
    site = f"""<!doctype html>
<html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="description" content="Локальная инженерная Wiki проекта Dom3D Pro"><title>Dom3D Pro Wiki</title><style>{STYLE}</style></head>
<body><div class="app"><aside class="sidebar"><header class="brand"><div class="brand-mark">Engineering knowledge</div><h1>Dom3D Pro Wiki</h1><p>Код, решения и эксперименты проекта</p></header>
<div class="search-wrap"><label class="search"><span>⌕</span><input id="search" type="search" placeholder="Поиск по Wiki" aria-label="Поиск по Wiki"><kbd>/</kbd></label></div>
<nav class="nav">{''.join(nav_parts)}<div id="empty" class="empty">Ничего не найдено</div></nav>
<footer class="sidebar-footer"><span>{len(pages)} страниц · локально</span><button id="theme" class="icon-button" type="button" title="Сменить тему">◐</button></footer></aside>
<main class="main"><header class="topbar"><button id="menu" class="icon-button menu-button" type="button">Меню</button><div id="breadcrumb" class="breadcrumb">Wiki</div><span class="breadcrumb">Обновлено автоматически</span></header><div class="content">{articles}</div></main></div><script>{SCRIPT}</script></body></html>"""
    index_path = output_root / "index.html"
    with index_path.open("w", encoding="utf-8", newline="\n") as output:
        output.write(site)
    return index_path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-root", default=str(Path(__file__).resolve().parent.parent))
    args = parser.parse_args()
    index_path = build(Path(args.project_root).resolve())
    print(f"HTML Wiki generated: {index_path}")


if __name__ == "__main__":
    main()

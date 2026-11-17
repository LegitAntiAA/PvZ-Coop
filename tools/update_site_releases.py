#!/usr/bin/env python3
"""Cache GitHub Releases as static, localized changelogs for GitHub Pages.

Run once per publication via .github/workflows/site-releases.yml. On subsequent
runs, unchanged releases reuse their previously rendered HTML; site visitors
never call the GitHub API.
"""
from __future__ import annotations

import html
import json
import os
import re
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen

REPO = os.environ.get("GITHUB_REPOSITORY", "LegitAntiAA/PvZ-Coop")
TOKEN = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
CACHE_FILE = Path("docs/releases.json")
INDEX_FILE = Path("docs/index.html")
LANGUAGE_HEADING = re.compile(r"^\s*#{1,3}\s*(English|Russian|Русский)\s*$", re.I)
VERSION = re.compile(r"^v?(\d+(?:\.\d+)+(?:[a-z]\d*)?)", re.I)


def api(path: str, payload: dict | None = None, *, html_response: bool = False):
    headers = {
        "Accept": "text/html" if html_response else "application/vnd.github+json",
        "User-Agent": "PvZ-Coop-Release-Site",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    if TOKEN:
        headers["Authorization"] = f"Bearer {TOKEN}"
    data = None
    if payload is not None:
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        headers["Content-Type"] = "application/json"
    request = Request("https://api.github.com" + path, data=data, headers=headers)
    try:
        with urlopen(request, timeout=30) as response:
            raw = response.read().decode("utf-8")
            return raw if html_response else json.loads(raw)
    except HTTPError as error:
        reason = error.read().decode("utf-8", errors="replace")[:1000]
        raise RuntimeError(f"GitHub API {path} failed: HTTP {error.code}: {reason}") from error


def published_releases() -> list[dict]:
    result = []
    for page in range(1, 21):
        batch = api(f"/repos/{REPO}/releases?per_page=100&page={page}")
        if not isinstance(batch, list):
            raise RuntimeError("GitHub Releases API returned unexpected data")
        result.extend(release for release in batch if not release.get("draft"))
        if len(batch) < 100:
            return result
    raise RuntimeError("Release list exceeds 20 pages; increase the pagination limit")


def localized_markdown(body: str) -> dict[str, str]:
    lines = body.splitlines()
    if not any(LANGUAGE_HEADING.match(line) for line in lines):
        return {"en": body.strip(), "ru": body.strip()}
    sections: dict[str, list[str]] = {"en": [], "ru": []}
    language = None
    for line in lines:
        match = LANGUAGE_HEADING.match(line)
        if match:
            language = "en" if match.group(1).lower() == "english" else "ru"
            continue
        if language is not None:
            # Some old notes have a translated title before the Russian marker.
            if language == "en" and line.strip().lower() == "# открытый альфа-тест":
                continue
            sections[language].append(line)
    en = "\n".join(sections["en"]).strip()
    ru = "\n".join(sections["ru"]).strip()
    return {"en": en or ru, "ru": ru or en}


def markdown_html(text: str) -> str:
    if not text.strip():
        return ""
    # GitHub renders and sanitizes its own Markdown, including nested lists,
    # emphasis, headings, links, and code blocks. This is done only on updates.
    return api("/markdown", {"text": text, "mode": "gfm", "context": REPO}, html_response=True)


def make_entry(release: dict, old: dict | None) -> dict:
    assets = release.get("assets") or []
    preferred = next(
        (asset for asset in assets if str(asset.get("name", "")).lower().endswith(".zip")),
        assets[0] if assets else None,
    )
    tag = str(release.get("tag_name") or "")
    match = VERSION.match(tag)
    version = match.group(1) if match else tag
    last_updated = release.get("updated_at") or ""
    if old and old.get("updated_at") == last_updated and old.get("tag") == tag and old.get("notes_format") == "github-gfm" and isinstance(old.get("notes"), dict):
        notes = old["notes"]
    else:
        localized = localized_markdown(str(release.get("body") or ""))
        notes = {locale: markdown_html(text) for locale, text in localized.items()}
        print(f"Rendered Markdown for {tag}")
    return {
        "id": release["id"],
        "tag": tag,
        "version": version,
        "title": release.get("name") or tag,
        "published_at": release.get("published_at") or release.get("created_at") or "",
        "updated_at": last_updated,
        "url": release.get("html_url") or f"https://github.com/{REPO}/releases/tag/{tag}",
        "download_url": preferred.get("browser_download_url") if preferred else None,
        "notes": notes,
        "notes_format": "github-gfm",
    }


def update_index_metadata(latest: dict | None) -> None:
    if not latest or not INDEX_FILE.exists():
        return
    version = "v" + latest["version"] if latest["version"][:1].isdigit() else latest["version"]
    safe_version = html.escape(version, quote=True)
    source = INDEX_FILE.read_text(encoding="utf-8")
    replacements = (
        (r'(<title>)[^<]*(</title>)',
         f"PvZ Co-op {safe_version} — Same lawn. New company."),
        (r'(<meta name="description" content=")[^"]*(")',
         f"PvZ Co-op {safe_version}: Play online with up to four players. Browse Fusion, co-op and all past releases. Original game required."),
        (r'(<meta property="og:description" content=")[^"]*(")',
         f"PvZ Co-op {safe_version}: Online multiplayer, Fusion and a complete release history."),
    )
    for pattern, replacement in replacements:
        source, count = re.subn(pattern, lambda match: match.group(1) + replacement + match.group(2), source, count=1)
        if count != 1:
            raise RuntimeError(f"Site metadata pattern was not found: {pattern}")
    if INDEX_FILE.read_text(encoding="utf-8") != source:
        INDEX_FILE.write_text(source, encoding="utf-8")
        print(f"Updated site SEO metadata to {version}")


def main() -> None:
    existing = {}
    if CACHE_FILE.exists():
        source = json.loads(CACHE_FILE.read_text(encoding="utf-8"))
        existing = {entry["id"]: entry for entry in source.get("releases", [])}
    releases = published_releases()
    entries = [make_entry(release, existing.get(release["id"])) for release in releases]
    entries.sort(key=lambda entry: (entry["published_at"], entry["id"]), reverse=True)
    output = json.dumps({"schema": 1, "releases": entries}, ensure_ascii=False, indent=2) + "\n"
    if not CACHE_FILE.exists() or CACHE_FILE.read_text(encoding="utf-8") != output:
        CACHE_FILE.write_text(output, encoding="utf-8")
        print(f"Updated cached release history: {len(entries)} releases")
    else:
        print(f"Release history unchanged: {len(entries)} releases")
    update_index_metadata(entries[0] if entries else None)


if __name__ == "__main__":
    main()

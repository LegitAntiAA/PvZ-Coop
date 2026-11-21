"""Reuse existing Russian Fusion translations; never infer or correct literal IDs.

Only complete Chinese source matches with one exact zero-based literal ID are
eligible. Russian cross-references are diagnostics, not translated UI strings.
No third-party packages, game execution, or source-analysis writes are needed.
"""

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import sys


REPO = Path(__file__).resolve().parents[2]
MOD = REPO.parent / "fusion/mod"
SOURCE_VERSION = "Fusion 2.3.1"
ASSEMBLY_SHA256 = "b19cfc30b2061514689a93191a979e76335dd02f5c12e550e1bb62de9578b94f"
LITERALS_SHA256 = "de8999374bc97fa6bfcd2b2f38ee9cf7881190cd91922f849e051f5f7f1e254d"
REFERENCES = ("reference_cn_1.md", "reference_cn_2.md")
CHINESE = re.compile(r"[\u4e00-\u9fff]")
CYRILLIC = re.compile(r"[\u0400-\u04ff]")
PLACEHOLDER = re.compile(r"(?<!\{)\{([0-9]+)(?:,[^:{}]*)?(?::[^{}]*)?\}(?!\})")


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_literals(path):
    records = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(records, list) or not records:
        raise ValueError("authoritative literals must be a nonempty JSON array")
    for index, record in enumerate(records):
        if not isinstance(record, dict) or not isinstance(record.get("value"), str):
            raise ValueError(f"literal {index} must contain a string value")
    return [record["value"] for record in records]


def markdown_cells(line):
    # A literal vertical bar inside a Markdown cell must be escaped.
    return [cell.strip().replace(r"\|", "|") for cell in re.split(r"(?<!\\)\|", line)[1:-1]]


def source_text(cell):
    # Match the reference documents' explicit escapes; do not normalize words,
    # punctuation, spaces, ellipses, or guessed numeric IDs.
    return re.sub(r"\\([nrt])", lambda match: {"n": "\n", "r": "\r", "t": "\t"}[match[1]], cell)


def collect(analysis, literals):
    entries = {}
    rejected = []
    warnings = []
    counts = Counter()
    for filename in REFERENCES:
        path = analysis / "sections" / filename
        for line_number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
            if not line.startswith("|"):
                continue
            cells = markdown_cells(line)
            if not cells or cells[0] == "Инд." or all(re.fullmatch(r"[-: ]*", cell) for cell in cells):
                continue
            counts["tableRows"] += 1
            row = {"file": "analysis/sections/" + filename, "line": line_number, "cells": cells}
            reasons = []
            if len(cells) != 3 or not line.rstrip().endswith("|"):
                row["reasons"] = ["malformed_table_row"]
                rejected.append(row)
                continue
            index_cell, chinese_cell, russian = cells
            bilingual = bool(CHINESE.search(chinese_cell) and CYRILLIC.search(russian))
            if bilingual:
                counts["bilingualRows"] += 1
            else:
                reasons.append("missing_chinese_or_russian")
            if "…" in chinese_cell or "..." in chinese_cell:
                row["sourceEllipsis"] = True
            if not re.fullmatch(r"[0-9]+", index_cell):
                reasons.append("ambiguous_literal_id")
                if re.search(r"\d[–−-]\d", index_cell):
                    row["idRange"] = True
            else:
                index = int(index_cell)
                if bilingual:
                    counts["exactIndexedBilingualRows"] += 1
                if index >= len(literals):
                    reasons.append("literal_id_out_of_range")
                elif source_text(chinese_cell) != literals[index]:
                    reasons.append("source_mismatch")
                    row["actualSource"] = literals[index]
                elif bilingual:
                    counts["sourceVerifiedRows"] += 1
                if re.search(r"\bсм\.", russian, re.IGNORECASE):
                    reasons.append("reference_only_translation")
                if Counter(PLACEHOLDER.findall(source_text(chinese_cell))) != Counter(PLACEHOLDER.findall(russian)):
                    reasons.append("placeholder_mismatch")
            if reasons:
                row["reasons"] = reasons
                rejected.append(row)
                continue
            entry = {"literalId": index, "source": literals[index], "ru": russian,
                     "references": [{"file": row["file"], "line": line_number}]}
            quality = []
            if re.search(r"\([^)]*\?[^)]*\)", russian):
                quality.append("uncertain_terminology_annotation")
            if russian.startswith(("…", "...")):
                quality.append("fragment_translation")
            if re.fullmatch(r"\[мусор(?:[- ][^\]]*)?\]", russian, re.IGNORECASE):
                quality.append("source_service_marker")
            if quality:
                entry["qualityWarnings"] = quality
                warnings.append({"literalId": index, "file": row["file"], "line": line_number,
                                 "warnings": quality, "ru": russian})
            previous = entries.get(index)
            if previous:
                if previous["source"] != entry["source"] or previous["ru"] != russian:
                    raise ValueError(f"conflicting translations for literal {index}")
                previous["references"].extend(entry["references"])
                counts["duplicateIdenticalLiteralRows"] += 1
            else:
                entries[index] = entry
    if not entries:
        raise ValueError("no complete, source-verified Russian translations found")

    by_source = defaultdict(list)
    for entry in entries.values():
        by_source[entry["source"]].append(entry)
    lookup, collisions = {}, []
    for chinese, candidates in sorted(by_source.items()):
        if len({entry["ru"] for entry in candidates}) == 1:
            lookup[chinese] = candidates[0]["ru"]
        else:
            collisions.append({"source": chinese, "candidates": [
                {"literalId": entry["literalId"], "ru": entry["ru"]} for entry in candidates]})
    counts.update({"importedEntries": len(entries), "rejectedRows": len(rejected),
                   "sourceTextLookupEntries": len(lookup), "sourceTextCollisions": len(collisions),
                   "qualityFlaggedEntries": sum(bool(entry.get("qualityWarnings")) for entry in entries.values())})
    rejection_reasons = Counter(reason for row in rejected for reason in row["reasons"])
    diagnostics = {"counts": dict(counts), "rejectedRows": rejected,
                   "rejectionReasons": dict(sorted(rejection_reasons.items())),
                   "sourceEllipsisRows": [row for row in rejected if row.get("sourceEllipsis")],
                   "idRangeRows": [row for row in rejected if row.get("idRange")],
                   "translationQualityWarnings": warnings, "sourceTextCollisions": collisions}
    return [entries[index] for index in sorted(entries)], lookup, diagnostics


def write_json(path, value):
    payload = json.dumps(value, ensure_ascii=False, indent=2) + "\n"
    json.loads(payload)  # Validate complete output before touching its destination.
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    try:
        temporary.write_text(payload, encoding="utf-8", newline="\n")
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--analysis", type=Path, default=MOD / "analysis")
    parser.add_argument("--literals", type=Path, default=MOD / "il2cppdumper/stringliteral.json")
    parser.add_argument("--assembly", type=Path, default=MOD / "GameAssembly.dll")
    parser.add_argument("--output", type=Path, default=REPO / "assets/fusion/catalog/strings.ru.json")
    parser.add_argument("--diagnostics", type=Path, default=REPO / "work/fusion/translations-diagnostics.json")
    args = parser.parse_args(argv)
    for destination in (args.output, args.diagnostics):
        resolved = destination.resolve()
        if resolved.is_relative_to(args.analysis.resolve()) or resolved in (args.literals.resolve(), args.assembly.resolve()):
            raise ValueError("output must not overwrite source analysis, literals, or assembly")
    if args.output.resolve() == args.diagnostics.resolve():
        raise ValueError("catalog and diagnostics need separate output paths")
    literals = load_literals(args.literals)
    if sha256(args.assembly) != ASSEMBLY_SHA256 or sha256(args.literals) != LITERALS_SHA256:
        raise ValueError("source hashes do not match the verified Fusion 2.3.1 inputs")
    entries, lookup, diagnostics = collect(args.analysis, literals)
    sources = [{"path": "GameAssembly.dll", "sha256": ASSEMBLY_SHA256},
               {"path": "il2cppdumper/stringliteral.json", "sha256": LITERALS_SHA256}]
    sources.extend({"path": "analysis/sections/" + name,
                    "sha256": sha256(args.analysis / "sections" / name)} for name in REFERENCES)
    catalog = {"schema": 1, "language": "ru", "sourceVersion": SOURCE_VERSION,
               "indexConvention": "zero-based stringliteral.json index; Ghidra StringLiteral_N uses N-1",
               "sources": sources, "counts": diagnostics["counts"],
               "translationQuality": "Existing semantic translations; source identity checked, human linguistic review not performed.",
               "almanacCoverage": "Not a translated plant/zombie almanac catalog.",
               "entries": entries, "sourceTextLookup": lookup}
    diagnostics = {"schema": 1, "sourceVersion": SOURCE_VERSION, "sources": sources, **diagnostics}
    # Diagnostics first: a validation/write failure never replaces the catalog.
    write_json(args.diagnostics, diagnostics)
    write_json(args.output, catalog)
    print(json.dumps({"output": str(args.output.resolve()), **catalog["counts"]}, ensure_ascii=True))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"translation export failed: {error}", file=sys.stderr)
        sys.exit(1)

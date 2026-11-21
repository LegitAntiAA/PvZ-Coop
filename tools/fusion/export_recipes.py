"""Export exact Fusion 2.3.1 directed recipes from complete Ghidra listings.

Read-only source inputs: GameAssembly.dll, analysis/classes.json, and the JSON
responses from Ghidra disassemble_function. Decompiled C is deliberately not an
input: FirstMix is truncated at 120000 characters by the bridge. Use --check to
verify the committed products without rewriting them.
"""
import argparse
import ast
import hashlib
import json
import re
from pathlib import Path

ORDER = ['InitTravel', 'InitMixData', 'FogPlant', 'RoofPlant', 'PuffMix', 'FirstMix', 'SubInit']
COUNTS = dict(InitTravel=51, InitMixData=5, FogPlant=46, RoofPlant=62,
              PuffMix=22, FirstMix=202, SubInit=24, SpecialPlant=5)
FILES = {name: 'asm-' + name + '.json' for name in COUNTS}
FILES['FirstMix'] = 'firstmix-fullasm.json'


def register(name):
    return 'R' + name[1:] if name.startswith('E') else name


def expression(text, registers):
    def visit(node):
        if isinstance(node, ast.Constant) and isinstance(node.value, int):
            return node.value
        if isinstance(node, ast.Name):
            return registers[register(node.id)]
        if isinstance(node, ast.BinOp):
            left, right = visit(node.left), visit(node.right)
            if left is None or right is None:
                return None
            if isinstance(node.op, ast.Add): return left + right
            if isinstance(node.op, ast.Sub): return left - right
            if isinstance(node.op, ast.Mult): return left * right
        raise ValueError(text)
    try:
        return visit(ast.parse(text.strip(), mode='eval').body)
    except (KeyError, ValueError, SyntaxError):
        return None


def verify_addresses(instructions, records):
    """Independent x64 effective-address calculation, without bounds constants."""
    type_info, static_fields, matrix, bounds = (0x200000000, 0x300000000, 0x400000000, 0x500000000)
    memory = {0x181CE1A98: type_info, type_info + 0xB8: static_fields,
              static_fields: matrix, matrix + 0x10: bounds, bounds + 0x10: 2048}
    expected = {rec['instruction_va'][2:]: rec for rec in records}
    registers = {}
    checked = 0
    for ins in instructions:
        op, _, text = ins['instruction'].partition(' ')
        args = text.split(',')
        if op == 'MOV' and len(args) == 2:
            dest, source = args
            if re.fullmatch(r'[RE][A-Z0-9]+', dest):
                if 'ptr [' in source:
                    address = expression(source[source.index('[')+1:source.index(']')], registers)
                    registers[register(dest)] = memory.get(address)
                else:
                    registers[register(dest)] = expression(source, registers)
            elif dest.startswith('dword ptr [') and ins['address'] in expected:
                address = expression(dest[dest.index('[')+1:dest.index(']')], registers)
                rec = expected[ins['address']]
                wanted = matrix + 0x20 + 4 * (rec['existing'] * 2048 + rec['used'])
                if address != wanted:
                    raise ValueError(f"Effective address mismatch at {ins['address']}: {address} != {wanted}")
                checked += 1
        elif op == 'LEA' and len(args) == 2:
            registers[register(args[0])] = expression(args[1][1:-1], registers)
        elif op in ['ADD', 'SUB', 'SHL'] and len(args) == 2:
            left, right = expression(args[0], registers), expression(args[1], registers)
            value = None
            if left is not None and right is not None:
                value = left + right if op == 'ADD' else left - right if op == 'SUB' else left << right
            registers[register(args[0])] = value
        elif op == 'IMUL' and len(args) == 3:
            left, right = expression(args[1], registers), expression(args[2], registers)
            registers[register(args[0])] = None if left is None or right is None else left * right
        elif op in ['INC', 'DEC'] and len(args) == 1:
            value = expression(args[0], registers)
            registers[register(args[0])] = None if value is None else value + (1 if op == 'INC' else -1)
    if checked != len(records):
        raise ValueError('Not every recipe store was independently checked')
    return checked


def extract(instructions, method, address, names):
    row = col = None
    result = []
    for ins in instructions:
        asm = ins['instruction']
        match = re.fullmatch(r'CMP dword ptr \[([A-Z0-9]+)\],(0x[0-9a-f]+)', asm)
        if match:
            row, col = int(match[2], 16), None
        match = re.fullmatch(r'CMP E[A-Z0-9]+,(0x[0-9a-f]+)', asm)
        if match and row is not None: col = int(match[1], 16)
        match = re.fullmatch(r'CMP dword ptr \[[A-Z0-9]+ \+ 0x10\],(0x[0-9a-f]+)', asm)
        if match and row is not None: col = int(match[1], 16)
        if re.fullmatch(r'TEST (E[A-Z0-9]+),\1', asm) and row is not None: col = 0
        match = re.fullmatch(r'MOV dword ptr \[(.+)\],(0x[0-9a-f]+)', asm)
        if not match:
            continue
        output = int(match[2], 16)
        if row is None or col is None or any(value not in names for value in [row, col, output]):
            raise ValueError(f"Unresolved or unknown recipe at {ins['address']}")
        result.append(dict(existing=row, used=col, result=output,
                           existing_name=names[row], used_name=names[col], result_name=names[output],
                           source_method=method, source_va=address,
                           instruction_va='0x'+ins['address'], instruction=asm))
    if len(result) != COUNTS[method]:
        raise ValueError(f'Unexpected store count for {method}: {len(result)}')
    verify_addresses(instructions, result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--analysis-root', type=Path, default=Path(r'C:\PvZ\fusion\mod\analysis'))
    parser.add_argument('--disassembly-root', type=Path, required=True)
    parser.add_argument('--game-assembly', type=Path, default=Path(r'C:\PvZ\fusion\mod\GameAssembly.dll'))
    parser.add_argument('--repo-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    classes = json.loads((args.analysis_root/'classes.json').read_text(encoding='utf-8'))
    names = {int(item['value']): item['name'] for item in classes['PlantType']['values']}
    methods = {item['sig'].split('(')[0].split()[-1]: item['va'] for item in classes['MixData']['methods']}
    sources, records = {}, []
    for method, file in FILES.items():
        raw = (args.disassembly_root/file).read_bytes()
        listing = json.loads(raw.decode('utf-8-sig'))
        rows = extract(listing['instructions'], method, methods[method], names)
        sources[method] = dict(va=methods[method], stores=len(rows),
                               instructions=len(listing['instructions']), sha256=hashlib.sha256(raw).hexdigest())
        records += rows
    final, overwrites = {}, []
    for method in ORDER:
        for rec in records:
            if rec['source_method'] != method: continue
            key = rec['existing'], rec['used']
            if key in final:
                old = final[key]['result']
                overwrites.append(dict(existing=key[0], used=key[1], old=old, new=rec['result']))
                if old != rec['result']: raise ValueError('Unexpected conflicting init writes')
            final[key] = rec
    table = [final[key] for key in sorted(final)]
    if len(table) != 410 or len(overwrites) != 2 or len(records) != 417:
        raise ValueError('Unexpected recovered table shape')
    document = dict(version='Fusion 2.3.1', source_binary='GameAssembly.dll',
                    source_sha256=hashlib.sha256(args.game_assembly.read_bytes()).hexdigest(),
                    source_classes_sha256=hashlib.sha256((args.analysis_root/'classes.json').read_bytes()).hexdigest(),
                    matrix_dimensions=[2048,2048], initialization_order=ORDER,
                    directional=True, init_store_count=412, final_entry_count=410,
                    address_checks_passed=417, sources=sources,
                    writes=records, overwrites=overwrites, entries=table)
    cpp = ['// Generated by tools/fusion/export_recipes.py from Fusion 2.3.1 native code.',
           '// Source SHA-256: '+document['source_sha256'], '#include "FusionRecipe.h"', '',
           'namespace LawnFusion', '{', 'namespace', '{', 'const FusionRecipe kRecipes[] =', '{']
    cpp += ['    {%d, %d, %d}, // %s + %s -> %s' % (r['existing'],r['used'],r['result'],r['existing_name'],r['used_name'],r['result_name']) for r in table]
    cpp += ['};', '}', '', 'const FusionRecipe* FindFusionRecipe(FusionPlantId existing, FusionPlantId used)', '{',
            '    size_t first = 0;', '    size_t last = GetFusionRecipeCount();', '    while (first < last)', '    {',
            '        const size_t middle = first + (last - first) / 2;', '        const FusionRecipe& recipe = kRecipes[middle];',
            '        if (recipe.existing < existing || (recipe.existing == existing && recipe.used < used))',
            '            first = middle + 1;', '        else', '            last = middle;', '    }',
            '    if (first < GetFusionRecipeCount() && kRecipes[first].existing == existing && kRecipes[first].used == used)',
            '        return &kRecipes[first];', '    return nullptr;', '}', '',
            'const FusionRecipe* GetFusionRecipes()', '{', '    return kRecipes;', '}', '',
            'size_t GetFusionRecipeCount()', '{', '    return sizeof(kRecipes) / sizeof(kRecipes[0]);', '}', '}', '']
    products = {args.repo_root/'assets/fusion/catalog/fusion-recipes.json':json.dumps(document,ensure_ascii=False,indent=2)+'\n',
                args.repo_root/'Lawn/Fusion/FusionRecipe.cpp':'\n'.join(cpp)}
    for path, value in products.items():
        if args.check:
            if not path.exists() or path.read_text(encoding='utf-8') != value:
                raise ValueError(f'Generated product is stale: {path}')
        else:
            path.parent.mkdir(parents=True,exist_ok=True)
            path.write_text(value,encoding='utf-8',newline='\n')
    print('Passed: 417 independent address checks; 412 initialization stores; 410 directed recipes; 2 identical overwrites; generated products '+('match' if args.check else 'written'))


if __name__ == '__main__':
    main()

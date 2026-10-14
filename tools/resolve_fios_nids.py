import json, glob

nids = [
    'H2e8t5ScQGc', 'Ou3iL1abvng', 'hcuQgD53UxM', 'Q2V+iqvjgC0', 'vNe1w4diLCs',
    'lbQKqsERhtE', 'fJgP+wqifno', '8zTFvBIAIN8', 'Q3VBxCXhUHs', 'MUjC4lbHrK4',
    'GMpvxPFW924', '1mecP7RgI2A', '5bBacGLyLOs', 'efhK-YSUYYQ', 'n7AepwR0s34',
    'QBi7HCK03hw', 'ejekcaNQNq0', 'wLlFkwG9UcQ', '9BcDykPmo1I', 'UK2Tl2DWUns',
    'kBwCPsYX-m4', 'fTx66l5iWIA', 'VW3TVZiM4-E', 'taRWhTJFTgE', 'oib76F-12fk',
    '1-LFLmRFxxM', '1G3lF1Gg1k8', '+r3rMFwItV4', 'nKWi-N2HBV4', 'Cg4srZ6TKbU',
    '52NcYU9+lEo', 'naInUjYt3so', 'eV9wAD2riIA', 'WlyEA-sLDf0', 'AUXVxWeJU-A',
    '4wSze92BhLI', 'g8cM39EUZ6o', 'eR2bZFAAU0Q', 'JGgj7Uvrl+A', 'g+PZd2hiacg',
    '2Tb92quprl0', 'kDh-NfxgMtE', 'BmMjYxmew1w', 'WKAXJ4XBPQ4', '2Of0f+3mhhE',
    'cmo1RIYva9o', '9UK1vLZQft4', 'upoVrzMHFeE', 'tn3VlD0hG60', 'smWEktiyyG0',
    'F8bUHwAG284', '1FGvU0i9saQ', 'iMp8QpE+XO4', 'BB+kb08Tl9A', '6ULAa0fq4jA',
    'Ox9i0c7L5w0', 'XD3mDeybCnk', 'bIHoZCTomsI', '+L98PIbGttk', 'mqdNorrB+gI',
    '1jfXLRVzisc', '62KCwEMmzcM', 'nsYoNRywwNg', '3qxgM4ezETA', '-Wreprtu0Qs',
    'eXbUSpEaTsA', 'DzES9hQF4f4', 'UTXzJbWhhTE', '6UgtwV+0zb4', '3PtV6p3QNX4',
    'rcrVFJsQWRY', '1tKyG7RlMJo', 'onNY9Byn-W8', 'aI+OeCz8xrQ', 'bt3CTBKmGyI',
    'W0Hpm2X0uPE', 'tIhsqj0qsFE', 'gQX+4GDQjpM'
]

nid_map = {}
for path in glob.glob('tools/ps4libdoc/**/*.json', recursive=True):
    with open(path, 'r', encoding='utf-8-sig') as f:
        try:
            doc = json.load(f)
            for mod in doc.get('modules', []):
                for lib in mod.get('libraries', []):
                    for sym in lib.get('symbols', []):
                        encoded = sym.get('encoded_id')
                        name = sym.get('name')
                        if encoded:
                            nid_map[encoded] = name
        except Exception:
            pass

for nid in nids:
    clean = nid.split('#')[0]
    print(f'{clean} -> {nid_map.get(clean, "UNKNOWN")}')

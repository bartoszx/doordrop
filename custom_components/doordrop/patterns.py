# patterns.py
PATTERNS = {
    'Allegro': r'A000[a-zA-Z0-9]{6}',
    'InPost': r'\d{24}',
    'DPD': r'10007\d{9}U',
    'DHL': [
        r'JJD\d{21}',  # Preferred pattern for DHL
        r'\d{10}',     # Alternative pattern for DHL
        r'000\d{21}'   # Another alternative pattern for DHL
    ],
    'Pocztex': r'PX\d{10}',
    'Poczta Polska': r'\d{20}',  # Pattern for Poczta Polska
    'Generic13Digit': r'\d{13}', # Generic 13-digit pattern
    'Generic12Digit': r'\d{12}'  # Generic 12-digit pattern
}

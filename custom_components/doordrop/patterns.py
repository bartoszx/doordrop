# patterns.py
PATTERNS = {
    'Allegro': r'A000[a-zA-Z0-9]{6}',
    'InPost': r'\d{24}',
    'DPD': r'1000727398456[U]\d{13}',  # Popraw wzorzec zgodnie z dokładnymi wymaganiami
    'DHL': r'((1|2)\d{10})|(000\d{21})|(JJD\d{17})|([A-Z]{2}\d{9}[A-Z]{2})',
    'Pocztex': r'PX\d{10}',
    'Generic13Digit': r'\d{13}',  # Ogó
    'Generic12Digit': r'\d{12}'  # Ogólny wzorzec dla 13-cyfrowych numerów do testow
}

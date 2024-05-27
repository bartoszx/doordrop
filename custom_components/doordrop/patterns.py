# patterns.py
PATTERNS = {
    'Allegro': r'A000[a-zA-Z0-9]{6}',
    'InPost': r'\d{24}',
    'DPD': r'1000727398456[U]\d{13}',
    'DHL': [
        r'JJD\d{21}',  # Wzorzec dla DHL z numerem o stałej długości 21
        r'\d{10}',     # Alternatywny wzorzec dla DHL
        r'000\d{21}'   # Inny alternatywny wzorzec dla DHL
    ],
    'Pocztex': r'PX\d{10}',
    'Poczta Polska': [  # Wzorzec dla Poczty Polskiej
        r'\d{20}'',  # Wzorzec dla DHL z numerem o stałej długości 21
        r'PX\d{10}
    ],    
    'Generic13Digit': r'\d{13}',
    'Generic12Digit': r'\d{12}'
}


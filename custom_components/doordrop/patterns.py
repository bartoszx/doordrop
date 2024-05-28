# patterns.py
PATTERNS = {
    'Allegro': r'A000[a-zA-Z0-9]{6}',
    'InPost': r'\d{24}',
    'DPD': [
        r'\d{13}[A-Z]',     # Numer przesyłki DPD kończący się dowolną literą
        r'\d{14}',          # Numer przesyłki DPD składający się z 14 cyfr
        r'[A-Z0-9]{14}'     # Numer przesyłki DPD zawierający 14 znaków (duże litery i cyfry)
    ],
    'DHL': [
        r'JJD\d{21}',       # Wzorzec dla DHL z numerem o stałej długości 21
        r'\d{10}',          # Alternatywny wzorzec dla DHL
        r'000\d{21}'        # Inny alternatywny wzorzec dla DHL
    ],
    'Pocztex': r'PX\d{10}',
    'Poczta Polska': [  # Wzorzec dla Poczty Polskiej
        r'\d{20}',  # Wzorzec dla DHL z numerem o stałej długości 21
        r'PX\d{10}'
    ],    
    'Generic13Digit': r'\d{13}', # Ogólny wzorzec dla 13-cyfrowych numerów
    'Generic12Digit': r'\d{12}'  # Ogólny wzorzec dla 12-cyfrowych numerów
}


# patterns.py
PATTERNS = {
    'Allegro': r'A000[a-zA-Z0-9]{6}',
    'InPost': r'\d{24}\d*',
    'DPD': [
        r'\d{13}[A-Z]',     # Numer przesyłki DPD kończący się dowolną literą
        r'\d{14}',          # Numer przesyłki DPD składający się z 14 cyfr
        r'[A-Z0-9]{14}'     # Numer przesyłki DPD zawierający 14 znaków (duże litery i cyfry)
    ],
    'DHL': [
        r'JJD\d{20}\d*',       # Wzorzec dla DHL z numerem o stałej długości 21
        r'\d{10}\d*',       # Alternatywny wzorzec dla DHL: minimum 10 cyfr, ciągnie do końca numeru
        r'000\d{21}'        # Inny alternatywny wzorzec dla DHL
    ],
    'Pocztex': r'PX\d{10}',
    'Poczta Polska': [  # Wzorzec dla Poczty Polskiej
        r'\d{10}\d*',  # Wzorzec dla DHL z numerem o stałej długości 21
        r'PX\d{10}\d*'
    ],    
    'Fedex': [  # Wzorzec Fedex 
        r'\d{10}\d*' 
    ],
    'Generic13Digit': r'\d{13}', # Ogólny wzorzec dla 13-cyfrowych numerów
    'Generic12Digit': r'\d{12}'  # Ogólny wzorzec dla 12-cyfrowych numerów
}

CUSTOM_PATTERNS = {
    'DHL': [
        r'\bdhl\b', r'\.dhl\.', r'\bdhl\b.*@', r'dhl.com', r'\bDHL\b', r'<b>.*DHL.*<'
    ],
    'Allegro': [
        r'\ballegro\b', r'\.allegro\.', r'\ballegro\b.*@', r'allegro.com', r'\bAllegro\b', r'<b>.*Allegro.*<'
    ],
    'InPost': [
        r'\binpost\b', r'\.inpost\.', r'\binpost\b.*@', r'inpost.com', r'\bInPost\b', r'<b>.*InPost.*<'
    ],
    'DPD': [
        r'\bdpd\b', r'\.dpd\.', r'\bdpd\b.*@', r'dpd.com', r'\bDPD\b', r'<b>.*DPD.*<'
    ],
    'Pocztex': [
        r'\bpocztex\b', r'\.pocztex\.', r'\bpocztex\b.*@', r'pocztex.com', r'\bPocztex\b', r'<b>.*Pocztex.*<'
    ],
    'Poczta Polska': [
        r'\bpoczta\s?polska\b', r'\.poczta\s?polska\.', r'\bpoczta\s?polska\b.*@', r'poczta-polska.com', r'\bPoczta\s?Polska\b', r'<b>.*Poczta\s?Polska.*<'
    ],
    'Fedex': [
        r'\bfedex\b', r'\.fedex\.', r'\bfedex\b.*@', r'fedex.com', r'\bFedex\b', r'<b>.*Fedex.*<'
    ]
}

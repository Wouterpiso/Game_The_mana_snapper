import os
from paddleocr import PaddleOCR

# 1. Initialiseer PaddleOCR lokaal
# use_gpu=False zorgt dat het model op je CPU draait
# lang='en' werkt perfect voor Latijnse letters/woorden (Nederlands/Engels)
ocr = PaddleOCR(use_angle_cls=True, lang='en', use_gpu=False)

def extract_words_from_image(image_path):
    # Controleer of het bestand wel echt bestaat
    if not os.path.exists(image_path):
        return f"Fout: Afbeelding '{image_path}' niet gevonden."

    # 2. Voer de lokale OCR-detectie uit
    result = ocr.ocr(image_path, cls=True)
    
    # PaddleOCR geeft soms None terug als er helemaal geen tekst is
    if result is None or result[0] is None:
        return "Geen tekst herkend in de afbeelding."
        
    extracted_words = []
    
    # 3. Loop door de resultaten en filter ALLEEN de tekst eruit
    for line in result[0]:
        # line[1][0] bevat de herkende tekst
        # line[1][1] bevat de betrouwbaarheidsscore (accuracy)
        word_or_sentence = line[1][0]
        extracted_words.append(word_or_sentence)
        
    return extracted_words

# --- HOE JE HET GEBRUIKT ---
# Zorg dat er een afbeelding genaamd 'factuur.png' of 'foto.jpg' in je map staat
mijn_afbeelding = 'factuur.png' 

woorden_lijst = extract_words_from_image(mijn_afbeelding)

# Print het resultaat als een lijst met woorden/zinnen
print("\n--- Herkende Woorden ---")
for woord in woorden_lijst:
    print(woord)

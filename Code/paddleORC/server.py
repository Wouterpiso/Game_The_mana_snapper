import json
import os
import cv2
import numpy as np
from flask import Flask, request, jsonify
from paddleocr import PaddleOCR

app = Flask(__name__)

# Waar de ESP32 zijn collectie neerzet met /export en weer ophaalt met /import.
EXPORT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "collection_export.json")

# Initialiseer PaddleOCR met de nieuwe parameters.
# use_textline_orientation vervangt use_angle_cls.
# PaddleOCR-gpu selecteert nu automatisch je GPU.
ocr = PaddleOCR(use_textline_orientation=True, lang='en', enable_mkldnn=False)

@app.route('/ocr', methods=['POST'])
def process_ocr():
    # 1. Controleer of er een bestand is meegestuurd
    if 'image' not in request.files:
        return jsonify({"error": "Geen afbeelding ontvangen"}), 400
        
    file = request.files['image']
    
    # 2. Converteer het binair bestand direct naar een OpenCV afbeelding
    img_bytes = file.read()
    nparr = np.frombuffer(img_bytes, np.uint8)
    img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

    if img is None:
        return jsonify({"error": "Ongeldig afbeeldingsbestand ontvangen"}), 400

    # 3. Voer PaddleOCR uit op de afbeelding (PaddleOCR 3.x: predict, geen cls meer)
    result = ocr.predict(img)

    words_list = []
    for res in result:
        words_list.extend(res["rec_texts"])
            
    # 4. Stuur de lijst met woorden terug naar de ESP32
    print(f"Verwerkt op GPU! Gevonden tekst: {words_list}")
    return jsonify({"words": words_list})

@app.route('/export', methods=['POST'])
def export_collection():
    # De ESP32 stuurt de hele collectie als JSON; die wordt hier bewaard.
    collection = request.get_json(silent=True)
    if collection is None:
        return jsonify({"error": "Geen geldige JSON ontvangen"}), 400
    with open(EXPORT_PATH, 'w', encoding='utf-8') as handle:
        json.dump(collection, handle, ensure_ascii=False, indent=2)
    print(f"Collectie bewaard in {EXPORT_PATH}")
    return jsonify({"ok": True})

@app.route('/import', methods=['GET'])
def import_collection():
    # De ESP32 haalt de laatst bewaarde collectie weer op.
    if not os.path.exists(EXPORT_PATH):
        return jsonify({"error": "Geen exportbestand gevonden"}), 404
    with open(EXPORT_PATH, encoding='utf-8') as handle:
        return handle.read(), 200, {"Content-Type": "application/json"}

if __name__ == '__main__':
    # host='0.0.0.0' zorgt dat de ESP32 je pc kan bereiken via wifi
    app.run(host='0.0.0.0', port=5000, debug=True)

import os
import cv2
import numpy as np
from flask import Flask, request, jsonify
from paddleocr import PaddleOCR

app = Flask(__name__)

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

if __name__ == '__main__':
    # host='0.0.0.0' zorgt dat de ESP32 je pc kan bereiken via wifi
    app.run(host='0.0.0.0', port=5000, debug=True)

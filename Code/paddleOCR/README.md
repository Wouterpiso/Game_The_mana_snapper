# PC service (Flask + PaddleOCR)

Small HTTP service that gives the Master two capabilities it cannot perform on
its own: reading a card title with OCR, and storing/returning the collection as a
JSON file. There is no user interface; the Screen drives everything.

## Endpoints

| Endpoint | Method | Request | Response |
| --- | --- | --- | --- |
| `/ocr` | POST (multipart/form-data) | JPEG in the `image` field | `200 {"words": ["...", "..."]}` or `400` with an error message. |
| `/export` | POST (application/json) | Complete collection document | `200 {"ok": true}`; writes `collection_export.json`. |
| `/import` | GET | - | `200` with the stored document, or `404` when no export exists. |

The Master treats the first non-empty word from `/ocr` as the card title and
uses the exported document unchanged for import, so the JSON round-trips.

## Setup

Use a 64-bit Python 3.10 or newer. From this folder:

```bash
python -m venv .venv
.venv\Scripts\activate            # Windows
# source .venv/bin/activate       # Linux / macOS
python -m pip install --upgrade pip
pip install flask paddleocr paddlepaddle opencv-python numpy
```

`paddleocr` 3.x is required: `server.py` uses `use_textline_orientation` and
`predict()`, which do not exist in older 2.x releases. A CPU build of
PaddlePaddle is sufficient; a GPU build can be installed instead when available.

## Run

```bash
python server.py
```

The server binds to `0.0.0.0:5000`, which makes it reachable from the Master on
the same network. Keep the terminal open while using the device. On the first
start, PaddleOCR downloads its detection and recognition models; later starts use
the cache.

Set the Master's **Card server base URL** to `http://<pc-address>:5000`, for
example `http://192.168.1.79:5000`, and allow inbound TCP port 5000 in the PC
firewall.

## Data file

`collection_export.json` is created next to `server.py` by `/export` and read by
`/import`. Delete it to make the next import fail with 404, or copy it elsewhere
to keep a backup. The file is plain JSON and uses the same document as the
collection on the Master:

```json
{
  "object": "list",
  "total_cards": 1,
  "has_more": false,
  "next_page": null,
  "data": [
    { "name": "Test Card", "colors": ["W"], "amount": 1 }
  ]
}
```

## Quick tests

From this folder, with the server running:

```bash
curl -X POST -F "image=@../Master/test_Image/test1.jpeg" http://localhost:5000/ocr
curl -X POST -H "Content-Type: application/json" -d "{\"object\":\"list\",\"data\":[]}" http://localhost:5000/export
curl http://localhost:5000/import
```

The second command replaces the stored export with an empty collection; use it
only when that is safe. On Windows PowerShell, `curl` is an alias for
`Invoke-WebRequest`; use `curl.exe` to run the commands above.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| The Master reports OCR FAILED | Can the PC be reached at the configured base URL? Check firewall, port 5000 and that both devices are on the same 2.4 GHz network. |
| `/ocr` returns 400 | The request did not contain an `image` file, or the JPEG could not be decoded. Test with the curl command above first. |
| `/import` returns 404 | No `/export` has been done yet; the file `collection_export.json` does not exist. |
| Server takes long on first start | PaddleOCR is downloading models. The first run needs internet; later runs use the cache. |
| Import succeeds but the device shows the old collection | The Master only replaces its collection after validating the document; check the Monitor log for an invalid-JSON warning. |

## Related

- [Firmware and PC service overview](../README.md)
- [Master firmware](../Master/README.md)

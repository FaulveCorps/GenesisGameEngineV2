import asyncio
import json
from pathlib import Path

from winrt.windows.media.ocr import OcrEngine
from winrt.windows.graphics.imaging import BitmapDecoder
from winrt.windows.storage.streams import InMemoryRandomAccessStream, DataWriter


async def software_bitmap_from_bytes(data: bytes):
    stream = InMemoryRandomAccessStream()
    writer = DataWriter(stream)
    writer.write_bytes(data)
    await writer.store_async()
    await writer.flush_async()
    writer.detach_stream()
    stream.seek(0)
    decoder = await BitmapDecoder.create_async(stream)
    return await decoder.get_software_bitmap_async()


async def ocr_words(path: Path):
    sb = await software_bitmap_from_bytes(path.read_bytes())
    engine = OcrEngine.try_create_from_user_profile_languages()
    if engine is None:
        raise RuntimeError("No OcrEngine available")

    res = await engine.recognize_async(sb)
    out = []
    for line in res.lines:
        for w in line.words:
            r = w.bounding_rect
            out.append(
                {
                    "text": (w.text or ""),
                    "x": float(r.x),
                    "y": float(r.y),
                    "w": float(r.width),
                    "h": float(r.height),
                }
            )
    return out


async def main() -> int:
    img = Path(r"C:\Users\jpfau\Desktop\Project\GenesisGameEngine\artifacts\editor_client.png")
    out_json = Path(r"C:\Users\jpfau\Desktop\Project\GenesisGameEngine\artifacts\editor_client_ocr_words.json")

    words = await ocr_words(img)
    out_json.write_text(json.dumps(words, indent=2), encoding="utf-8")

    # also print a compact summary
    print(f"Wrote {out_json} ({len(words)} words)")
    # show top 80 words with rough positions
    for w in words[:80]:
        print(f"{w['text']:<18} @ ({w['x']:.0f},{w['y']:.0f}) {w['w']:.0f}x{w['h']:.0f}")

    return 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))

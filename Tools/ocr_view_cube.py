import asyncio
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


async def ocr_file(path: Path) -> str:
    data = path.read_bytes()
    sb = await software_bitmap_from_bytes(data)
    engine = OcrEngine.try_create_from_user_profile_languages()
    if engine is None:
        return "<no OcrEngine available>"
    res = await engine.recognize_async(sb)
    return (res.text or "").strip()


async def main() -> int:
    imgs = [
        Path(r"C:\Users\jpfau\Desktop\Project\GenesisGameEngine\artifacts\view_cube_region.png"),
        Path(r"C:\Users\jpfau\Desktop\Project\GenesisGameEngine\artifacts\view_cube_region_hover.png"),
        Path(r"C:\Users\jpfau\Desktop\Project\GenesisGameEngine\artifacts\view_cube_region_v2.png"),
        Path(r"C:\Users\jpfau\Desktop\Project\GenesisGameEngine\artifacts\view_cube_region_v2_hover.png"),
    ]

    for p in imgs:
        print(f"== {p.name} ==")
        if not p.exists():
            print("<missing>\n")
            continue
        try:
            text = await ocr_file(p)
        except Exception as e:
            print(f"<ocr error: {type(e).__name__}: {e}>\n")
            continue

        print(text if text else "<no text>")
        print()

    return 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))

#!/usr/bin/env python3
"""movement_config.h の watch_faces[] と MOVEMENT_SECONDARY_FACE_INDEX だけを差し替える。

他の設定（LED の色、24時間表示、タイムアウト等）はリポジトリの値をそのまま残す。
フェイス名が実在するかを先に検査して、綴り違いはビルドを回す前に落とす。
"""
import os
import re
import sys
from pathlib import Path

CONFIG = Path("movement_config.h")
FACE_DIR = Path("watch-faces")


def known_faces():
    """watch-faces/ 以下のヘッダが宣言しているフェイスの一覧

    フェイスは `#define wadokei_face ((const watch_face_t) { ... })` という
    マクロとして定義されている。念のため extern 宣言の書き方も拾う。
    """
    macro = re.compile(r"^#define\s+(\w+)\s*\(\(\s*const\s+watch_face_t\s*\)", re.M)
    extern = re.compile(r"extern\s+const\s+watch_face_t\s+(\w+)\s*;")
    found = set()
    for header in FACE_DIR.rglob("*.h"):
        text = header.read_text(encoding="utf-8", errors="replace")
        found.update(macro.findall(text))
        found.update(extern.findall(text))
    return found


def parse(value):
    return [name.strip() for name in value.split(",") if name.strip()]


def main():
    rotation = parse(os.environ.get("FACES", ""))
    secondary = parse(os.environ.get("SECONDARY", ""))

    if not rotation:
        sys.exit("faces が空です。最低 1 つは指定してください。")

    known = known_faces()
    unknown = [n for n in rotation + secondary if n not in known]
    if unknown:
        print("知らないフェイス名です: " + ", ".join(unknown), file=sys.stderr)
        print("\n使える名前:", file=sys.stderr)
        for name in sorted(known):
            print("  " + name, file=sys.stderr)
        sys.exit(1)

    seen = set()
    duplicated = [n for n in rotation + secondary if n in seen or seen.add(n)]
    if duplicated:
        sys.exit("同じフェイスが 2 回入っています: " + ", ".join(duplicated))

    # 先頭は時計に戻る先になるので、時計が 1 つも無ければ clock_face を足す
    if not any(name.endswith("clock_face") or name == "clock_face" for name in rotation):
        rotation.insert(0, "clock_face")
        print("時計のフェイスが無かったので、先頭に clock_face を足しました")

    # 設定と時刻合わせが無いと、焼いたあとで時計を設定できなくなる
    for required in ("settings_face", "set_timelocation_face"):
        if required in rotation or required in secondary:
            continue
        if required == "set_timelocation_face" and "set_time_face" in rotation + secondary:
            continue
        secondary.append(required)
        print(f"{required} が無かったので、長押し側に足しました")

    faces = rotation + secondary
    body = "".join(f"    {name},\n" for name in faces)
    array = "const watch_face_t watch_faces[] = {\n" + body + "};"

    text = CONFIG.read_text(encoding="utf-8")
    text, n = re.subn(r"const watch_face_t watch_faces\[\] = \{.*?\};", array, text, count=1, flags=re.S)
    if n != 1:
        sys.exit("watch_faces[] が見つかりませんでした")

    # 末尾から secondary の個数ぶんが、ローテーションから外れる
    index = f"(MOVEMENT_NUM_FACES - {len(secondary)})" if secondary else "0"
    text, n = re.subn(r"#define MOVEMENT_SECONDARY_FACE_INDEX .*", f"#define MOVEMENT_SECONDARY_FACE_INDEX {index}", text, count=1)
    if n != 1:
        sys.exit("MOVEMENT_SECONDARY_FACE_INDEX が見つかりませんでした")

    CONFIG.write_text(text, encoding="utf-8")

    print(f"フェイス {len(faces)} 個（うち長押し側 {len(secondary)} 個）")
    for i, name in enumerate(faces):
        mark = "  ※" if i >= len(rotation) else "   "
        print(f"{i:3d}{mark} {name}")


if __name__ == "__main__":
    main()

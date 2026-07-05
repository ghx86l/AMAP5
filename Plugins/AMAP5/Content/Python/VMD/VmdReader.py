import os
import struct
import sys
import math
try:
    import unreal
    _HAS_UNREAL = True
except Exception:
    unreal = None
    _HAS_UNREAL = False
from PySide6 import QtWidgets, QtCore, QtGui




def _read_cstr_fixed(data: bytes, size: int, enc: str) -> str:
    b = data[:size]
    b = b.split(b"\x00", 1)[0]
    return b.decode(enc, errors="replace")


class VmdReader:
    @staticmethod
    def read(path: str) -> dict:
        with open(path, "rb") as f:
            data = f.read()

        off = 0
        if len(data) < 30 + 20 + 4:
            raise ValueError("VMDファイルが短すぎます。")

        header = _read_cstr_fixed(data[off:off + 30], 30, "ascii")
        off += 30
        model = _read_cstr_fixed(data[off:off + 20], 20, "shift_jis")
        off += 20

        bone_count = struct.unpack_from("<I", data, off)[0]
        off += 4

        bones = {}
        bone_order = []
        for _ in range(bone_count):
            name = _read_cstr_fixed(data[off:off + 15], 15, "shift_jis")
            off += 15
            frame = struct.unpack_from("<I", data, off)[0]
            off += 4
            pos = struct.unpack_from("<fff", data, off)
            off += 12
            rot = struct.unpack_from("<ffff", data, off)
            off += 16
            bezier = data[off:off + 16]
            off += 16
            off += 48

            if name not in bones:
                bones[name] = []
                bone_order.append(name)
            bones[name].append((frame, pos, rot, bezier))

        morph_count = struct.unpack_from("<I", data, off)[0]
        off += 4

        morphs = {}
        morph_order = []
        for _ in range(morph_count):
            name = _read_cstr_fixed(data[off:off + 15], 15, "shift_jis")
            off += 15
            frame = struct.unpack_from("<I", data, off)[0]
            off += 4
            weight = struct.unpack_from("<f", data, off)[0]
            off += 4

            if name not in morphs:
                morphs[name] = []
                morph_order.append(name)
            morphs[name].append((frame, weight))

        for _, v in bones.items():
            v.sort(key=lambda t: t[0])
        for _, v in morphs.items():
            v.sort(key=lambda t: t[0])

        return {
            "header": header,
            "model": model,
            "bones": bones,
            "bone_order": bone_order,
            "morphs": morphs,
            "morph_order": morph_order,
        }



def _infer_total_frames(vmd: dict, fps: float = None) -> int:
    max_f = -1
    bones = vmd.get("bones", {}) or {}
    morphs = vmd.get("morphs", {}) or {}
    for keys in bones.values():
        for k in keys:
            f = k[0]
            if f > max_f:
                max_f = f
    for keys in morphs.values():
        for k in keys:
            f = k[0]
            if f > max_f:
                max_f = f
    return max(1, int(max_f) + 1)

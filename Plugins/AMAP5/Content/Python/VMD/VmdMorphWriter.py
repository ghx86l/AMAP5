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


def apply_morphs(ctrl, morphs, skeleton, morph_target_names, fps):
    for name, keys in morphs.items():
        if not name:
            continue
        if name.startswith("__"):
            continue
        if name not in morph_target_names:
            continue

        curve_id = skeleton.get_curve_identifier(name, unreal.RawCurveTrackTypes.RCT_FLOAT)
        try:
            if hasattr(curve_id, "get_name") and curve_id.get_name() == "__CURVE_CONTROL":
                continue
        except Exception:
            pass

        try:
            ctrl.add_curve(curve_id, 4, False)
        except Exception:
            pass

        curve_keys = []
        for frame, w in keys:
            t = float(frame) / float(fps)
            curve_keys.append(unreal.RichCurveKey(time=t, value=float(w)))

        try:
            ctrl.set_curve_keys(curve_id, curve_keys, False)
        except Exception:
            continue

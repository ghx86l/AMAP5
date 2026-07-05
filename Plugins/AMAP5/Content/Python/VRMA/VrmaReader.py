import json
import math
import struct

_GLTF_MAGIC = 0x46546C67
_CHUNK_JSON = 0x4E4F534A
_CHUNK_BIN = 0x004E4942

_COMP_SIZE = {5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4}
_TYPE_COUNT = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}
_COMP_FMT = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}

_HUMAN_TO_VRM = {
    "hips": "J_Bip_C_Hips",
    "spine": "J_Bip_C_Spine",
    "chest": "J_Bip_C_Chest",
    "upperChest": "J_Bip_C_UpperChest",
    "neck": "J_Bip_C_Neck",
    "head": "J_Bip_C_Head",
    "leftShoulder": "J_Bip_L_Shoulder",
    "leftUpperArm": "J_Bip_L_UpperArm",
    "leftLowerArm": "J_Bip_L_LowerArm",
    "leftHand": "J_Bip_L_Hand",
    "leftThumbMetacarpal": "J_Bip_L_Thumb1",
    "leftThumbProximal": "J_Bip_L_Thumb2",
    "leftThumbDistal": "J_Bip_L_Thumb3",
    "leftIndexProximal": "J_Bip_L_Index1",
    "leftIndexIntermediate": "J_Bip_L_Index2",
    "leftIndexDistal": "J_Bip_L_Index3",
    "leftMiddleProximal": "J_Bip_L_Middle1",
    "leftMiddleIntermediate": "J_Bip_L_Middle2",
    "leftMiddleDistal": "J_Bip_L_Middle3",
    "leftRingProximal": "J_Bip_L_Ring1",
    "leftRingIntermediate": "J_Bip_L_Ring2",
    "leftRingDistal": "J_Bip_L_Ring3",
    "leftLittleProximal": "J_Bip_L_Little1",
    "leftLittleIntermediate": "J_Bip_L_Little2",
    "leftLittleDistal": "J_Bip_L_Little3",
    "rightShoulder": "J_Bip_R_Shoulder",
    "rightUpperArm": "J_Bip_R_UpperArm",
    "rightLowerArm": "J_Bip_R_LowerArm",
    "rightHand": "J_Bip_R_Hand",
    "rightThumbMetacarpal": "J_Bip_R_Thumb1",
    "rightThumbProximal": "J_Bip_R_Thumb2",
    "rightThumbDistal": "J_Bip_R_Thumb3",
    "rightIndexProximal": "J_Bip_R_Index1",
    "rightIndexIntermediate": "J_Bip_R_Index2",
    "rightIndexDistal": "J_Bip_R_Index3",
    "rightMiddleProximal": "J_Bip_R_Middle1",
    "rightMiddleIntermediate": "J_Bip_R_Middle2",
    "rightMiddleDistal": "J_Bip_R_Middle3",
    "rightRingProximal": "J_Bip_R_Ring1",
    "rightRingIntermediate": "J_Bip_R_Ring2",
    "rightRingDistal": "J_Bip_R_Ring3",
    "rightLittleProximal": "J_Bip_R_Little1",
    "rightLittleIntermediate": "J_Bip_R_Little2",
    "rightLittleDistal": "J_Bip_R_Little3",
    "leftUpperLeg": "J_Bip_L_UpperLeg",
    "leftLowerLeg": "J_Bip_L_LowerLeg",
    "leftFoot": "J_Bip_L_Foot",
    "leftToes": "J_Bip_L_ToeBase",
    "rightUpperLeg": "J_Bip_R_UpperLeg",
    "rightLowerLeg": "J_Bip_R_LowerLeg",
    "rightFoot": "J_Bip_R_Foot",
    "rightToes": "J_Bip_R_ToeBase",
}

_EXPR_TO_VRM = {
    "aa": "Fcl_MTH_A",
    "ih": "Fcl_MTH_I",
    "ou": "Fcl_MTH_U",
    "ee": "Fcl_MTH_E",
    "oh": "Fcl_MTH_O",
    "blink": "Fcl_EYE_Close",
    "blinkLeft": "Fcl_EYE_Close_L",
    "blinkRight": "Fcl_EYE_Close_R",
    "happy": "Fcl_EYE_Joy",
    "angry": "Fcl_BRW_Angry",
    "sad": "Fcl_BRW_Sorrow",
    "surprised": "Fcl_EYE_Surprised",
    "relaxed": "Fcl_BRW_Joy",
}


def _read_accessor(json_data, bin_data, accessor_idx):
    acc = json_data["accessors"][accessor_idx]
    comp_type = acc["componentType"]
    type_str = acc["type"]
    count = acc["count"]
    type_count = _TYPE_COUNT.get(type_str, 1)
    fmt = _COMP_FMT.get(comp_type, "f")
    comp_size = _COMP_SIZE.get(comp_type, 4)

    bv_idx = acc.get("bufferView")
    if bv_idx is None:
        zero = (0.0,) * type_count
        return [zero if type_count > 1 else 0.0] * count

    bv = json_data["bufferViews"][bv_idx]
    byte_offset = acc.get("byteOffset", 0) + bv.get("byteOffset", 0)
    byte_stride = bv.get("byteStride", comp_size * type_count)

    result = []
    for i in range(count):
        off = byte_offset + i * byte_stride
        vals = struct.unpack_from(f"<{type_count}{fmt}", bin_data, off)
        result.append(vals[0] if type_count == 1 else vals)
    return result


def _mat4_mul(a, b):
    return [
        [
            a[0][0] * b[0][0] + a[0][1] * b[1][0] + a[0][2] * b[2][0] + a[0][3] * b[3][0],
            a[0][0] * b[0][1] + a[0][1] * b[1][1] + a[0][2] * b[2][1] + a[0][3] * b[3][1],
            a[0][0] * b[0][2] + a[0][1] * b[1][2] + a[0][2] * b[2][2] + a[0][3] * b[3][2],
            a[0][0] * b[0][3] + a[0][1] * b[1][3] + a[0][2] * b[2][3] + a[0][3] * b[3][3],
        ],
        [
            a[1][0] * b[0][0] + a[1][1] * b[1][0] + a[1][2] * b[2][0] + a[1][3] * b[3][0],
            a[1][0] * b[0][1] + a[1][1] * b[1][1] + a[1][2] * b[2][1] + a[1][3] * b[3][1],
            a[1][0] * b[0][2] + a[1][1] * b[1][2] + a[1][2] * b[2][2] + a[1][3] * b[3][2],
            a[1][0] * b[0][3] + a[1][1] * b[1][3] + a[1][2] * b[2][3] + a[1][3] * b[3][3],
        ],
        [
            a[2][0] * b[0][0] + a[2][1] * b[1][0] + a[2][2] * b[2][0] + a[2][3] * b[3][0],
            a[2][0] * b[0][1] + a[2][1] * b[1][1] + a[2][2] * b[2][1] + a[2][3] * b[3][1],
            a[2][0] * b[0][2] + a[2][1] * b[1][2] + a[2][2] * b[2][2] + a[2][3] * b[3][2],
            a[2][0] * b[0][3] + a[2][1] * b[1][3] + a[2][2] * b[2][3] + a[2][3] * b[3][3],
        ],
        [
            a[3][0] * b[0][0] + a[3][1] * b[1][0] + a[3][2] * b[2][0] + a[3][3] * b[3][0],
            a[3][0] * b[0][1] + a[3][1] * b[1][1] + a[3][2] * b[2][1] + a[3][3] * b[3][1],
            a[3][0] * b[0][2] + a[3][1] * b[1][2] + a[3][2] * b[2][2] + a[3][3] * b[3][2],
            a[3][0] * b[0][3] + a[3][1] * b[1][3] + a[3][2] * b[2][3] + a[3][3] * b[3][3],
        ],
    ]


def _quat_to_mat3(q):
    x, y, z, w = q
    xx = x * x
    yy = y * y
    zz = z * z
    xy = x * y
    xz = x * z
    yz = y * z
    wx = w * x
    wy = w * y
    wz = w * z
    return [
        [1 - 2 * (yy + zz), 2 * (xy - wz), 2 * (xz + wy)],
        [2 * (xy + wz), 1 - 2 * (xx + zz), 2 * (yz - wx)],
        [2 * (xz - wy), 2 * (yz + wx), 1 - 2 * (xx + yy)],
    ]


def _node_local_matrix(node):
    matrix = node.get("matrix")
    if matrix is not None and len(matrix) == 16:
        return [
            [float(matrix[0]), float(matrix[4]), float(matrix[8]), float(matrix[12])],
            [float(matrix[1]), float(matrix[5]), float(matrix[9]), float(matrix[13])],
            [float(matrix[2]), float(matrix[6]), float(matrix[10]), float(matrix[14])],
            [float(matrix[3]), float(matrix[7]), float(matrix[11]), float(matrix[15])],
        ]

    t = node.get("translation", [0.0, 0.0, 0.0])
    r = node.get("rotation", [0.0, 0.0, 0.0, 1.0])
    sc = node.get("scale", [1.0, 1.0, 1.0])
    rm = _quat_to_mat3((float(r[0]), float(r[1]), float(r[2]), float(r[3])))
    sx = float(sc[0])
    sy = float(sc[1])
    sz = float(sc[2])
    return [
        [rm[0][0] * sx, rm[0][1] * sy, rm[0][2] * sz, float(t[0])],
        [rm[1][0] * sx, rm[1][1] * sy, rm[1][2] * sz, float(t[1])],
        [rm[2][0] * sx, rm[2][1] * sy, rm[2][2] * sz, float(t[2])],
        [0.0, 0.0, 0.0, 1.0],
    ]


def _build_parent_map(nodes):
    parent_map = {}
    for parent_idx, node in enumerate(nodes):
        for child_idx in node.get("children", []) or []:
            parent_map[int(child_idx)] = int(parent_idx)
    return parent_map


def _world_matrix(nodes, parent_map, node_idx, cache):
    if node_idx in cache:
        return cache[node_idx]
    local = _node_local_matrix(nodes[node_idx])
    parent_idx = parent_map.get(node_idx)
    if parent_idx is None:
        cache[node_idx] = local
        return local
    cache[node_idx] = _mat4_mul(_world_matrix(nodes, parent_map, parent_idx, cache), local)
    return cache[node_idx]


def _world_position(nodes, parent_map, node_idx, cache):
    wm = _world_matrix(nodes, parent_map, node_idx, cache)
    return (wm[0][3], wm[1][3], wm[2][3])


def _dist(a, b):
    dx = float(a[0]) - float(b[0])
    dy = float(a[1]) - float(b[1])
    dz = float(a[2]) - float(b[2])
    return math.sqrt(dx * dx + dy * dy + dz * dz)


def _human_node(human_bones, name, nodes_len):
    node_idx = human_bones.get(name, {}).get("node")
    if node_idx is None:
        return None
    node_idx = int(node_idx)
    if node_idx < 0 or node_idx >= nodes_len:
        return None
    return node_idx


def _calc_coordinate_scale_info(nodes, human_bones):
    default = {
        "coordinate_scale_n": 1,
        "coordinate_scale": 100.0,
        "human_leg_chain_length": 0.0,
        "left_leg_chain_length": 0.0,
        "right_leg_chain_length": 0.0,
    }
    needed = (
        "hips",
        "leftUpperLeg", "leftLowerLeg", "leftFoot", "leftToes",
        "rightUpperLeg", "rightLowerLeg", "rightFoot", "rightToes",
    )
    idx = {}
    for name in needed:
        node_idx = _human_node(human_bones, name, len(nodes))
        if node_idx is None:
            return default
        idx[name] = node_idx

    parent_map = _build_parent_map(nodes)
    cache = {}
    p = {}
    for name, node_idx in idx.items():
        p[name] = _world_position(nodes, parent_map, node_idx, cache)

    left_len = (
        _dist(p["hips"], p["leftUpperLeg"])
        + _dist(p["leftUpperLeg"], p["leftLowerLeg"])
        + _dist(p["leftLowerLeg"], p["leftFoot"])
        + _dist(p["leftFoot"], p["leftToes"])
    )
    right_len = (
        _dist(p["hips"], p["rightUpperLeg"])
        + _dist(p["rightUpperLeg"], p["rightLowerLeg"])
        + _dist(p["rightLowerLeg"], p["rightFoot"])
        + _dist(p["rightFoot"], p["rightToes"])
    )
    human_len = (left_len + right_len) * 0.5
    if human_len <= 0.0:
        return default

    n = math.floor((math.log10(100.0 / human_len) * 0.5) + 0.5)
    scale = math.pow(100.0, int(n))
    return {
        "coordinate_scale_n": int(n),
        "coordinate_scale": float(scale),
        "human_leg_chain_length": float(human_len),
        "left_leg_chain_length": float(left_len),
        "right_leg_chain_length": float(right_len),
    }


class VrmaReader:
    @staticmethod
    def read(path: str, fps: float = 30.0) -> dict:
        with open(path, "rb") as f:
            raw = f.read()

        magic, version, total_len = struct.unpack_from("<III", raw, 0)
        if magic != _GLTF_MAGIC:
            raise ValueError("VRMAファイルのマジックナンバーが不正です。")

        off = 12
        json_data = None
        bin_data = b""
        while off + 8 <= len(raw):
            chunk_len, chunk_type = struct.unpack_from("<II", raw, off)
            off += 8
            chunk_bytes = raw[off:off + chunk_len]
            off += chunk_len
            if chunk_type == _CHUNK_JSON:
                json_data = json.loads(chunk_bytes.decode("utf-8"))
            elif chunk_type == _CHUNK_BIN:
                bin_data = chunk_bytes

        if json_data is None:
            raise ValueError("VRMAファイルのJSONチャンクが見つかりません。")

        extensions = json_data.get("extensions", {})
        vrma_ext = extensions.get("VRMC_vrm_animation", {})

        humanoid = vrma_ext.get("humanoid", {})
        human_bones = humanoid.get("humanBones", {})
        node_to_human = {}
        for hb_name, hb_data in human_bones.items():
            node_idx = hb_data.get("node")
            if node_idx is not None:
                node_to_human[node_idx] = hb_name

        nodes = json_data.get("nodes", [])
        scale_info = _calc_coordinate_scale_info(nodes, human_bones)
        hips_node_idx = human_bones.get("hips", {}).get("node")
        if hips_node_idx is not None and hips_node_idx < len(nodes):
            _ht = nodes[hips_node_idx].get("translation", [0.0, 0.0, 0.0])
            hips_node_rest = (float(_ht[0]), float(_ht[1]), float(_ht[2]))
        else:
            hips_node_rest = (0.0, 0.0, 0.0)

        animations = json_data.get("animations", [])
        if not animations:
            return {
                "bones": {}, "bone_order": [],
                "expressions": {}, "expression_order": [],
                "duration": 0.0,
                "hips_node_rest": hips_node_rest,
                **scale_info,
            }

        anim = animations[0]
        channels = anim.get("channels", [])
        samplers = anim.get("samplers", [])

        raw_bones = {}
        expressions = {}
        bone_order = []
        expression_order = []
        max_time = 0.0

        for ch in channels:
            sampler_idx = ch.get("sampler", 0)
            target = ch.get("target", {})
            sampler = samplers[sampler_idx]
            input_acc = sampler.get("input")
            output_acc = sampler.get("output")
            if input_acc is None or output_acc is None:
                continue

            times = _read_accessor(json_data, bin_data, input_acc)
            values = _read_accessor(json_data, bin_data, output_acc)

            if not times:
                continue

            max_time = max(max_time, float(times[-1]))

            path = target.get("path", "")
            node_idx = target.get("node")

            target_ext = target.get("extensions", {})
            khr_ptr = target_ext.get("KHR_animation_pointer", {})
            pointer = khr_ptr.get("pointer", "")

            if pointer and "expressions" in pointer:
                parts = pointer.strip("/").split("/")
                expr_name = parts[-2] if len(parts) >= 2 and parts[-1] == "weight" else parts[-1]
                vrm_name = _EXPR_TO_VRM.get(expr_name, expr_name)
                if vrm_name not in expressions:
                    expressions[vrm_name] = []
                    expression_order.append(vrm_name)
                for t, v in zip(times, values):
                    frame = round(float(t) * fps)
                    w = float(v) if not isinstance(v, (list, tuple)) else float(v[0])
                    expressions[vrm_name].append((frame, w))
                continue

            if node_idx is None or path not in ("translation", "rotation"):
                continue

            human_name = node_to_human.get(node_idx)
            if human_name is None:
                continue

            vrm_bone = _HUMAN_TO_VRM.get(human_name)
            if vrm_bone is None:
                continue

            if vrm_bone not in raw_bones:
                raw_bones[vrm_bone] = {"translation": [], "rotation": []}
                bone_order.append(vrm_bone)

            raw_bones[vrm_bone][path] = list(zip(times, values))

        bones = {}
        for bone_name in bone_order:
            trans = raw_bones[bone_name].get("translation", [])
            rots = raw_bones[bone_name].get("rotation", [])

            frame_data = {}
            for t, pos in trans:
                f = round(float(t) * fps)
                if f not in frame_data:
                    frame_data[f] = [None, None]
                frame_data[f][0] = pos
            for t, rot in rots:
                f = round(float(t) * fps)
                if f not in frame_data:
                    frame_data[f] = [None, None]
                frame_data[f][1] = rot

            zero_pos = (0.0, 0.0, 0.0)
            zero_rot = (0.0, 0.0, 0.0, 1.0)

            merged = []
            for f in sorted(frame_data.keys()):
                pos = frame_data[f][0] or zero_pos
                rot = frame_data[f][1] or zero_rot
                merged.append((f, pos, rot, None))

            if merged:
                bones[bone_name] = merged

        for expr_name in expression_order:
            expressions[expr_name].sort(key=lambda x: x[0])

        return {
            "bones": bones,
            "bone_order": [b for b in bone_order if b in bones],
            "expressions": expressions,
            "expression_order": expression_order,
            "duration": max_time,
            "hips_node_rest": hips_node_rest,
            **scale_info,
        }


def _infer_total_frames(vrma: dict, fps: float = 30.0) -> int:
    duration = vrma.get("duration", 0.0)
    return max(1, round(duration * fps) + 1)

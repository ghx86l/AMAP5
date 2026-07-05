import math
import struct

from . import Coords


def read_pmx(path: str) -> dict:
    """PMXファイルを1パスで解析し、物理・IK・付与親データをすべて返す。"""
    import os
    if not path or not os.path.isfile(path):
        raise FileNotFoundError(path)

    with open(path, "rb") as f:

        # ---- ヘッダー ----
        if f.read(4) != b"PMX ":
            raise ValueError("Not a PMX file")
        version    = struct.unpack("<f", f.read(4))[0]
        data_count = f.read(1)[0]
        if data_count != 8:
            raise ValueError(f"Unexpected header data count: {data_count}")
        encoding_flag = f.read(1)[0]
        extra_uv      = f.read(1)[0]
        size_vi       = f.read(1)[0]   # 頂点インデックスサイズ
        size_ti       = f.read(1)[0]   # テクスチャインデックスサイズ
        size_mi       = f.read(1)[0]   # マテリアルインデックスサイズ
        size_bi       = f.read(1)[0]   # ボーンインデックスサイズ
        size_moi      = f.read(1)[0]   # モーフインデックスサイズ
        size_ri       = f.read(1)[0]   # 剛体インデックスサイズ
        encoding = "utf-16-le" if encoding_flag == 0 else "utf-8"

        # ---- プリミティブ読み込みヘルパー ----
        def r(n):
            b = f.read(n)
            if len(b) != n:
                raise EOFError
            return b

        def r_i8():  return struct.unpack("<b", r(1))[0]
        def r_u8():  return struct.unpack("<B", r(1))[0]
        def r_u16(): return struct.unpack("<H", r(2))[0]
        def r_i16(): return struct.unpack("<h", r(2))[0]
        def r_i32(): return struct.unpack("<i", r(4))[0]
        def r_f():   return struct.unpack("<f", r(4))[0]
        def r_f3():  return struct.unpack("<3f", r(12))

        def r_idx(size):
            """符号付きインデックス（ボーン・マテリアル・剛体 など）"""
            if size == 1: return r_i8()
            if size == 2: return r_i16()
            if size == 4: return r_i32()
            return 0

        def r_idx_v(size):
            """符号なしインデックス（頂点）"""
            if size == 1: return r_u8()
            if size == 2: return r_u16()
            if size == 4: return r_i32()
            return 0

        def r_str():
            n = r_i32()
            if n <= 0:
                return ""
            try:
                return r(n).decode(encoding, errors="replace")
            except Exception:
                return ""

        # ---- モデル情報 ----
        model_name = r_str()
        r_str(); r_str(); r_str()   # 英名・コメント・英コメント

        # ---- 頂点 ----
        for _ in range(r_i32()):
            r(12); r(12); r(8)          # 位置・法線・UV
            for _ in range(extra_uv):
                r(16)
            wt = r_u8()
            if wt == 0:
                r_idx(size_bi)
            elif wt == 1:
                r_idx(size_bi); r_idx(size_bi); r(4)
            elif wt == 2:
                for _ in range(4): r_idx(size_bi)
                r(16)
            elif wt == 3:
                r_idx(size_bi); r_idx(size_bi); r(4); r(12); r(12); r(12)
            else:   # wt == 4 (QDEF)
                for _ in range(4): r_idx(size_bi)
                r(16)
            r(4)    # エッジ倍率

        # ---- 面（インデックス） ----
        r(r_i32() * size_vi)

        # ---- テクスチャ ----
        for _ in range(r_i32()):
            r_str()

        # ---- マテリアル ----
        for _ in range(r_i32()):
            r_str(); r_str()
            r(16); r(12); r(4); r(12); r(1); r(16); r(4)
            r_idx(size_ti); r_idx(size_ti); r(1)
            if r_i8() == 0:     # Toonフラグ
                r_idx(size_ti)
            else:
                r_u8()
            r_str(); r(4)       # メモ・面数

        # ---- ボーン（IK・付与親を抽出）----
        bone_names = []
        bone_classes = []
        ik_raw     = []
        grant_raw  = []

        for bi in range(r_i32()):
            name = r_str()
            r_str()             # 英名
            r_f3()              # 位置
            r_idx(size_bi)      # 親ボーンインデックス
            bone_class = r_i32()
            flag = r_u16()

            if flag & 0x0001:   # 接続先ボーン
                r_idx(size_bi)
            else:               # 接続先オフセット
                r_f3()

            if flag & 0x0100 or flag & 0x0200:  # 付与回転 / 付与移動
                grant_parent_idx = r_idx(size_bi)
                grant_weight     = r_f()
                grant_raw.append((
                    bi,
                    grant_parent_idx,
                    grant_weight,
                    bool(flag & 0x0100),    # 付与回転
                    bool(flag & 0x0200),    # 付与移動
                ))

            if flag & 0x0400: r_f3()            # 軸固定
            if flag & 0x0800: r_f3(); r_f3()    # ローカル軸
            if flag & 0x2000: r_i32()           # 外部親変形キー

            if flag & 0x0020:   # IK
                target      = r_idx(size_bi)
                loop        = r_i32()
                angle       = r_f()
                link_count  = r_i32()
                links = []
                for _ in range(link_count):
                    lidx = r_idx(size_bi)
                    lim  = r_u8()
                    vmin = vmax = None
                    if lim:
                        vmin = r_f3()
                        vmax = r_f3()
                    links.append((lidx, bool(lim), vmin, vmax))
                ik_raw.append((bi, target, loop, angle, links))

            bone_names.append(name)
            bone_classes.append(int(bone_class))

        # ---- モーフ ----
        for _ in range(r_i32()):
            r_str(); r_str(); r(1)
            kind  = r_i8()
            count = r_i32()
            if kind == 0:                   # グループ
                for _ in range(count): r_idx(size_moi); r(4)
            elif kind == 1:                 # 頂点
                for _ in range(count): r_idx_v(size_vi); r(12)
            elif kind == 2:                 # ボーン
                for _ in range(count): r_idx(size_bi); r(12); r(16)
            elif kind in (3, 4, 5, 6, 7):  # UV系
                for _ in range(count): r_idx_v(size_vi); r(16)
            elif kind == 8:                 # マテリアル
                for _ in range(count):
                    r_idx(size_mi); r(1)
                    r(16); r(12); r(4); r(12); r(16); r(4); r(16); r(16); r(16)
            elif kind == 9:                 # フリップ
                for _ in range(count): r_idx(size_moi); r(4)
            elif kind == 10:                # インパルス
                for _ in range(count): r_idx(size_ri); r(1); r(12); r(12)
            else:
                raise ValueError(f"Unknown morph kind: {kind}")

        # ---- 表示枠 ----
        for _ in range(r_i32()):
            r_str(); r_str(); r(1)
            for _ in range(r_i32()):
                flg = r_i8()
                if flg == 0:
                    r_idx(size_bi)
                else:
                    r_idx(size_moi)

        # ---- 剛体 ----
        bodies = []
        for _ in range(r_i32()):
            rb_name = r_str()
            r_str()
            bone_idx       = r_idx(size_bi)
            rb_group       = r_u8()
            rb_pass_group  = r_u16()
            shape          = r_i8()
            rb_size        = r_f3()
            rb_pos         = r_f3()
            rb_rot         = r_f3()
            rb_mass        = r_f()
            rb_lin_damp    = r_f()
            rb_ang_damp    = r_f()
            rb_restitution = r_f()
            rb_friction    = r_f()
            rb_type        = r_i8()

            rb_bone_name = bone_names[bone_idx] if 0 <= bone_idx < len(bone_names) else ""

            radius = width = height = depth = 0.0
            if shape == 0:
                radius = 10.0 * rb_size[0]
            elif shape == 1:
                width  = 10.0 * rb_size[0]
                height = 10.0 * rb_size[1]
                depth  = 10.0 * rb_size[2]
            elif shape == 2:
                radius = 10.0 * rb_size[0]
                height = 10.0 * rb_size[1]

            rb_w_pos, calc_pos, rb_w_rot, calc_rot = Coords.calc_positions_chain_from_pmx(rb_pos, rb_rot)

            bodies.append({
                "rb_name":         rb_name,
                "rb_bone_name":    rb_bone_name,
                "rb_bone_index":   int(bone_idx),
                "group":           int(rb_group),
                "pass_group":      int(rb_pass_group),
                "type":            int(rb_type),
                "shape":           int(shape),
                "width":           round(float(width),  7),
                "radius":          round(float(radius), 7),
                "height":          round(float(height), 7),
                "depth":           round(float(depth),  7),
                "rb_w_pos.x":      round(float(rb_w_pos.x), 7),
                "rb_w_pos.y":      round(float(rb_w_pos.y), 7),
                "rb_w_pos.z":      round(float(rb_w_pos.z), 7),
                "calc_pos_x":      round(float(calc_pos[0]), 7),
                "calc_pos_y":      round(float(calc_pos[1]), 7),
                "calc_pos_z":      round(float(calc_pos[2]), 7),
                "rb_w_rot.x":      round(float(rb_w_rot.x), 7),
                "rb_w_rot.y":      round(float(rb_w_rot.y), 7),
                "rb_w_rot.z":      round(float(rb_w_rot.z), 7),
                "calc_rot_x":      round(float(calc_rot[0]), 7),
                "calc_rot_y":      round(float(calc_rot[1]), 7),
                "calc_rot_z":      round(float(calc_rot[2]), 7),
                "calc_rot_w":      round(float(calc_rot[3]), 7),
                "mass":            round(float(rb_mass), 7),
                "linear_damping":  round(float(rb_lin_damp), 7),
                "angular_damping": round(float(rb_ang_damp), 7),
                "restitution":     round(float(rb_restitution), 7),
                "friction":        round(float(rb_friction), 7),
            })

        # ---- ジョイント ----
        joints = []
        for _ in range(r_i32()):
            jt_name = r_str()
            r_str()
            kind   = r_i8()
            body_a = r_idx(size_ri)
            body_b = r_idx(size_ri)
            jt_pos      = r_f3()
            jt_rot      = r_f3()
            move_lo     = r_f3()
            move_hi     = r_f3()
            angle_lo    = r_f3()
            angle_hi    = r_f3()
            spring_move = r_f3()
            spring_rot  = r_f3()

            (
                j_w_pos, calc_pos,
                j_w_rot, calc_rot,
                calc_move_lo, calc_move_hi,
                calc_angle_lo, calc_angle_hi,
                calc_spring_move, calc_spring_rotate,
            ) = Coords.calc_joint_from_pmx(
                jt_pos, jt_rot,
                move_lo, move_hi,
                angle_lo, angle_hi,
                spring_move, spring_rot,
            )

            body_a_name = bodies[int(body_a)]["rb_name"] if 0 <= int(body_a) < len(bodies) else ""
            body_b_name = bodies[int(body_b)]["rb_name"] if 0 <= int(body_b) < len(bodies) else ""

            joints.append({
                "joint_name":           jt_name,
                "kind":                 int(kind),
                "body_a":               int(body_a),
                "body_a_name":          body_a_name,
                "body_b":               int(body_b),
                "body_b_name":          body_b_name,
                "j_w_pos.x":            round(float(j_w_pos.x), 7),
                "j_w_pos.y":            round(float(j_w_pos.y), 7),
                "j_w_pos.z":            round(float(j_w_pos.z), 7),
                "calc_pos_x":           round(float(calc_pos[0]), 7),
                "calc_pos_y":           round(float(calc_pos[1]), 7),
                "calc_pos_z":           round(float(calc_pos[2]), 7),
                "j_w_rot.x":            round(float(j_w_rot.x), 7),
                "j_w_rot.y":            round(float(j_w_rot.y), 7),
                "j_w_rot.z":            round(float(j_w_rot.z), 7),
                "calc_rot_x":           round(float(calc_rot[0]), 7),
                "calc_rot_y":           round(float(calc_rot[1]), 7),
                "calc_rot_z":           round(float(calc_rot[2]), 7),
                "calc_rot_w":           round(float(calc_rot[3]), 7),
                "calc_move_lo_x":       round(float(calc_move_lo[0]), 7),
                "calc_move_lo_y":       round(float(calc_move_lo[1]), 7),
                "calc_move_lo_z":       round(float(calc_move_lo[2]), 7),
                "calc_move_hi_x":       round(float(calc_move_hi[0]), 7),
                "calc_move_hi_y":       round(float(calc_move_hi[1]), 7),
                "calc_move_hi_z":       round(float(calc_move_hi[2]), 7),
                "calc_angle_lo_x":      round(float(calc_angle_lo[0]), 7),
                "calc_angle_lo_y":      round(float(calc_angle_lo[1]), 7),
                "calc_angle_lo_z":      round(float(calc_angle_lo[2]), 7),
                "calc_angle_hi_x":      round(float(calc_angle_hi[0]), 7),
                "calc_angle_hi_y":      round(float(calc_angle_hi[1]), 7),
                "calc_angle_hi_z":      round(float(calc_angle_hi[2]), 7),
                "calc_spring_move_x":   round(float(calc_spring_move[0]), 7),
                "calc_spring_move_y":   round(float(calc_spring_move[1]), 7),
                "calc_spring_move_z":   round(float(calc_spring_move[2]), 7),
                "calc_spring_rotate_x": round(float(calc_spring_rotate[0]), 7),
                "calc_spring_rotate_y": round(float(calc_spring_rotate[1]), 7),
                "calc_spring_rotate_z": round(float(calc_spring_rotate[2]), 7),
            })

    # ---- IKリストを構築 ----
    ik_list = []
    for bi, target, loop, angle, links in ik_raw:
        ik_name     = bone_names[bi]     if 0 <= bi     < len(bone_names) else str(bi)
        target_name = bone_names[target] if 0 <= target < len(bone_names) else str(target)
        link_items = []
        for idx, lim, vmin, vmax in links:
            n  = bone_names[idx] if 0 <= idx < len(bone_names) else str(idx)
            it = {
                "index":        idx,
                "link_bone":    n,
                "enable_lim_x": False,
                "enable_lim_y": False,
                "enable_lim_z": False,
                "min_lim":      None,
                "max_lim":      None,
            }
            if lim and vmin is not None and vmax is not None:
                deg_min = [math.degrees(vmin[i]) for i in range(3)]
                deg_max = [math.degrees(vmax[i]) for i in range(3)]
                it["enable_lim_x"] = not (deg_min[0] == 0.0 and deg_max[0] == 0.0)
                it["enable_lim_y"] = not (deg_min[1] == 0.0 and deg_max[1] == 0.0)
                it["enable_lim_z"] = not (deg_min[2] == 0.0 and deg_max[2] == 0.0)
                it["min_lim"] = deg_min
                it["max_lim"] = deg_max
            link_items.append(it)
        bone_class = bone_classes[bi] if 0 <= bi < len(bone_classes) else 0
        ik_list.append({
            "BoneClass":   int(bone_class),
            "BoneIndex":   int(bi),
            "bone_index":  bi,
            "ik_bone":     ik_name,
            "target_bone": target_name,
            "loop":        loop,
            "angle":       math.degrees(angle),
            "links":       link_items,
        })

    # ---- 付与親リストを構築 ----
    grants = []
    for bi, parent_idx, weight, rot, trans in grant_raw:
        bone_class = bone_classes[bi] if 0 <= bi < len(bone_classes) else 0
        grants.append({
            "BoneClass":         int(bone_class),
            "BoneIndex":         int(bi),
            "bone_index":        bi,
            "bone_name":         bone_names[bi]         if 0 <= bi         < len(bone_names) else str(bi),
            "grant_bone_name":   bone_names[parent_idx] if 0 <= parent_idx < len(bone_names) else str(parent_idx),
            "grant_bone_index":  parent_idx,
            "grant_weight":      round(weight, 7),
            "grant_rotation":    rot,
            "grant_translation": trans,
        })
    ik_list.sort(key=lambda x: (int(x.get("BoneClass", 0)), int(x.get("BoneIndex", 0))))
    grants.sort(key=lambda x: (int(x.get("BoneClass", 0)), int(x.get("BoneIndex", 0))))

    return {
        "file_version": float(version),
        "model_name":   model_name,
        "bodies":       bodies,
        "joints":       joints,
        "ik":           ik_list,
        "grants":       grants,
    }


# ---- 後方互換ラッパー ----

def read_pmx_physics(path: str) -> dict:
    try:
        d = read_pmx(path)
        return {
            "file_version": d["file_version"],
            "model_name":   d["model_name"],
            "bodies":       d["bodies"],
            "joints":       d["joints"],
        }
    except Exception:
        return None


def read_pmx_ik(path: str) -> dict:
    import os
    if not path or not os.path.isfile(path):
        raise FileNotFoundError(path)
    d = read_pmx(path)
    return {"model": d["model_name"], "ik": d["ik"]}

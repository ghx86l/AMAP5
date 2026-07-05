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

def _pos_mmd_to_ue(pos):
    return (float(pos[0]) * 10.0, -float(pos[2]) * 10.0, float(pos[1]) * 10.0)


def _pos_mmd_to_unity(pos):
    return (float(pos[0]) * 8.0, -float(pos[2]) * 8.0, float(pos[1]) * 8.0)


def _mat3_mul(a, b):
    return [
        [
            a[0][0] * b[0][0] + a[0][1] * b[1][0] + a[0][2] * b[2][0],
            a[0][0] * b[0][1] + a[0][1] * b[1][1] + a[0][2] * b[2][1],
            a[0][0] * b[0][2] + a[0][1] * b[1][2] + a[0][2] * b[2][2],
        ],
        [
            a[1][0] * b[0][0] + a[1][1] * b[1][0] + a[1][2] * b[2][0],
            a[1][0] * b[0][1] + a[1][1] * b[1][1] + a[1][2] * b[2][1],
            a[1][0] * b[0][2] + a[1][1] * b[1][2] + a[1][2] * b[2][2],
        ],
        [
            a[2][0] * b[0][0] + a[2][1] * b[1][0] + a[2][2] * b[2][0],
            a[2][0] * b[0][1] + a[2][1] * b[1][1] + a[2][2] * b[2][1],
            a[2][0] * b[0][2] + a[2][1] * b[1][2] + a[2][2] * b[2][2],
        ],
    ]


def _mat3_transpose(a):
    return [
        [a[0][0], a[1][0], a[2][0]],
        [a[0][1], a[1][1], a[2][1]],
        [a[0][2], a[1][2], a[2][2]],
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


def _mat3_to_quat(m):
    tr = m[0][0] + m[1][1] + m[2][2]
    if tr > 0.0:
        s = math.sqrt(tr + 1.0) * 2.0
        w = 0.25 * s
        x = (m[2][1] - m[1][2]) / s
        y = (m[0][2] - m[2][0]) / s
        z = (m[1][0] - m[0][1]) / s
    elif m[0][0] > m[1][1] and m[0][0] > m[2][2]:
        s = math.sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2.0
        w = (m[2][1] - m[1][2]) / s
        x = 0.25 * s
        y = (m[0][1] + m[1][0]) / s
        z = (m[0][2] + m[2][0]) / s
    elif m[1][1] > m[2][2]:
        s = math.sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2.0
        w = (m[0][2] - m[2][0]) / s
        x = (m[0][1] + m[1][0]) / s
        y = 0.25 * s
        z = (m[1][2] + m[2][1]) / s
    else:
        s = math.sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2.0
        w = (m[1][0] - m[0][1]) / s
        x = (m[0][2] + m[2][0]) / s
        y = (m[1][2] + m[2][1]) / s
        z = 0.25 * s
    mag = math.sqrt(x * x + y * y + z * z + w * w)
    if mag > 0.0:
        inv = 1.0 / mag
        return (x * inv, y * inv, z * inv, w * inv)
    return (0.0, 0.0, 0.0, 1.0)


def _quat_mmd_to_ue(rot):
    B = [
        [1.0, 0.0, 0.0],
        [0.0, 0.0, -1.0],
        [0.0, 1.0, 0.0],
    ]
    Bt = _mat3_transpose(B)
    Rm = _quat_to_mat3((float(rot[0]), float(rot[1]), float(rot[2]), float(rot[3])))
    Rue = _mat3_mul(_mat3_mul(B, Rm), Bt)
    return _mat3_to_quat(Rue)


def _quat_mmd_to_unity(rot):
    B = [
        [1.0,  0.0,  0.0],
        [0.0,  0.0, -1.0],
        [0.0, -1.0,  0.0],
    ]
    Bt = _mat3_transpose(B)
    Rm = _quat_to_mat3((float(rot[0]), float(rot[1]), float(rot[2]), float(rot[3])))
    Runity = _mat3_mul(_mat3_mul(B, Rm), Bt)
    return _mat3_to_quat(Runity)


def _quat_nlerp(a, b, t: float):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    dot = ax * bx + ay * by + az * bz + aw * bw
    if dot < 0.0:
        bx, by, bz, bw = -bx, -by, -bz, -bw
    x = ax + (bx - ax) * t
    y = ay + (by - ay) * t
    z = az + (bz - az) * t
    w = aw + (bw - aw) * t
    mag = math.sqrt(x * x + y * y + z * z + w * w)
    if mag > 0.0:
        inv = 1.0 / mag
        return (x * inv, y * inv, z * inv, w * inv)
    return (0.0, 0.0, 0.0, 1.0)


def _quat_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    x = aw * bx + ax * bw + ay * bz - az * by
    y = aw * by - ax * bz + ay * bw + az * bx
    z = aw * bz + ax * by - ay * bx + az * bw
    w = aw * bw - ax * bx - ay * by - az * bz
    mag = math.sqrt(x * x + y * y + z * z + w * w)
    if mag > 0.0:
        inv = 1.0 / mag
        return (x * inv, y * inv, z * inv, w * inv)
    return (0.0, 0.0, 0.0, 1.0)


def _quat_axis_angle_y(deg):
    h = math.radians(float(deg)) * 0.5
    return (0.0, math.sin(h), 0.0, math.cos(h))


def _apply_vrm_arm_a_stance(bone_name, q):
    if bone_name == "J_Bip_L_UpperArm":
        return _quat_mul(_quat_axis_angle_y(30.0), q)
    if bone_name == "J_Bip_R_UpperArm":
        return _quat_mul(_quat_axis_angle_y(-30.0), q)
    return q


def _apply_vrm_arm_a2t_stance(bone_name, q):
    if bone_name == "J_Bip_L_UpperArm":
        return _quat_mul(_quat_axis_angle_y(-30.0), q)
    if bone_name == "J_Bip_R_UpperArm":
        return _quat_mul(_quat_axis_angle_y(30.0), q)
    return q



def _interpolate_bezier(x1, y1, x2, y2, x):
    x = 0.0 if x < 0.0 else (1.0 if x > 1.0 else float(x))
    t = 0.5
    s = 0.5
    for i in range(15):
        ft = (3.0 * s * s * t * x1) + (3.0 * s * t * t * x2) + (t * t * t) - x
        if abs(ft) < 0.0001:
            break
        if ft > 0.0:
            t -= 1.0 / float(4 << i)
        else:
            t += 1.0 / float(4 << i)
        s = 1.0 - t
    return (3.0 * s * s * t * y1) + (3.0 * s * t * t * y2) + (t * t * t)


def _bezier_params(bezier16: bytes, kind: int):
    if not bezier16 or len(bezier16) < 16:
        return None
    x1 = bezier16[kind] / 127.0
    y1 = bezier16[4 + kind] / 127.0
    x2 = bezier16[8 + kind] / 127.0
    y2 = bezier16[12 + kind] / 127.0
    return x1, y1, x2, y2


def _quat_slerp(q0, q1, t):
    t = 0.0 if t < 0.0 else (1.0 if t > 1.0 else float(t))
    x0, y0, z0, w0 = q0
    x1, y1, z1, w1 = q1
    dot = x0 * x1 + y0 * y1 + z0 * z1 + w0 * w1
    if dot < 0.0:
        dot = -dot
        x1 = -x1
        y1 = -y1
        z1 = -z1
        w1 = -w1
    if dot > 0.9995:
        return _quat_nlerp((x0, y0, z0, w0), (x1, y1, z1, w1), t)
    theta_0 = math.acos(dot)
    sin_theta_0 = math.sin(theta_0)
    if sin_theta_0 == 0.0:
        return (x0, y0, z0, w0)
    theta = theta_0 * t
    sin_theta = math.sin(theta)
    s0 = math.cos(theta) - dot * sin_theta / sin_theta_0
    s1 = sin_theta / sin_theta_0
    return (x0 * s0 + x1 * s1, y0 * s0 + y1 * s1, z0 * s0 + z1 * s1, w0 * s0 + w1 * s1)


def _sample_vmd_key(keys_sorted, f):
    if not keys_sorted:
        return (0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0)
    first_f = int(keys_sorted[0][0])
    last_f = int(keys_sorted[-1][0])
    if f <= first_f:
        return keys_sorted[0][1], keys_sorted[0][2]
    if f >= last_f:
        return keys_sorted[-1][1], keys_sorted[-1][2]
    j = 0
    while j + 1 < len(keys_sorted) and int(keys_sorted[j + 1][0]) <= f:
        j += 1
    f0, pos0, rot0 = keys_sorted[j][0], keys_sorted[j][1], keys_sorted[j][2]
    f1, pos1, rot1 = keys_sorted[j + 1][0], keys_sorted[j + 1][1], keys_sorted[j + 1][2]
    f0 = int(f0)
    f1 = int(f1)
    if f1 == f0:
        return pos0, rot0
    t = float(f - f0) / float(f1 - f0)
    bez = None
    if len(keys_sorted[j + 1]) >= 4:
        bez = keys_sorted[j + 1][3]
    tx = t
    ty = t
    tz = t
    tr = t
    if bez is not None:
        px = _bezier_params(bez, 0)
        py = _bezier_params(bez, 1)
        pz = _bezier_params(bez, 2)
        pr = _bezier_params(bez, 3)
        if px is not None:
            tx = _interpolate_bezier(px[0], px[1], px[2], px[3], t)
        if py is not None:
            ty = _interpolate_bezier(py[0], py[1], py[2], py[3], t)
        if pz is not None:
            tz = _interpolate_bezier(pz[0], pz[1], pz[2], pz[3], t)
        if pr is not None:
            tr = _interpolate_bezier(pr[0], pr[1], pr[2], pr[3], t)
    pos = (
        float(pos0[0]) + (float(pos1[0]) - float(pos0[0])) * tx,
        float(pos0[1]) + (float(pos1[1]) - float(pos0[1])) * ty,
        float(pos0[2]) + (float(pos1[2]) - float(pos0[2])) * tz,
    )
    rot = _quat_slerp(rot0, rot1, tr)
    return pos, rot


def _rotation_only_keys(keys):
    out = []
    for key in sorted(keys, key=lambda x: x[0]):
        frame = key[0]
        rot = key[2]
        bezier = key[3] if len(key) >= 4 else b""
        out.append((frame, (0.0, 0.0, 0.0), rot, bezier))
    return out


def _make_vrm_test_hips_keys(bones, num_frames):
    center_keys = sorted((bones or {}).get("センター", []), key=lambda x: x[0])
    lower_keys = sorted((bones or {}).get("下半身", []), key=lambda x: x[0])
    if not center_keys and not lower_keys:
        return []
    out = []
    for f in range(int(num_frames)):
        pos, _ = _sample_vmd_key(center_keys, f)
        _, rot = _sample_vmd_key(lower_keys, f)
        out.append((f, pos, rot, b""))
    return out


def prepare_vrm_test_bones(bones, num_frames, bone_map):
    out = {}
    skip = {"センター", "下半身", "上半身", "上半身2"}
    for name, keys in (bones or {}).items():
        if name in skip:
            continue
        target = bone_map.get(name, name)
        out[target] = keys
    hips_keys = _make_vrm_test_hips_keys(bones, num_frames)
    if hips_keys:
        out["J_Bip_C_Hips"] = hips_keys
    if "上半身" in (bones or {}):
        out["J_Bip_C_Spine"] = _rotation_only_keys((bones or {})["上半身"])
    if "上半身2" in (bones or {}):
        out["J_Bip_C_Chest"] = _rotation_only_keys((bones or {})["上半身2"])
    return out


def _quat_continuity(keys):
    out = list(keys)
    for i in range(1, len(out)):
        f, pos, rot, bez = out[i]
        pf, pp, prev, pb = out[i - 1]
        dot = prev[0]*rot[0] + prev[1]*rot[1] + prev[2]*rot[2] + prev[3]*rot[3]
        if dot < 0.0:
            rot = (-rot[0], -rot[1], -rot[2], -rot[3])
        out[i] = (f, pos, rot, bez)
    return out


def _axis_angle_quat(ax, ay, az, angle_deg):
    a = math.radians(angle_deg) * 0.5
    s = math.sin(a)
    return (ax * s, ay * s, az * s, math.cos(a))


def _quat_inverse(q):
    x, y, z, w = q
    return (-x, -y, -z, w)


def _linearize_R_bezier(bez):
    if not bez or len(bez) < 16:
        return bez
    b = bytearray(bez)
    b[3] = b[7] = b[11] = b[15] = 64
    return bytes(b)


def prepare_vrm_beta_bones(bones, num_frames, bone_map):
    out = {}
    for name, keys in (bones or {}).items():
        if not keys:
            continue
        target = bone_map.get(name, name)
        out[target] = keys
    return out


def apply_bones(ctrl, bones, fps, num_frames, skeletal_mesh=None, is_vrm=False, axis_map=None, is_vrm_test=False):
    ref_pos_map = {}
    comp = None
    missing_bones = []
    created_tracks = 0
    written_tracks = 0


    if unreal is not None and skeletal_mesh is not None:
        try:
            comp = unreal.SkeletalMeshComponent()
        except Exception:
            comp = None

    if comp is not None:
        set_ok = False
        if hasattr(comp, 'set_skeletal_mesh_asset'):
            try:
                comp.set_skeletal_mesh_asset(skeletal_mesh)
                set_ok = True
            except Exception:
                set_ok = False
        if not set_ok:
            for prop in ('skeletal_mesh', 'skinned_asset', 'skeletal_mesh_asset', 'SkeletalMesh'):
                try:
                    comp.set_editor_property(prop, skeletal_mesh)
                    set_ok = True
                    break
                except Exception:
                    pass

        if set_ok:

            for bn in bones.keys():
                if not bn:
                    continue
                try:
                    bi = comp.get_bone_index(bn)
                except Exception:
                    bi = -1
                if bi is None or int(bi) < 0:
                    continue
                try:
                    rp = comp.get_ref_pose_position(int(bi))
                except Exception:
                    rp = None
                if rp is not None:
                    ref_pos_map[bn] = rp


    def _bone_exists(name: str) -> bool:
        if not name:
            return False
        if comp is None:

            return True
        try:
            bi = comp.get_bone_index(name)
            return bi is not None and int(bi) >= 0
        except Exception:
            return True


    def _ensure_bone_track(name: str) -> bool:
        try:
            if hasattr(ctrl, "add_bone_track"):
                ctrl.add_bone_track(name, False)
                return True
            if hasattr(ctrl, "insert_bone_track"):
                ctrl.insert_bone_track(name, 0, False)
                return True
        except Exception as e:
            try:
                unreal.log_warning(f"[VMD] insert/add bone track failed: {name} / {e}")
            except Exception:
                pass
        return False

    use_vrm = is_vrm or is_vrm_test
    if use_vrm and axis_map is not None:
        def _rot_to_ue(rot):
            B = [[float(v) for v in axis_map[0]], [float(v) for v in axis_map[1]], [float(v) for v in axis_map[2]]]
            Bt = _mat3_transpose(B)
            Rm = _quat_to_mat3((float(rot[0]), float(rot[1]), float(rot[2]), float(rot[3])))
            return _mat3_to_quat(_mat3_mul(_mat3_mul(B, Rm), Bt))
    else:
        _rot_to_ue = _quat_mmd_to_unity if use_vrm else _quat_mmd_to_ue
    _pos_to_ue = _pos_mmd_to_unity if use_vrm else _pos_mmd_to_ue

    for bone_name, keys in (bones or {}).items():
        if not bone_name or not keys:
            continue

        if not _bone_exists(bone_name):
            missing_bones.append(str(bone_name))
            continue

        keys_sorted = sorted(keys, key=lambda x: x[0])
        if not _ensure_bone_track(bone_name):

            missing_bones.append(str(bone_name))
            continue
        created_tracks += 1

        pos_keys = []
        rot_keys = []
        scl_keys = []
        first_f = int(keys_sorted[0][0])
        last_f = int(keys_sorted[-1][0])
        prev_q = None

        j = 0
        for f in range(int(num_frames)):
            if f <= first_f:
                pos = keys_sorted[0][1]
                rot = keys_sorted[0][2]
                p = _pos_to_ue(pos)
                q = _rot_to_ue(rot)
            elif f >= last_f:
                pos = keys_sorted[-1][1]
                rot = keys_sorted[-1][2]
                p = _pos_to_ue(pos)
                q = _rot_to_ue(rot)
            else:
                while j + 1 < len(keys_sorted) and int(keys_sorted[j + 1][0]) <= f:
                    j += 1
                f0, pos0, rot0 = keys_sorted[j][0], keys_sorted[j][1], keys_sorted[j][2]
                f1, pos1, rot1 = keys_sorted[j + 1][0], keys_sorted[j + 1][1], keys_sorted[j + 1][2]
                f0 = int(f0)
                f1 = int(f1)
                if f1 == f0:
                    p = _pos_to_ue(pos0)
                    q = _rot_to_ue(rot0)
                else:
                    t = float(f - f0) / float(f1 - f0)
                    bez = None
                    if len(keys_sorted[j + 1]) >= 4:
                        bez = keys_sorted[j + 1][3]
                    tx = t
                    ty = t
                    tz = t
                    tr = t
                    if bez is not None:
                        px = _bezier_params(bez, 0)
                        py = _bezier_params(bez, 1)
                        pz = _bezier_params(bez, 2)
                        pr = _bezier_params(bez, 3)
                        if px is not None:
                            tx = _interpolate_bezier(px[0], px[1], px[2], px[3], t)
                        if py is not None:
                            ty = _interpolate_bezier(py[0], py[1], py[2], py[3], t)
                        if pz is not None:
                            tz = _interpolate_bezier(pz[0], pz[1], pz[2], pz[3], t)
                        if pr is not None:
                            tr = _interpolate_bezier(pr[0], pr[1], pr[2], pr[3], t)

                    pos_mmd = (
                        float(pos0[0]) + (float(pos1[0]) - float(pos0[0])) * tx,
                        float(pos0[1]) + (float(pos1[1]) - float(pos0[1])) * ty,
                        float(pos0[2]) + (float(pos1[2]) - float(pos0[2])) * tz,
                    )
                    p = _pos_to_ue(pos_mmd)
                    q0 = _rot_to_ue(rot0)
                    q1 = _rot_to_ue(rot1)
                    q = _quat_slerp(q0, q1, tr)


            if bone_name in ref_pos_map:
                rp = ref_pos_map[bone_name]
                p = (p[0] + float(rp.x), p[1] + float(rp.y), p[2] + float(rp.z))
            if prev_q is not None:
                dot = prev_q[0]*q[0] + prev_q[1]*q[1] + prev_q[2]*q[2] + prev_q[3]*q[3]
                if dot < 0.0:
                    q = (-q[0], -q[1], -q[2], -q[3])
            prev_q = q
            pos_keys.append(unreal.Vector(p[0], p[1], p[2]))
            rot_keys.append(unreal.Quat(q[0], q[1], q[2], q[3]))
            scl_keys.append(unreal.Vector(1.0, 1.0, 1.0))

        try:
            ctrl.set_bone_track_keys(bone_name, pos_keys, rot_keys, scl_keys, False)
            written_tracks += 1
        except Exception as e:
            try:
                unreal.log_warning(f"[VMD] set bone track keys failed: {bone_name} / {e}")
            except Exception:
                pass
            continue


    try:
        if missing_bones:
            unreal.log_warning(f"[VMD] missing bones in skeleton: {len(missing_bones)}")
        unreal.log(f"[VMD] bone tracks created: {created_tracks}, written: {written_tracks}")
    except Exception:
        pass

import math

try:
    import unreal
    _HAS_UNREAL = True
except Exception:
    unreal = None
    _HAS_UNREAL = False


def _lerp(a, b, t):
    return a + (b - a) * t


def _quat_nlerp(a, b, t):
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


def _quat_slerp(q0, q1, t):
    t = max(0.0, min(1.0, float(t)))
    x0, y0, z0, w0 = q0
    x1, y1, z1, w1 = q1
    dot = x0 * x1 + y0 * y1 + z0 * z1 + w0 * w1
    if dot < 0.0:
        dot = -dot
        x1, y1, z1, w1 = -x1, -y1, -z1, -w1
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


def _quat_vrma_to_ue(rot):
    B = [
        [1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0],
        [0.0, 1.0, 0.0],
    ]
    Bt = _mat3_transpose(B)
    Rv = _quat_to_mat3((float(rot[0]), float(rot[1]), float(rot[2]), float(rot[3])))
    Rue = _mat3_mul(_mat3_mul(B, Rv), Bt)
    return _mat3_to_quat(Rue)


def apply_bones(ctrl, bones, fps, num_frames, skeletal_mesh=None, hips_node_rest=None, ue_hips_ref=None, position_import_scale=100.0):
    comp = None
    pos_scale = float(position_import_scale)
    ref_pos_map = {}
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
        if hasattr(comp, "set_skeletal_mesh_asset"):
            try:
                comp.set_skeletal_mesh_asset(skeletal_mesh)
                set_ok = True
            except Exception:
                pass
        if not set_ok:
            for prop in ("skeletal_mesh", "skinned_asset", "skeletal_mesh_asset", "SkeletalMesh"):
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

    def _bone_exists(name):
        if not name:
            return False
        if comp is None:
            return True
        try:
            bi = comp.get_bone_index(name)
            return bi is not None and int(bi) >= 0
        except Exception:
            return True

    def _ensure_bone_track(name):
        try:
            if hasattr(ctrl, "add_bone_track"):
                ctrl.add_bone_track(name, False)
                return True
            if hasattr(ctrl, "insert_bone_track"):
                ctrl.insert_bone_track(name, 0, False)
                return True
        except Exception as e:
            try:
                unreal.log_warning(f"[VRMA] insert/add bone track failed: {name} / {e}")
            except Exception:
                pass
        return False

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

        first_f = int(keys_sorted[0][0])
        last_f = int(keys_sorted[-1][0])

        pos_keys = []
        rot_keys = []
        scl_keys = []

        j = 0
        for f in range(int(num_frames)):
            if f <= first_f:
                pos = keys_sorted[0][1]
                rot = keys_sorted[0][2]
            elif f >= last_f:
                pos = keys_sorted[-1][1]
                rot = keys_sorted[-1][2]
            else:
                while j + 1 < len(keys_sorted) and int(keys_sorted[j + 1][0]) <= f:
                    j += 1
                f0 = int(keys_sorted[j][0])
                f1 = int(keys_sorted[j + 1][0])
                pos0 = keys_sorted[j][1]
                rot0 = keys_sorted[j][2]
                pos1 = keys_sorted[j + 1][1]
                rot1 = keys_sorted[j + 1][2]
                if f1 == f0:
                    pos = pos0
                    rot = rot0
                else:
                    t = float(f - f0) / float(f1 - f0)
                    pos = (
                        _lerp(float(pos0[0]), float(pos1[0]), t),
                        _lerp(float(pos0[1]), float(pos1[1]), t),
                        _lerp(float(pos0[2]), float(pos1[2]), t),
                    )
                    rot = _quat_slerp(
                        (float(rot0[0]), float(rot0[1]), float(rot0[2]), float(rot0[3])),
                        (float(rot1[0]), float(rot1[1]), float(rot1[2]), float(rot1[3])),
                        t,
                    )

            if (bone_name == "J_Bip_C_Hips"
                    and hips_node_rest is not None
                    and ue_hips_ref is not None):
                hrx, hry, hrz = hips_node_rest
                dx = float(pos[0]) - hrx
                dy = float(pos[1]) - hry
                dz = float(pos[2]) - hrz
                pos_keys.append(unreal.Vector(
                    ue_hips_ref[0] + dx * pos_scale,
                    ue_hips_ref[1] + dz * pos_scale,
                    ue_hips_ref[2] + dy * pos_scale,
                ))
            else:
                rp = ref_pos_map.get(bone_name) if bone_name != "J_Bip_C_Hips" else None
                ref_x = float(rp.x) if rp is not None else 0.0
                ref_y = float(rp.y) if rp is not None else 0.0
                ref_z = float(rp.z) if rp is not None else 0.0
                pos_keys.append(unreal.Vector(
                    ref_x + float(pos[0]) * pos_scale,
                    ref_y + float(pos[2]) * pos_scale,
                    ref_z + float(pos[1]) * pos_scale,
                ))
            q = _quat_vrma_to_ue(rot)
            rot_keys.append(unreal.Quat(q[0], q[1], q[2], q[3]))
            scl_keys.append(unreal.Vector(1.0, 1.0, 1.0))

        try:
            ctrl.set_bone_track_keys(bone_name, pos_keys, rot_keys, scl_keys, False)
            written_tracks += 1
        except Exception as e:
            try:
                unreal.log_warning(f"[VRMA] set bone track keys failed: {bone_name} / {e}")
            except Exception:
                pass
            continue

    try:
        if missing_bones:
            unreal.log_warning(f"[VRMA] missing bones in skeleton: {len(missing_bones)}")
        unreal.log(f"[VRMA] bone tracks created: {created_tracks}, written: {written_tracks}")
    except Exception:
        pass

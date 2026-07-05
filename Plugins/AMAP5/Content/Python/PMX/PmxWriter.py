import json
import unreal


def _to_float(v, default=0.0):
    try:
        return float(v)
    except Exception:
        return default


def _rb_data_to_json_list(rb_data):
    result = []
    for rb in rb_data or []:
        shape = int(_to_float(rb.get("shape", 0)))
        if shape == 0:
            shape_size = [_to_float(rb.get("radius", 0.0)), 0.0, 0.0]
        elif shape == 1:
            shape_size = [_to_float(rb.get("width", 0.0)),
                          _to_float(rb.get("depth", 0.0)),
                          _to_float(rb.get("height", 0.0))]
        elif shape == 2:
            shape_size = [_to_float(rb.get("radius", 0.0)),
                          _to_float(rb.get("height", 0.0)), 0.0]
        else:
            shape_size = [0.0, 0.0, 0.0]

        result.append({
            "name":            str(rb.get("rb_name", "")),
            "bone_name":       str(rb.get("rb_bone_name", "")),
            "group":           int(_to_float(rb.get("group", 0))),
            "pass_group":      int(_to_float(rb.get("pass_group", 0xFFFF))),
            "shape_type":      shape,
            "shape_size":      shape_size,
            "position":        [_to_float(rb.get("calc_pos_x", 0.0)),
                                _to_float(rb.get("calc_pos_y", 0.0)),
                                _to_float(rb.get("calc_pos_z", 0.0))],
            "rotation":        [_to_float(rb.get("calc_rot_x", 0.0)),
                                _to_float(rb.get("calc_rot_y", 0.0)),
                                _to_float(rb.get("calc_rot_z", 0.0)),
                                _to_float(rb.get("calc_rot_w", 1.0))],
            "mass":            _to_float(rb.get("mass", 1.0)),
            "linear_damping":  _to_float(rb.get("linear_damping", 0.5)),
            "angular_damping": _to_float(rb.get("angular_damping", 0.5)),
            "restitution":     _to_float(rb.get("restitution", 0.0)),
            "friction":        _to_float(rb.get("friction", 0.5)),
            "mode":            int(_to_float(rb.get("type", 0))),
        })
    return result


def _joint_data_to_json_list(joint_data):
    result = []
    for jt in joint_data or []:
        result.append({
            "name":           str(jt.get("joint_name", "")),
            "kind":           int(_to_float(jt.get("kind", 0))),
            "body_a":         int(_to_float(jt.get("body_a", -1))),
            "body_b":         int(_to_float(jt.get("body_b", -1))),
            "position":       [_to_float(jt.get("calc_pos_x", 0.0)),
                               _to_float(jt.get("calc_pos_y", 0.0)),
                               _to_float(jt.get("calc_pos_z", 0.0))],
            "rotation":       [_to_float(jt.get("calc_rot_x", 0.0)),
                               _to_float(jt.get("calc_rot_y", 0.0)),
                               _to_float(jt.get("calc_rot_z", 0.0)),
                               _to_float(jt.get("calc_rot_w", 1.0))],
            "move_lo":        [_to_float(jt.get("calc_move_lo_x", 0.0)),
                               _to_float(jt.get("calc_move_lo_y", 0.0)),
                               _to_float(jt.get("calc_move_lo_z", 0.0))],
            "move_hi":        [_to_float(jt.get("calc_move_hi_x", 0.0)),
                               _to_float(jt.get("calc_move_hi_y", 0.0)),
                               _to_float(jt.get("calc_move_hi_z", 0.0))],
            "angle_lo":       [_to_float(jt.get("calc_angle_lo_x", 0.0)),
                               _to_float(jt.get("calc_angle_lo_y", 0.0)),
                               _to_float(jt.get("calc_angle_lo_z", 0.0))],
            "angle_hi":       [_to_float(jt.get("calc_angle_hi_x", 0.0)),
                               _to_float(jt.get("calc_angle_hi_y", 0.0)),
                               _to_float(jt.get("calc_angle_hi_z", 0.0))],
            "spring_move":    [_to_float(jt.get("calc_spring_move_x", 0.0)),
                               _to_float(jt.get("calc_spring_move_y", 0.0)),
                               _to_float(jt.get("calc_spring_move_z", 0.0))],
            "spring_rotate":  [_to_float(jt.get("calc_spring_rotate_x", 0.0)),
                               _to_float(jt.get("calc_spring_rotate_y", 0.0)),
                               _to_float(jt.get("calc_spring_rotate_z", 0.0))],
        })
    return result


def _grant_data_to_json_list(grant_data):
    result = []
    for g in grant_data or []:
        bone_index = int(_to_float(g.get("BoneIndex", g.get("bone_index", -1))))
        result.append({
            "BoneClass":         int(_to_float(g.get("BoneClass", g.get("bone_class", 0)))),
            "BoneIndex":         bone_index,
            "bone_index":        bone_index,
            "bone_name":         str(g.get("bone_name", "")),
            "grant_bone_name":   str(g.get("grant_bone_name", "")),
            "grant_weight":      _to_float(g.get("grant_weight", 1.0)),
            "grant_rotation":    bool(g.get("grant_rotation", False)),
            "grant_translation": bool(g.get("grant_translation", False)),
        })
    return result


def apply_rb_limits_to_selected_nodes(rb_data, targets, anim_bp):
    if not rb_data or anim_bp is None:
        return False
    json_str = json.dumps(_rb_data_to_json_list(rb_data), ensure_ascii=False)
    result = unreal.PPR_LoaderLibrary.apply_pmx_physics_to_anim_blueprint(anim_bp, json_str)
    if result:
        unreal.log(f"[Pmx] 完了: {len(rb_data)}件の剛体を適用しました。")
    else:
        unreal.log_warning("[Pmx] apply_pmx_physics_to_anim_blueprint が False を返しました。")
    return result


def apply_joints_to_anim_blueprint(joint_data, anim_bp):
    if not joint_data or anim_bp is None:
        return False
    json_str = json.dumps(_joint_data_to_json_list(joint_data), ensure_ascii=False)
    try:
        result = unreal.PPR_LoaderLibrary.apply_pmx_joints_to_anim_blueprint(anim_bp, json_str)
    except AttributeError:
        unreal.log_warning("[Pmx] apply_pmx_joints_to_anim_blueprint は未対応のためスキップしました。")
        return True
    if result:
        unreal.log(f"[Pmx] 完了: {len(joint_data)}件のジョイントを適用しました。")
    else:
        unreal.log_warning("[Pmx] apply_pmx_joints_to_anim_blueprint が False を返しました。")
    return result


def apply_physics_and_joints(rb_data, joint_data, anim_bp):
    if not rb_data or anim_bp is None:
        return False
    rb_json = json.dumps(_rb_data_to_json_list(rb_data), ensure_ascii=False)
    jt_json = json.dumps(_joint_data_to_json_list(joint_data or []), ensure_ascii=False)
    try:
        result = unreal.PPR_LoaderLibrary.apply_pmx_physics_and_joints_to_anim_blueprint(anim_bp, rb_json, jt_json)
        if result:
            unreal.log(f"[Pmx] 完了: {len(rb_data)}件の剛体, {len(joint_data or [])}件のジョイントを適用しました。")
        else:
            unreal.log_warning("[Pmx] apply_pmx_physics_and_joints_to_anim_blueprint が False を返しました。")
        return result
    except AttributeError:
        unreal.log_warning("[Pmx] apply_pmx_physics_and_joints_to_anim_blueprint は未対応のため剛体のみ適用します。")
        result = unreal.PPR_LoaderLibrary.apply_pmx_physics_to_anim_blueprint(anim_bp, rb_json)
        if result:
            unreal.log(f"[Pmx] 完了: {len(rb_data)}件の剛体を適用しました。(ジョイントはスキップ)")
        else:
            unreal.log_warning("[Pmx] apply_pmx_physics_to_anim_blueprint が False を返しました。")
        return result


def apply_grants_to_anim_blueprint(grant_data, anim_bp):
    if not grant_data or anim_bp is None:
        return False
    json_str = json.dumps(_grant_data_to_json_list(grant_data), ensure_ascii=False)
    result = unreal.IK_LoaderImportLibrary.apply_pmx_grants_to_anim_blueprint(anim_bp, json_str)
    if result:
        unreal.log(f"[Pmx] 完了: {len(grant_data)}件の付与を適用しました。")
    else:
        unreal.log_warning("[Pmx] apply_pmx_grants_to_anim_blueprint が False を返しました。")
    return result

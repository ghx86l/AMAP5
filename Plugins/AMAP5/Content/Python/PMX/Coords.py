import math
from collections import namedtuple

Vec3 = namedtuple("Vec3", ["x", "y", "z"])

_EULER_ORDER = "YXZ"


def _to_float(v, default=0.0):
    try:
        return float(v)
    except Exception:
        return default


def _map_axes_xyz_to_x_negz_y(values):
    x = _to_float(values[0])
    y = _to_float(values[1])
    z = _to_float(values[2])
    return (x, -z, y)


def _map_limit_axes_xyz_to_x_negz_y(low, high):
    return (
        _to_float(low[0]),
        -_to_float(high[2]),
        _to_float(low[1]),
    ), (
        _to_float(high[0]),
        -_to_float(low[2]),
        _to_float(high[1]),
    )


def _quat_mul(q1, q2):
    x1, y1, z1, w1 = q1
    x2, y2, z2, w2 = q2
    return (
        w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
        w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
        w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2,
        w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2,
    )


def _euler_xyz_to_quat(rx, ry, rz):
    cx = math.cos(rx * 0.5)
    sx = math.sin(rx * 0.5)
    cy = math.cos(ry * 0.5)
    sy = math.sin(ry * 0.5)
    cz = math.cos(rz * 0.5)
    sz = math.sin(rz * 0.5)
    qx = (sx, 0.0, 0.0, cx)
    qy = (0.0, sy, 0.0, cy)
    qz = (0.0, 0.0, sz, cz)
    order = _EULER_ORDER.upper()
    if order == "XYZ":
        return _quat_mul(_quat_mul(qx, qy), qz)
    if order == "XZY":
        return _quat_mul(_quat_mul(qx, qz), qy)
    if order == "YXZ":
        return _quat_mul(_quat_mul(qy, qx), qz)
    if order == "YZX":
        return _quat_mul(_quat_mul(qy, qz), qx)
    if order == "ZXY":
        return _quat_mul(_quat_mul(qz, qx), qy)
    if order == "ZYX":
        return _quat_mul(_quat_mul(qz, qy), qx)
    raise ValueError(_EULER_ORDER)


def _map_position_xyz10_to_x_negz_y(values):
    mapped = _map_axes_xyz_to_x_negz_y(values)
    return (
        mapped[0] * 10.0,
        mapped[1] * 10.0,
        mapped[2] * 10.0,
    )


def _quat_xyz_to_x_negz_y(quat):
    mapped = _map_axes_xyz_to_x_negz_y(quat)
    return (
        mapped[0],
        mapped[1],
        mapped[2],
        _to_float(quat[3], 1.0),
    )


def _calc_initial_rot_quat_from_pmx(rot):
    return _quat_xyz_to_x_negz_y(
        _euler_xyz_to_quat(
            _to_float(rot[0]),
            _to_float(rot[1]),
            _to_float(rot[2]),
        )
    )



def calc_positions_chain_from_pmx(rigidbody_world_pos, rigidbody_world_rot):
    rb_w_pos = Vec3(
        x=_to_float(rigidbody_world_pos[0]) * 10.0,
        y=_to_float(rigidbody_world_pos[1]) * 10.0,
        z=_to_float(rigidbody_world_pos[2]) * 10.0,
    )
    calc_pos = _map_position_xyz10_to_x_negz_y(rigidbody_world_pos)
    rb_w_rot = Vec3(
        x=_to_float(rigidbody_world_rot[0]),
        y=_to_float(rigidbody_world_rot[1]),
        z=_to_float(rigidbody_world_rot[2]),
    )
    calc_rot = _calc_initial_rot_quat_from_pmx(rb_w_rot)
    return rb_w_pos, calc_pos, rb_w_rot, calc_rot


def calc_joint_from_pmx(joint_world_pos, joint_world_rot, move_lo, move_hi, angle_lo, angle_hi, spring_move, spring_rotate):
    j_w_pos = Vec3(
        x=_to_float(joint_world_pos[0]) * 10.0,
        y=_to_float(joint_world_pos[1]) * 10.0,
        z=_to_float(joint_world_pos[2]) * 10.0,
    )
    calc_pos = _map_position_xyz10_to_x_negz_y(joint_world_pos)
    j_w_rot = Vec3(
        x=_to_float(joint_world_rot[0]),
        y=_to_float(joint_world_rot[1]),
        z=_to_float(joint_world_rot[2]),
    )
    calc_rot = _calc_initial_rot_quat_from_pmx(j_w_rot)
    _move_lo_raw, _move_hi_raw = _map_limit_axes_xyz_to_x_negz_y(move_lo, move_hi)
    calc_move_lo = (
        _move_lo_raw[0] * 10.0,
        _move_lo_raw[1] * 10.0,
        _move_lo_raw[2] * 10.0,
    )
    calc_move_hi = (
        _move_hi_raw[0] * 10.0,
        _move_hi_raw[1] * 10.0,
        _move_hi_raw[2] * 10.0,
    )
    calc_angle_lo, calc_angle_hi = _map_limit_axes_xyz_to_x_negz_y(angle_lo, angle_hi)
    calc_spring_move = (
        _to_float(spring_move[0]),
        _to_float(spring_move[2]),
        _to_float(spring_move[1]),
    )
    calc_spring_rotate = (
        _to_float(spring_rotate[0]),
        _to_float(spring_rotate[2]),
        _to_float(spring_rotate[1]),
    )
    return (
        j_w_pos, calc_pos,
        j_w_rot, calc_rot,
        calc_move_lo, calc_move_hi,
        calc_angle_lo, calc_angle_hi,
        calc_spring_move, calc_spring_rotate,
    )

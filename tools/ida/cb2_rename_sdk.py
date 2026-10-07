# cb2_rename_sdk.py - a third-party DLL built on a CodeRed-generated UE3 SDK: name every SDK wrapper after the
# "Function Pkg.Class.Func" / "Class Pkg.Name" string it references (each wrapper is the only user of its
# string), apply the FIXED names below (from reading cb1), then re-decompile the mod core range so calls
# read as fn_Class__Func(). CORE_RANGE and FIXED are per DLL: edit them. Output gitignored (third-party code).
import os, re
import idaapi, idautils, idc, ida_auto, ida_bytes, ida_funcs, ida_hexrays, ida_name, ida_nalt
OUT_PATH = os.environ.get("DVR_IDA_OUT")
ida_auto.auto_wait(); base = idaapi.get_imagebase(); ida_hexrays.init_hexrays_plugin()
L = []; w = L.append
named = 0
# CorvoBody v1.1 (md5 dd05f9aa...), from cb1: the hook handlers StartAddress installs and the helpers read.
CORE_RANGE = (0x10001000, 0x1001C300)
fixed = {0x10004850: "hook_GameEngine_Tick", 0x10004AC0: "hook_CollectGarbage", 0x10004F20: "hook_StaticExit",
         0x10005180: "hook_GetPlayerViewPoint", 0x10005DD0: "body_update", 0x10001B40: "load_settings",
         0x100010C0: "cb_log", 0x10001890: "chain_load", 0x10001180: "init_paths", 0x10011E40: "resolve_bone_names",
         0x10011FA0: "attach_body", 0x10012050: "log_camera_curve", 0x10012480: "try_next_body_source",
         0x100125C0: "find_mesh_by_name", 0x10012280: "detach_shared_arrays"}
for ea, nm in fixed.items():
    if ida_funcs.get_func(ea): ida_name.set_name(ea, nm, ida_name.SN_FORCE); named += 1
for s in idautils.Strings():
    txt = str(s)
    m = re.match(r'^(Function|Class) ([A-Za-z0-9_]+)\.([A-Za-z0-9_.]+)$', txt)
    if not m: continue
    users = sorted(set(ida_funcs.get_func(x).start_ea for x in idautils.DataRefsTo(s.ea) if ida_funcs.get_func(x)))
    users = [u for u in users if u >= CORE_RANGE[1]]
    if len(users) != 1: continue
    tail = m.group(3).replace(".", "__")
    nm = ("fn_" if m.group(1) == "Function" else "cls_") + tail
    cur = ida_name.get_name(users[0]) or ""
    if cur.startswith("sub_"):
        ida_name.set_name(users[0], nm, ida_name.SN_FORCE | ida_name.SN_NOCHECK); named += 1
w("named %d functions" % named)
C = []
for f in idautils.Functions(*CORE_RANGE):
    fn = ida_funcs.get_func(f)
    C.append("// ===== 0x%08X (+0x%X) %s size %d" % (f, f-base, ida_name.get_name(f), fn.end_ea-fn.start_ea))
    try: C.append(str(ida_hexrays.decompile(f)))
    except Exception as e: C.append("// DECOMPILE FAILED: %s" % e)
    C.append("")
with open(os.path.join(os.path.dirname(OUT_PATH), "cb2_core_named_%s.c" % os.path.splitext(ida_nalt.get_root_filename())[0]), "w", encoding="utf-8") as fh: fh.write("\n".join(C))
with open(OUT_PATH, "w", encoding="utf-8") as fh: fh.write("\n".join(L)+"\n")
idc.qexit(0)

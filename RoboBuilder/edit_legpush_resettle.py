import shutil, glob, re

def backup(path):
    existing = glob.glob(path + ".bak*")
    nums = [int(re.search(r"\.bak(\d+)$", f).group(1)) for f in existing if re.search(r"\.bak(\d+)$", f)]
    n = max(nums) + 1 if nums else 1
    b = f"{path}.bak{n}"
    shutil.copyfile(path, b)
    print("backup:", b)
    return b

path = "canvas.c"
backup(path)
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = """                    translateActiveRobot(0.0f, clear);
                    printf("[LEGPUSH] kneeAngle=%.2f pushed body up by %.5f to keep the leg clear\\n",
                           r->kneeAngle, clear);
                }"""
assert content.count(old) == 1, f"old match count: {content.count(old)}"

new = """                    translateActiveRobot(0.0f, clear);
                    printf("[LEGPUSH] kneeAngle=%.2f pushed body up by %.5f to keep the leg clear\\n",
                           r->kneeAngle, clear);

                    // Re-arm advanceRockySettle -- a real report showed
                    // the body ending up visibly floating after a long
                    // run of E/Q presses, each individually a small
                    // necessary lift, none of them ever given back. The
                    // reason: this lift keeps the body just barely clear
                    // (same as a normal resting contact), so gravity's own
                    // wasLanded/suppressThisCall bookkeeping in
                    // applyGravityStep never sees a fresh "was airborne,
                    // now landed" transition here -- it just looks like
                    // the SAME landing continuing -- so rockySettleConverged
                    // (already TRUE from settling once before) is never
                    // reset, and advanceRockySettle keeps a no-op return
                    // forever after, leaving every one of these small
                    // pushes permanently un-settled. Resetting both flags
                    // right here, the same way a fresh landing does,
                    // means the very next gravity tick (auto-gravity's
                    // timer, or a plain G press) gets a full-resolution
                    // settle pass that can pull the body back down as far
                    // as this new knee angle actually allows.
                    rockySettleConverged = FALSE;
                    rockyKneeSettleStep = SIMULATION_LEG_SETTLE_STEP_DEG;
                    rockyBodySettleStep = SIMULATION_BODY_SETTLE_STEP_DEG;
                }"""

content = content.replace(old, new)
with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("done")

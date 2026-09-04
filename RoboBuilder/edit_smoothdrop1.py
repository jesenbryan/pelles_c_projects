import shutil, glob, re

def backup(path):
    existing = glob.glob(path + ".bak*")
    nums = [int(re.search(r"\.bak(\d+)$", f).group(1)) for f in existing if re.search(r"\.bak(\d+)$", f)]
    n = max(nums) + 1 if nums else 1
    b = f"{path}.bak{n}"
    shutil.copyfile(path, b)
    print("backup:", b)
    return b

# ---- config.h: add the new cap constant ----
path = "config.h"
backup(path)
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = """#define SIMULATION_LEG_SETTLE_MIN_STEP_DEG 0.01f
"""
assert content.count(old) == 1, f"config old match count: {content.count(old)}"

new = old + """
// Hard cap, in world units, on how far advanceRockySettle is allowed to
// actually TRANSLATE the body in a single tick when a probe finds a real
// drop to commit. Without this, a real report showed the body appearing
// to sit still (small angle steps swinging the dangling foot, or tiny
// sub-threshold drops) and then suddenly SNAP down by a large amount the
// instant dropActiveRobotToRest's search finally found a big opening --
// its search isn't bounded by the small per-tick angle step above, only
// by SIMULATION_LEG_PUSH_SEARCH_MAX, so a single 1-degree knee/body
// change can reveal a resting depth far bigger than the angle change
// itself would suggest (a long shin arc swinging past a critical point,
// for instance). Capping the committed translate to the same order of
// magnitude as ordinary per-tick gravity (SIMULATION_GRAVITY_STEP) means
// any leftover drop just gets picked up on the following tick(s) instead
// -- either by applyGravityStep's own normal fall (if the remaining gap
// is bigger than one gravity step) or by advanceRockySettle re-probing
// again immediately (if it's smaller), so a big resting-depth discovery
// still reads as a smooth multi-frame sink instead of an instant snap.
#define SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK SIMULATION_GRAVITY_STEP
"""

content = content.replace(old, new)
with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("config.h done")

# ---- canvas.c: cap the two commit sites ----
path = "canvas.c"
backup(path)
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old_knee = """        if (bestDrop > SIMULATION_LEG_SETTLE_MIN_DROP)
        {
            r->kneeAngle = bestKneeAngle;
            translateActiveRobot(0.0f, -bestDrop);
            printf("[SETTLE] kneeAngle=%.2f bent the knee %.4f deg, body fell %.5f further this tick\\n",
                   r->kneeAngle, rockyKneeSettleStep, bestDrop);
            // Keep this probe's step size as-is -- it's still finding
            // real gains at this scale, no need to refine yet.
        }"""
assert content.count(old_knee) == 1, f"old_knee match count: {content.count(old_knee)}"

new_knee = """        if (bestDrop > SIMULATION_LEG_SETTLE_MIN_DROP)
        {
            // Cap what's actually applied THIS tick -- see
            // SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK's own comment: a
            // small angle step can still uncover a large resting-depth
            // change, and applying all of it in one tick is what read as
            // a sudden snap after looking "stuck." Any amount left over
            // just shows up again as a fresh bestDrop next tick (from the
            // now-lower position), so nothing is lost, only spread out.
            float appliedDrop = bestDrop;
            if (appliedDrop > SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK)
                appliedDrop = SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK;

            r->kneeAngle = bestKneeAngle;
            translateActiveRobot(0.0f, -appliedDrop);
            printf("[SETTLE] kneeAngle=%.2f bent the knee %.4f deg, body fell %.5f further this tick (of %.5f available)\\n",
                   r->kneeAngle, rockyKneeSettleStep, appliedDrop, bestDrop);
            // Keep this probe's step size as-is -- it's still finding
            // real gains at this scale, no need to refine yet.
        }"""

content = content.replace(old_knee, new_knee)

old_body = """        if (bestDrop > SIMULATION_LEG_SETTLE_MIN_DROP)
        {
            r->angle = bestBodyAngle;
            translateActiveRobot(0.0f, -bestDrop);
            printf("[SETTLE] bodyAngle=%.2f rotated the rectangle %.4f deg, body fell %.5f further this tick\\n",
                   r->angle, rockyBodySettleStep, bestDrop);
        }"""
assert content.count(old_body) == 1, f"old_body match count: {content.count(old_body)}"

new_body = """        if (bestDrop > SIMULATION_LEG_SETTLE_MIN_DROP)
        {
            // Same per-tick cap as the knee probe above, same reason --
            // see SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK's comment.
            float appliedDrop = bestDrop;
            if (appliedDrop > SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK)
                appliedDrop = SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK;

            r->angle = bestBodyAngle;
            translateActiveRobot(0.0f, -appliedDrop);
            printf("[SETTLE] bodyAngle=%.2f rotated the rectangle %.4f deg, body fell %.5f further this tick (of %.5f available)\\n",
                   r->angle, rockyBodySettleStep, appliedDrop, bestDrop);
        }"""

content = content.replace(old_body, new_body)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("canvas.c done")

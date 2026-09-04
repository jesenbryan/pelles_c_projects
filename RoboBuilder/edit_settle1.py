import shutil, glob

path = "config.h"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''#define SIMULATION_LEG_PUSH_SEARCH_START 0.02f
#define SIMULATION_LEG_PUSH_SEARCH_MAX 1.0f'''

new = '''#define SIMULATION_LEG_PUSH_SEARCH_START 0.02f
#define SIMULATION_LEG_PUSH_SEARCH_MAX 1.0f

// Rocky-only, and only relevant once the knee can be detached from the
// rectangle (see app.h's own comment on draggingRockyKnee): once gravity
// lands the robot, whichever part is LOWEST (often the foot, if the knee
// was dragged far from the body) is what stops the fall -- the rectangle
// just ends up wherever that leaves it, which can look like it's
// floating well above the ground with a big gap underneath. See
// canvas.c's settleRockyKneeUnderBody: right after landing, it hill-
// climbs kneeAngle (foot stays roughly where it landed, knee bends) to
// let the body fall further wherever that's geometrically possible,
// closing that gap instead of leaving it there permanently.
//
// Deliberately a ONE-SHOT search run once per fresh landing, not a
// per-tick nudge -- see applyGravityStep's own comment on why an
// earlier, similar per-tick idea (slope alignment) was removed for
// reading as a visible bounce/wobble. STEP_DEG is the search's starting
// angle increment, halved SIMULATION_LEG_SETTLE_REFINEMENTS times (so
// smaller than pressing E/Q once, and fine enough by the last pass to
// settle right up against the true limit instead of stopping early).
#define SIMULATION_LEG_SETTLE_STEP_DEG 15.0f
#define SIMULATION_LEG_SETTLE_REFINEMENTS 10'''

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

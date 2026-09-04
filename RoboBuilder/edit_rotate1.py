import shutil, glob

path = "config.h"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''// Rocky-only, and only relevant once the knee can be detached from the
// rectangle (see app.h's own comment on draggingRockyKnee): the
// whole-body gravity drop above stops the instant its LOWEST part (often
// the foot, if the knee was dragged far from the body) touches something
// -- the rectangle just ends up wherever that leaves it, which can look
// like it's floating well above the ground with a big gap underneath.
//
// See canvas.c's advanceRockyKneeSettle: called every gravity tick
// (applyGravityStep, alongside the whole-body translate above) while any
// part of Rocky is touching something, it tries bending the knee by this
// many degrees in each direction and keeps whichever direction (if
// either) lets the body fall further, closing that gap gradually over
// several ticks instead of all at once. By explicit request this runs
// EVERY tick, continuously, rather than once after landing -- earlier,
// a similar continuous idea (slope alignment, see applyGravityStep's own
// comment) caused a visible bounce/wobble, but that one accumulated a
// velocity/damping state across ticks that could overshoot. This has no
// such state: every tick is an independent deterministic search
// (dropActiveRobotToRest, the same binary-search idiom the rest of
// gravity already uses) that only ever COMMITS a change when it
// confirms the body genuinely falls further and stays collision-free --
// there's nothing to oscillate, so it can settle smoothly without the
// stuck-state risk the old knee pendulum had.
//
// Kept deliberately small (smaller than one E/Q press,
// SIMULATION_JOINT_ROTATE_STEP_DEG) so the settle reads as a smooth
// gradual sink across many ticks rather than a single visible snap.
#define SIMULATION_LEG_SETTLE_STEP_DEG 1.0f'''

new = '''// Rocky-only, and only relevant once the knee can be detached from the
// rectangle (see app.h's own comment on draggingRockyKnee): the
// whole-body gravity drop above stops the instant its LOWEST part (often
// the foot, if the knee was dragged far from the body) touches something
// -- the rectangle just ends up wherever that leaves it, which can look
// like it's floating well above the ground with a big gap underneath.
//
// See canvas.c's advanceRockySettle: called every gravity tick
// (applyGravityStep, alongside the whole-body translate above) while any
// part of Rocky is touching something, it tries two independent things,
// each by one small step in each direction, and keeps whichever helps:
// bending the KNEE (SIMULATION_LEG_SETTLE_STEP_DEG) and rotating the
// RECTANGLE itself (SIMULATION_BODY_SETTLE_STEP_DEG, Rocky's own
// `angle` field) -- so the torso can tip toward a more natural resting
// orientation, not just sink straight down at whatever angle it started
// at. Either one closes the "floating body" gap gradually over several
// ticks instead of all at once. By explicit request this runs EVERY
// tick, continuously, rather than once after landing -- earlier, a
// similar continuous idea (slope alignment, see applyGravityStep's own
// comment) caused a visible bounce/wobble, but that one accumulated a
// velocity/damping state across ticks that could overshoot. This has no
// such state: every tick is an independent deterministic search
// (dropActiveRobotToRest, the same binary-search idiom the rest of
// gravity already uses) that only ever COMMITS a change when it
// confirms the body genuinely falls further and stays collision-free --
// there's nothing to oscillate, so it can settle smoothly without the
// stuck-state risk the old knee pendulum had.
//
// Both kept deliberately small (smaller than one E/Q press or one
// Left/Right whole-body rotate press, SIMULATION_JOINT_ROTATE_STEP_DEG/
// SIMULATION_WHOLE_BODY_ROTATE_STEP_DEG) so the settle reads as a smooth
// gradual sink across many ticks rather than a single visible snap.
#define SIMULATION_LEG_SETTLE_STEP_DEG 1.0f
#define SIMULATION_BODY_SETTLE_STEP_DEG 1.0f'''

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

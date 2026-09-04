import shutil, glob

path = "config.h"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''// E/Q's "glide over an obstruction" behavior (sweeping past a contact and
// jumping to clear ground on the far side) was removed by explicit
// request -- the foot was technically still passing through solid ground
// to get there, which is wrong regardless of how it reads. E/Q now hard-
// stops at first contact (see canvas.c's WM_KEYDOWN, ROBOT_KIND_ROCKY
// branch) using the same GRAVITY_CONTACT_SEARCH_ITERATIONS binary search
// applyGravityStep uses for the whole-body landing.'''

new = '''// E/Q's "glide over an obstruction" behavior (sweeping past a contact and
// jumping to clear ground on the far side) was removed by explicit
// request -- the foot was technically still passing through solid ground
// to get there, which is wrong regardless of how it reads.
//
// What replaced it (see canvas.c's WM_KEYDOWN, ROBOT_KIND_ROCKY branch)
// is a real lever, not a freeze: rotating the knee always applies in
// full, and if that would bury the leg (the foot circle OR either shin
// fillet arc -- see the Rocky struct's own comment on shinArc1Angle/
// shinArc2Angle in app.h) in the environment, the whole robot is pushed
// straight up instead, exactly like a real leg pressing its foot/shin
// against the ground would lever the torso upward rather than the leg
// simply stopping dead. Only when even a generous push (see
// SIMULATION_LEG_PUSH_SEARCH_MAX below) still can't clear it -- the leg
// boxed in on every side, an extreme edge case -- does it fall back to
// the old hard-stop-at-first-contact behavior, using the same
// GRAVITY_CONTACT_SEARCH_ITERATIONS binary search applyGravityStep uses
// for the whole-body landing.

// Starting distance and sanity cap for the "push the body up" search
// above. Doubles from START each attempt until the leg clears or MAX is
// reached, then binary-searches within whatever range that found --
// unlike SIMULATION_SLOPE_CORRECTION_MAX above (a small fixed budget for
// tiny post-ROTATION residuals from re-aligning to a slope), a
// deliberate leg extension can legitimately need to lift the body by far
// more than that, and there's no way to know the right amount in
// advance. Both are in the robot's own local world units, same space
// SIMULATION_GRAVITY_STEP is in -- START is one gravity step, MAX is
// several times Rocky's default knee-to-foot leg length (~0.3, see
// app_init.c's initRockyStandingPosition) so a genuinely large push
// still succeeds instead of being mistaken for "impossible."
#define SIMULATION_LEG_PUSH_SEARCH_START 0.02f
#define SIMULATION_LEG_PUSH_SEARCH_MAX 1.0f'''

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

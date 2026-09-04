import shutil, glob

path = "config.h"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''#define SIMULATION_LEG_SETTLE_STEP_DEG 1.0f
#define SIMULATION_BODY_SETTLE_STEP_DEG 1.0f'''

new = '''#define SIMULATION_LEG_SETTLE_STEP_DEG 1.0f
#define SIMULATION_BODY_SETTLE_STEP_DEG 1.0f

// The smallest drop advanceRockySettle will actually treat as "this step
// helped." dropActiveRobotToRest's own binary search (GRAVITY_CONTACT_
// SEARCH_ITERATIONS above) only converges to within roughly 1/2^12 of
// whatever range it grew to -- close to the true contact point, but not
// exact -- so right at the real resting pose, re-probing a tiny knee/body
// rotation can keep reporting a wafer-thin "drop" that's really just that
// search's own precision noise, not a genuine further settle. Without a
// floor, advanceRockySettle would happily commit that noise every single
// tick forever (never reaching a tick with truly nothing left to
// improve), which is exactly what reads as visible vibration instead of
// a clean stop. Comparing against this instead of a bare > 0.0f is what
// lets it actually finish and go quiet once it's really done.
#define SIMULATION_LEG_SETTLE_MIN_DROP 0.001f'''

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

import shutil, glob

path = "config.h"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''// lets it actually finish and go quiet once it's really done.
#define SIMULATION_LEG_SETTLE_MIN_DROP 0.001f'''

new = '''// lets it actually finish and go quiet once it's really done.
#define SIMULATION_LEG_SETTLE_MIN_DROP 0.001f

// Floor for advanceRockySettle's own per-probe step size (separate from
// SIMULATION_LEG_SETTLE_STEP_DEG/SIMULATION_BODY_SETTLE_STEP_DEG above,
// which are just the STARTING size). A fixed 1-degree-ish step that
// never shrinks can decide "converged" too early -- there are poses
// where the real remaining improvement at THAT step size rounds down
// under SIMULATION_LEG_SETTLE_MIN_DROP even though a genuinely lower
// resting position exists a finer fraction of a degree away (the same
// "coarse step overshoots a narrow useful window" problem the original
// one-shot version of this search used to guard against by halving its
// step on every failed attempt). So now each probe does the same: halve
// its OWN step whenever a try finds nothing worth committing, instead of
// immediately giving up, and only that probe's search is considered
// exhausted once its step has shrunk below this floor. Both probes have
// to be exhausted before advanceRockySettle calls the whole thing
// converged and stops for good -- see rockySettleConverged.
#define SIMULATION_LEG_SETTLE_MIN_STEP_DEG 0.01f'''

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

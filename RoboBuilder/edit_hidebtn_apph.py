import shutil, glob, re

def backup(path):
    existing = glob.glob(path + ".bak*")
    nums = [int(re.search(r"\.bak(\d+)$", f).group(1)) for f in existing if re.search(r"\.bak(\d+)$", f)]
    n = max(nums) + 1 if nums else 1
    b = f"{path}.bak{n}"
    shutil.copyfile(path, b)
    print("backup:", b)
    return b

path = "app.h"
backup(path)
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

# 1. Add two button IDs, right after ID_WEIGHT_RATIO_SLIDER's definition.
old_ids = "#define ID_WEIGHT_RATIO_SLIDER 1015\n"
assert content.count(old_ids) == 1
new_ids = old_ids + """
// Testing-only toggles, Rocky editor only (Design > Robot, Rocky
// selected) -- lets you drop the leg (knee/foot circles + shin arcs) or
// the rectangular body out of both rendering and collision entirely, one
// at a time, so a specific part's behavior (does gravity/settle work
// right for JUST the body, or JUST the leg) can be tested in isolation
// without the other part's own geometry interfering. See Rocky's own
// legHidden/bodyHidden fields and input.c's WM_COMMAND handling.
#define ID_ROCKY_TOGGLE_LEG_BUTTON 1016
#define ID_ROCKY_TOGGLE_BODY_BUTTON 1017
"""
content = content.replace(old_ids, new_ids)

# 2. Add legHidden/bodyHidden fields to the Rocky struct.
old_rocky_tail = """    // The real total mass written as the "(gewicht)" value on Rob.txt's
    // first line (save.c's saveRockyAsRobArm), separate from bodyWeight/
    // legWeight above -- see input.c's hActualWeightEdit/
    // hActualWeightLabel. Defaulted to 1.0f alongside bodyWeight/legWeight.
    float actualWeight;
} Rocky;"""
assert content.count(old_rocky_tail) == 1

new_rocky_tail = """    // The real total mass written as the "(gewicht)" value on Rob.txt's
    // first line (save.c's saveRockyAsRobArm), separate from bodyWeight/
    // legWeight above -- see input.c's hActualWeightEdit/
    // hActualWeightLabel. Defaulted to 1.0f alongside bodyWeight/legWeight.
    float actualWeight;

    // Testing-only visibility/collision toggles (see
    // ID_ROCKY_TOGGLE_LEG_BUTTON/ID_ROCKY_TOGGLE_BODY_BUTTON above) --
    // NOT part of the robot's actual pose/geometry, so printRockyAsInit/
    // saveRockyAsRobArm deliberately don't touch these, and they're left
    // out of Rob.txt/Arm.txt exports the same way. Both default FALSE
    // (nothing hidden) since `app` is a plain zero-initialized global
    // (see main.c) -- no explicit app_init.c initialization needed.
    // legHidden drops the knee/foot circles AND the shin fillet arcs
    // (computeRockyBodyCircles/computeRockyArcPoints) from both
    // drawRocky and robotCollidesWithEnvironment; bodyHidden does the
    // same for the rectangular torso (computeRockyRectSegments) alone.
    BOOL legHidden;
    BOOL bodyHidden;
} Rocky;"""
content = content.replace(old_rocky_tail, new_rocky_tail)

# 3. Add two HWND UI fields, right after hDebugLogButton.
old_ui = """    HWND hDebugLogButton;
"""
assert content.count(old_ui) == 1
new_ui = old_ui + """
    // Rocky-only testing toggles (ID_ROCKY_TOGGLE_LEG_BUTTON/
    // ID_ROCKY_TOGGLE_BODY_BUTTON) -- same checkbox-as-button convention
    // as hViewSegmentsButton above. Shown only while Rocky is the active
    // robot kind (see ID_ROBOT_SELECTOR's CBN_SELCHANGE handling in
    // input.c, same "hide what has nothing to act on" treatment as
    // hMirrorButton2), since they toggle Rocky's own legHidden/
    // bodyHidden fields.
    HWND hRockyToggleLegButton;
    HWND hRockyToggleBodyButton;
"""
content = content.replace(old_ui, new_ui)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("app.h done")

import shutil, glob, re

def backup(path):
    existing = glob.glob(path + ".bak*")
    nums = [int(re.search(r"\.bak(\d+)$", f).group(1)) for f in existing if re.search(r"\.bak(\d+)$", f)]
    n = max(nums) + 1 if nums else 1
    b = f"{path}.bak{n}"
    shutil.copyfile(path, b)
    print("backup:", b)
    return b

path = "editor_mode.c"
backup(path)
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = """    if (app.ui.hMirrorButton2)
        ShowWindow(app.ui.hMirrorButton2,
                   (semniActive && app.robotScene.activeKind == ROBOT_KIND_STILO) ? SW_SHOW : SW_HIDE);
"""
assert content.count(old) == 1, f"match: {content.count(old)}"

new = old + """    // Rocky-only testing toggles (ID_ROCKY_TOGGLE_LEG_BUTTON/
    // ID_ROCKY_TOGGLE_BODY_BUTTON) -- same ANDed-with-semniActive
    // treatment as hMirrorButton2 just above, just gated on ROCKY
    // instead of STILO. Missing this is exactly the bug this function's
    // own hWeightRatioLabel/etc. comment already describes further
    // down: gate on activeKind alone (input.c's ID_ROBOT_SELECTOR
    // handler, which only fires on a KIND switch) and the button stays
    // visible in every EDITOR MODE too, including the ArcSpline
    // ("Environment") canvas, since nothing here was hiding it on a
    // MODE switch.
    if (app.ui.hRockyToggleLegButton)
        ShowWindow(app.ui.hRockyToggleLegButton,
                   (semniActive && app.robotScene.activeKind == ROBOT_KIND_ROCKY) ? SW_SHOW : SW_HIDE);
    if (app.ui.hRockyToggleBodyButton)
        ShowWindow(app.ui.hRockyToggleBodyButton,
                   (semniActive && app.robotScene.activeKind == ROBOT_KIND_ROCKY) ? SW_SHOW : SW_HIDE);
"""

content = content.replace(old, new)
with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("editor_mode.c done")

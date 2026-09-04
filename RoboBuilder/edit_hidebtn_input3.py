import shutil, glob, re

def backup(path):
    existing = glob.glob(path + ".bak*")
    nums = [int(re.search(r"\.bak(\d+)$", f).group(1)) for f in existing if re.search(r"\.bak(\d+)$", f)]
    n = max(nums) + 1 if nums else 1
    b = f"{path}.bak{n}"
    shutil.copyfile(path, b)
    print("backup:", b)
    return b

path = "input.c"
backup(path)
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = """                        if (app->ui.hMirrorButton2)
                            ShowWindow(app->ui.hMirrorButton2,
                                       (app->robotScene.activeKind == ROBOT_KIND_STILO) ? SW_SHOW : SW_HIDE);
"""
assert content.count(old) == 1, f"match: {content.count(old)}"

new = old + """
                        // Rocky-only testing toggles (ID_ROCKY_TOGGLE_LEG_
                        // BUTTON/ID_ROCKY_TOGGLE_BODY_BUTTON) -- same
                        // "hide what has nothing to act on" treatment as
                        // hMirrorButton2 above, just gated on ROCKY
                        // instead of STILO.
                        if (app->ui.hRockyToggleLegButton)
                            ShowWindow(app->ui.hRockyToggleLegButton,
                                       (app->robotScene.activeKind == ROBOT_KIND_ROCKY) ? SW_SHOW : SW_HIDE);
                        if (app->ui.hRockyToggleBodyButton)
                            ShowWindow(app->ui.hRockyToggleBodyButton,
                                       (app->robotScene.activeKind == ROBOT_KIND_ROCKY) ? SW_SHOW : SW_HIDE);
"""

content = content.replace(old, new)
with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("input.c step3 (visibility) done")

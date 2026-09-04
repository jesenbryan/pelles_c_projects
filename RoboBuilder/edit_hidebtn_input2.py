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

# 1. Extend the relY layout chain with a new row for the two toggle buttons,
#    right after relYDebug/before panelH.
old_rows = """            int relYSeg      = relYScale    + sliderH    + rowGap; // View Segments
            int relYDebug    = relYSeg      + btnH       + rowGap; // Debug Log
            int panelH       = relYDebug    + btnH       + pad;"""
assert content.count(old_rows) == 1, f"old_rows match: {content.count(old_rows)}"

new_rows = """            int relYSeg      = relYScale    + sliderH    + rowGap; // View Segments
            int relYDebug    = relYSeg      + btnH       + rowGap; // Debug Log
            int relYRockyHide = relYDebug   + btnH       + rowGap; // Remove Leg | Remove Body (Rocky only, testing)
            int panelH       = relYRockyHide + btnH      + pad;"""
content = content.replace(old_rows, new_rows)

# 2. Position the two buttons side-by-side (col1X/col2X), same row layout
#    idiom as hMirrorButton/hMirrorButton2 above.
old_pos = """            SetWindowPos(app->ui.hDebugLogButton, NULL,
                 col1X, panelY + relYDebug, contentW, btnH,
                 SWP_NOZORDER);
"""
assert content.count(old_pos) == 1, f"old_pos match: {content.count(old_pos)}"

new_pos = old_pos + """
            SetWindowPos(app->ui.hRockyToggleLegButton, NULL,
                 col1X, panelY + relYRockyHide, colW, btnH,
                 SWP_NOZORDER);

            SetWindowPos(app->ui.hRockyToggleBodyButton, NULL,
                 col2X, panelY + relYRockyHide, colW, btnH,
                 SWP_NOZORDER);
"""
content = content.replace(old_pos, new_pos)

# 3. Add the two buttons to the repaint-invalidate array.
old_arr = """                app->ui.hViewSegmentsButton,
                app->ui.hDebugLogButton,
                app->ui.hHoverPanel,"""
assert content.count(old_arr) == 1, f"old_arr match: {content.count(old_arr)}"
new_arr = """                app->ui.hViewSegmentsButton,
                app->ui.hDebugLogButton,
                app->ui.hRockyToggleLegButton,
                app->ui.hRockyToggleBodyButton,
                app->ui.hHoverPanel,"""
content = content.replace(old_arr, new_arr)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("input.c step2 (layout) done")

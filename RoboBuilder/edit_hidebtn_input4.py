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

old = """                case ID_VIEW_SEGMENTS_BUTTON:
                {
                    // BS_AUTOCHECKBOX already flipped its own check state
                    // before this notification fires, so read it back
                    // rather than tracking a separate bool -- same pattern
                    // as the ArcSpline canvas's hViewSegBtn (ui.c). Works
                    // identically for all three robot kinds now (Rocky/
                    // Stilo each have their own compute/draw pair mirroring
                    // Semni's, see renderer.c/h's computeRocky*/
                    // computeStilo* and drawRocky/drawStilo's own View
                    // Segments blocks) -- whichever kind is active reads
                    // this same app->showCircleSegments flag.
                    BOOL nowChecked = (SendMessage(app->ui.hViewSegmentsButton, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    app->showCircleSegments = nowChecked;
                    SetFocus(app->hwndMain);
                    break;
                }
"""
assert content.count(old) == 1, f"match: {content.count(old)}"

new = old + """
                case ID_ROCKY_TOGGLE_LEG_BUTTON:
                {
                    // Same "button IS the toggle state" pattern as
                    // ID_VIEW_SEGMENTS_BUTTON above -- read back the
                    // checkbox BS_AUTOCHECKBOX already flipped, rather
                    // than tracking a separate bool. Drives Rocky's own
                    // legHidden (app.h), which both drawRocky (renderer.c)
                    // and robotCollidesWithEnvironment (canvas.c) gate on
                    // -- see their own comments for why a hidden part
                    // needs to drop out of BOTH, not just rendering, for
                    // this to actually be useful as an isolation test.
                    BOOL nowChecked = (SendMessage(app->ui.hRockyToggleLegButton, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    app->robotScene.rocky.legHidden = nowChecked;
                    InvalidateRect(app->hwndMain, NULL, FALSE);
                    SetFocus(app->hwndMain);
                    break;
                }

                case ID_ROCKY_TOGGLE_BODY_BUTTON:
                {
                    BOOL nowChecked = (SendMessage(app->ui.hRockyToggleBodyButton, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    app->robotScene.rocky.bodyHidden = nowChecked;
                    InvalidateRect(app->hwndMain, NULL, FALSE);
                    SetFocus(app->hwndMain);
                    break;
                }
"""

content = content.replace(old, new)
with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("input.c step4 (WM_COMMAND) done")

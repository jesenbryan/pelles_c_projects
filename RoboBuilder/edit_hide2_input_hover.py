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

old = """                app->hoverRockyMassCenter = rockyMassWinsNow;
                app->hoverRockyBody = !app->hoverRockyMassCenter && isNear(app->mouseGL, rockyCenter, HIP_HANDLE_RADIUS);
                app->hoverRockyEdge = (app->hoverRockyMassCenter || app->hoverRockyBody || !rockyEdgeWinsNow) ? ROCKY_EDGE_NONE : rockyEdgeHitNow;
                app->hoverRockyKnee = !app->hoverRockyMassCenter && !app->hoverRockyBody && app->hoverRockyEdge == ROCKY_EDGE_NONE && isNear(app->mouseGL, kneeWorld, KNEE_HANDLE_RADIUS);
                app->hoverRockyFoot = !app->hoverRockyMassCenter && !app->hoverRockyBody && app->hoverRockyEdge == ROCKY_EDGE_NONE && !app->hoverRockyKnee && isNear(app->mouseGL, footWorld, FOOT_HANDLE_RADIUS);

                // Requested directly: hovering a shin arc handle (not just
                // dragging it) should also turn it yellow -- same treatment
                // Semni's own hoverSeamArc1/hoverThigh1/etc. give their own
                // arc handles.
                BOOL hoverRockyShin1Now = isNear(app->mouseGL, rockyShin1World, ROCKY_SHIN_HANDLE_RADIUS);
                BOOL hoverRockyShin2Now = isNear(app->mouseGL, rockyShin2World, ROCKY_SHIN_HANDLE_RADIUS);
                app->hoverRockyShin1 = hoverRockyShin1Now;
                app->hoverRockyShin2 = hoverRockyShin2Now;"""
assert content.count(old) == 1, f"match: {content.count(old)}"

new = """                // Testing-only toggles (ID_ROCKY_TOGGLE_LEG_BUTTON/
                // ID_ROCKY_TOGGLE_BODY_BUTTON) -- AND'd into each
                // assignment below (rather than restructuring the whole
                // priority cascade above) so a hidden part's condition
                // simply reads false and the cascade falls through to
                // whatever's next in priority exactly like the part was
                // never there, same treatment as the mirrored
                // WM_LBUTTONDOWN cascade above.
                BOOL rockyLegHidden = app->robotScene.rocky.legHidden;
                BOOL rockyBodyHidden = app->robotScene.rocky.bodyHidden;

                app->hoverRockyMassCenter = rockyMassWinsNow;
                app->hoverRockyBody = !rockyBodyHidden && !app->hoverRockyMassCenter && isNear(app->mouseGL, rockyCenter, HIP_HANDLE_RADIUS);
                app->hoverRockyEdge = (rockyBodyHidden || app->hoverRockyMassCenter || app->hoverRockyBody || !rockyEdgeWinsNow) ? ROCKY_EDGE_NONE : rockyEdgeHitNow;
                app->hoverRockyKnee = !rockyLegHidden && !app->hoverRockyMassCenter && !app->hoverRockyBody && app->hoverRockyEdge == ROCKY_EDGE_NONE && isNear(app->mouseGL, kneeWorld, KNEE_HANDLE_RADIUS);
                app->hoverRockyFoot = !rockyLegHidden && !app->hoverRockyMassCenter && !app->hoverRockyBody && app->hoverRockyEdge == ROCKY_EDGE_NONE && !app->hoverRockyKnee && isNear(app->mouseGL, footWorld, FOOT_HANDLE_RADIUS);

                // Requested directly: hovering a shin arc handle (not just
                // dragging it) should also turn it yellow -- same treatment
                // Semni's own hoverSeamArc1/hoverThigh1/etc. give their own
                // arc handles.
                BOOL hoverRockyShin1Now = !rockyLegHidden && isNear(app->mouseGL, rockyShin1World, ROCKY_SHIN_HANDLE_RADIUS);
                BOOL hoverRockyShin2Now = !rockyLegHidden && isNear(app->mouseGL, rockyShin2World, ROCKY_SHIN_HANDLE_RADIUS);
                app->hoverRockyShin1 = hoverRockyShin1Now;
                app->hoverRockyShin2 = hoverRockyShin2Now;"""

content = content.replace(old, new)
with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("input.c (hover gating) done")

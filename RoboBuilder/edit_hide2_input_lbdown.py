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

replacements = [
    (
        "else if (isNear(app->mouseGL, rockyCenter, HIP_HANDLE_RADIUS))\n                {\n                    app->draggingRockyBody = 1;",
        "else if (!app->robotScene.rocky.bodyHidden && isNear(app->mouseGL, rockyCenter, HIP_HANDLE_RADIUS))\n                {\n                    app->draggingRockyBody = 1;",
    ),
    (
        "else if (rockyEdgeHitDown != ROCKY_EDGE_NONE &&\n                         rockyEdgeWins(app->mouseGL, rockyEdgeMidWorld(app->robotScene.rocky, rockyEdgeHitDown, rockyCenter),\n                                       kneeWorld, footWorld, rockyShin1World, rockyShin2World))",
        "else if (!app->robotScene.rocky.bodyHidden && rockyEdgeHitDown != ROCKY_EDGE_NONE &&\n                         rockyEdgeWins(app->mouseGL, rockyEdgeMidWorld(app->robotScene.rocky, rockyEdgeHitDown, rockyCenter),\n                                       kneeWorld, footWorld, rockyShin1World, rockyShin2World))",
    ),
    (
        "else if (isNear(app->mouseGL, kneeWorld, KNEE_HANDLE_RADIUS))\n                {\n                    app->draggingRockyKnee = 1;",
        "else if (!app->robotScene.rocky.legHidden && isNear(app->mouseGL, kneeWorld, KNEE_HANDLE_RADIUS))\n                {\n                    app->draggingRockyKnee = 1;",
    ),
    (
        "else if (isNear(app->mouseGL, footWorld, FOOT_HANDLE_RADIUS))\n                {\n                    // no offset capture needed -- nothing hangs off the\n                    // foot, same as Semni's own draggingFoot\n                    app->draggingRockyFoot = 1;",
        "else if (!app->robotScene.rocky.legHidden && isNear(app->mouseGL, footWorld, FOOT_HANDLE_RADIUS))\n                {\n                    // no offset capture needed -- nothing hangs off the\n                    // foot, same as Semni's own draggingFoot\n                    app->draggingRockyFoot = 1;",
    ),
    (
        "else if (isNear(app->mouseGL, rockyShin1World, ROCKY_SHIN_HANDLE_RADIUS))\n                {\n                    app->draggingRockyShin1 = 1;",
        "else if (!app->robotScene.rocky.legHidden && isNear(app->mouseGL, rockyShin1World, ROCKY_SHIN_HANDLE_RADIUS))\n                {\n                    app->draggingRockyShin1 = 1;",
    ),
    (
        "else if (isNear(app->mouseGL, rockyShin2World, ROCKY_SHIN_HANDLE_RADIUS))\n                {\n                    app->draggingRockyShin2 = 1;",
        "else if (!app->robotScene.rocky.legHidden && isNear(app->mouseGL, rockyShin2World, ROCKY_SHIN_HANDLE_RADIUS))\n                {\n                    app->draggingRockyShin2 = 1;",
    ),
]

for old, new in replacements:
    cnt = content.count(old)
    assert cnt == 1, f"match count {cnt} for: {old[:70]!r}"
    content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("input.c (WM_LBUTTONDOWN gating) done")

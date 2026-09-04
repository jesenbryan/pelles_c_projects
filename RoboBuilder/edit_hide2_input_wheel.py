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
        "if (isNear(mouse, rockyCenter, HIP_HANDLE_RADIUS))\n                {\n                    float sizeStep = 0.02f;",
        "if (!app->robotScene.rocky.bodyHidden && isNear(mouse, rockyCenter, HIP_HANDLE_RADIUS))\n                {\n                    float sizeStep = 0.02f;",
    ),
    (
        "else if (isNear(mouse, kneeWorld, KNEE_HANDLE_RADIUS) && shiftHeld)\n                {\n                    // rotate the knee joint",
        "else if (!app->robotScene.rocky.legHidden && isNear(mouse, kneeWorld, KNEE_HANDLE_RADIUS) && shiftHeld)\n                {\n                    // rotate the knee joint",
    ),
    (
        "else if (isNear(mouse, kneeWorld, KNEE_HANDLE_RADIUS))\n                {\n                    // plain scroll (no Shift) resizes the knee circle",
        "else if (!app->robotScene.rocky.legHidden && isNear(mouse, kneeWorld, KNEE_HANDLE_RADIUS))\n                {\n                    // plain scroll (no Shift) resizes the knee circle",
    ),
    (
        "else if (isNear(mouse, footWorld, FOOT_HANDLE_RADIUS))\n                {\n                    // plain scroll on the foot handle resizes it",
        "else if (!app->robotScene.rocky.legHidden && isNear(mouse, footWorld, FOOT_HANDLE_RADIUS))\n                {\n                    // plain scroll on the foot handle resizes it",
    ),
]

for old, new in replacements:
    cnt = content.count(old)
    assert cnt == 1, f"match count {cnt} for: {old[:70]!r}"
    content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("input.c (WM_MOUSEWHEEL gating) done")

import shutil, glob, re

def backup(path):
    existing = glob.glob(path + ".bak*")
    nums = [int(re.search(r"\.bak(\d+)$", f).group(1)) for f in existing if re.search(r"\.bak(\d+)$", f)]
    n = max(nums) + 1 if nums else 1
    b = f"{path}.bak{n}"
    shutil.copyfile(path, b)
    print("backup:", b)
    return b

path = "renderer.c"
backup(path)
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = """void drawRocky(Rocky b, RenderState* rs, int includeHandles, float opacity)
{
    drawRockyBodyRect(b, rs, opacity);
    drawRockyLeg(b, rs, opacity);"""
assert content.count(old) == 1, f"match count: {content.count(old)}"

new = """void drawRocky(Rocky b, RenderState* rs, int includeHandles, float opacity)
{
    // Testing-only toggles (ID_ROCKY_TOGGLE_LEG_BUTTON/
    // ID_ROCKY_TOGGLE_BODY_BUTTON, see app.h's Rocky comment) -- drop
    // either part out of rendering entirely so its behavior (mostly
    // gravity/collision, see canvas.c's matching guard in
    // robotCollidesWithEnvironment) can be watched in isolation from the
    // other part's own geometry.
    if (!b.bodyHidden)
        drawRockyBodyRect(b, rs, opacity);
    if (!b.legHidden)
        drawRockyLeg(b, rs, opacity);"""

content = content.replace(old, new)
with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("renderer.c done")

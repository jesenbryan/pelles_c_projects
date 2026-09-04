import shutil, glob

path = "app.h"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''    // Rocky's knee handle -- the joint where the leg (kneeCircle->
    // footCircle, see the Rocky struct comment) attaches to the
    // rectangle, same role Semni's own kneeCircle plays relative to its
    // hip. Mirrors Semni's hoverKnee/draggingKnee treatment for the
    // hover/scroll/Shift+scroll behaviors, but the drag itself is freer
    // than Semni's own axis-constrained hip->knee drag: it can be dropped
    // anywhere inside the rectangle, clamped so the knee circle's own
    // circumference never pokes outside the body. Plain scroll resizes
    // kneeRadius, Shift+scroll bends kneeAngle (swings footCircle around
    // the knee) -- see input.c's WM_LBUTTONDOWN/WM_MOUSEMOVE/'''

new = '''    // Rocky's knee handle -- the joint where the leg (kneeCircle->
    // footCircle, see the Rocky struct comment) attaches to the
    // rectangle, same role Semni's own kneeCircle plays relative to its
    // hip. Mirrors Semni's hoverKnee/draggingKnee treatment for the
    // hover/scroll/Shift+scroll behaviors, but the drag itself is freer
    // than Semni's own axis-constrained hip->knee drag: it can be
    // dropped ANYWHERE, including outside the rectangle entirely -- by
    // explicit request the knee is no longer clamped to stay inside the
    // body, so the whole leg can be detached and posed off to the side
    // of the torso instead of always hanging from somewhere within it.
    // Plain scroll resizes kneeRadius, Shift+scroll bends kneeAngle
    // (swings footCircle around the knee) -- see input.c's
    // WM_LBUTTONDOWN/WM_MOUSEMOVE/'''

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

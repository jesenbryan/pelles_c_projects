import shutil, glob

path = "canvas.c"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''// Draws a small disc at every single point robotCollidesWithEnvironment
// (and, for Rocky, rockyFootCollides) actually tests against the
// environment -- every body circle, every fillet-arc sample point, and
// (Rocky only) every rectangle-edge sample point -- colored red if THAT
// exact point is colliding right now, dim green otherwise. Deliberately
// mirrors robotCollidesWithEnvironment's own per-kind iteration
// line-for-line instead of calling it, since that function early-returns
// on the first hit and this needs every point's own status to draw
// anything.
//
// The point of this isn't just to show WHERE contact is (the red dots),
// it's to show what ISN'T being checked at all: for Rocky specifically,
// nothing is ever drawn -- or tested, anywhere in this file -- along the
// shin between the knee and foot circles, only at those two endpoints.
// A thin ridge or corner snagging the middle of the leg has no dot
// anywhere near it and no collision check watching it either, which is
// the gap discussed when this was added (see the E/Q handler and
// advanceAutoGravity's own comments on rockyFootCollides being a single
// point, not the whole leg).
//'''

new = '''// Draws a small disc at every single point robotCollidesWithEnvironment
// itself tests against the environment -- every body circle, every
// fillet-arc sample point (this includes Rocky's two shin arcs between
// the knee and foot circles, computeRockyArcPoints -- an earlier version
// of this comment claimed those went untested/undrawn, which stopped
// being true once the loop below was added), and (Rocky only) every
// rectangle-edge sample point -- colored red if THAT exact point is
// colliding right now, dim green otherwise. Deliberately mirrors
// robotCollidesWithEnvironment's own per-kind iteration line-for-line
// instead of calling it, since that function early-returns on the first
// hit and this needs every point's own status to draw anything.
//'''

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

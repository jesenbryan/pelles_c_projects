import shutil, glob

path = "canvas.c"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old1 = '''// turns out to be. Used by advanceRockyKneeSettle below to answer "how'''
new1 = '''// turns out to be. Used by advanceRockySettle below to answer "how'''
assert content.count(old1) == 1, f"old1 match count: {content.count(old1)}"
content = content.replace(old1, new1)

old2 = '''        // Rocky specifically gets one more thing here, EVERY tick this
        // block runs (not gated to suppressThisCall/fresh-landing-only --
        // by explicit request this is continuous, unlike the removed
        // slope-alignment idea referenced just above): if the knee was
        // dragged far from the rectangle (the knee-detach feature, app.h's
        // own comment on draggingRockyKnee), the LEG can land on
        // something while the body itself is still floating well above
        // the ground -- see advanceRockyKneeSettle's own comment for why
        // running it every tick is safe here (no velocity/damping state
        // to oscillate, unlike the old knee pendulum) and how it bends
        // the knee a little further each call until the body has nowhere
        // left to go.
        advanceRockyKneeSettle();
    }'''

new2 = '''        // Rocky specifically gets one more thing here, EVERY tick this
        // block runs (not gated to suppressThisCall/fresh-landing-only --
        // by explicit request this is continuous, unlike the removed
        // slope-alignment idea referenced just above): if the knee was
        // dragged far from the rectangle (the knee-detach feature, app.h's
        // own comment on draggingRockyKnee), the LEG can land on
        // something while the body itself is still floating well above
        // the ground -- see advanceRockySettle's own comment for why
        // running it every tick is safe here (no velocity/damping state
        // to oscillate, unlike the old knee pendulum), and how it bends
        // the knee AND rotates the rectangle a little further each call
        // (by explicit request, so the body can tip instead of only ever
        // sinking straight down) until there's nowhere left to go.
        advanceRockySettle();
    }'''
assert content.count(old2) == 1, f"old2 match count: {content.count(old2)}"
content = content.replace(old2, new2)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

import shutil, glob

path = "canvas.c"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''        // Rocky specifically gets one more thing here, but as a ONE-SHOT
        // search rather than a per-tick nudge (exactly to avoid the kind
        // of wobble the removed slope alignment above is about): if the
        // knee was dragged far from the rectangle (the knee-detach
        // feature, app.h's own comment on draggingRockyKnee), the LEG can
        // land on something while the body itself is still floating well
        // above the ground -- see settleRockyKneeUnderBody's own comment
        // for how it resolves that by bending the knee. Gated to
        // !suppressThisCall so it runs once per FRESH landing, not on
        // every already-resting tick while Auto Gravity's timer keeps
        // calling this.
        if (!suppressThisCall)
            settleRockyKneeUnderBody();
    }'''

new = '''        // Rocky specifically gets one more thing here, EVERY tick this
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

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

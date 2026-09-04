import shutil, glob

path = "canvas.c"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''        // Plain gravity + contact only: once the fall is stopped by the
        // binary search above, the robot stays exactly where it landed.
        // (Previously this also incrementally rotated the whole body to
        // align with the ground slope every landed tick -- removed because
        // that per-tick lean + re-settle read as a visible bounce/wobble
        // instead of a clean stop.)
    }'''

new = '''        // Plain gravity + contact only: once the fall is stopped by the
        // binary search above, the robot stays exactly where it landed.
        // (Previously this also incrementally rotated the whole body to
        // align with the ground slope every landed tick -- removed because
        // that per-tick lean + re-settle read as a visible bounce/wobble
        // instead of a clean stop.)
        //
        // Rocky specifically gets one more thing here, but as a ONE-SHOT
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

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

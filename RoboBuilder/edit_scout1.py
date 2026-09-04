import shutil, glob, re

def backup(path):
    existing = glob.glob(path + ".bak*")
    nums = [int(re.search(r"\.bak(\d+)$", f).group(1)) for f in existing if re.search(r"\.bak(\d+)$", f)]
    n = max(nums) + 1 if nums else 1
    b = f"{path}.bak{n}"
    shutil.copyfile(path, b)
    print("backup:", b)
    return b

# ---- config.h: add scout-step constant ----
path = "config.h"
backup(path)
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = "#define SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK SIMULATION_GRAVITY_STEP\n"
assert content.count(old) == 1, f"config old match count: {content.count(old)}"

new = old + """
// Fixed (NOT adaptive/shrinking) lookahead angle both settle probes use
// to escape a local "flat spot" before giving up and halving their own
// fine step. A real report showed Rocky settling into a visibly dangling
// -- foot pose (rectangle resting on one corner, foot hanging in open
// air) that never improved further even with auto-gravity left running:
// the fine step (SIMULATION_LEG_SETTLE_STEP_DEG, shrinking toward
// SIMULATION_LEG_SETTLE_MIN_STEP_DEG) had landed exactly on a point where
// the foot's height barely changes with kneeAngle -- a near-zero local
// slope -- even though continuing further in the same direction clearly
// keeps helping. Halving an already-tiny step can never discover that:
// every even-finer neighboring angle is just as flat. Scouting this much
// bigger, constant-size angle purely to test "is there real improvement
// out here" (never committed directly -- only used to decide whether to
// keep nudging by the normal small per-tick step instead of shrinking
// it) breaks out of that trap while still moving at the same smooth,
// gradual pace as everywhere else in the settle system.
#define SIMULATION_LEG_SETTLE_SCOUT_STEP_DEG 20.0f
"""

content = content.replace(old, new)
with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("config.h done")

# ---- canvas.c: add scouting to both probes' "nothing found" branches ----
path = "canvas.c"
backup(path)
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old_knee_else = """        else
        {
            rockyKneeSettleStep *= 0.5f; // nothing here -- get finer before giving up
        }
    }

    // Probe 2: rotate the rectangle itself."""
assert content.count(old_knee_else) == 1, f"old_knee_else match count: {content.count(old_knee_else)}"

new_knee_else = """        else
        {
            // Before shrinking further, scout a much bigger, FIXED angle
            // in each direction -- see SIMULATION_LEG_SETTLE_SCOUT_STEP_DEG's
            // own comment for why a shrinking step alone can get stuck on
            // a local flat spot. Only ever used to decide direction/
            // worthwhileness; the actual committed move below is still
            // just one ordinary rockyKneeSettleStep-sized nudge, so this
            // can't itself cause a visible jump.
            BOOL scoutedKnee = FALSE;

            for (int dir = -1; dir <= 1 && !scoutedKnee; dir += 2)
            {
                r->kneeAngle = baseKneeAngle + dir * SIMULATION_LEG_SETTLE_SCOUT_STEP_DEG;

                float scoutDrop = dropActiveRobotToRest();
                if (scoutDrop > 0.0f)
                    translateActiveRobot(0.0f, scoutDrop); // undo the probe's own move

                BOOL scoutClear = !robotCollidesWithEnvironment();
                float scoutGain = -1.0f;

                if (scoutClear)
                {
                    PointF sCenter = getRockyCenter(*r);
                    PointF sFootWorld = jointToWorld(r->footCircle, r->kneeCircle, r->kneeAngle, sCenter, r->angle);
                    float sfecx, sfecy;
                    robotPointToEnvWorld(sFootWorld.x, sFootWorld.y, &sfecx, &sfecy);
                    float sClearance = nearestEnvDistance(sfecx, sfecy) - footCombinedRadius;
                    scoutGain = baseFootClearance - sClearance;
                }

                if (scoutDrop > SIMULATION_LEG_SETTLE_MIN_DROP
                    || (scoutClear && scoutGain > SIMULATION_LEG_SETTLE_MIN_DROP))
                {
                    r->kneeAngle = baseKneeAngle + dir * rockyKneeSettleStep;
                    printf("[SETTLE] kneeAngle=%.2f scouted real improvement %.1f deg out (drop=%.5f clearanceGain=%.5f) -- nudging that way instead of shrinking\\n",
                           r->kneeAngle, dir * SIMULATION_LEG_SETTLE_SCOUT_STEP_DEG, scoutDrop, scoutGain);
                    scoutedKnee = TRUE;
                }
            }

            if (!scoutedKnee)
            {
                r->kneeAngle = baseKneeAngle;
                rockyKneeSettleStep *= 0.5f; // nothing here, and nothing further out either -- get finer before giving up
            }
        }
    }

    // Probe 2: rotate the rectangle itself."""

assert content.count(new_knee_else) == 0
content = content.replace(old_knee_else, new_knee_else)

old_body_else = """        else
        {
            rockyBodySettleStep *= 0.5f;
        }
    }

    if (rockyKneeSettleStep < SIMULATION_LEG_SETTLE_MIN_STEP_DEG"""
assert content.count(old_body_else) == 1, f"old_body_else match count: {content.count(old_body_else)}"

new_body_else = """        else
        {
            // Same scouting idea as the knee probe above, same reason --
            // just the drop-based criterion (rotating the whole body has
            // no separate "dangling part" fallback of its own; a big body
            // rotation either opens up a real drop or it doesn't).
            BOOL scoutedBody = FALSE;

            for (int dir = -1; dir <= 1 && !scoutedBody; dir += 2)
            {
                r->angle = baseBodyAngle + dir * SIMULATION_LEG_SETTLE_SCOUT_STEP_DEG;

                float scoutDrop = dropActiveRobotToRest();
                if (scoutDrop > 0.0f)
                    translateActiveRobot(0.0f, scoutDrop); // undo the probe's own move

                if (scoutDrop > SIMULATION_LEG_SETTLE_MIN_DROP)
                {
                    r->angle = baseBodyAngle + dir * rockyBodySettleStep;
                    printf("[SETTLE] bodyAngle=%.2f scouted real improvement %.1f deg out (drop=%.5f) -- nudging that way instead of shrinking\\n",
                           r->angle, dir * SIMULATION_LEG_SETTLE_SCOUT_STEP_DEG, scoutDrop);
                    scoutedBody = TRUE;
                }
            }

            if (!scoutedBody)
            {
                r->angle = baseBodyAngle;
                rockyBodySettleStep *= 0.5f;
            }
        }
    }

    if (rockyKneeSettleStep < SIMULATION_LEG_SETTLE_MIN_STEP_DEG"""

assert content.count(new_body_else) == 0
content = content.replace(old_body_else, new_body_else)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("canvas.c done")

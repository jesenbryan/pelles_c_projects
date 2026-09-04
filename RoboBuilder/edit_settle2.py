import shutil, glob

path = "canvas.c"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''    translateActiveRobot(0.0f, safe);
    InvalidateRect(hWnd, NULL, FALSE);
}

// Applies one gravity step to the robot -- tentatively translates it down'''

new = '''    translateActiveRobot(0.0f, safe);
    InvalidateRect(hWnd, NULL, FALSE);
}

// Drops the whole active robot (translateActiveRobot -- same rigid,
// all-parts-together move applyGravityStep itself uses) straight down
// from wherever it is RIGHT NOW until first contact, in one call --
// unlike applyGravityStep, which is built around one small fixed-size
// SIMULATION_GRAVITY_STEP nudge per call (right for a per-tick fall), this
// grows the search range from scratch each time, so it finds the TRUE
// resting height in a single shot regardless of how big the actual gap
// turns out to be. Used by settleRockyKneeUnderBody below to answer "how
// far would the body fall if the knee were bent differently," which
// needs a real answer for a potentially large gap, not one tick's worth.
// Returns how far down it moved (0.0f if it couldn't move at all -- it
// was already resting, or already embedded right where it started).
static float dropActiveRobotToRest(void)
{
    if (robotCollidesWithEnvironment())
        return 0.0f; // already touching/embedded -- nothing to drop

    float safe = 0.0f;      // largest confirmed-clear drop so far
    float push = SIMULATION_LEG_PUSH_SEARCH_START;

    for (;;)
    {
        translateActiveRobot(0.0f, -push);
        BOOL hits = robotCollidesWithEnvironment();
        translateActiveRobot(0.0f, push); // undo probe

        if (hits) break;

        safe = push;
        if (push >= SIMULATION_LEG_PUSH_SEARCH_MAX)
            break; // sanity cap -- nothing below it for a long way (freefall)

        push *= 2.0f;
    }

    float blocked = push;

    for (int i = 0; i < GRAVITY_CONTACT_SEARCH_ITERATIONS; i++)
    {
        float mid = (safe + blocked) * 0.5f;

        translateActiveRobot(0.0f, -mid);
        BOOL hits = robotCollidesWithEnvironment();
        translateActiveRobot(0.0f, mid); // undo probe

        if (hits) blocked = mid; else safe = mid;
    }

    translateActiveRobot(0.0f, -safe);
    return safe;
}

// Rocky-only: right after gravity lands the robot (applyGravityStep,
// below, calls this once per fresh landing -- see its own comment),
// checks whether BENDING THE KNEE would let the body fall any further.
// This is specifically for the knee-detach feature (app.h's own comment
// on draggingRockyKnee): once the knee can sit far from the rectangle,
// gravity's own whole-body drop stops the instant its LOWEST part (often
// the foot) touches something, leaving the rectangle exactly as high up
// as that pose geometrically places it -- which can look like the body
// is floating, when really it's just resting on top of an oddly long/
// offset leg. Real robots settle that kind of thing by the knee bending
// under the body's own weight until something else (the rectangle
// itself) also finds the ground; this hill-climbs kneeAngle to do the
// same thing here.
//
// Coordinate-descent over the ONE free variable (kneeAngle): try a small
// step in both rotation directions, keep whichever one lets
// dropActiveRobotToRest fall further, and repeat -- halving the step
// each pass a fixed number of times (SIMULATION_LEG_SETTLE_REFINEMENTS)
// so it converges to a precise angle instead of overshooting back and
// forth forever. This is a ONE-SHOT search that runs to completion
// inside a single call, NOT a per-tick nudge repeated every frame --
// applyGravityStep's own comment on why a similar per-tick idea (slope
// alignment) was removed (it read as a visible bounce/wobble) is exactly
// the failure mode a multi-frame version of this would risk.
static void settleRockyKneeUnderBody(void)
{
    if (app.robotScene.activeKind != ROBOT_KIND_ROCKY)
        return;

    Rocky* r = &app.robotScene.rocky;
    float angleStep = SIMULATION_LEG_SETTLE_STEP_DEG;
    float totalDrop = 0.0f;

    for (int refine = 0; refine < SIMULATION_LEG_SETTLE_REFINEMENTS; refine++)
    {
        float baseKneeAngle = r->kneeAngle;
        float bestDrop = 0.0f;
        float bestKneeAngle = baseKneeAngle;

        for (int dir = -1; dir <= 1; dir += 2)
        {
            r->kneeAngle = baseKneeAngle + dir * angleStep;

            float drop = dropActiveRobotToRest();
            if (drop > 0.0f)
                translateActiveRobot(0.0f, drop); // undo the probe's own move

            if (drop > bestDrop)
            {
                bestDrop = drop;
                bestKneeAngle = r->kneeAngle;
            }
        }

        r->kneeAngle = baseKneeAngle;

        if (bestDrop > 0.0f)
        {
            // One of the two directions actually helped -- commit to it
            // for real this time, then keep refining from here at the
            // SAME step size (it may still have more room to give in
            // that same direction) rather than immediately halving.
            r->kneeAngle = bestKneeAngle;
            translateActiveRobot(0.0f, -bestDrop);
            totalDrop += bestDrop;
        }
        else
        {
            // Neither direction helped at this step size -- a coarse
            // step can overshoot past a narrow useful window, so try a
            // finer one before concluding there's really nothing left.
            angleStep *= 0.5f;
        }
    }

    if (totalDrop > 0.0f)
        printf("[SETTLE] kneeAngle=%.2f bent the knee to let the body fall %.5f further\\n",
               r->kneeAngle, totalDrop);
}

// Applies one gravity step to the robot -- tentatively translates it down'''

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

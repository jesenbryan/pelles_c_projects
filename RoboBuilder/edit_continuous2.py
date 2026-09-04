import shutil, glob

path = "canvas.c"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''// Rocky-only: right after gravity lands the robot (applyGravityStep,
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

    // Hard cap on total probe rounds regardless of how the refine/improve
    // pattern below plays out -- a shallow slope could in principle offer
    // a tiny further improvement every single round at the same step
    // size, and this function has to return promptly no matter what the
    // drawn environment looks like (this runs on the UI thread, once per
    // fresh landing -- it must never be able to hang the app). Chosen
    // generously above SIMULATION_LEG_SETTLE_REFINEMENTS (a few dozen
    // "kept improving" rounds per halving, worst case) rather than tuned
    // tightly, since finishing a little early just leaves a fraction of a
    // degree of possible settle on the table, not a visible difference.
    const int maxRounds = SIMULATION_LEG_SETTLE_REFINEMENTS * 40;
    int refinementsLeft = SIMULATION_LEG_SETTLE_REFINEMENTS;

    for (int round = 0; round < maxRounds && refinementsLeft > 0; round++)
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
            // for real this time, then keep trying the SAME step size
            // again next round (it may still have more room to give in
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
            // Counts against refinementsLeft (the precision budget);
            // an improving round above does not, so a slope that keeps
            // giving a little more at the same step size never eats into
            // how fine the search is ultimately allowed to get.
            angleStep *= 0.5f;
            refinementsLeft--;
        }
    }

    if (totalDrop > 0.0f)
        printf("[SETTLE] kneeAngle=%.2f bent the knee to let the body fall %.5f further\\n",
               r->kneeAngle, totalDrop);
}'''

new = '''// Rocky-only: called every single gravity tick (applyGravityStep, below,
// calls this every time any part of Rocky is touching something -- see
// its own comment), checks whether bending the knee by ONE small step
// (SIMULATION_LEG_SETTLE_STEP_DEG) would let the body fall any further
// right now, and commits that one step if so. This is specifically for
// the knee-detach feature (app.h's own comment on draggingRockyKnee):
// once the knee can sit far from the rectangle, gravity's own whole-body
// drop stops the instant its LOWEST part (often the foot) touches
// something, leaving the rectangle exactly as high up as that pose
// geometrically places it -- which can look like the body is floating,
// when really it's just resting on top of an oddly long/offset leg.
// Real robots settle that kind of thing by the knee bending under the
// body's own weight until something else (the rectangle itself) also
// finds the ground; being called every tick lets this happen gradually,
// over as many ticks as it takes, rather than snapping to the final pose
// the instant it lands.
//
// Deliberately does only ONE bounded step per call rather than
// hill-climbing to full convergence in one shot -- called continuously
// (every tick, by explicit request) instead of once, it doesn't need to
// finish in a single call; a tiny fixed step per tick is what actually
// produces a smooth multi-frame sink rather than an instant snap. Safe
// to call this often because there's no velocity or damping state
// carried between calls (unlike the removed knee pendulum) -- each call
// is an independent, deterministic probe that only ever commits a
// change it has already confirmed (via dropActiveRobotToRest, the same
// binary-search idiom the rest of gravity uses) genuinely lets the body
// fall further while staying collision-free, so there is nothing here
// that can accumulate into an oscillation or a stuck state.
static void advanceRockyKneeSettle(void)
{
    if (app.robotScene.activeKind != ROBOT_KIND_ROCKY)
        return;

    Rocky* r = &app.robotScene.rocky;
    float baseKneeAngle = r->kneeAngle;
    float bestDrop = 0.0f;
    float bestKneeAngle = baseKneeAngle;

    for (int dir = -1; dir <= 1; dir += 2)
    {
        r->kneeAngle = baseKneeAngle + dir * SIMULATION_LEG_SETTLE_STEP_DEG;

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
        r->kneeAngle = bestKneeAngle;
        translateActiveRobot(0.0f, -bestDrop);
        printf("[SETTLE] kneeAngle=%.2f bent the knee %.2f deg, body fell %.5f further this tick\\n",
               r->kneeAngle, SIMULATION_LEG_SETTLE_STEP_DEG, bestDrop);
    }
}'''

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

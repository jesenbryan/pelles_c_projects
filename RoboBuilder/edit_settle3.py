import shutil, glob

path = "canvas.c"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''static void settleRockyKneeUnderBody(void)
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
}'''

new = '''static void settleRockyKneeUnderBody(void)
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

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

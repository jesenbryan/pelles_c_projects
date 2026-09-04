import shutil, glob

path = "canvas.c"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''// Persists across calls: TRUE once a tick's probes both come up empty
// (nothing left worth improving for the CURRENT landing), so this stops
// actually re-probing every subsequent tick instead of continuing to
// nudge kneeAngle/angle back and forth by fractions of a degree forever.
// Reset to FALSE by applyGravityStep right when a FRESH landing begins
// (see its own comment) so a robot that's lifted off and lands again
// later still gets a full settle pass of its own.
static BOOL rockySettleConverged = FALSE;

static void advanceRockySettle(void)
{
    if (app.robotScene.activeKind != ROBOT_KIND_ROCKY)
        return;

    if (rockySettleConverged)
        return; // already found nothing left to improve for this landing

    Rocky* r = &app.robotScene.rocky;
    BOOL improvedThisTick = FALSE;

    // Probe 1: bend the knee.
    {
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

        // Compared against SIMULATION_LEG_SETTLE_MIN_DROP rather than a
        // bare > 0.0f -- see that constant's own comment on why: without
        // a floor, the search binary search's own precision noise right
        // at the true resting pose reads as an endless string of
        // wafer-thin "improvements," which is exactly what showed up as
        // visible vibration instead of a clean stop.
        if (bestDrop > SIMULATION_LEG_SETTLE_MIN_DROP)
        {
            r->kneeAngle = bestKneeAngle;
            translateActiveRobot(0.0f, -bestDrop);
            improvedThisTick = TRUE;
            printf("[SETTLE] kneeAngle=%.2f bent the knee %.2f deg, body fell %.5f further this tick\\n",
                   r->kneeAngle, SIMULATION_LEG_SETTLE_STEP_DEG, bestDrop);
        }
    }

    // Probe 2: rotate the rectangle itself. Rotating r->angle turns the
    // WHOLE robot (rectangle, kneeCircle, and footCircle-after-kneeAngle)
    // around (bodyX, bodyY) -- see jointToWorld/rotatePoint -- so unlike
    // bending the knee, this can also shift the foot sideways along the
    // ground, exactly like a real object tipping over while one point of
    // it stays roughly planted. dropActiveRobotToRest only re-tests
    // straight-down clearance afterward, so a rotation only ever commits
    // if it genuinely opens up more room to fall, same guarantee as the
    // knee probe above.
    {
        float baseBodyAngle = r->angle;
        float bestDrop = 0.0f;
        float bestBodyAngle = baseBodyAngle;

        for (int dir = -1; dir <= 1; dir += 2)
        {
            r->angle = baseBodyAngle + dir * SIMULATION_BODY_SETTLE_STEP_DEG;

            float drop = dropActiveRobotToRest();
            if (drop > 0.0f)
                translateActiveRobot(0.0f, drop); // undo the probe's own move

            if (drop > bestDrop)
            {
                bestDrop = drop;
                bestBodyAngle = r->angle;
            }
        }

        r->angle = baseBodyAngle;

        if (bestDrop > SIMULATION_LEG_SETTLE_MIN_DROP)
        {
            r->angle = bestBodyAngle;
            translateActiveRobot(0.0f, -bestDrop);
            improvedThisTick = TRUE;
            printf("[SETTLE] bodyAngle=%.2f rotated the rectangle %.2f deg, body fell %.5f further this tick\\n",
                   r->angle, SIMULATION_BODY_SETTLE_STEP_DEG, bestDrop);
        }
    }

    if (!improvedThisTick)
    {
        // Neither probe found anything worth committing this tick --
        // this IS the resting pose. Stop actually testing from here on
        // (until the next fresh landing) rather than continuing to probe
        // every tick and finding the same nothing over and over.
        rockySettleConverged = TRUE;
        printf("[SETTLE] converged -- kneeAngle=%.2f bodyAngle=%.2f, no further improvement found\\n",
               r->kneeAngle, r->angle);
    }
}'''

new = '''// Persists across calls: TRUE once BOTH probes below are individually
// exhausted (see rockyKneeSettleStep/rockyBodySettleStep) for the
// CURRENT landing, so this stops actually re-probing every subsequent
// tick instead of continuing to test forever. Reset to FALSE by
// applyGravityStep right when a FRESH landing begins (see its own
// comment) so a robot that's lifted off and lands again later still
// gets a full settle pass of its own.
static BOOL rockySettleConverged = FALSE;

// Each probe's OWN current step size, shrinking independently as it runs
// out of room -- separate statics (not the SIMULATION_LEG_SETTLE_STEP_DEG/
// SIMULATION_BODY_SETTLE_STEP_DEG constants, which are just where these
// start) because a fixed step that never adapts can call things
// "converged" too early: the real remaining improvement at a 1-degree
// step can round down under SIMULATION_LEG_SETTLE_MIN_DROP even though a
// genuinely lower resting pose exists a finer fraction of a degree away
// (a real report of exactly this -- settling stopping with the body
// still visibly floating). Halving on every failed attempt instead of
// giving straight up is the same "a coarse step can overshoot a narrow
// useful window" fix the original one-shot version of this search used,
// just kept as per-probe state now that this runs continuously instead
// of to completion in one call. Both reset to their starting constants
// by applyGravityStep alongside rockySettleConverged.
static float rockyKneeSettleStep = 0.0f;
static float rockyBodySettleStep = 0.0f;

static void advanceRockySettle(void)
{
    if (app.robotScene.activeKind != ROBOT_KIND_ROCKY)
        return;

    if (rockySettleConverged)
        return; // both probes already confirmed exhausted for this landing

    // Lazily initialize on first-ever use (also re-primed by
    // applyGravityStep on every fresh landing -- see its own comment) --
    // 0.0f only ever occurs before either has been set, since a real
    // landing always (re)sets both to their starting constants.
    if (rockyKneeSettleStep <= 0.0f)
        rockyKneeSettleStep = SIMULATION_LEG_SETTLE_STEP_DEG;
    if (rockyBodySettleStep <= 0.0f)
        rockyBodySettleStep = SIMULATION_BODY_SETTLE_STEP_DEG;

    Rocky* r = &app.robotScene.rocky;

    // Probe 1: bend the knee.
    if (rockyKneeSettleStep >= SIMULATION_LEG_SETTLE_MIN_STEP_DEG)
    {
        float baseKneeAngle = r->kneeAngle;
        float bestDrop = 0.0f;
        float bestKneeAngle = baseKneeAngle;

        for (int dir = -1; dir <= 1; dir += 2)
        {
            r->kneeAngle = baseKneeAngle + dir * rockyKneeSettleStep;

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

        // Compared against SIMULATION_LEG_SETTLE_MIN_DROP rather than a
        // bare > 0.0f -- see that constant's own comment on why: without
        // a floor, the search binary search's own precision noise right
        // at the true resting pose reads as an endless string of
        // wafer-thin "improvements," which is exactly what showed up as
        // visible vibration instead of a clean stop.
        if (bestDrop > SIMULATION_LEG_SETTLE_MIN_DROP)
        {
            r->kneeAngle = bestKneeAngle;
            translateActiveRobot(0.0f, -bestDrop);
            printf("[SETTLE] kneeAngle=%.2f bent the knee %.4f deg, body fell %.5f further this tick\\n",
                   r->kneeAngle, rockyKneeSettleStep, bestDrop);
            // Keep this probe's step size as-is -- it's still finding
            // real gains at this scale, no need to refine yet.
        }
        else
        {
            rockyKneeSettleStep *= 0.5f; // nothing here -- get finer before giving up
        }
    }

    // Probe 2: rotate the rectangle itself. Rotating r->angle turns the
    // WHOLE robot (rectangle, kneeCircle, and footCircle-after-kneeAngle)
    // around (bodyX, bodyY) -- see jointToWorld/rotatePoint -- so unlike
    // bending the knee, this can also shift the foot sideways along the
    // ground, exactly like a real object tipping over while one point of
    // it stays roughly planted. dropActiveRobotToRest only re-tests
    // straight-down clearance afterward, so a rotation only ever commits
    // if it genuinely opens up more room to fall, same guarantee as the
    // knee probe above.
    if (rockyBodySettleStep >= SIMULATION_LEG_SETTLE_MIN_STEP_DEG)
    {
        float baseBodyAngle = r->angle;
        float bestDrop = 0.0f;
        float bestBodyAngle = baseBodyAngle;

        for (int dir = -1; dir <= 1; dir += 2)
        {
            r->angle = baseBodyAngle + dir * rockyBodySettleStep;

            float drop = dropActiveRobotToRest();
            if (drop > 0.0f)
                translateActiveRobot(0.0f, drop); // undo the probe's own move

            if (drop > bestDrop)
            {
                bestDrop = drop;
                bestBodyAngle = r->angle;
            }
        }

        r->angle = baseBodyAngle;

        if (bestDrop > SIMULATION_LEG_SETTLE_MIN_DROP)
        {
            r->angle = bestBodyAngle;
            translateActiveRobot(0.0f, -bestDrop);
            printf("[SETTLE] bodyAngle=%.2f rotated the rectangle %.4f deg, body fell %.5f further this tick\\n",
                   r->angle, rockyBodySettleStep, bestDrop);
        }
        else
        {
            rockyBodySettleStep *= 0.5f;
        }
    }

    if (rockyKneeSettleStep < SIMULATION_LEG_SETTLE_MIN_STEP_DEG
        && rockyBodySettleStep < SIMULATION_LEG_SETTLE_MIN_STEP_DEG)
    {
        // Both probes have been refined all the way down to the floor
        // and still found nothing worth committing -- THIS is the
        // resting pose. Stop actually testing from here on (until the
        // next fresh landing) rather than continuing to probe every tick
        // for the same nothing.
        rockySettleConverged = TRUE;
        printf("[SETTLE] converged -- kneeAngle=%.2f bodyAngle=%.2f, no further improvement found\\n",
               r->kneeAngle, r->angle);
    }
}'''

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

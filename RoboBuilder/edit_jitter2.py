import shutil, glob

path = "canvas.c"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''static void advanceRockySettle(void)
{
    if (app.robotScene.activeKind != ROBOT_KIND_ROCKY)
        return;

    Rocky* r = &app.robotScene.rocky;

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

        if (bestDrop > 0.0f)
        {
            r->kneeAngle = bestKneeAngle;
            translateActiveRobot(0.0f, -bestDrop);
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

        if (bestDrop > 0.0f)
        {
            r->angle = bestBodyAngle;
            translateActiveRobot(0.0f, -bestDrop);
            printf("[SETTLE] bodyAngle=%.2f rotated the rectangle %.2f deg, body fell %.5f further this tick\\n",
                   r->angle, SIMULATION_BODY_SETTLE_STEP_DEG, bestDrop);
        }
    }
}'''

new = '''// Persists across calls: TRUE once a tick's probes both come up empty
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

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

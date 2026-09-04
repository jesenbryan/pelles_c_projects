import shutil, glob

path = "canvas.c"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''// Rocky-only: called every single gravity tick (applyGravityStep, below,
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

new = '''// Rocky-only: called every single gravity tick (applyGravityStep, below,
// calls this every time any part of Rocky is touching something -- see
// its own comment), tries two independent one-small-step probes -- bend
// the KNEE, and rotate the RECTANGLE itself (Rocky's own `angle` field)
// -- and commits whichever of them (if either) lets the body fall any
// further right now. This is specifically for the knee-detach feature
// (app.h's own comment on draggingRockyKnee): once the knee can sit far
// from the rectangle, gravity's own whole-body drop stops the instant
// its LOWEST part (often the foot) touches something, leaving the
// rectangle exactly as high up (and at whatever ORIENTATION) that pose
// geometrically places it -- which can look like the body is floating,
// when really it's just resting on top of an oddly long/offset leg.
// Real robots settle that kind of thing by both the knee bending AND the
// torso tipping under its own weight until something else (the
// rectangle itself) also finds the ground; being called every tick lets
// this happen gradually, over as many ticks as it takes, rather than
// snapping to the final pose the instant it lands.
//
// Each probe is independent coordinate-descent, one variable at a time
// (first kneeAngle, then r->angle, each against whatever the OTHER just
// ended up as) -- not a single combined search over both together, so a
// resting pose that only opens up once BOTH change together at once
// could be missed. In practice this converges fine over enough ticks
// since each variable's own improvement still keeps nudging the other's
// available range too, and it keeps each individual probe cheap and easy
// to reason about.
//
// Deliberately does only ONE bounded step per probe per call rather than
// hill-climbing to full convergence in one shot -- called continuously
// (every tick, by explicit request) instead of once, it doesn't need to
// finish in a single call; a tiny fixed step per tick is what actually
// produces a smooth multi-frame sink/tip rather than an instant snap.
// Safe to call this often because there's no velocity or damping state
// carried between calls (unlike the removed knee pendulum) -- each probe
// is an independent, deterministic test that only ever commits a change
// it has already confirmed (via dropActiveRobotToRest, the same
// binary-search idiom the rest of gravity uses) genuinely lets the body
// fall further while staying collision-free, so there is nothing here
// that can accumulate into an oscillation or a stuck state.
static void advanceRockySettle(void)
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

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

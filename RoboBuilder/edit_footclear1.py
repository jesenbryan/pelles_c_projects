import shutil, glob, re

path = "canvas.c"

existing = glob.glob(path + ".bak*")
nums = [int(re.search(r"\.bak(\d+)$", f).group(1)) for f in existing if re.search(r"\.bak(\d+)$", f)]
n = max(nums) + 1 if nums else 1
backup = f"{path}.bak{n}"
shutil.copyfile(path, backup)
print("backup:", backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = """        float baseKneeAngle = r->kneeAngle;
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
    }"""

assert content.count(old) == 1, f"old match count: {content.count(old)}"

new = """        float baseKneeAngle = r->kneeAngle;
        float bestDrop = 0.0f;
        float bestKneeAngle = baseKneeAngle;

        // Baseline clearance of the FOOT alone (same measurement
        // rockyEscapeLegPenetration's own [STUCK] print uses), captured
        // before either candidate angle is tried below. This is the
        // fallback criterion for when dropActiveRobotToRest can't see an
        // improvement at all: see the comment inside the loop for why
        // that happens and why this catches it.
        PointF baseCenter = getRockyCenter(*r);
        PointF baseFootWorld = jointToWorld(r->footCircle, r->kneeCircle, baseKneeAngle, baseCenter, r->angle);
        float baseFecx, baseFecy;
        robotPointToEnvWorld(baseFootWorld.x, baseFootWorld.y, &baseFecx, &baseFecy);
        float footCombinedRadius = robotLengthToEnvWorld(r->footRadius) + simEnvLineHalfWidthWorld();
        float baseFootClearance = nearestEnvDistance(baseFecx, baseFecy) - footCombinedRadius;

        float bestClearanceGain = 0.0f;
        float bestClearanceKneeAngle = baseKneeAngle;

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

            // Fallback for exactly the "rectangle rests on a corner,
            // foot dangles in mid-air" pose a real report showed:
            // dropActiveRobotToRest tests a whole-body translate, but
            // bending the knee never moves the rectangle's own corners
            // -- once THOSE are already what's resting on the ground,
            // any candidate kneeAngle re-collides via that same corner
            // the instant the body is pushed down at all, so drop comes
            // back exactly 0.0f no matter how much closer the dangling
            // foot itself would get. Comparing the foot's own clearance
            // directly catches that case: if this candidate angle
            // doesn't embed anything and brings the foot measurably
            // closer to the ground than the baseline, that is a real
            // improvement even though the body itself never moves.
            if (!robotCollidesWithEnvironment())
            {
                PointF center = getRockyCenter(*r);
                PointF footWorld = jointToWorld(r->footCircle, r->kneeCircle, r->kneeAngle, center, r->angle);
                float fecx, fecy;
                robotPointToEnvWorld(footWorld.x, footWorld.y, &fecx, &fecy);
                float clearance = nearestEnvDistance(fecx, fecy) - footCombinedRadius;
                float gain = baseFootClearance - clearance;

                if (clearance >= 0.0f && gain > bestClearanceGain)
                {
                    bestClearanceGain = gain;
                    bestClearanceKneeAngle = r->kneeAngle;
                }
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
        else if (bestClearanceGain > SIMULATION_LEG_SETTLE_MIN_DROP)
        {
            // The body itself didn't fall any further, but the dangling
            // foot is now genuinely closer to the ground than it was --
            // commit the knee rotation alone. No translateActiveRobot
            // call here: the body isn't moving, only the foot swings
            // around the still-fixed knee.
            r->kneeAngle = bestClearanceKneeAngle;
            printf("[SETTLE] kneeAngle=%.2f swung the dangling foot %.4f deg closer to the ground (clearance improved %.5f, body stayed put)\\n",
                   r->kneeAngle, rockyKneeSettleStep, bestClearanceGain);
        }
        else
        {
            rockyKneeSettleStep *= 0.5f; // nothing here -- get finer before giving up
        }
    }"""

content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("done")

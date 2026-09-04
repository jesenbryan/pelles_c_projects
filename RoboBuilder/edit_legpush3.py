import shutil, glob

path = "canvas.c"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''        if ((wParam == 'E' || wParam == 'Q') && appMode == APP_MODE_SIMULATION && app.robotScene.activeKind == ROBOT_KIND_ROCKY)
        {
            float step = (wParam == 'E') ? -SIMULATION_JOINT_ROTATE_STEP_DEG : SIMULATION_JOINT_ROTATE_STEP_DEG;

            // Propose the rotation, then check the FOOT specifically
            // (rockyFootCollides, not the whole-robot
            // robotCollidesWithEnvironment resolveUpwardIfPenetrating
            // uses) -- if it would now be inside the ground, binary-search
            // the largest safe fraction of this one step instead of
            // applying the full rotation and only afterward trying a
            // small fixed-budget whole-body nudge to undo it.
            // resolveUpwardIfPenetrating's budget
            // (SIMULATION_SLOPE_CORRECTION_MAX) exists for tiny
            // slope-alignment residuals, not a real joint rotation --
            // a large enough E/Q press while resting on the ground could
            // plunge the foot deeper than that budget could ever pull it
            // back out of, leaving it visibly stuck through the floor.
            Rocky* r = &app.robotScene.rocky;

            // Self-healing step -- see rockyEscapeFootPenetration's own
            // comment -- so everything below always starts from a
            // genuinely safe oldKneeAngle instead of possibly a
            // hair-embedded one.
            rockyEscapeFootPenetration();

            float oldKneeAngle = r->kneeAngle;
            r->kneeAngle = oldKneeAngle + step;

            if (rockyFootCollides())
            {
                // HARD STOP at the ground -- per explicit request, E/Q must
                // never let the foot pass through the environment, not even
                // by jumping to a clear spot on the far side of an
                // obstruction (an earlier version of this did exactly that,
                // "gliding" over whatever was in the way -- technically the
                // foot was still passing through solid ground to get there,
                // which reads as wrong the moment you think about it rather
                // than just "stuck").
                //
                // rockyEscapeFootPenetration already guarantees oldKneeAngle
                // itself is collision-free (or leaves it alone if the WHOLE
                // circle is blocked, an extreme edge case), so this binary
                // search always has a genuinely safe point at delta=0 to
                // shrink toward -- it settles at the largest fraction of
                // THIS one step that stays clear, i.e. exactly at first
                // contact, and simply refuses to go further in that
                // direction until the key is pressed the other way.
                float safe = 0.0f;
                float blocked = step;

                for (int i = 0; i < GRAVITY_CONTACT_SEARCH_ITERATIONS; i++)
                {
                    float mid = (safe + blocked) * 0.5f;
                    r->kneeAngle = oldKneeAngle + mid;
                    if (rockyFootCollides()) blocked = mid; else safe = mid;
                }

                r->kneeAngle = oldKneeAngle + safe;
            }

            // Re-arm the [CONTACT] one-shot dump (see drawSimulationContactDebug)
            // so the very next paint prints a fresh point/hit count reflecting
            // THIS rotation -- without this, that dump only ever fires once
            // right after toggling C and goes silent for every E/Q press
            // after that, which reads as "the debug log doesn't update" even
            // though the dots themselves (recomputed from the live kneeAngle
            // every frame) are already moving correctly. No-ops harmlessly if
            // showContactDebug is off.
            contactDebugLoggedThisToggle = FALSE;

            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }'''

new = '''        if ((wParam == 'E' || wParam == 'Q') && appMode == APP_MODE_SIMULATION && app.robotScene.activeKind == ROBOT_KIND_ROCKY)
        {
            float step = (wParam == 'E') ? -SIMULATION_JOINT_ROTATE_STEP_DEG : SIMULATION_JOINT_ROTATE_STEP_DEG;

            // The requested rotation always applies IN FULL now -- rather
            // than refusing a rotation that would bury the foot or a shin
            // arc (computeRockyArcPoints) in the ground, as soon as that
            // would happen the whole robot is pushed straight up instead,
            // exactly like a real leg pressing its foot/shin against the
            // ground levers the torso upward rather than the leg simply
            // stopping dead. Only if even a generous push can't clear it
            // (see SIMULATION_LEG_PUSH_SEARCH_MAX's own comment -- the leg
            // boxed in on every side, an extreme edge case) does this fall
            // back to the OLD hard-stop-at-first-contact behavior, so E/Q
            // still never lets the leg visibly pass through solid ground
            // either way -- it just tries lifting the body out of the way
            // first, before ever giving up and freezing the angle.
            Rocky* r = &app.robotScene.rocky;

            // Self-healing step -- see rockyEscapeLegPenetration's own
            // comment -- so everything below always starts from a
            // genuinely safe oldKneeAngle instead of possibly a
            // hair-embedded one.
            rockyEscapeLegPenetration();

            float oldKneeAngle = r->kneeAngle;
            r->kneeAngle = oldKneeAngle + step;

            if (robotCollidesWithEnvironment())
            {
                // Grow the push distance (doubling from
                // SIMULATION_LEG_PUSH_SEARCH_START) until lifting the body
                // by that much actually clears the leg, or the sanity cap
                // SIMULATION_LEG_PUSH_SEARCH_MAX is reached -- there's no
                // way to know in advance how far a deliberate leg
                // extension needs to lift the torso, so this starts small
                // and keeps doubling instead of guessing one fixed number
                // (unlike resolveUpwardIfPenetrating's own small fixed
                // budget, meant only for tiny post-rotation residuals).
                float blocked = 0.0f;   // push=0 already collides -- we just tested that above
                float clear = -1.0f;    // first push amount confirmed to clear, if any
                float push = SIMULATION_LEG_PUSH_SEARCH_START;

                for (;;)
                {
                    if (push > SIMULATION_LEG_PUSH_SEARCH_MAX)
                        push = SIMULATION_LEG_PUSH_SEARCH_MAX;

                    translateActiveRobot(0.0f, push);
                    BOOL stillHits = robotCollidesWithEnvironment();
                    translateActiveRobot(0.0f, -push);

                    if (!stillHits) { clear = push; break; }

                    blocked = push;
                    if (push >= SIMULATION_LEG_PUSH_SEARCH_MAX)
                        break; // exhausted the budget -- no escape found by pushing up

                    push *= 2.0f;
                }

                if (clear >= 0.0f)
                {
                    // Binary-search within [blocked, clear] for the exact
                    // push that first clears the leg, same
                    // GRAVITY_CONTACT_SEARCH_ITERATIONS idiom
                    // applyGravityStep's own downward search uses.
                    for (int i = 0; i < GRAVITY_CONTACT_SEARCH_ITERATIONS; i++)
                    {
                        float mid = (blocked + clear) * 0.5f;
                        translateActiveRobot(0.0f, mid);
                        BOOL stillHits = robotCollidesWithEnvironment();
                        translateActiveRobot(0.0f, -mid);

                        if (stillHits) blocked = mid; else clear = mid;
                    }

                    translateActiveRobot(0.0f, clear);
                    printf("[LEGPUSH] kneeAngle=%.2f pushed body up by %.5f to keep the leg clear\\n",
                           r->kneeAngle, clear);
                }
                else
                {
                    // Even SIMULATION_LEG_PUSH_SEARCH_MAX of lift doesn't
                    // clear it -- fall back to the OLD hard-stop: undo
                    // however much of THIS rotation still buries the leg,
                    // binary-searching the largest fraction of
                    // (oldKneeAngle -> proposed angle) that stays clear at
                    // the CURRENT (unlifted) body position, same as this
                    // handler did before push-up existed.
                    //
                    // rockyEscapeLegPenetration already guarantees
                    // oldKneeAngle itself is collision-free (or leaves it
                    // alone if the WHOLE circle is blocked, an extreme
                    // edge case), so this binary search always has a
                    // genuinely safe point at fraction=0 to shrink toward.
                    float proposedKneeAngle = r->kneeAngle;
                    float safeFrac = 0.0f;
                    float blockedFrac = 1.0f;

                    for (int i = 0; i < GRAVITY_CONTACT_SEARCH_ITERATIONS; i++)
                    {
                        float midFrac = (safeFrac + blockedFrac) * 0.5f;
                        r->kneeAngle = oldKneeAngle + (proposedKneeAngle - oldKneeAngle) * midFrac;
                        if (robotCollidesWithEnvironment()) blockedFrac = midFrac; else safeFrac = midFrac;
                    }

                    r->kneeAngle = oldKneeAngle + (proposedKneeAngle - oldKneeAngle) * safeFrac;

                    printf("[STUCK] E/Q: leg still buried even after pushing up by SIMULATION_LEG_PUSH_SEARCH_MAX=%.3f -- held at kneeAngle=%.2f\\n",
                           (float)SIMULATION_LEG_PUSH_SEARCH_MAX, r->kneeAngle);
                }
            }

            // Re-arm the [CONTACT] one-shot dump (see drawSimulationContactDebug)
            // so the very next paint prints a fresh point/hit count reflecting
            // THIS rotation -- without this, that dump only ever fires once
            // right after toggling C and goes silent for every E/Q press
            // after that, which reads as "the debug log doesn't update" even
            // though the dots themselves (recomputed from the live kneeAngle
            // every frame) are already moving correctly. No-ops harmlessly if
            // showContactDebug is off.
            contactDebugLoggedThisToggle = FALSE;

            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }'''

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

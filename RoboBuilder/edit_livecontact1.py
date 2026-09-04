import shutil, glob, re

def backup(path):
    existing = glob.glob(path + ".bak*")
    nums = [int(re.search(r"\.bak(\d+)$", f).group(1)) for f in existing if re.search(r"\.bak(\d+)$", f)]
    n = max(nums) + 1 if nums else 1
    b = f"{path}.bak{n}"
    shutil.copyfile(path, b)
    print("backup:", b)
    return b

path = "canvas.c"
backup(path)
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = """    gSuppressGravityDebug = savedSuppressGravityDebug;

    if (logThisPass)
    {
        printf("[CONTACT] dump: activeKind=%d pointsTested=%d currentlyColliding=%d\\n",
               app.robotScene.activeKind, dbgPointCount, dbgHitCount);
    }
}

// Rigidly moves whichever robot is currently active (app.robotScene."""
assert content.count(old) == 1, f"match: {content.count(old)}"

new = """    gSuppressGravityDebug = savedSuppressGravityDebug;

    if (logThisPass)
    {
        printf("[CONTACT] dump: activeKind=%d pointsTested=%d currentlyColliding=%d\\n",
               app.robotScene.activeKind, dbgPointCount, dbgHitCount);
    }
}

// Always-on companion to drawSimulationContactDebug above, by explicit
// request: that function only draws anything while the plain-C toggle
// (showContactDebug) is on, and even then draws EVERY sampled point --
// hits big and red, misses small and green -- as a diagnostic overlay of
// the whole collision test. This instead runs unconditionally any time
// we're in Simulation mode, and draws NOTHING for a point that isn't
// currently colliding -- just one small red dot exactly where contact is
// actually happening right now, so a real touch/land/collision always has
// a visible marker without needing to remember to switch on the fuller
// debug view. Live only, by explicit request -- no accumulated trail: a
// dot disappears the instant that point stops colliding, same as any
// other per-frame overlay.
//
// Same per-kind sampling as robotCollidesWithEnvironment/
// drawSimulationContactDebug (body circles, shin/thigh fillet arcs, and
// for Rocky the rectangle edges), including Rocky's own legHidden/
// bodyHidden testing toggles (ID_ROCKY_TOGGLE_LEG_BUTTON/
// ID_ROCKY_TOGGLE_BODY_BUTTON) -- a hidden part isn't sampled here
// either, consistent with it not being sampled for real collision or the
// debug overlay.
static void drawSimulationLiveContactDots(void)
{
    if (appMode != APP_MODE_SIMULATION) return;

    // Same suppression as drawSimulationContactDebug's own -- this calls
    // pointCollidesWithAnyEnvironmentStroke up to ~250 times EVERY FRAME
    // purely to decide dot color, and running unconditionally (not just
    // while a debug toggle is on) makes that even more important to keep
    // quiet than the toggle-gated overlay above.
    BOOL savedSuppressGravityDebug = gSuppressGravityDebug;
    gSuppressGravityDebug = TRUE;

    const float liveContactDotRadius = 0.007f;
    float eArcThickness = robotLengthToEnvWorld(SIMULATION_ARC_COLLISION_THICKNESS);

    switch (app.robotScene.activeKind)
    {
        case ROBOT_KIND_ROCKY:
        {
            if (!app.robotScene.rocky.legHidden)
            {
                CircleSegment bodyCircles[NUM_ROCKY_BODY_CIRCLES];
                computeRockyBodyCircles(app.robotScene.rocky, bodyCircles);

                for (int c = 0; c < NUM_ROCKY_BODY_CIRCLES; c++)
                {
                    float ecx, ecy;
                    robotPointToEnvWorld(bodyCircles[c].center.x, bodyCircles[c].center.y, &ecx, &ecy);
                    float eRadius = robotLengthToEnvWorld(bodyCircles[c].radius);
                    if (pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eRadius))
                        drawMarkerDisc(bodyCircles[c].center.x, bodyCircles[c].center.y, liveContactDotRadius, 0.95f, 0.1f, 0.1f, 1.0f);
                }

                PointF arcPts[NUM_ROCKY_CIRCLE_SEGMENTS][ARC_SAMPLE_COUNT];
                int arcCounts[NUM_ROCKY_CIRCLE_SEGMENTS];
                computeRockyArcPoints(app.robotScene.rocky, arcPts, arcCounts);

                for (int a = 0; a < NUM_ROCKY_CIRCLE_SEGMENTS; a++)
                {
                    for (int i = 0; i < arcCounts[a]; i++)
                    {
                        float ecx, ecy;
                        robotPointToEnvWorld(arcPts[a][i].x, arcPts[a][i].y, &ecx, &ecy);
                        if (pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eArcThickness))
                            drawMarkerDisc(arcPts[a][i].x, arcPts[a][i].y, liveContactDotRadius, 0.95f, 0.1f, 0.1f, 1.0f);
                    }
                }
            }

            if (!app.robotScene.rocky.bodyHidden)
            {
                RockyEdgeSegment rectEdges[NUM_ROCKY_RECT_SEGMENTS];
                computeRockyRectSegments(app.robotScene.rocky, rectEdges);

                for (int e = 0; e < NUM_ROCKY_RECT_SEGMENTS; e++)
                {
                    for (int i = 0; i < ARC_SAMPLE_COUNT; i++)
                    {
                        float t = (float)i / (float)(ARC_SAMPLE_COUNT - 1);
                        float lx = rectEdges[e].start.x + (rectEdges[e].end.x - rectEdges[e].start.x) * t;
                        float ly = rectEdges[e].start.y + (rectEdges[e].end.y - rectEdges[e].start.y) * t;

                        float ecx, ecy;
                        robotPointToEnvWorld(lx, ly, &ecx, &ecy);
                        if (pointCollidesWithAnyEnvironmentStroke(ecx, ecy, 0.0f))
                            drawMarkerDisc(lx, ly, liveContactDotRadius, 0.95f, 0.1f, 0.1f, 1.0f);
                    }
                }
            }
            break;
        }

        case ROBOT_KIND_STILO:
        {
            CircleSegment bodyCircles[NUM_STILO_BODY_CIRCLES];
            computeStiloBodyCircles(app.robotScene.stilo, bodyCircles);

            for (int c = 0; c < NUM_STILO_BODY_CIRCLES; c++)
            {
                float ecx, ecy;
                robotPointToEnvWorld(bodyCircles[c].center.x, bodyCircles[c].center.y, &ecx, &ecy);
                float eRadius = robotLengthToEnvWorld(bodyCircles[c].radius);
                if (pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eRadius))
                    drawMarkerDisc(bodyCircles[c].center.x, bodyCircles[c].center.y, liveContactDotRadius, 0.95f, 0.1f, 0.1f, 1.0f);
            }

            PointF arcPts[NUM_STILO_CIRCLE_SEGMENTS][ARC_SAMPLE_COUNT];
            int arcCounts[NUM_STILO_CIRCLE_SEGMENTS];
            computeStiloArcPoints(app.robotScene.stilo, arcPts, arcCounts);

            for (int a = 0; a < NUM_STILO_CIRCLE_SEGMENTS; a++)
            {
                for (int i = 0; i < arcCounts[a]; i++)
                {
                    float ecx, ecy;
                    robotPointToEnvWorld(arcPts[a][i].x, arcPts[a][i].y, &ecx, &ecy);
                    if (pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eArcThickness))
                        drawMarkerDisc(arcPts[a][i].x, arcPts[a][i].y, liveContactDotRadius, 0.95f, 0.1f, 0.1f, 1.0f);
                }
            }
            break;
        }

        case ROBOT_KIND_SEMNI:
        default:
        {
            CircleSegment bodyCircles[NUM_ROBOT_BODY_CIRCLES];
            computeSemniBodyCircles(app.robotScene.robot, bodyCircles);

            for (int c = 0; c < NUM_ROBOT_BODY_CIRCLES; c++)
            {
                float ecx, ecy;
                robotPointToEnvWorld(bodyCircles[c].center.x, bodyCircles[c].center.y, &ecx, &ecy);
                float eRadius = robotLengthToEnvWorld(bodyCircles[c].radius);
                if (pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eRadius))
                    drawMarkerDisc(bodyCircles[c].center.x, bodyCircles[c].center.y, liveContactDotRadius, 0.95f, 0.1f, 0.1f, 1.0f);
            }

            PointF arcPts[NUM_ROBOT_CIRCLE_SEGMENTS][ARC_SAMPLE_COUNT];
            int arcCounts[NUM_ROBOT_CIRCLE_SEGMENTS];
            computeSemniArcPoints(app.robotScene.robot, arcPts, arcCounts);

            for (int a = 0; a < NUM_ROBOT_CIRCLE_SEGMENTS; a++)
            {
                for (int i = 0; i < arcCounts[a]; i++)
                {
                    float ecx, ecy;
                    robotPointToEnvWorld(arcPts[a][i].x, arcPts[a][i].y, &ecx, &ecy);
                    if (pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eArcThickness))
                        drawMarkerDisc(arcPts[a][i].x, arcPts[a][i].y, liveContactDotRadius, 0.95f, 0.1f, 0.1f, 1.0f);
                }
            }
            break;
        }
    }

    gSuppressGravityDebug = savedSuppressGravityDebug;
}

// Rigidly moves whichever robot is currently active (app.robotScene."""

content = content.replace(old, new)
with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("canvas.c (new drawSimulationLiveContactDots) done")

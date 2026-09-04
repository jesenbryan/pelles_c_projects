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

old = """        case ROBOT_KIND_ROCKY:
        {
            CircleSegment bodyCircles[NUM_ROCKY_BODY_CIRCLES];
            computeRockyBodyCircles(app.robotScene.rocky, bodyCircles);

            for (int c = 0; c < NUM_ROCKY_BODY_CIRCLES; c++)
            {
                float ecx, ecy;
                robotPointToEnvWorld(bodyCircles[c].center.x, bodyCircles[c].center.y, &ecx, &ecy);
                float eRadius = robotLengthToEnvWorld(bodyCircles[c].radius);
                BOOL hit = pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eRadius);
                // Colliding points draw noticeably BIGGER (2.5x), not just
                // a different color -- a same-size color swap on an
                // already-small marker is easy to miss at a glance,
                // especially sitting right at ground contact where the
                // environment's own line is drawn close by. Size is the
                // primary signal here; color is secondary.
                drawMarkerDisc(bodyCircles[c].center.x, bodyCircles[c].center.y, hit ? dotRadius * 2.5f : dotRadius,
                               hit ? 0.95f : 0.15f, hit ? 0.15f : 0.85f, 0.15f, hit ? 1.0f : 0.9f);
                if (logThisPass) { dbgPointCount++; if (hit) dbgHitCount++; }
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
                    BOOL hit = pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eArcThickness);
                    drawMarkerDisc(arcPts[a][i].x, arcPts[a][i].y, hit ? smallDotRadius * 3.0f : smallDotRadius,
                                   hit ? 0.95f : 0.15f, hit ? 0.15f : 0.85f, 0.15f, hit ? 1.0f : 0.7f);
                    if (logThisPass) { dbgPointCount++; if (hit) dbgHitCount++; }
                }
            }

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
                    BOOL hit = pointCollidesWithAnyEnvironmentStroke(ecx, ecy, 0.0f);
                    drawMarkerDisc(lx, ly, hit ? smallDotRadius * 3.0f : smallDotRadius,
                                   hit ? 0.95f : 0.15f, hit ? 0.15f : 0.85f, 0.15f, hit ? 1.0f : 0.7f);
                    if (logThisPass) { dbgPointCount++; if (hit) dbgHitCount++; }
                }
            }
            break;
        }

        case ROBOT_KIND_STILO:"""
assert content.count(old) == 1, f"match: {content.count(old)}"

new = """        case ROBOT_KIND_ROCKY:
        {
            // Testing-only toggles (ID_ROCKY_TOGGLE_LEG_BUTTON/
            // ID_ROCKY_TOGGLE_BODY_BUTTON) -- a hidden part must not be
            // SAMPLED here either, not just excluded from the real
            // robotCollidesWithEnvironment check above: this overlay
            // runs its own completely separate point pass purely to draw
            // per-point hit/miss dots and print pointsTested= in the
            // [CONTACT] dump, so without this guard a hidden leg/body
            // would still show up in that count (and, worse, still draw
            // its dots) even though it's supposedly gone.
            if (!app.robotScene.rocky.legHidden)
            {
                CircleSegment bodyCircles[NUM_ROCKY_BODY_CIRCLES];
                computeRockyBodyCircles(app.robotScene.rocky, bodyCircles);

                for (int c = 0; c < NUM_ROCKY_BODY_CIRCLES; c++)
                {
                    float ecx, ecy;
                    robotPointToEnvWorld(bodyCircles[c].center.x, bodyCircles[c].center.y, &ecx, &ecy);
                    float eRadius = robotLengthToEnvWorld(bodyCircles[c].radius);
                    BOOL hit = pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eRadius);
                    // Colliding points draw noticeably BIGGER (2.5x), not just
                    // a different color -- a same-size color swap on an
                    // already-small marker is easy to miss at a glance,
                    // especially sitting right at ground contact where the
                    // environment's own line is drawn close by. Size is the
                    // primary signal here; color is secondary.
                    drawMarkerDisc(bodyCircles[c].center.x, bodyCircles[c].center.y, hit ? dotRadius * 2.5f : dotRadius,
                                   hit ? 0.95f : 0.15f, hit ? 0.15f : 0.85f, 0.15f, hit ? 1.0f : 0.9f);
                    if (logThisPass) { dbgPointCount++; if (hit) dbgHitCount++; }
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
                        BOOL hit = pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eArcThickness);
                        drawMarkerDisc(arcPts[a][i].x, arcPts[a][i].y, hit ? smallDotRadius * 3.0f : smallDotRadius,
                                       hit ? 0.95f : 0.15f, hit ? 0.15f : 0.85f, 0.15f, hit ? 1.0f : 0.7f);
                        if (logThisPass) { dbgPointCount++; if (hit) dbgHitCount++; }
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
                        BOOL hit = pointCollidesWithAnyEnvironmentStroke(ecx, ecy, 0.0f);
                        drawMarkerDisc(lx, ly, hit ? smallDotRadius * 3.0f : smallDotRadius,
                                       hit ? 0.95f : 0.15f, hit ? 0.15f : 0.85f, 0.15f, hit ? 1.0f : 0.7f);
                        if (logThisPass) { dbgPointCount++; if (hit) dbgHitCount++; }
                    }
                }
            }
            break;
        }

        case ROBOT_KIND_STILO:"""

content = content.replace(old, new)
with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("canvas.c (contact debug) done")

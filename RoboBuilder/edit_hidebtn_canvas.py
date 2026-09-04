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

                if (pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eRadius))
                    return TRUE;
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
                        return TRUE;
                }
            }
"""
assert content.count(old) == 1, f"old match count: {content.count(old)}"

new = """        case ROBOT_KIND_ROCKY:
        {
            // Testing-only toggles (ID_ROCKY_TOGGLE_LEG_BUTTON/
            // ID_ROCKY_TOGGLE_BODY_BUTTON, see app.h's Rocky comment) --
            // a hidden part is dropped out of collision entirely, not
            // just rendering (drawRocky has the matching guard), so
            // gravity/settle behavior for the remaining part alone can
            // actually be tested without the hidden part's own geometry
            // silently still anchoring/blocking it.
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
                        return TRUE;
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
                            return TRUE;
                    }
                }
            }
"""

content = content.replace(old, new)

old_rect = """            RockyEdgeSegment rectEdges[NUM_ROCKY_RECT_SEGMENTS];
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
                        return TRUE;
                }
            }

            return FALSE;
        }

        case ROBOT_KIND_STILO:"""
assert content.count(old_rect) == 1, f"old_rect match count: {content.count(old_rect)}"

new_rect = """            if (!app.robotScene.rocky.bodyHidden)
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
                            return TRUE;
                    }
                }
            }

            return FALSE;
        }

        case ROBOT_KIND_STILO:"""

content = content.replace(old_rect, new_rect)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("canvas.c done")

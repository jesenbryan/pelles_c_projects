import shutil, glob, re

def backup(path):
    existing = glob.glob(path + ".bak*")
    nums = [int(re.search(r"\.bak(\d+)$", f).group(1)) for f in existing if re.search(r"\.bak(\d+)$", f)]
    n = max(nums) + 1 if nums else 1
    b = f"{path}.bak{n}"
    shutil.copyfile(path, b)
    print("backup:", b)
    return b

path = "renderer.c"
backup(path)
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = """    BOOL handlesVisible = includeHandles && (editorModeState.currentMode == EDITOR_MODE_SEMNI);
    if (handlesVisible)
    {
        PointF center = getRockyCenter(b);
        drawHandle(center, rs->draggingRockyBody || rs->hoverRockyBody, HIP_HANDLE_RADIUS, opacity);

        // Body/Leg Weight mass-center dot -- draggable alternative to
        // dragging hWeightRatioSlider (see input.c's
        // hoverRockyMassCenter/draggingRockyMassCenter). Gated the same as
        // every OTHER handle here now that it's a real drag target, not
        // just an informational overlay -- so it's hidden in an exported
        // BMP/the dimmed background copy, same as the body/knee/foot dots
        // above and below.
        drawRockyMassCenterMarker(computeRockyMassCenterWorld(b), rs->draggingRockyMassCenter || rs->hoverRockyMassCenter, MASS_CENTER_HANDLE_RADIUS, opacity);

        // Knee handle -- where the leg attaches to the rectangle (see
        // drawRockyLeg's own kneeWorld), same visible-dot treatment as
        // Semni's own knee handle (drawThigh's drawHandle(kneeWorld, ...)).
        PointF kneeWorld = rotatePoint(b.kneeCircle, center, b.angle);
        drawHandle(kneeWorld, rs->draggingRockyKnee || rs->hoverRockyKnee, KNEE_HANDLE_RADIUS, opacity);

        // Foot handle -- far end of the shin, same visible-dot treatment
        // as Semni's own foot handle (drawShinHandles' drawHandle(footWorld, ...)).
        PointF footWorld = jointToWorld(b.footCircle, b.kneeCircle, b.kneeAngle, center, b.angle);
        drawHandle(footWorld, rs->draggingRockyFoot || rs->hoverRockyFoot, FOOT_HANDLE_RADIUS, opacity);

        // Shin connector-arc handles -- same circleAtAxisMid construction
        // (pinned to the exact middle of the knee->foot axis) Semni's own
        // drawShinHandles uses for its shin1MidLocal/shin2MidLocal, NOT
        // drawRockyLeg's own circleTowardPoint bulge point (that one's for
        // where the curve is actually DRAWN, this one's for where the
        // handle DOT sits -- Semni keeps that same distinction). Drag-only
        // highlight, no separate hover state, same as Semni's shin handles.
        {
            PointF shinAxisMidLocal = { (b.kneeCircle.x + b.footCircle.x) * 0.5f, (b.kneeCircle.y + b.footCircle.y) * 0.5f };

            Fillet shin1Fillet = filletFromAttachAngle(b.kneeCircle, b.kneeRadius, b.footCircle, b.footRadius, b.shinArc1Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
            PointF shin1NearLocal = circleTowardPoint(shin1Fillet.center, shin1Fillet.radius, shinAxisMidLocal);
            PointF shin1MidLocal = circleAtAxisMid(shin1Fillet.center, shin1Fillet.radius, b.kneeCircle, b.footCircle, shin1NearLocal);

            Fillet shin2Fillet = filletFromAttachAngleConcave(b.kneeCircle, b.kneeRadius, b.footCircle, b.footRadius, b.shinArc2Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC2_CONCAVE_R);
            PointF shin2NearLocal = circleTowardPoint(shin2Fillet.center, shin2Fillet.radius, shinAxisMidLocal);
            PointF shin2MidLocal = circleAtAxisMid(shin2Fillet.center, shin2Fillet.radius, b.kneeCircle, b.footCircle, shin2NearLocal);

            PointF shin1World = jointToWorld(shin1MidLocal, b.kneeCircle, b.kneeAngle, center, b.angle);
            PointF shin2World = jointToWorld(shin2MidLocal, b.kneeCircle, b.kneeAngle, center, b.angle);

            drawHandle(shin1World, rs->draggingRockyShin1 || rs->hoverRockyShin1, ROCKY_SHIN_HANDLE_RADIUS, opacity);
            drawHandle(shin2World, rs->draggingRockyShin2 || rs->hoverRockyShin2, ROCKY_SHIN_HANDLE_RADIUS, opacity);
        }

        // One small handle at the midpoint of each of the 4 edges --
        // marks where hovering + dragging resizes just that side (see
        // input.c's hitTestRockyEdge), same idea as the body handle
        // above but sized down a notch (KNEE_HANDLE_RADIUS) so the two
        // read as different, less prominent handles.
        float angle = b.angle;

        PointF leftMid   = rotatePoint((PointF){ b.bodyX - b.bodyHalfWidth, b.bodyY }, center, angle);
        PointF rightMid  = rotatePoint((PointF){ b.bodyX + b.bodyHalfWidth, b.bodyY }, center, angle);
        PointF bottomMid = rotatePoint((PointF){ b.bodyX, b.bodyY - b.bodyHalfHeight }, center, angle);
        PointF topMid    = rotatePoint((PointF){ b.bodyX, b.bodyY + b.bodyHalfHeight }, center, angle);

        drawHandle(leftMid,   rs->draggingRockyEdge == ROCKY_EDGE_LEFT   || rs->hoverRockyEdge == ROCKY_EDGE_LEFT,   KNEE_HANDLE_RADIUS, opacity);
        drawHandle(rightMid,  rs->draggingRockyEdge == ROCKY_EDGE_RIGHT  || rs->hoverRockyEdge == ROCKY_EDGE_RIGHT,  KNEE_HANDLE_RADIUS, opacity);
        drawHandle(bottomMid, rs->draggingRockyEdge == ROCKY_EDGE_BOTTOM || rs->hoverRockyEdge == ROCKY_EDGE_BOTTOM, KNEE_HANDLE_RADIUS, opacity);
        drawHandle(topMid,    rs->draggingRockyEdge == ROCKY_EDGE_TOP    || rs->hoverRockyEdge == ROCKY_EDGE_TOP,    KNEE_HANDLE_RADIUS, opacity);
    }"""
assert content.count(old) == 1, f"match: {content.count(old)}"

new = """    BOOL handlesVisible = includeHandles && (editorModeState.currentMode == EDITOR_MODE_SEMNI);
    if (handlesVisible)
    {
        PointF center = getRockyCenter(b);

        // Body move handle -- testing-only toggle (ID_ROCKY_TOGGLE_BODY_
        // BUTTON): dropped along with the rectangle itself when
        // bodyHidden, same as the edge handles further below (also
        // body-only).
        if (!b.bodyHidden)
            drawHandle(center, rs->draggingRockyBody || rs->hoverRockyBody, HIP_HANDLE_RADIUS, opacity);

        // Body/Leg Weight mass-center dot -- draggable alternative to
        // dragging hWeightRatioSlider (see input.c's
        // hoverRockyMassCenter/draggingRockyMassCenter). Gated the same as
        // every OTHER handle here now that it's a real drag target, not
        // just an informational overlay -- so it's hidden in an exported
        // BMP/the dimmed background copy, same as the body/knee/foot dots
        // above and below. NOT gated on legHidden/bodyHidden -- unlike
        // the other handles, this one still means something with either
        // part removed: computeRockyMassCenterWorld itself now pins it
        // fully onto whichever part is still present (see that
        // function's own comment), so it keeps showing where the
        // (adjusted) mass center actually is.
        drawRockyMassCenterMarker(computeRockyMassCenterWorld(b), rs->draggingRockyMassCenter || rs->hoverRockyMassCenter, MASS_CENTER_HANDLE_RADIUS, opacity);

        // Knee/foot/shin handles -- testing-only toggle
        // (ID_ROCKY_TOGGLE_LEG_BUTTON): all part of the leg drawRockyLeg
        // itself already skips when legHidden, so their handles need the
        // same guard or they'd keep floating on screen with nothing left
        // to attach to.
        if (!b.legHidden)
        {
            // Knee handle -- where the leg attaches to the rectangle (see
            // drawRockyLeg's own kneeWorld), same visible-dot treatment as
            // Semni's own knee handle (drawThigh's drawHandle(kneeWorld, ...)).
            PointF kneeWorld = rotatePoint(b.kneeCircle, center, b.angle);
            drawHandle(kneeWorld, rs->draggingRockyKnee || rs->hoverRockyKnee, KNEE_HANDLE_RADIUS, opacity);

            // Foot handle -- far end of the shin, same visible-dot treatment
            // as Semni's own foot handle (drawShinHandles' drawHandle(footWorld, ...)).
            PointF footWorld = jointToWorld(b.footCircle, b.kneeCircle, b.kneeAngle, center, b.angle);
            drawHandle(footWorld, rs->draggingRockyFoot || rs->hoverRockyFoot, FOOT_HANDLE_RADIUS, opacity);

            // Shin connector-arc handles -- same circleAtAxisMid construction
            // (pinned to the exact middle of the knee->foot axis) Semni's own
            // drawShinHandles uses for its shin1MidLocal/shin2MidLocal, NOT
            // drawRockyLeg's own circleTowardPoint bulge point (that one's for
            // where the curve is actually DRAWN, this one's for where the
            // handle DOT sits -- Semni keeps that same distinction). Drag-only
            // highlight, no separate hover state, same as Semni's shin handles.
            {
                PointF shinAxisMidLocal = { (b.kneeCircle.x + b.footCircle.x) * 0.5f, (b.kneeCircle.y + b.footCircle.y) * 0.5f };

                Fillet shin1Fillet = filletFromAttachAngle(b.kneeCircle, b.kneeRadius, b.footCircle, b.footRadius, b.shinArc1Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
                PointF shin1NearLocal = circleTowardPoint(shin1Fillet.center, shin1Fillet.radius, shinAxisMidLocal);
                PointF shin1MidLocal = circleAtAxisMid(shin1Fillet.center, shin1Fillet.radius, b.kneeCircle, b.footCircle, shin1NearLocal);

                Fillet shin2Fillet = filletFromAttachAngleConcave(b.kneeCircle, b.kneeRadius, b.footCircle, b.footRadius, b.shinArc2Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC2_CONCAVE_R);
                PointF shin2NearLocal = circleTowardPoint(shin2Fillet.center, shin2Fillet.radius, shinAxisMidLocal);
                PointF shin2MidLocal = circleAtAxisMid(shin2Fillet.center, shin2Fillet.radius, b.kneeCircle, b.footCircle, shin2NearLocal);

                PointF shin1World = jointToWorld(shin1MidLocal, b.kneeCircle, b.kneeAngle, center, b.angle);
                PointF shin2World = jointToWorld(shin2MidLocal, b.kneeCircle, b.kneeAngle, center, b.angle);

                drawHandle(shin1World, rs->draggingRockyShin1 || rs->hoverRockyShin1, ROCKY_SHIN_HANDLE_RADIUS, opacity);
                drawHandle(shin2World, rs->draggingRockyShin2 || rs->hoverRockyShin2, ROCKY_SHIN_HANDLE_RADIUS, opacity);
            }
        }

        // One small handle at the midpoint of each of the 4 edges --
        // marks where hovering + dragging resizes just that side (see
        // input.c's hitTestRockyEdge), same idea as the body handle
        // above but sized down a notch (KNEE_HANDLE_RADIUS) so the two
        // read as different, less prominent handles. Body-only, same
        // ID_ROCKY_TOGGLE_BODY_BUTTON guard as the move handle above.
        if (!b.bodyHidden)
        {
            float angle = b.angle;

            PointF leftMid   = rotatePoint((PointF){ b.bodyX - b.bodyHalfWidth, b.bodyY }, center, angle);
            PointF rightMid  = rotatePoint((PointF){ b.bodyX + b.bodyHalfWidth, b.bodyY }, center, angle);
            PointF bottomMid = rotatePoint((PointF){ b.bodyX, b.bodyY - b.bodyHalfHeight }, center, angle);
            PointF topMid    = rotatePoint((PointF){ b.bodyX, b.bodyY + b.bodyHalfHeight }, center, angle);

            drawHandle(leftMid,   rs->draggingRockyEdge == ROCKY_EDGE_LEFT   || rs->hoverRockyEdge == ROCKY_EDGE_LEFT,   KNEE_HANDLE_RADIUS, opacity);
            drawHandle(rightMid,  rs->draggingRockyEdge == ROCKY_EDGE_RIGHT  || rs->hoverRockyEdge == ROCKY_EDGE_RIGHT,  KNEE_HANDLE_RADIUS, opacity);
            drawHandle(bottomMid, rs->draggingRockyEdge == ROCKY_EDGE_BOTTOM || rs->hoverRockyEdge == ROCKY_EDGE_BOTTOM, KNEE_HANDLE_RADIUS, opacity);
            drawHandle(topMid,    rs->draggingRockyEdge == ROCKY_EDGE_TOP    || rs->hoverRockyEdge == ROCKY_EDGE_TOP,    KNEE_HANDLE_RADIUS, opacity);
        }
    }"""

content = content.replace(old, new)
with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("renderer.c (handles) done")

import shutil, glob

path = "input.c"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

# 1) Remove the clampRockyKneeToBody function entirely (with its doc comment).
old1 = '''// Keeps the knee handle honest whenever the rectangle's own size changes
// (scroll on the body handle, or dragging an edge -- see WM_MOUSEWHEEL/
// WM_MOUSEMOVE's ROBOT_KIND_ROCKY branches): shrinking the body can
// otherwise leave the knee handle sitting outside the new, smaller
// rectangle, which the free-drag clamp in WM_MOUSEMOVE only enforces
// WHILE actively dragging the knee itself, not when the body changes size
// out from under it. Only the handle's PIVOT POINT has to stay inside the
// rectangle -- the knee CIRCLE it draws (whatever kneeRadius happens to
// be) is free to poke outside the body's edges, so this clamps position
// only, straight to the rectangle's bounds, with no radius inset and no
// radius clamp.
static void clampRockyKneeToBody(AppState* app)
{
    Rocky* r = &app->robotScene.rocky;

    float minX = r->bodyX - r->bodyHalfWidth;
    float maxX = r->bodyX + r->bodyHalfWidth;
    float minY = r->bodyY - r->bodyHalfHeight;
    float maxY = r->bodyY + r->bodyHalfHeight;

    PointF oldKnee = r->kneeCircle;
    PointF newKnee = oldKnee;
    if (newKnee.x < minX) newKnee.x = minX;
    if (newKnee.x > maxX) newKnee.x = maxX;
    if (newKnee.y < minY) newKnee.y = minY;
    if (newKnee.y > maxY) newKnee.y = maxY;

    // carry the foot/shin along rigidly by the same delta, so pushing the
    // knee back inside the body doesn't also silently reshape the leg --
    // same "preserve the offset" idea as rockyKneeDragFootOffset
    PointF delta = { newKnee.x - oldKnee.x, newKnee.y - oldKnee.y };
    r->kneeCircle = newKnee;
    r->footCircle.x += delta.x;
    r->footCircle.y += delta.y;
}

'''
assert content.count(old1) == 1, f"old1 match count: {content.count(old1)}"
content = content.replace(old1, "")

# 2) Remove the clamp inside the interactive knee-drag branch, so the knee
#    handle just follows the mouse everywhere (detached from the body).
old2 = '''                else if (app->draggingRockyKnee)
                {
                    // Free to move anywhere in the rectangle (not
                    // constrained to an axis the way Semni's hip->knee
                    // drag is), clamped so the handle's PIVOT POINT never
                    // leaves the body -- straight to the rectangle's own
                    // bounds, no radius inset. The knee CIRCLE itself
                    // (drawn at whatever kneeRadius happens to be) is free
                    // to stick out past the edges; only the draggable dot
                    // has to stay inside.
                    float minX = app->robotScene.rocky.bodyX - app->robotScene.rocky.bodyHalfWidth;
                    float maxX = app->robotScene.rocky.bodyX + app->robotScene.rocky.bodyHalfWidth;
                    float minY = app->robotScene.rocky.bodyY - app->robotScene.rocky.bodyHalfHeight;
                    float maxY = app->robotScene.rocky.bodyY + app->robotScene.rocky.bodyHalfHeight;

                    float newKneeX = localMouse.x;
                    if (newKneeX < minX) newKneeX = minX;
                    if (newKneeX > maxX) newKneeX = maxX;

                    float newKneeY = localMouse.y;
                    if (newKneeY < minY) newKneeY = minY;
                    if (newKneeY > maxY) newKneeY = maxY;

                    PointF newKnee = { newKneeX, newKneeY };
                    app->robotScene.rocky.kneeCircle = newKnee;

                    // re-anchor the shin to the offset captured when the
                    // drag started, so its length/shape can't drift over a
                    // long drag -- only the knee's own position within the
                    // body is changing here
                    app->robotScene.rocky.footCircle.x = newKnee.x + app->rockyKneeDragFootOffset.x;
                    app->robotScene.rocky.footCircle.y = newKnee.y + app->rockyKneeDragFootOffset.y;
                }'''

new2 = '''                else if (app->draggingRockyKnee)
                {
                    // Detached by explicit request -- the knee handle
                    // just follows the mouse directly, with no clamp
                    // keeping its pivot point inside the rectangle
                    // (previously it was clamped straight to the body's
                    // own bounds). The whole leg can now be dragged out
                    // and posed anywhere, not just somewhere within the
                    // torso -- there's no other invariant relying on the
                    // knee staying inside the body (Rocky's own default
                    // poses already place the foot outside it, see
                    // app_init.c's initRockyStandingPosition/
                    // initRockyHomePosition).
                    PointF newKnee = localMouse;
                    app->robotScene.rocky.kneeCircle = newKnee;

                    // re-anchor the shin to the offset captured when the
                    // drag started, so its length/shape can't drift over a
                    // long drag -- only the knee's own position is
                    // changing here
                    app->robotScene.rocky.footCircle.x = newKnee.x + app->rockyKneeDragFootOffset.x;
                    app->robotScene.rocky.footCircle.y = newKnee.y + app->rockyKneeDragFootOffset.y;
                }'''

assert content.count(old2) == 1, f"old2 match count: {content.count(old2)}"
content = content.replace(old2, new2)

# 3) The two edge-drag call sites (width/height stretch) that used to pull
#    the knee back inside a shrinking rectangle -- no longer needed now
#    that the knee isn't bound to the body at all.
old3 = '''                    app->robotScene.rocky.bodyHalfWidth = newHalfWidth;

                    // the rectangle may have just shrunk -- keep the knee
                    // (and the foot/shin hanging off it) from ending up
                    // outside the new bounds
                    clampRockyKneeToBody(app);
                }'''
new3 = '''                    app->robotScene.rocky.bodyHalfWidth = newHalfWidth;

                    // The knee (and the foot/shin hanging off it) used to
                    // get pulled back inside a shrinking rectangle here --
                    // removed along with the knee's own drag clamp above,
                    // since the knee isn't bound to the body at all
                    // anymore (detached by explicit request).
                }'''
assert content.count(old3) == 1, f"old3 match count: {content.count(old3)}"
content = content.replace(old3, new3)

old4 = '''                    app->robotScene.rocky.bodyHalfHeight = newHalfHeight;

                    // same reasoning as the width edges above
                    clampRockyKneeToBody(app);
                }'''
new4 = '''                    app->robotScene.rocky.bodyHalfHeight = newHalfHeight;

                    // same reasoning as the width edges above -- no
                    // longer needed, see that comment
                }'''
assert content.count(old4) == 1, f"old4 match count: {content.count(old4)}"
content = content.replace(old4, new4)

# 4) The scroll-to-resize call site (WM_MOUSEWHEEL, both halves at once).
old5 = '''                    // shrinking the body can leave the knee (and the
                    // foot/shin hanging off it) outside the new bounds --
                    // pull it back in if so
                    clampRockyKneeToBody(app);
                }'''
new5 = '''                    // The knee (and the foot/shin hanging off it) used
                    // to get pulled back inside a shrinking rectangle
                    // here -- removed along with the knee's own drag
                    // clamp (WM_MOUSEMOVE's draggingRockyKnee branch),
                    // since the knee isn't bound to the body at all
                    // anymore (detached by explicit request).
                }'''
assert content.count(old5) == 1, f"old5 match count: {content.count(old5)}"
content = content.replace(old5, new5)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)

#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <math.h>

#include "save.h"
#include "renderer.h"
#include "config.h"    // For MIN_SHIN_ARC_R/MAX_SHIN_ARC_R/MAX_SHIN_ARC2_CONCAVE_R, used by saveRockyAsRobArm below

int saveCanvasAsBMP(const char* filename, HWND hwnd, AppState* app)
{
    RECT rect;
    GetClientRect(hwnd, &rect);

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    if (width <= 0 || height <= 0)
        return 0;

    // BMP rows must be padded out to a multiple of 4 bytes -- telling GL
    // to pack with the same 4-byte alignment means the buffer it fills in
    // is already laid out exactly the way the file needs it
    int rowSize = ((width * 3 + 3) / 4) * 4;
    int dataSize = rowSize * height;

    unsigned char* pixels = (unsigned char*)malloc(dataSize);
    if (!pixels)
        return 0;

    // draw a handle-free version of the scene into the back buffer --
    // nothing here reaches the screen since we never call SwapBuffers,
    // so what's currently displayed (with handles) is untouched
    renderAppForSave(app);
    glFinish();

    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    // glReadPixels' rows run bottom-to-top, which is exactly what a BMP
    // with a positive biHeight expects, so no flipping needed -- just
    // swap each pixel's R and B (GL gives RGB, BMP wants BGR)
    for (int y = 0; y < height; y++)
    {
        unsigned char* row = pixels + y * rowSize;

        for (int x = 0; x < width; x++)
        {
            unsigned char* px = row + x * 3;
            unsigned char tmp = px[0];
            px[0] = px[2];
            px[2] = tmp;
        }
    }

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    ZeroMemory(&fileHeader, sizeof(fileHeader));
    ZeroMemory(&infoHeader, sizeof(infoHeader));

    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = width;
    infoHeader.biHeight = height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 24;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = dataSize;

    fileHeader.bfType = 0x4D42; // 'BM'
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = fileHeader.bfOffBits + dataSize;

    FILE* f = fopen(filename, "wb");
    if (!f)
    {
        free(pixels);
        return 0;
    }

    fwrite(&fileHeader, sizeof(fileHeader), 1, f);
    fwrite(&infoHeader, sizeof(infoHeader), 1, f);
    fwrite(pixels, dataSize, 1, f);

    fclose(f);
    free(pixels);

    // explicitly the ANSI (narrow) API, regardless of whether the project
    // defines UNICODE -- keeps this a plain char* end to end, matching
    // filename's type and plain printf's %s, instead of the wchar_t/char*
    // mismatch that came from GetCurrentDirectory's TCHAR macro silently
    // picking the wide version
    char path[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, path);
    printf("Saved canvas as 24-bit BMP in: %s\\%s\n", path, filename);

    return 1;
}

int saveRobotAsEquations(const char* filename, AppState* app)
{
    FILE* f = fopen(filename, "w");
    if (!f)
        return 0;

    Semni* robot = &app->robotScene.robot;

    // BODY - Head Circle
    fprintf(f, "HEAD_X=%.6f\n", robot->headX);
    fprintf(f, "HEAD_Y=%.6f\n", robot->y);
    fprintf(f, "HEAD_RADIUS=%.6f\n", robot->headRadius);

    // BODY - Butt Circle
    fprintf(f, "BUTT_X=%.6f\n", robot->buttX);
    fprintf(f, "BUTT_Y=%.6f\n", robot->y);
    fprintf(f, "BUTT_RADIUS=%.6f\n", robot->buttRadius);

    // BODY - Seam Arcs
    fprintf(f, "SEAM_ARC1_ANGLE=%.6f\n", robot->seamArc1Angle);
    fprintf(f, "SEAM_ARC2_ANGLE=%.6f\n", robot->seamArc2Angle);

    // HIP Joint
    fprintf(f, "HIP_X=%.6f\n", robot->innerCircle.x);
    fprintf(f, "HIP_Y=%.6f\n", robot->innerCircle.y);
    fprintf(f, "HIP_RADIUS=%.6f\n", robot->innerRadius);
    fprintf(f, "HIP_ANGLE=%.6f\n", robot->hipAngle);

    // THIGH - Knee Circle
    fprintf(f, "KNEE_X=%.6f\n", robot->kneeCircle.x);
    fprintf(f, "KNEE_Y=%.6f\n", robot->kneeCircle.y);
    fprintf(f, "KNEE_RADIUS=%.6f\n", robot->kneeRadius);
    fprintf(f, "KNEE_ANGLE=%.6f\n", robot->kneeAngle);

    // THIGH - Arcs
    fprintf(f, "THIGH_ARC1_ANGLE=%.6f\n", robot->thighArc1Angle);
    fprintf(f, "THIGH_ARC2_ANGLE=%.6f\n", robot->thighArc2Angle);

    // SHIN - Foot Circle
    fprintf(f, "FOOT_X=%.6f\n", robot->footCircle.x);
    fprintf(f, "FOOT_Y=%.6f\n", robot->footCircle.y);
    fprintf(f, "FOOT_RADIUS=%.6f\n", robot->footRadius);

    // SHIN - Arcs
    fprintf(f, "SHIN_ARC1_ANGLE=%.6f\n", robot->shinArc1Angle);
    fprintf(f, "SHIN_ARC2_ANGLE=%.6f\n", robot->shinArc2Angle);

    // BODY - Whole
    fprintf(f, "BODY_ANGLE=%.6f\n", robot->angle);

    fclose(f);

    char path[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, path);
    printf("Saved robot equations as TXT in: %s\\%s\n", path, filename);

    return 1;
}

int saveRockyAsEquations(const char* filename, AppState* app)
{
    FILE* f = fopen(filename, "w");
    if (!f)
        return 0;

    Rocky* r = &app->robotScene.rocky;

    // BODY - Rectangle
    fprintf(f, "BODY_X=%.6f\n", r->bodyX);
    fprintf(f, "BODY_Y=%.6f\n", r->bodyY);
    fprintf(f, "BODY_HALF_WIDTH=%.6f\n", r->bodyHalfWidth);
    fprintf(f, "BODY_HALF_HEIGHT=%.6f\n", r->bodyHalfHeight);
    fprintf(f, "BODY_ANGLE=%.6f\n", r->angle);

    // LEG - Knee Circle
    fprintf(f, "KNEE_X=%.6f\n", r->kneeCircle.x);
    fprintf(f, "KNEE_Y=%.6f\n", r->kneeCircle.y);
    fprintf(f, "KNEE_RADIUS=%.6f\n", r->kneeRadius);
    fprintf(f, "KNEE_ANGLE=%.6f\n", r->kneeAngle);

    // LEG - Foot Circle
    fprintf(f, "FOOT_X=%.6f\n", r->footCircle.x);
    fprintf(f, "FOOT_Y=%.6f\n", r->footCircle.y);
    fprintf(f, "FOOT_RADIUS=%.6f\n", r->footRadius);

    // LEG - Shin Arcs
    fprintf(f, "SHIN_ARC1_ANGLE=%.6f\n", r->shinArc1Angle);
    fprintf(f, "SHIN_ARC2_ANGLE=%.6f\n", r->shinArc2Angle);

    fclose(f);

    char path[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, path);
    printf("Saved Rocky equations as TXT in: %s\\%s\n", path, filename);

    return 1;
}

int saveStiloAsEquations(const char* filename, AppState* app)
{
    FILE* f = fopen(filename, "w");
    if (!f)
        return 0;

    Stilo* s = &app->robotScene.stilo;

    // BODY - Head Circle
    fprintf(f, "HEAD_X=%.6f\n", s->headX);
    fprintf(f, "HEAD_Y=%.6f\n", s->y);
    fprintf(f, "HEAD_RADIUS=%.6f\n", s->headRadius);

    // BODY - Butt Circle
    fprintf(f, "BUTT_X=%.6f\n", s->buttX);
    fprintf(f, "BUTT_Y=%.6f\n", s->y);
    fprintf(f, "BUTT_RADIUS=%.6f\n", s->buttRadius);

    // BODY - Seam Arcs
    fprintf(f, "SEAM_ARC1_ANGLE=%.6f\n", s->seamArc1Angle);
    fprintf(f, "SEAM_ARC2_ANGLE=%.6f\n", s->seamArc2Angle);

    // LEG 1 - Hip Joint
    fprintf(f, "HIP1_X=%.6f\n", s->hip1Circle.x);
    fprintf(f, "HIP1_Y=%.6f\n", s->hip1Circle.y);
    fprintf(f, "HIP1_RADIUS=%.6f\n", s->hip1Radius);
    fprintf(f, "HIP1_ANGLE=%.6f\n", s->hip1Angle);

    // LEG 1 - Feet Circle (Stilo now has just a hip->feet chain per leg,
    // no knee/shin/foot sub-stage any more -- see app.h's Stilo comment)
    fprintf(f, "FEET1_X=%.6f\n", s->feet1Circle.x);
    fprintf(f, "FEET1_Y=%.6f\n", s->feet1Circle.y);
    fprintf(f, "FEET1_RADIUS=%.6f\n", s->feet1Radius);

    // LEG 1 - Thigh Arcs (hip1-to-feet1)
    fprintf(f, "THIGH1_ARC1_ANGLE=%.6f\n", s->thigh1Arc1Angle);
    fprintf(f, "THIGH1_ARC2_ANGLE=%.6f\n", s->thigh1Arc2Angle);

    // BODY - Whole
    fprintf(f, "BODY_ANGLE=%.6f\n", s->angle);

    // ---- leg 2 -- same fields as leg 1 above, HIP2_/FEET2_/THIGH2_-
    // prefixed keys so they can't collide with leg 1's (see app.h's Stilo
    // comment). No leg-2 equivalent of the torso keys (HEAD_*/BUTT_*/
    // SEAM_ARC*/BODY_ANGLE) -- both legs share the one torso, already
    // written above.
    fprintf(f, "HIP2_X=%.6f\n", s->hip2Circle.x);
    fprintf(f, "HIP2_Y=%.6f\n", s->hip2Circle.y);
    fprintf(f, "HIP2_RADIUS=%.6f\n", s->hip2Radius);
    fprintf(f, "HIP2_ANGLE=%.6f\n", s->hip2Angle);

    fprintf(f, "FEET2_X=%.6f\n", s->feet2Circle.x);
    fprintf(f, "FEET2_Y=%.6f\n", s->feet2Circle.y);
    fprintf(f, "FEET2_RADIUS=%.6f\n", s->feet2Radius);

    fprintf(f, "THIGH2_ARC1_ANGLE=%.6f\n", s->thigh2Arc1Angle);
    fprintf(f, "THIGH2_ARC2_ANGLE=%.6f\n", s->thigh2Arc2Angle);

    fclose(f);

    char path[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, path);
    printf("Saved Stilo equations as TXT in: %s\\%s\n", path, filename);

    return 1;
}

// ---- pose file loaders (read back what save*AsEquations wrote) ----

int loadRobotPoseFromFile(const char* filename, Semni* out)
{
    FILE* f = fopen(filename, "r");
    if (!f)
        return 0;

    char line[128];
    char key[64];
    float value;

    while (fgets(line, sizeof(line), f))
    {
        if (sscanf(line, "%63[^=]=%f", key, &value) != 2)
            continue;

        if      (strcmp(key, "HEAD_X") == 0) out->headX = value;
        else if (strcmp(key, "HEAD_Y") == 0) out->y = value;
        else if (strcmp(key, "HEAD_RADIUS") == 0) out->headRadius = value;
        else if (strcmp(key, "BUTT_X") == 0) out->buttX = value;
        else if (strcmp(key, "BUTT_Y") == 0) out->y = value; // same field as HEAD_Y -- both restate Semni's one shared body Y
        else if (strcmp(key, "BUTT_RADIUS") == 0) out->buttRadius = value;
        else if (strcmp(key, "SEAM_ARC1_ANGLE") == 0) out->seamArc1Angle = value;
        else if (strcmp(key, "SEAM_ARC2_ANGLE") == 0) out->seamArc2Angle = value;
        else if (strcmp(key, "HIP_X") == 0) out->innerCircle.x = value;
        else if (strcmp(key, "HIP_Y") == 0) out->innerCircle.y = value;
        else if (strcmp(key, "HIP_RADIUS") == 0) out->innerRadius = value;
        else if (strcmp(key, "HIP_ANGLE") == 0) out->hipAngle = value;
        else if (strcmp(key, "KNEE_X") == 0) out->kneeCircle.x = value;
        else if (strcmp(key, "KNEE_Y") == 0) out->kneeCircle.y = value;
        else if (strcmp(key, "KNEE_RADIUS") == 0) out->kneeRadius = value;
        else if (strcmp(key, "KNEE_ANGLE") == 0) out->kneeAngle = value;
        else if (strcmp(key, "THIGH_ARC1_ANGLE") == 0) out->thighArc1Angle = value;
        else if (strcmp(key, "THIGH_ARC2_ANGLE") == 0) out->thighArc2Angle = value;
        else if (strcmp(key, "FOOT_X") == 0) out->footCircle.x = value;
        else if (strcmp(key, "FOOT_Y") == 0) out->footCircle.y = value;
        else if (strcmp(key, "FOOT_RADIUS") == 0) out->footRadius = value;
        // ANKLE_* is the old key name (from before the ankle->foot rename) --
        // accepted here too so pose files already saved to disk under the
        // old name still load correctly. Only FOOT_* is ever written now.
        else if (strcmp(key, "ANKLE_X") == 0) out->footCircle.x = value;
        else if (strcmp(key, "ANKLE_Y") == 0) out->footCircle.y = value;
        else if (strcmp(key, "ANKLE_RADIUS") == 0) out->footRadius = value;
        else if (strcmp(key, "SHIN_ARC1_ANGLE") == 0) out->shinArc1Angle = value;
        else if (strcmp(key, "SHIN_ARC2_ANGLE") == 0) out->shinArc2Angle = value;
        else if (strcmp(key, "BODY_ANGLE") == 0) out->angle = value;
    }

    fclose(f);
    return 1;
}

int loadRockyPoseFromFile(const char* filename, Rocky* out)
{
    FILE* f = fopen(filename, "r");
    if (!f)
        return 0;

    char line[128];
    char key[64];
    float value;

    while (fgets(line, sizeof(line), f))
    {
        if (sscanf(line, "%63[^=]=%f", key, &value) != 2)
            continue;

        if      (strcmp(key, "BODY_X") == 0) out->bodyX = value;
        else if (strcmp(key, "BODY_Y") == 0) out->bodyY = value;
        else if (strcmp(key, "BODY_HALF_WIDTH") == 0) out->bodyHalfWidth = value;
        else if (strcmp(key, "BODY_HALF_HEIGHT") == 0) out->bodyHalfHeight = value;
        else if (strcmp(key, "BODY_ANGLE") == 0) out->angle = value;
        else if (strcmp(key, "KNEE_X") == 0) out->kneeCircle.x = value;
        else if (strcmp(key, "KNEE_Y") == 0) out->kneeCircle.y = value;
        else if (strcmp(key, "KNEE_RADIUS") == 0) out->kneeRadius = value;
        else if (strcmp(key, "KNEE_ANGLE") == 0) out->kneeAngle = value;
        else if (strcmp(key, "FOOT_X") == 0) out->footCircle.x = value;
        else if (strcmp(key, "FOOT_Y") == 0) out->footCircle.y = value;
        else if (strcmp(key, "FOOT_RADIUS") == 0) out->footRadius = value;
        // legacy key name, see loadRobotPoseFromFile's comment above
        else if (strcmp(key, "ANKLE_X") == 0) out->footCircle.x = value;
        else if (strcmp(key, "ANKLE_Y") == 0) out->footCircle.y = value;
        else if (strcmp(key, "ANKLE_RADIUS") == 0) out->footRadius = value;
        else if (strcmp(key, "SHIN_ARC1_ANGLE") == 0) out->shinArc1Angle = value;
        else if (strcmp(key, "SHIN_ARC2_ANGLE") == 0) out->shinArc2Angle = value;
    }

    fclose(f);
    return 1;
}

int loadStiloPoseFromFile(const char* filename, Stilo* out)
{
    FILE* f = fopen(filename, "r");
    if (!f)
        return 0;

    char line[128];
    char key[64];
    float value;

    while (fgets(line, sizeof(line), f))
    {
        if (sscanf(line, "%63[^=]=%f", key, &value) != 2)
            continue;

        if      (strcmp(key, "HEAD_X") == 0) out->headX = value;
        else if (strcmp(key, "HEAD_Y") == 0) out->y = value;
        else if (strcmp(key, "HEAD_RADIUS") == 0) out->headRadius = value;
        else if (strcmp(key, "BUTT_X") == 0) out->buttX = value;
        else if (strcmp(key, "BUTT_Y") == 0) out->y = value; // same field as HEAD_Y -- both restate Stilo's one shared body Y
        else if (strcmp(key, "BUTT_RADIUS") == 0) out->buttRadius = value;
        else if (strcmp(key, "SEAM_ARC1_ANGLE") == 0) out->seamArc1Angle = value;
        else if (strcmp(key, "SEAM_ARC2_ANGLE") == 0) out->seamArc2Angle = value;
        else if (strcmp(key, "HIP1_X") == 0) out->hip1Circle.x = value;
        else if (strcmp(key, "HIP1_Y") == 0) out->hip1Circle.y = value;
        else if (strcmp(key, "HIP1_RADIUS") == 0) out->hip1Radius = value;
        else if (strcmp(key, "HIP1_ANGLE") == 0) out->hip1Angle = value;
        else if (strcmp(key, "FEET1_X") == 0) out->feet1Circle.x = value;
        else if (strcmp(key, "FEET1_Y") == 0) out->feet1Circle.y = value;
        else if (strcmp(key, "FEET1_RADIUS") == 0) out->feet1Radius = value;
        else if (strcmp(key, "THIGH1_ARC1_ANGLE") == 0) out->thigh1Arc1Angle = value;
        else if (strcmp(key, "THIGH1_ARC2_ANGLE") == 0) out->thigh1Arc2Angle = value;
        // legacy key names, from before this leg was renumbered/renamed
        // (hip->HIP1, knee->FEET1, thighArc->THIGH1_ARC) -- accepted here
        // as pure renames since they describe the exact same joint at the
        // exact same position, so an older pose file saved under the old
        // names still loads correctly.
        else if (strcmp(key, "HIP_X") == 0) out->hip1Circle.x = value;
        else if (strcmp(key, "HIP_Y") == 0) out->hip1Circle.y = value;
        else if (strcmp(key, "HIP_RADIUS") == 0) out->hip1Radius = value;
        else if (strcmp(key, "HIP_ANGLE") == 0) out->hip1Angle = value;
        else if (strcmp(key, "KNEE_X") == 0) out->feet1Circle.x = value;
        else if (strcmp(key, "KNEE_Y") == 0) out->feet1Circle.y = value;
        else if (strcmp(key, "KNEE_RADIUS") == 0) out->feet1Radius = value;
        else if (strcmp(key, "THIGH_ARC1_ANGLE") == 0) out->thigh1Arc1Angle = value;
        else if (strcmp(key, "THIGH_ARC2_ANGLE") == 0) out->thigh1Arc2Angle = value;
        // FOOT_*/ANKLE_*/SHIN_ARC*_ANGLE/KNEE_ANGLE (old keys describing the
        // now-deleted knee-to-foot stage) are deliberately NOT accepted --
        // same reasoning as loadRobotPoseFromFile's own comment on
        // LEG_ARC1_ANGLE: FOOT_X described a joint one stage further out
        // than the new FEET1 (which is where KNEE used to sit), so
        // reinterpreting it would misplace the leg's end instead of
        // preserving it. Any pose file saved under the old hip->knee->foot
        // Stilo needs to be re-saved from scratch under the new structure.
        else if (strcmp(key, "BODY_ANGLE") == 0) out->angle = value;
        // ---- leg 2 -- same HIP2_/FEET2_/THIGH2_-prefixed keys
        // saveStiloAsEquations writes above. An older pose file saved
        // before leg 2 existed simply has none of these keys, so
        // out->hip2Circle/etc is left whatever it already was (zero on a
        // fresh launch) -- same "seed the hardcoded default first, then
        // load on top" fix input.c's ID_STANDING_POSITION_BUTTON/
        // ID_HOME_POSITION_BUTTON already apply for exactly this reason.
        else if (strcmp(key, "HIP2_X") == 0) out->hip2Circle.x = value;
        else if (strcmp(key, "HIP2_Y") == 0) out->hip2Circle.y = value;
        else if (strcmp(key, "HIP2_RADIUS") == 0) out->hip2Radius = value;
        else if (strcmp(key, "HIP2_ANGLE") == 0) out->hip2Angle = value;
        else if (strcmp(key, "FEET2_X") == 0) out->feet2Circle.x = value;
        else if (strcmp(key, "FEET2_Y") == 0) out->feet2Circle.y = value;
        else if (strcmp(key, "FEET2_RADIUS") == 0) out->feet2Radius = value;
        else if (strcmp(key, "THIGH2_ARC1_ANGLE") == 0) out->thigh2Arc1Angle = value;
        else if (strcmp(key, "THIGH2_ARC2_ANGLE") == 0) out->thigh2Arc2Angle = value;
        // legacy LEG2_-prefixed key names, same rename reasoning as leg 1's
        // HIP_*/KNEE_*/THIGH_ARC* aliases above
        else if (strcmp(key, "LEG2_HIP_X") == 0) out->hip2Circle.x = value;
        else if (strcmp(key, "LEG2_HIP_Y") == 0) out->hip2Circle.y = value;
        else if (strcmp(key, "LEG2_HIP_RADIUS") == 0) out->hip2Radius = value;
        else if (strcmp(key, "LEG2_HIP_ANGLE") == 0) out->hip2Angle = value;
        else if (strcmp(key, "LEG2_KNEE_X") == 0) out->feet2Circle.x = value;
        else if (strcmp(key, "LEG2_KNEE_Y") == 0) out->feet2Circle.y = value;
        else if (strcmp(key, "LEG2_KNEE_RADIUS") == 0) out->feet2Radius = value;
        else if (strcmp(key, "LEG2_THIGH_ARC1_ANGLE") == 0) out->thigh2Arc1Angle = value;
        else if (strcmp(key, "LEG2_THIGH_ARC2_ANGLE") == 0) out->thigh2Arc2Angle = value;
        // LEG2_FOOT_*/LEG2_SHIN_ARC*_ANGLE/LEG2_KNEE_ANGLE dropped for the
        // same reason as leg 1's FOOT_*/SHIN_ARC*_ANGLE/KNEE_ANGLE above.
    }

    fclose(f);
    return 1;
}

// ---- Rob.txt / Arm.txt export (Rocky only) ----
//
// A completely separate export format from the KEY=value save*AsEquations/
// load*PoseFromFile round-trip above -- built for an external consumer that
// wants: a mass center + weight, the shared joint the two files attach at,
// and the shape's own parts as a list of (start, end, center) point
// triples. Rob.txt is Rocky's rectangular body (4 straight edges). Arm.txt
// is its single leg (kneeCircle -> 2 shin fillets -> footCircle), written
// as 4 segments tracing the leg's trimmed outline as ONE continuous closed
// loop: the knee circle's own visible arc, a shin fillet connector, the
// foot circle's own visible arc, and the other shin fillet connector --
// see the Arm.txt block's own comment below. (A 6-segment version -- both
// circles written in full, split into 2 arcs apiece, plus the 2
// connectors -- was tried in between; this 4-segment outline is simpler
// and is what's wanted for now.)
//
// The two files do NOT share one frame -- each uses whatever origin is
// natural for its own shape, and the "joint location" line says where the
// shared attach point (kneeCircle) falls within THAT shape's own frame
// (this is what actually reunites them -- an external assembler translates
// Arm.txt by Rob.txt's stated joint offset). Confirmed against a real
// reference Rob.txt: its rectangle ran x:[0,0.825] y:[0,3.25] with a
// NON-zero joint line, i.e. Rob.txt's origin is the rectangle's own
// bottom-left corner, not the joint -- an earlier version of this function
// wrongly used kneeCircle as the origin for BOTH files (so Rob.txt's joint
// line was always "0 0"), which doesn't match. Arm.txt's own convention is
// unchanged and still matches its own original reference example: origin
// AT the joint (kneeCircle), so ITS joint line is trivially "0 0" -- that
// one's a leg, and the joint is simply the one natural reference point for
// an irregular arc shape the way a corner is for a rectangle.
//
// Both files' lengths are scaled by config.h's ROCKY_EXPORT_SCALE right
// before writing -- an empirically-calibrated factor for this specific
// external consumer's own unit system, NOT millimeters (MM_PER_WORLD_UNIT
// is a different, unrelated constant used elsewhere, e.g. input.c's
// on-screen size readout). See ROCKY_EXPORT_SCALE's own comment for the
// calibration. Applied identically to both files since they share one
// joint/unit system by construction. Like saveRockyAsEquations above,
// this uses Rocky's RAW local (pre-whole-body-angle, pre-kneeAngle)
// fields, not the current on-screen rotated pose -- same convention the
// rest of this file already follows.
//
// Straight edges (Rob.txt's 4 rectangle sides) are written in the exact
// same "start, end, center" shape as a circular arc, just with a degenerate
// "center" -- the midpoint of the edge. Since every rectangle edge here is
// axis-aligned (this frame is pre-rotation), that midpoint always shares
// an X (vertical edge) or a Y (horizontal edge) with both endpoints, which
// is what tells the consumer "this one's a straight line, not a circle".
// Edge order/direction matches the reference Rob.txt too: left edge
// (bottom-left -> top-left), top edge, right edge (going back down),
// bottom edge -- a CCW traversal starting by going up the left side.

#define ROB_ARM_ARC_SAMPLES 48   // per-arc polygon subdivision count, for the approximate centroid below only -- doesn't affect the exact start/end/center values written to Arm.txt

typedef struct { PointF start, end, center; } RobArmSegment;

// Angle (degrees) of "p" around "center", atan2 convention -- same as
// circleEdge's own angle parameter.
static float robArmAngleAroundDeg(PointF center, PointF p)
{
    return atan2f(p.y - center.y, p.x - center.x) * 180.0f / 3.1415926f;
}

static float robArmWrap360(float deg)
{
    float d = fmodf(deg, 360.0f);
    if (d < 0.0f) d += 360.0f;
    return d;
}

// Appends ROB_ARM_ARC_SAMPLES+1 points (from "start" to "end" inclusive)
// tracing the circle (center, implied by start's own distance to it) into
// "out", resolving the start/end direction ambiguity via a known third
// point ("mid") the arc must pass through -- the same "near"/bulge point
// drawRockyLeg already computes for shin1/shin2 (shin1NearLocal/
// shin2NearLocal) via circleTowardPoint(fillet, axisMidLocal). Used for
// the two shin fillet arcs, where that bulge point is already at hand.
static int robArmSampleArcThroughMid(PointF center, PointF start, PointF mid, PointF end, int steps, PointF* out)
{
    float r = sqrtf((start.x - center.x) * (start.x - center.x) + (start.y - center.y) * (start.y - center.y));
    float a0 = robArmAngleAroundDeg(center, start);
    float am = robArmWrap360(robArmAngleAroundDeg(center, mid) - a0);
    float a2 = robArmWrap360(robArmAngleAroundDeg(center, end) - a0);

    // if the mid point is reached before the end point while sweeping in
    // the increasing-angle direction, that's the right direction; if the
    // end point comes first, the true arc must instead sweep the other
    // (decreasing-angle) way around
    float sweep = (am <= a2) ? a2 : (a2 - 360.0f);

    for (int i = 0; i <= steps; i++)
    {
        float t = (float)i / (float)steps;
        float a = (a0 + sweep * t) * 3.1415926f / 180.0f;
        out[i].x = center.x + cosf(a) * r;
        out[i].y = center.y + sinf(a) * r;
    }
    return steps + 1;
}

// Same idea as robArmSampleArcThroughMid, but for an arc where only the two
// endpoints are known -- used for the knee/foot circle's OWN boundary arc
// (the portion of that circle not covered by either shin fillet), which
// drawRockyLeg never needs a "mid" point for since it always draws the
// full circle instead (see drawCircle(kneeWorld,...)/drawCircle(footWorld,
// ...)). Between the two possible arcs linking "start" and "end", picks
// whichever one bulges AWAY from "otherCenter" (the other circle in the
// knee/foot pair) -- the visible/outer side of a two-circle-plus-fillets
// leg silhouette like Rocky's is always the one facing away from the other
// circle, never the one tucked in toward it.
static int robArmSampleArcAwayFrom(PointF center, PointF start, PointF end, PointF otherCenter, int steps, PointF* out)
{
    float r = sqrtf((start.x - center.x) * (start.x - center.x) + (start.y - center.y) * (start.y - center.y));
    float a0 = robArmAngleAroundDeg(center, start);
    float a2 = robArmWrap360(robArmAngleAroundDeg(center, end) - a0);

    float sweepShort = a2;          // increasing-angle candidate, 0..360
    float sweepLong = a2 - 360.0f;  // the other way around, -360..0

    float midShortRad = (a0 + sweepShort * 0.5f) * 3.1415926f / 180.0f;
    float midLongRad = (a0 + sweepLong * 0.5f) * 3.1415926f / 180.0f;

    PointF pShort = { center.x + r * cosf(midShortRad), center.y + r * sinf(midShortRad) };
    PointF pLong = { center.x + r * cosf(midLongRad), center.y + r * sinf(midLongRad) };

    float dShort = (pShort.x - otherCenter.x) * (pShort.x - otherCenter.x) + (pShort.y - otherCenter.y) * (pShort.y - otherCenter.y);
    float dLong = (pLong.x - otherCenter.x) * (pLong.x - otherCenter.x) + (pLong.y - otherCenter.y) * (pLong.y - otherCenter.y);

    float sweep = (dShort >= dLong) ? sweepShort : sweepLong;

    for (int i = 0; i <= steps; i++)
    {
        float t = (float)i / (float)steps;
        float a = (a0 + sweep * t) * 3.1415926f / 180.0f;
        out[i].x = center.x + cosf(a) * r;
        out[i].y = center.y + sinf(a) * r;
    }
    return steps + 1;
}

// Standard shoelace-formula polygon centroid, applied to the fine polygon
// approximation of the leg outline the two sample functions above build --
// an APPROXIMATION of the true area-weighted centroid (exact only in the
// limit of infinite samples), which is what saveRockyAsRobArm's mass
// center is meant to be: there's no material/density model in this editor
// to compute a true one from. Consecutive arcs sharing an endpoint just
// contribute a duplicate point, which the shoelace sum treats as a
// zero-area sliver -- harmless, so callers don't need to de-duplicate.
static PointF robArmPolygonCentroid(PointF* pts, int n)
{
    float area = 0.0f, cx = 0.0f, cy = 0.0f;

    for (int i = 0; i < n; i++)
    {
        PointF p0 = pts[i];
        PointF p1 = pts[(i + 1) % n];
        float cross = p0.x * p1.y - p1.x * p0.y;
        area += cross;
        cx += (p0.x + p1.x) * cross;
        cy += (p0.y + p1.y) * cross;
    }

    area *= 0.5f;

    if (fabsf(area) < 1e-9f)
    {
        // degenerate (near-zero-area) shape -- fall back to the plain
        // average of the sample points rather than dividing by ~0
        PointF fallback = { 0.0f, 0.0f };
        for (int i = 0; i < n; i++)
        {
            fallback.x += pts[i].x;
            fallback.y += pts[i].y;
        }
        fallback.x /= (float)n;
        fallback.y /= (float)n;
        return fallback;
    }

    cx /= (6.0f * area);
    cy /= (6.0f * area);

    return (PointF){ cx, cy };
}

int saveRockyAsRobArm(AppState* app)
{
    Rocky* r = &app->robotScene.rocky;

    // Both files go in one RockyExport folder alongside rocky.bmp/
    // rocky.txt (see canvas.c's File > Save handler (ID_SAVE), which
    // creates this same folder before calling this function) -- also created here so this
    // function is self-contained if it's ever called from anywhere else.
    // A no-op once the folder already exists (CreateDirectoryA returns
    // FALSE, GetLastError() == ERROR_ALREADY_EXISTS), safe to ignore.
    CreateDirectoryA("RockyExport", NULL);

    // shared attach point -- Arm.txt's own local (0,0) (see this section's
    // top comment: Arm.txt, unlike Rob.txt below, IS joint-centered)
    PointF joint = r->kneeCircle;

    // ---- leg geometry + centroid, computed FIRST ----
    //
    // Needed before Rob.txt below can be written: Rob.txt's own mass
    // center is the combined body+leg system's center of mass (weighted
    // by bodyWeight/legWeight), not just the rectangle's own centroid --
    // confirmed against a real reference Rob.txt, whose mass center sat
    // well below the rectangle's true geometric center (close to the
    // joint, i.e. pulled toward the leg), which a rectangle-only centroid
    // can never produce on its own.
    //
    // exact same fillet/tangent-point construction drawRockyLeg uses to
    // render this leg (renderer.c) -- kept in sync with that so this
    // export always matches what's actually on screen
    PointF axisMidLocal = { (r->kneeCircle.x + r->footCircle.x) * 0.5f, (r->kneeCircle.y + r->footCircle.y) * 0.5f };

    Fillet shin1Fillet = filletFromAttachAngle(r->kneeCircle, r->kneeRadius, r->footCircle, r->footRadius, r->shinArc1Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
    PointF shin1KneeTangent = circleEdge(r->kneeCircle, r->kneeRadius, r->shinArc1Angle);
    PointF shin1FootTangent = internalTangentPoint(shin1Fillet.center, shin1Fillet.radius, r->footCircle, r->footRadius);
    PointF shin1Mid = circleTowardPoint(shin1Fillet.center, shin1Fillet.radius, axisMidLocal);

    Fillet shin2Fillet = filletFromAttachAngleConcave(r->kneeCircle, r->kneeRadius, r->footCircle, r->footRadius, r->shinArc2Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC2_CONCAVE_R);
    PointF shin2KneeTangent = circleEdge(r->kneeCircle, r->kneeRadius, r->shinArc2Angle);
    PointF shin2FootTangent = circleTowardPoint(shin2Fillet.center, shin2Fillet.radius, r->footCircle);
    PointF shin2Mid = circleTowardPoint(shin2Fillet.center, shin2Fillet.radius, axisMidLocal);

    // fine polygon approximation of the leg's true trimmed-outline
    // silhouette (the union of the 2 full circles + 2 fillets Arm.txt
    // below lists separately) -- purely for the approximate centroid,
    // independent of Arm.txt's own 6-segment written representation
    PointF poly[4 * (ROB_ARM_ARC_SAMPLES + 1)];
    int polyN = 0;
    polyN += robArmSampleArcThroughMid(shin1Fillet.center, shin1KneeTangent, shin1Mid, shin1FootTangent, ROB_ARM_ARC_SAMPLES, poly + polyN);
    polyN += robArmSampleArcAwayFrom(r->footCircle, shin1FootTangent, shin2FootTangent, r->kneeCircle, ROB_ARM_ARC_SAMPLES, poly + polyN);
    polyN += robArmSampleArcThroughMid(shin2Fillet.center, shin2FootTangent, shin2Mid, shin2KneeTangent, ROB_ARM_ARC_SAMPLES, poly + polyN);
    polyN += robArmSampleArcAwayFrom(r->kneeCircle, shin2KneeTangent, shin1KneeTangent, r->footCircle, ROB_ARM_ARC_SAMPLES, poly + polyN);

    PointF legCentroidWorld = robArmPolygonCentroid(poly, polyN);
    // leg's own centroid relative to the joint -- this IS what Arm.txt
    // writes as its own mass center below, unchanged from before
    PointF legCentroidRelJoint = { legCentroidWorld.x - joint.x, legCentroidWorld.y - joint.y };

    // ---- Rob.txt: rectangular body ----
    //
    // Origin is the rectangle's own bottom-left corner (see this section's
    // top comment) -- NOT the joint. jointInRob is the knee's real,
    // generally-non-zero position in that corner-relative frame; the
    // rectangle's own corners are listed left/top/right/bottom (CCW,
    // starting by going up the left edge), matching the reference file.
    {
        PointF corner = { r->bodyX - r->bodyHalfWidth, r->bodyY - r->bodyHalfHeight };
        float width = r->bodyHalfWidth * 2.0f;
        float height = r->bodyHalfHeight * 2.0f;

        PointF bl = { 0.0f, 0.0f };
        PointF tl = { 0.0f, height };
        PointF tr = { width, height };
        PointF br = { width, 0.0f };

        PointF jointInRob = { r->kneeCircle.x - corner.x, r->kneeCircle.y - corner.y };

        // rectangle's own centroid (uniform density assumed) -- exactly
        // its own center
        PointF rectCentroid = { r->bodyHalfWidth, r->bodyHalfHeight };

        // the leg's centroid, translated from Arm.txt's joint-relative
        // frame into this same corner-relative frame, by adding the
        // joint's own corner-relative offset computed just above
        PointF legCentroidInRob = { legCentroidRelJoint.x + jointInRob.x, legCentroidRelJoint.y + jointInRob.y };

        // weighted combination -- falls back to the rectangle's own
        // centroid if both weights happen to be 0 (would otherwise divide
        // by zero)
        float totalWeight = r->bodyWeight + r->legWeight;
        PointF massCenter = rectCentroid;
        if (totalWeight > 1e-6f)
        {
            massCenter.x = (r->bodyWeight * rectCentroid.x + r->legWeight * legCentroidInRob.x) / totalWeight;
            massCenter.y = (r->bodyWeight * rectCentroid.y + r->legWeight * legCentroidInRob.y) / totalWeight;
        }

        // ROCKY_EXPORT_SCALE (config.h) applied here, right before writing
        // -- everything above this point stays in plain world units so the
        // combined-COM math above reads naturally; only the numbers
        // actually written to Rob.txt get scaled. Weight is left alone,
        // it's not a length.
        bl.x *= ROCKY_EXPORT_SCALE; bl.y *= ROCKY_EXPORT_SCALE;
        tl.x *= ROCKY_EXPORT_SCALE; tl.y *= ROCKY_EXPORT_SCALE;
        tr.x *= ROCKY_EXPORT_SCALE; tr.y *= ROCKY_EXPORT_SCALE;
        br.x *= ROCKY_EXPORT_SCALE; br.y *= ROCKY_EXPORT_SCALE;
        jointInRob.x *= ROCKY_EXPORT_SCALE; jointInRob.y *= ROCKY_EXPORT_SCALE;
        massCenter.x *= ROCKY_EXPORT_SCALE; massCenter.y *= ROCKY_EXPORT_SCALE;

        FILE* f = fopen("RockyExport\\Rob.txt", "w");
        if (!f)
            return 0;

        fprintf(f, "%.6f %.6f %.6f\n", massCenter.x, massCenter.y, r->bodyWeight);
        fprintf(f, "%.6f %.6f\n", jointInRob.x, jointInRob.y);

        // left, top, right, bottom -- each one's end matches the next
        // one's start, same chained convention as Arm.txt below
        RobArmSegment robEdges[4] = {
            { bl, tl, { (bl.x + tl.x) * 0.5f, (bl.y + tl.y) * 0.5f } },
            { tl, tr, { (tl.x + tr.x) * 0.5f, (tl.y + tr.y) * 0.5f } },
            { tr, br, { (tr.x + br.x) * 0.5f, (tr.y + br.y) * 0.5f } },
            { br, bl, { (br.x + bl.x) * 0.5f, (br.y + bl.y) * 0.5f } },
        };

        for (int i = 0; i < 4; i++)
        {
            // degenerate "center" for a straight edge -- see this
            // section's top comment
            fprintf(f, "%.6f %.6f %.6f %.6f %.6f %.6f\n",
                robEdges[i].start.x, robEdges[i].start.y,
                robEdges[i].end.x, robEdges[i].end.y,
                robEdges[i].center.x, robEdges[i].center.y);
        }

        fclose(f);
    }

    // ---- Arm.txt: knee circle + shin arc 1 + shin arc 2 + foot circle ----
    //
    // 6 lines total, all genuine 3-point arcs -- start, end, and a THIRD
    // POINT THAT ACTUALLY LIES ON THE ARC's curve between them -- the same
    // convention this app's own drawArc calls already use elsewhere
    // (renderer.c), rather than the (start, end, CENTER) triples this file
    // used everywhere else (Rob.txt's edges, and this same Arm.txt block's
    // own 4/6-segment versions before this one):
    //   1-2: knee circle, split into its 2 halves at shin2KneeTangent/
    //        shin1KneeTangent (same points the connectors tangent it at)
    //   3:   shin arc 1 (the convex connector, knee -> foot)
    //   4:   shin arc 2 (the concave connector, knee -> foot)
    //   5-6: foot circle, split the same way as the knee, at
    //        shin1FootTangent/shin2FootTangent
    {
        // ---- knee circle, halved ----
        // shinArc1Angle/shinArc2Angle are the exact angles circleEdge
        // already placed shin1KneeTangent/shin2KneeTangent at, so the two
        // halves' own arc-midpoints fall right out of them: sweeping from
        // kneeAngle1 to kneeAngle2 the increasing-angle way covers "da"
        // degrees, continuing the same direction the rest of the way
        // around covers the complementary (360-da) -- together the two
        // exactly complete the circle.
        float kneeAngle1 = r->shinArc2Angle;
        float kneeAngle2 = r->shinArc1Angle;
        float kneeDa = robArmWrap360(kneeAngle2 - kneeAngle1);
        PointF kneeMidHalf1 = circleEdge(r->kneeCircle, r->kneeRadius, kneeAngle1 + kneeDa * 0.5f);
        PointF kneeMidHalf2 = circleEdge(r->kneeCircle, r->kneeRadius, kneeAngle1 + (kneeDa - 360.0f) * 0.5f);

        // ---- foot circle, halved the same way ----
        // shin1FootTangent/shin2FootTangent weren't placed via a stored
        // angle parameter the way the knee's own tangent points were (they
        // came out of internalTangentPoint/circleTowardPoint instead), so
        // their angles around footCircle have to be measured directly.
        float footAngle1 = robArmAngleAroundDeg(r->footCircle, shin1FootTangent);
        float footAngle2 = robArmAngleAroundDeg(r->footCircle, shin2FootTangent);
        float footDa = robArmWrap360(footAngle2 - footAngle1);
        PointF footMidHalf1 = circleEdge(r->footCircle, r->footRadius, footAngle1 + footDa * 0.5f);
        PointF footMidHalf2 = circleEdge(r->footCircle, r->footRadius, footAngle1 + (footDa - 360.0f) * 0.5f);

        // joint-relative + ROCKY_EXPORT_SCALE (config.h), same convention
        // as everything else this function writes -- has to match Rob.txt's
        // own scale since the joint is the literal same physical point in
        // both files.
#define ARM_PT(p) { ((p).x - joint.x) * ROCKY_EXPORT_SCALE, ((p).y - joint.y) * ROCKY_EXPORT_SCALE }
        PointF kneeP1 = ARM_PT(shin2KneeTangent);
        PointF kneeP2 = ARM_PT(shin1KneeTangent);
        PointF kneeM1 = ARM_PT(kneeMidHalf1);
        PointF kneeM2 = ARM_PT(kneeMidHalf2);

        PointF shin1Start = ARM_PT(shin1KneeTangent);
        PointF shin1End   = ARM_PT(shin1FootTangent);
        PointF shin1Mid_  = ARM_PT(shin1Mid);

        PointF shin2Start = ARM_PT(shin2KneeTangent);
        PointF shin2End   = ARM_PT(shin2FootTangent);
        PointF shin2Mid_  = ARM_PT(shin2Mid);

        PointF footP1 = ARM_PT(shin1FootTangent);
        PointF footP2 = ARM_PT(shin2FootTangent);
        PointF footM1 = ARM_PT(footMidHalf1);
        PointF footM2 = ARM_PT(footMidHalf2);
#undef ARM_PT

        // legCentroidRelJoint was already computed above (needed earlier,
        // for Rob.txt's own combined-system mass center) from the FULL
        // leg's true outline (knee + foot + both connectors) -- still the
        // whole leg's center of mass, same meaning as before.
        FILE* f = fopen("RockyExport\\Arm.txt", "w");
        if (!f)
            return 0;

        fprintf(f, "%.6f %.6f %.6f\n",
            legCentroidRelJoint.x * ROCKY_EXPORT_SCALE,
            legCentroidRelJoint.y * ROCKY_EXPORT_SCALE,
            r->legWeight);
        fprintf(f, "%.6f %.6f\n", 0.0f, 0.0f);

        fprintf(f, "%.6f %.6f %.6f %.6f %.6f %.6f\n", kneeP1.x, kneeP1.y, kneeP2.x, kneeP2.y, kneeM1.x, kneeM1.y);
        fprintf(f, "%.6f %.6f %.6f %.6f %.6f %.6f\n", kneeP1.x, kneeP1.y, kneeP2.x, kneeP2.y, kneeM2.x, kneeM2.y);
        fprintf(f, "%.6f %.6f %.6f %.6f %.6f %.6f\n", shin1Start.x, shin1Start.y, shin1End.x, shin1End.y, shin1Mid_.x, shin1Mid_.y);
        fprintf(f, "%.6f %.6f %.6f %.6f %.6f %.6f\n", shin2Start.x, shin2Start.y, shin2End.x, shin2End.y, shin2Mid_.x, shin2Mid_.y);
        fprintf(f, "%.6f %.6f %.6f %.6f %.6f %.6f\n", footP1.x, footP1.y, footP2.x, footP2.y, footM1.x, footM1.y);
        fprintf(f, "%.6f %.6f %.6f %.6f %.6f %.6f\n", footP1.x, footP1.y, footP2.x, footP2.y, footM2.x, footM2.y);

        fclose(f);
    }

    char path[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, path);
    printf("Saved Rocky body/leg as Rob.txt + Arm.txt in: %s\\RockyExport\n", path);

    return 1;
}

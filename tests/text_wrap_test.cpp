// A text measured at its own width must not wrap.
//
// A box measures its text, then lays it out again with the measured width as the bound, but
// that bound has been through the box's padding: (w + p) - p, which in float need not give
// back w. When it came back a hair smaller the text wrapped inside a box one line tall and
// every word after the first vanished: a weighted "Emit .rc" drew as "Emit". Whether a label
// hit it depended only on its width (the regular-weight "Emit .rc" happened not to).
//
// fixtures/text_wrap_slack.rc (from text_wrap_slack.json) stacks four padded boxes: "Emit .rc"
// at weight 500 clipped and not, at the default weight, and "Emit rc" at weight 500. Each must
// show ink past its first word, in the strip x 60..92 where ".rc" / "rc" are drawn.
//
//   text_wrap_test <fixtures dir>
#include "rccore/WireBuffer.h"
#include "rccore/CoreDocument.h"
#include "rccore/RemoteContext.h"
#include "rcskia/SkiaPaintContext.h"

#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::fprintf(stderr, "FAIL: %s\n", msg); ++failures; } \
    else         { std::fprintf(stderr, "ok:   %s\n", msg); } \
} while (0)

int main(int argc, char** argv) {
    std::string dir = (argc > 1) ? argv[1] : "tests/fixtures";
    std::string path = dir + "/text_wrap_slack.rc";
    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> data{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
    if (data.empty()) { std::fprintf(stderr, "FAIL: cannot read %s\n", path.c_str()); return 1; }

    rccore::WireBuffer buffer(data.data(), data.size());
    rccore::CoreDocument doc;
    if (!doc.initFromBuffer(buffer)) { std::fprintf(stderr, "FAIL: fixture does not parse\n"); return 1; }

    const int W = doc.getWidth(), H = doc.getHeight();
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(W, H));
    SkCanvas* canvas = surface->getCanvas();
    rccore::RemoteContext ctx;
    rcskia::SkiaPaintContext paint(ctx, canvas);
    ctx.setPaintContext(&paint);
    ctx.setDocument(&doc);
    ctx.mWidth = static_cast<float>(W);
    ctx.mHeight = static_cast<float>(H);
    doc.registerListeners(ctx);
    canvas->clear(SK_ColorBLACK);
    doc.applyDataOperations(ctx, -2);
    doc.paint(ctx, -2);

    std::vector<uint32_t> px(static_cast<size_t>(W) * H, 0);
    surface->readPixels(SkImageInfo::MakeN32Premul(W, H), px.data(), W * 4, 0, 0);
    // Light text on a dark box: count bright pixels in the strip after the first word.
    auto ink = [&](int y0, int y1) {
        int n = 0;
        for (int y = y0; y < y1 && y < H; ++y) {
            for (int x = 60; x < 92 && x < W; ++x) {
                uint32_t c = px[static_cast<size_t>(y) * W + x];
                int sum = (c & 0xff) + ((c >> 8) & 0xff) + ((c >> 16) & 0xff);
                if (sum > 450) ++n;
            }
        }
        return n;
    };
    // Box tops from the column (padding 10, spacedBy 8): about 10, 56, 102, 145; 35-38 tall.
    // A whole label leaves 50-65 bright pixels in the strip; a cut one leaves none.
    CHECK(ink(10, 48) > 20, "weight 500, clipped box: \"Emit .rc\" draws past \"Emit\"");
    CHECK(ink(56, 94) > 20, "weight 500: \"Emit .rc\" draws past \"Emit\"");
    CHECK(ink(102, 137) > 20, "default weight: \"Emit .rc\" draws past \"Emit\"");
    CHECK(ink(145, 183) > 20, "weight 500: \"Emit rc\" draws past \"Emit\"");

    std::fprintf(stderr, failures ? "\n%d FAILURE(S)\n" : "\nALL PASSED\n", failures);
    return failures ? 1 : 0;
}

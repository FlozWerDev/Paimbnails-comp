import assert from "node:assert/strict";
import { readFileSync, mkdtempSync, rmSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { join } from "node:path";

const root = new URL("../", import.meta.url);
const read = (path) => readFileSync(new URL(path, root), "utf8").replace(/^\uFEFF/, "");
const prefetch = read("src/core/MainLevelPrefetch.hpp");
const start = prefetch.indexOf("template <typename LoadFn>");
assert.notEqual(start, -1);
assert.match(prefetch.slice(start), /^template <typename LoadFn>\s+void staggerMainLevelThumbnailLoads\(/);
const body = prefetch.indexOf("{", start);
let depth = 1;
let end = body + 1;
while (depth && end < prefetch.length) {
    if (prefetch[end] === "{") ++depth;
    if (prefetch[end] === "}") --depth;
    ++end;
}
assert.equal(depth, 0);
const ids = read("src/core/MainLevels.hpp").match(/inline constexpr int kMainLevel(?:Min|Max)ID = \d+;/g);
assert.equal(ids?.length, 2);

const source = `
#include <algorithm>
#include <cassert>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

namespace geode {
template<class T> using CopyableFunction = std::function<T>;
}
namespace paimon {
${ids.join("\n")}
bool shuttingDown = false;
bool isRuntimeShuttingDown() { return shuttingDown; }
struct Task {
    float delay;
    std::function<void()> callback;
};
std::deque<Task> tasks;
void scheduleMainThreadDelay(float delay, std::function<void()> callback) {
    tasks.push_back({delay, std::move(callback)});
}
void tick() {
    assert(tasks.size() == 1);
    auto task = std::move(tasks.front());
    tasks.pop_front();
    task.callback();
}
namespace preload {
${prefetch.slice(start, end)}
}
}

int main() {
    using namespace paimon;
    using preload::staggerMainLevelThumbnailLoads;
    struct Case { int batch; float delay; int expectedBatch; float expectedDelay; };
    for (auto test : {Case{4, 0.06f, 4, 0.06f}, Case{0, 0.0f, 1, 0.03f},
                     Case{-3, -1.0f, 1, 0.03f}, Case{1, 0.03f, 1, 0.03f},
                     Case{7, 0.2f, 7, 0.2f}, Case{30, 0.06f, 22, 0.06f}}) {
        std::vector<int> loaded;
        auto capture = std::make_shared<int>(42);
        std::weak_ptr<int> weak = capture;
        staggerMainLevelThumbnailLoads([capture, &loaded](int id) {
            assert(*capture == 42);
            loaded.push_back(id);
        }, test.batch, test.delay);
        capture.reset();
        assert(loaded.size() == static_cast<size_t>(test.expectedBatch));
        while (loaded.size() < 22) {
            assert(!weak.expired());
            assert(tasks.size() == 1 && tasks.front().delay == test.expectedDelay);
            auto expected = std::min(size_t{22}, loaded.size() + test.expectedBatch);
            tick();
            assert(loaded.size() == expected);
        }
        assert(tasks.empty() && weak.expired());
        assert(loaded.size() == 22);
        for (int i = 0; i < 22; ++i) assert(loaded[i] == i + 1);
    }

    for (int stop = 0; stop < 3; ++stop) {
        std::vector<int> loaded;
        shuttingDown = stop == 2;
        auto capture = std::make_shared<int>(42);
        std::weak_ptr<int> weak = capture;
        staggerMainLevelThumbnailLoads([capture, &loaded](int id) {
            assert(*capture == 42);
            loaded.push_back(id);
        });
        capture.reset();
        if (stop == 2) {
            assert(loaded.empty());
        } else {
            assert((loaded == std::vector<int>{1, 2, 3, 4}));
            assert(!weak.expired() && tasks.size() == 1);
            assert(tasks.front().delay == 0.06f);
            if (stop == 0) {
                shuttingDown = true;
                tick();
            } else {
                tasks.clear();
            }
            assert((loaded == std::vector<int>{1, 2, 3, 4}));
        }
        assert(tasks.empty() && weak.expired());
        shuttingDown = false;
    }
    std::cout << "main level prefetch regression tests passed\\n";
}
`;

const temp = mkdtempSync(join(tmpdir(), "main-level-prefetch-"));
try {
    const compiler = process.env.CXX || "g++";
    const binary = join(temp, "regression");
    const compile = spawnSync(compiler, ["-std=c++20", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-g", "-x", "c++", "-", "-o", binary],
        { input: source, encoding: "utf8" });
    assert.equal(compile.status, 0, compile.error?.message || compile.stderr);
    const run = spawnSync(binary, [], { cwd: temp, encoding: "utf8", timeout: 15000 });
    assert.equal(run.status, 0, run.error?.message || run.stderr);
    process.stdout.write(run.stdout);
} finally {
    rmSync(temp, { recursive: true, force: true });
}

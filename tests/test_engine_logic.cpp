#include <cstring>
#include <string>
#include <thread>

#include "core/frame_buffer.h"
#include "core/main_thread_queue.h"
#include "core/navigation_gate.h"
#include "testing.h"

using namespace nc;

TEST(queue_runs_in_order) {
    MainThreadQueue q;
    std::string s;
    q.post([&] { s += "a"; });
    q.post([&] { s += "b"; });
    CHECK_EQ(s, std::string(""));
    q.drain();
    CHECK_EQ(s, std::string("ab"));
    q.drain();
    CHECK_EQ(s, std::string("ab"));
}

TEST(queue_post_from_other_thread) {
    MainThreadQueue q;
    int n = 0;
    std::thread t([&] {
        for (int i = 0; i < 1000; ++i) q.post([&] { ++n; });
    });
    t.join();
    q.drain();
    CHECK_EQ(n, 1000);
}

TEST(queue_task_posting_more_tasks_runs_next_drain) {
    MainThreadQueue q;
    int n = 0;
    q.post([&] { q.post([&] { ++n; }); });
    q.drain();
    CHECK_EQ(n, 0);
    q.drain();
    CHECK_EQ(n, 1);
}

TEST(paint_waits_for_response) {
    FrameBuffer fb;
    uint32_t px1 = 0x11223344, px2 = 0x55667788;
    fb.store(&px1, 1, 1);
    int calls = 0, last_tex = 0;
    uint32_t seen = 0;
    auto deliver = [&](int tex, int w, int h, const unsigned char* p) {
        ++calls;
        last_tex = tex;
        seen = *reinterpret_cast<const uint32_t*>(p);
        CHECK_EQ(w, 1);
        CHECK_EQ(h, 1);
    };
    fb.deliver_if_ready(deliver);
    CHECK_EQ(calls, 1);
    CHECK_EQ(seen, px1);
    fb.store(&px2, 1, 1);
    fb.deliver_if_ready(deliver);  // still waiting for the game's NeedsPaintResponse
    CHECK_EQ(calls, 1);
    fb.on_response(last_tex);
    fb.deliver_if_ready(deliver);
    CHECK_EQ(calls, 2);
    CHECK_EQ(seen, px2);
}

TEST(paint_nothing_new_nothing_sent) {
    FrameBuffer fb;
    int calls = 0;
    auto deliver = [&](int, int, int, const unsigned char*) { ++calls; };
    fb.deliver_if_ready(deliver);
    CHECK_EQ(calls, 0);
    uint32_t px = 1;
    fb.store(&px, 1, 1);
    fb.deliver_if_ready(deliver);
    fb.on_response(1);
    fb.deliver_if_ready(deliver);
    CHECK_EQ(calls, 1);
}

TEST(paint_stale_response_ignored) {
    FrameBuffer fb;
    uint32_t px = 1;
    int calls = 0, tex = 0;
    auto deliver = [&](int t, int, int, const unsigned char*) { ++calls; tex = t; };
    fb.store(&px, 1, 1);
    fb.deliver_if_ready(deliver);
    fb.store(&px, 1, 1);
    fb.on_response(tex + 100);  // not ours
    fb.deliver_if_ready(deliver);
    CHECK_EQ(calls, 1);
}

TEST(paint_popup_composited) {
    FrameBuffer fb;
    uint32_t view[4] = {1, 1, 1, 1};  // 2x2
    uint32_t popup[1] = {9};
    fb.store(view, 2, 2);
    fb.store_popup(popup, 1, 1);
    fb.set_popup_rect(1, 1, 1, 1);
    uint32_t got[4] = {};
    fb.deliver_if_ready([&](int, int, int, const unsigned char* p) { memcpy(got, p, 16); });
    CHECK_EQ(got[0], 1u);
    CHECK_EQ(got[3], 9u);
    fb.hide_popup();
    fb.on_response(1);
    fb.deliver_if_ready([&](int, int, int, const unsigned char* p) { memcpy(got, p, 16); });
    CHECK_EQ(got[3], 1u);
}

TEST(nav_first_load_allowed) {
    NavigationGate g;
    CHECK(g.on_before_browse("https://a.ru/", true));
}

TEST(nav_user_navigation_asks_game) {
    NavigationGate g;
    g.on_before_browse("https://a.ru/", true);
    g.mark_loaded();
    CHECK(!g.on_before_browse("https://b.ru/", true));  // cancelled, asked the game
    CHECK_EQ(g.pending(), std::string("https://b.ru/"));
    CHECK_EQ(g.on_response(true), std::string("https://b.ru/"));  // reissue this url
    CHECK(g.on_before_browse("https://b.ru/", true));  // the reissued load passes
}

TEST(nav_denied_by_game) {
    NavigationGate g;
    g.mark_loaded();
    CHECK(!g.on_before_browse("steam://x", true));
    CHECK_EQ(g.on_response(false), std::string(""));
    CHECK(!g.on_before_browse("steam://x", true));
}

TEST(nav_redirects_and_subframes_pass) {
    NavigationGate g;
    g.mark_loaded();
    CHECK(g.on_before_browse("https://a.ru/", false));
    CHECK(g.on_before_browse("https://a.ru/iframe", false, /*is_redirect=*/false, /*main=*/false));
    CHECK(g.on_before_browse("https://a.ru/r", true, /*is_redirect=*/true));
}

TEST(nav_explicit_posturl_allowed) {
    NavigationGate g;
    g.mark_loaded();
    g.allow_once("https://c.ru/");
    CHECK(g.on_before_browse("https://c.ru/", true));
}

TEST(paint_response_inside_deliver) {
    // vgui2::HTML::BrowserNeedsPaint calls NeedsPaintResponse before returning.
    FrameBuffer fb;
    uint32_t px = 7;
    int calls = 0;
    std::thread t([&] {
        fb.store(&px, 1, 1);
        fb.deliver_if_ready([&](int tex, int, int, const unsigned char*) {
            ++calls;
            fb.on_response(tex);
        });
        fb.store(&px, 1, 1);
        fb.deliver_if_ready([&](int tex, int, int, const unsigned char*) {
            ++calls;
            fb.on_response(tex);
        });
    });
    t.join();
    CHECK_EQ(calls, 2);
}

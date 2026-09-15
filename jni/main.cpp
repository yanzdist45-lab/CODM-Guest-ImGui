#include <sys/types.h>
#include <zygisk.hpp>
#include <android/input.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <atomic>
#include <cstring>
#include <string>

#include "dobby.h"
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "CODM-Guest-ImGui", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "CODM-Guest-ImGui", __VA_ARGS__)

static constexpr const char *TARGET = "com.garena.game.codm";
static constexpr const char *APP_BASE = "/data/user/0/com.garena.game.codm";
static constexpr const char *SLOT_BASE = "/data/local/codm/accounts";

enum class Cmd : uint32_t { SAVE = 1, SWITCH = 2, DELETE_SLOT = 3 };
struct Request { uint32_t magic; Cmd cmd; uint32_t slot; };
static constexpr uint32_t MAGIC = 0x43474149; // CGAI

static zygisk::Api *g_api = nullptr;
static std::atomic<float> g_x{0}, g_y{0};
static std::atomic<bool> g_down{false};
static bool g_imgui = false;
static bool g_menu = true;

static int run_sh(const std::string &cmd) {
    pid_t p = fork();
    if (p == 0) {
        execl("/system/bin/sh", "sh", "-c", cmd.c_str(), (char*)nullptr);
        _exit(127);
    }
    int st = 0;
    if (p > 0) waitpid(p, &st, 0);
    return st;
}

static std::string slot_path(uint32_t slot) {
    return std::string(SLOT_BASE) + "/slot" + std::to_string(slot);
}

static void companion_handler(int fd) {
    Request r{};

    ssize_t n = read(fd, &r, sizeof(r));
    if (n != sizeof(r) ||
        r.magic != MAGIC ||
        r.slot < 1 ||
        r.slot > 8) {
        return;
    }

    const std::string slot = slot_path(r.slot);
    const std::string app = APP_BASE;
    const std::string target = TARGET;

    if (r.cmd == Cmd::SAVE) {
        std::string cmd =
            "mkdir -p '" + slot + "' && "
            "rm -rf '" + slot + "/shared_prefs' && "
            "cp -a '" + app + "/shared_prefs' '" +
            slot + "/shared_prefs'";

        run_sh(cmd);
        return;
    }

    if (r.cmd == Cmd::DELETE_SLOT) {
        run_sh("rm -rf '" + slot + "'");
        return;
    }

    if (r.cmd == Cmd::SWITCH) {
        if (access(
                (slot + "/shared_prefs").c_str(),
                F_OK) != 0) {
            return;
        }

        // Root companion continues running after CODM is stopped.
        std::string cmd =
            "am force-stop " + target + "; "
            "sleep 1; "

            "rm -rf '" + app + "/shared_prefs'; "

            "cp -a '" + slot +
            "/shared_prefs' '" +
            app + "/shared_prefs'; "

            "APPUID=$(dumpsys package " + target +
            " | grep -m1 -E 'uid=|userId=' "
            "| sed -E 's/.*(uid|userId)=([0-9]+).*/\\2/'); "

            "[ -n \"$APPUID\" ] && "
            "chown -R $APPUID:$APPUID '" +
            app + "/shared_prefs'; "

            "restorecon -RF '" +
            app +
            "/shared_prefs' >/dev/null 2>&1; "

            "sleep 1; "

            "monkey -p " + target +
            " -c android.intent.category.LAUNCHER "
            "1 >/dev/null 2>&1";

        run_sh(cmd);
    }
}

static void send_cmd(Cmd cmd, int slot) {
    if (!g_api) return;
    int fd = g_api->connectCompanion();
    if (fd < 0) return;
    Request r{MAGIC, cmd, static_cast<uint32_t>(slot)};
    write(fd, &r, sizeof(r));
    close(fd);
}

using SwapFn = EGLBoolean(*)(EGLDisplay, EGLSurface);
static SwapFn old_swap = nullptr;

static EGLBoolean hook_swap(EGLDisplay dpy, EGLSurface surface) {
    if (!g_imgui) {
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        ImGui_ImplOpenGL3_Init("#version 300 es");
        g_imgui = true;
    }

    EGLint w = 0, h = 0;
    eglQuerySurface(dpy, surface, EGL_WIDTH, &w);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);

    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.AddMousePosEvent(g_x.load(), g_y.load());
    io.AddMouseButtonEvent(0, g_down.load());

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    if (g_menu) {
        ImGui::SetNextWindowSize(ImVec2(520, 520), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("CODM Guest Account Manager", &g_menu)) {
            ImGui::TextUnformatted("shared_prefs slot switcher");
            ImGui::Separator();
            for (int i = 1; i <= 8; ++i) {
                ImGui::PushID(i);
                ImGui::Text("Guest Slot %d", i);
                ImGui::SameLine(180);
                if (ImGui::Button("Save Current", ImVec2(135, 0))) send_cmd(Cmd::SAVE, i);
                ImGui::SameLine();
                if (ImGui::Button("Switch", ImVec2(90, 0))) send_cmd(Cmd::SWITCH, i);
                ImGui::SameLine();
                if (ImGui::Button("Delete", ImVec2(80, 0))) send_cmd(Cmd::DELETE_SLOT, i);
                ImGui::PopID();
            }
            ImGui::Separator();
            ImGui::TextWrapped("Switch will force-stop CODM, restore the selected shared_prefs slot, fix ownership/SELinux context, then launch CODM again. Relog may still be required by the game.");
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return old_swap(dpy, surface);
}


using MotionGetXFn = float(*)(const AInputEvent*, size_t);
using MotionGetYFn = float(*)(const AInputEvent*, size_t);
using MotionGetActionFn = int32_t(*)(const AInputEvent*);

static MotionGetXFn old_motion_x = nullptr;
static MotionGetYFn old_motion_y = nullptr;
static MotionGetActionFn old_motion_action = nullptr;

static float hook_motion_x(const AInputEvent *event, size_t pointer_index) {
    float x = old_motion_x(event, pointer_index);

    if (pointer_index == 0) {
        g_x.store(x);
    }

    return x;
}

static float hook_motion_y(const AInputEvent *event, size_t pointer_index) {
    float y = old_motion_y(event, pointer_index);

    if (pointer_index == 0) {
        g_y.store(y);
    }

    return y;
}

static int32_t hook_motion_action(const AInputEvent *event) {
    int32_t action = old_motion_action(event);
    int32_t masked = action & AMOTION_EVENT_ACTION_MASK;

    if (masked == AMOTION_EVENT_ACTION_DOWN ||
        masked == AMOTION_EVENT_ACTION_POINTER_DOWN ||
        masked == AMOTION_EVENT_ACTION_MOVE) {
        g_down.store(true);
    }

    if (masked == AMOTION_EVENT_ACTION_UP ||
        masked == AMOTION_EVENT_ACTION_POINTER_UP ||
        masked == AMOTION_EVENT_ACTION_CANCEL) {
        g_down.store(false);
    }

    return action;
}

static void install_hooks() {
    void *egl = dlopen("libEGL.so", RTLD_NOW);
    void *android = dlopen("libandroid.so", RTLD_NOW);

    void *swap = egl
        ? dlsym(egl, "eglSwapBuffers")
        : nullptr;

    void *motion_x = android
        ? dlsym(android, "AMotionEvent_getX")
        : nullptr;

    void *motion_y = android
        ? dlsym(android, "AMotionEvent_getY")
        : nullptr;

    void *motion_action = android
        ? dlsym(android, "AMotionEvent_getAction")
        : nullptr;

    if (swap &&
        DobbyHook(
            swap,
            reinterpret_cast<dobby_dummy_func_t>(hook_swap),
            reinterpret_cast<dobby_dummy_func_t *>(&old_swap)
        ) == 0) {
        LOGI("eglSwapBuffers hooked");
    } else {
        LOGE("eglSwapBuffers hook failed");
    }

    if (motion_x &&
        DobbyHook(
            motion_x,
            reinterpret_cast<dobby_dummy_func_t>(hook_motion_x),
            reinterpret_cast<dobby_dummy_func_t *>(&old_motion_x)
        ) == 0) {
        LOGI("AMotionEvent_getX hooked");
    } else {
        LOGE("AMotionEvent_getX hook failed");
    }

    if (motion_y &&
        DobbyHook(
            motion_y,
            reinterpret_cast<dobby_dummy_func_t>(hook_motion_y),
            reinterpret_cast<dobby_dummy_func_t *>(&old_motion_y)
        ) == 0) {
        LOGI("AMotionEvent_getY hooked");
    } else {
        LOGE("AMotionEvent_getY hook failed");
    }

    if (motion_action &&
        DobbyHook(
            motion_action,
            reinterpret_cast<dobby_dummy_func_t>(hook_motion_action),
            reinterpret_cast<dobby_dummy_func_t *>(&old_motion_action)
        ) == 0) {
        LOGI("AMotionEvent_getAction hooked");
    } else {
        LOGE("AMotionEvent_getAction hook failed");
    }
}

// Keep a single module instance and cache JNIEnv for package filtering.
static JNIEnv *g_env = nullptr;

class Entry : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        g_api = api;
        g_env = env;
    }
    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        bool ok = false;
        if (g_env && args && args->nice_name) {
            const char *s = g_env->GetStringUTFChars(args->nice_name, nullptr);
            if (s) {
                ok = strcmp(s, TARGET) == 0;
                g_env->ReleaseStringUTFChars(args->nice_name, s);
            }
        }
        enabled_ = ok;
        if (!enabled_) g_api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }
    void postAppSpecialize(const zygisk::AppSpecializeArgs *) override {
        if (!enabled_) return;
        usleep(1500000);
        install_hooks();
    }
private:
    bool enabled_ = false;
};

REGISTER_ZYGISK_MODULE(Entry)
REGISTER_ZYGISK_COMPANION(companion_handler)

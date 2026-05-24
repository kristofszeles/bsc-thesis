#if defined(__ANDROID__)

#include "android_picker.h"

#include <functional>
#include <jni.h>
#include <mutex>

#include <SDL_system.h>
#include <SDL_log.h>

namespace {
    std::mutex g_mu;
    std::string g_pickedData;
    bool g_pickedReady = false;

    // Resolve the MainActivity class via the running activity's runtime type so the same JNI
    // bridge keeps working if the package or class is later renamed.
    void withActivityClass(const std::function<void(JNIEnv*, jobject, jclass)>& body) {
        JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
        jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
        if (!env || !activity) {
            if (env && activity) env->DeleteLocalRef(activity);
            return;
        }
        jclass cls = env->GetObjectClass(activity);
        if (cls) {
            body(env, activity, cls);
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
            }
            env->DeleteLocalRef(cls);
        }
        env->DeleteLocalRef(activity);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_bscthesis_maze_1game_MainActivity_nativeOnMapPicked(JNIEnv* env, jclass, jbyteArray data) {
    if (!data) {
        return;
    }
    const jsize len = env->GetArrayLength(data);
    if (len <= 0) {
        return;
    }
    std::string buf;
    buf.resize(static_cast<size_t>(len));
    env->GetByteArrayRegion(data, 0, len, reinterpret_cast<jbyte*>(&buf[0]));
    std::lock_guard<std::mutex> lock(g_mu);
    g_pickedData = std::move(buf);
    g_pickedReady = true;
}

namespace maze_android {

void launchOpenMapPicker() {
    withActivityClass([](JNIEnv* env, jobject /*activity*/, jclass cls) {
        jmethodID mid = env->GetStaticMethodID(cls, "openMapPicker", "()V");
        if (mid) {
            env->CallStaticVoidMethod(cls, mid);
        } else {
            SDL_Log("android_picker: openMapPicker static method not found");
        }
    });
}

void launchSaveMapPicker(const std::string& data) {
    withActivityClass([&data](JNIEnv* env, jobject /*activity*/, jclass cls) {
        jmethodID mid = env->GetStaticMethodID(cls, "saveMapPicker", "([B)V");
        if (!mid) {
            SDL_Log("android_picker: saveMapPicker static method not found");
            return;
        }
        jbyteArray arr = env->NewByteArray(static_cast<jsize>(data.size()));
        if (!arr) {
            return;
        }
        if (!data.empty()) {
            env->SetByteArrayRegion(arr, 0, static_cast<jsize>(data.size()),
                                    reinterpret_cast<const jbyte*>(data.data()));
        }
        env->CallStaticVoidMethod(cls, mid, arr);
        env->DeleteLocalRef(arr);
    });
}

bool consumePickedMap(std::string& out) {
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_pickedReady) {
        return false;
    }
    out = std::move(g_pickedData);
    g_pickedData.clear();
    g_pickedReady = false;
    return true;
}

}

#endif

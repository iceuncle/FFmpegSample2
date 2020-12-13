#include <jni.h>
#include <string>

#define JNI_CLASS_PATH "com/tianyang/firstjni/MainActivity"
#define JNI_STUDENT_PATH "com/tianyang/firstjni/Student"

extern "C" JNIEXPORT jstring JNICALL
Java_com_tianyang_firstjni_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {

    //step1
    jclass clazz = env->FindClass(JNI_STUDENT_PATH);
    //step2
    jmethodID method_init_id = env->GetMethodID(clazz, "<init>", "()V");
    jmethodID method_set_id = env->GetMethodID(clazz, "setAge", "(I)V");
    jmethodID method_get_id = env->GetMethodID(clazz, "getAge", "()I");
    //step3
    jobject obj = env->NewObject(clazz, method_init_id);
    //step4
    env->CallVoidMethod(obj, method_set_id, 18);
    int age = env->CallIntMethod(obj, method_get_id);

    char tmp[10];
    sprintf(tmp, "%d", age);

    std::string hello = "Hello from C++, year=";
    hello.append(tmp);
    return env->NewStringUTF(hello.c_str());
}


extern "C"
JNIEXPORT jstring JNICALL
Java_com_tianyang_firstjni_MainActivity_stringFromJNI2(JNIEnv *env, jobject thiz) {
    return env->NewStringUTF(",xxx");
}


extern "C"
JNIEXPORT jstring JNICALL
Java_com_tianyang_firstjni_MainActivity_stringFromJNI3(JNIEnv *env,
                                                       jobject thiz,
                                                       jstring str_) {
    const char *str = env->GetStringUTFChars(str_, 0);
    env->ReleaseStringUTFChars(str_, str);
    return env->NewStringUTF("aaa");
}

extern "C"
JNIEXPORT jstring JNICALL
my_test_register(JNIEnv *env, jobject thiz) {
    return env->NewStringUTF("this is a test for register!");
}

static JNINativeMethod g_methods[] = {
        {"_test", "()Ljava/lang/String;", (void *) my_test_register}
};

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env = NULL;
    vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    jclass clazz = env->FindClass(JNI_CLASS_PATH);
    env->RegisterNatives(clazz, g_methods, sizeof(g_methods) / sizeof(g_methods[0]));
    return JNI_VERSION_1_6;
}
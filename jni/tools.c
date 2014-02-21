#include "tools.h"

/**
 * throw java exceptions
 **/
void throwJavaException(JNIEnv* env, const char *name, const char *msg) {
	jclass cls = (*env)->FindClass(env, name);
	if (cls != NULL )
		(*env)->ThrowNew(env, cls, msg);
	(*env)->DeleteLocalRef(env, cls);
}

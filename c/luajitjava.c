
/******************************************************************************
* $Id$
* Copyright (C) 2003-2007 Kepler Project - 2017 David Fremont.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

/***************************************************************************
*
* $ED
*    This module is the c implementation of luajitjava's dynamic library.
*    It provides methods to luajit to access java classes, objects and properties
*    It is heavily based on Kepler project's luajava
*    https://github.com/jasonsantos/luajava
*    And has been modified to fit luajit simplier c library calls
*    And all code in luajava aimed at being called from java has been removed
*    This library is to provide access to java from luajit in the first place
*    wherever language luajit is launched from
*
*****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <jni.h>
#include <windows.h>

#include "luajitjava.h"

//opaque struct returned after started environment
typedef struct ljJavaEnvironment {
	JNIEnv* javaEnv;
	JavaVM* jvm;
} ljJavaEnvironment_t;



//set of useful java classes we bind with at init for convenience
// including the luajitjava binding class
static jclass    luajitjava_binding_class = NULL;
static jmethodID luajitjava_run_method = NULL;
static jmethodID luajitjava_java_new = NULL;
static jmethodID luajitjava_check_field = NULL;
static jclass    throwable_class = NULL;
static jmethodID throwable_tostring = NULL;
static jmethodID throwable_get_message = NULL;
static jclass    java_lang_class = NULL;
static jmethodID java_lang_class_forname = NULL;
static jclass    java_lang_object = NULL;

//keep track of the java class loader at init in jni context
// to be provided to java.lang.Class calls,
// as the default context in this case would not get external lib access
static jobject	 java_class_loader = NULL;

//type classes to be used when transmitting method or constructor parameters
static jobject	 java_byte_class = NULL;
static jmethodID java_new_byte = NULL;
static jmethodID java_byte_value = NULL;
static jobject	 java_short_class = NULL;
static jmethodID java_new_short = NULL;
static jmethodID java_short_value = NULL;
static jobject	 java_int_class = NULL;
static jmethodID java_new_int = NULL;
static jmethodID java_int_value = NULL;
static jobject	 java_long_class = NULL;
static jmethodID java_new_long = NULL;
static jmethodID java_long_value = NULL;
static jobject	 java_float_class = NULL;
static jmethodID java_new_float = NULL;
static jmethodID java_float_value = NULL;
static jobject	 java_double_class = NULL;
static jmethodID java_new_double = NULL;
static jmethodID java_double_value = NULL;
static jobject	 java_boolean_class = NULL;
static jmethodID java_new_boolean = NULL;
static jmethodID java_boolean_value = NULL;
static jobject	 java_string_class = NULL;


//utility function to check for exception after jni calls
jobject checkException(JNIEnv* javaEnv) {
	jthrowable exp = (*javaEnv)->ExceptionOccurred(javaEnv);

	/* Handles exception */
	if (exp != NULL)
	{
		jobject jstr;

		(*javaEnv)->ExceptionClear(javaEnv);
		jstr = (*javaEnv)->CallObjectMethod(javaEnv, exp, throwable_get_message);

		if (jstr == NULL)
		{
			jstr = (*javaEnv)->CallObjectMethod(javaEnv, exp, throwable_tostring);
		}

		(*javaEnv)->DeleteLocalRef(javaEnv, exp);
		return jstr;
	}
	return NULL;
}

//function to start the VM
JNIEnv* create_vm(JavaVM** jvm, const char* classPath) {
	JNIEnv *env;
	JavaVMInitArgs vm_args;
	const char* classpathPattern = "-Djava.class.path=%s";
	vm_args.version = JNI_VERSION_1_8; //JDK version. This indicates     version 1.8
	JNI_GetDefaultJavaVMInitArgs(&vm_args);
	JavaVMOption options;
	options.optionString = malloc((_mbstrlen(classpathPattern) - 1 + _mbstrlen(classPath)) * sizeof(char));
	sprintf(options.optionString, classpathPattern, classPath);
	vm_args.nOptions = 1;
	vm_args.options = &options;
	vm_args.ignoreUnrecognized = 0;
	JNI_CreateJavaVM(jvm, (void**)&env, &vm_args);
	return env;
}

//bind with all utility java objects
int bindJavaBaseLinks(JNIEnv* env)
{
	jclass tmpClass;

	tmpClass = (*env)->FindClass(env, "developpeur2000/luajitjava/LuaJitJavaAPI");
	if (tmpClass == NULL)
	{
		fprintf(stderr, "Could not find LuaJitJavaAPI class\n");
		return 0;
	}
	luajitjava_binding_class = (*env)->NewGlobalRef(env, tmpClass);
	(*env)->DeleteLocalRef(env, tmpClass);

	luajitjava_run_method = (*env)->GetStaticMethodID(env, luajitjava_binding_class, "runMethod",
		"(Ljava/lang/Object;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
	luajitjava_java_new = (*env)->GetStaticMethodID(env, luajitjava_binding_class, "javaNew",
		"(Ljava/lang/Class;[Ljava/lang/Object;)Ljava/lang/Object;");
	luajitjava_check_field = (*env)->GetStaticMethodID(env, luajitjava_binding_class, "checkField",
		"(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;");


	throwable_class = (*env)->FindClass(env, "java/lang/Throwable");
	throwable_get_message = (*env)->GetMethodID(env, throwable_class, "getMessage",
		"()Ljava/lang/String;");
	throwable_tostring = (*env)->GetMethodID(env, throwable_class, "toString",
		"()Ljava/lang/String;");

	java_lang_class = (*env)->FindClass(env, "java/lang/Class");
	java_lang_class_forname = (*env)->GetStaticMethodID(env, java_lang_class, "forName",
		"(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;");

	java_lang_object = (*env)->FindClass(env, "java/lang/Object");

	java_byte_class = (*env)->FindClass(env, "java/lang/Byte");
	java_new_byte = (*env)->GetMethodID(env, java_byte_class, "<init>", "(B)V");
	java_byte_value = (*env)->GetMethodID(env, java_byte_class, "intValue", "()I");

	java_short_class = (*env)->FindClass(env, "java/lang/Short");
	java_new_short = (*env)->GetMethodID(env, java_short_class, "<init>", "(S)V");
	java_short_value = (*env)->GetMethodID(env, java_short_class, "intValue", "()I");

	java_int_class = (*env)->FindClass(env, "java/lang/Integer");
	java_new_int = (*env)->GetMethodID(env, java_int_class, "<init>", "(I)V");
	java_int_value = (*env)->GetMethodID(env, java_int_class, "intValue", "()I");

	java_long_class = (*env)->FindClass(env, "java/lang/Long");
	java_new_long = (*env)->GetMethodID(env, java_long_class, "<init>", "(J)V");
	java_long_value = (*env)->GetMethodID(env, java_long_class, "longValue", "()J");

	java_float_class = (*env)->FindClass(env, "java/lang/Float");
	java_new_float = (*env)->GetMethodID(env, java_float_class, "<init>", "(F)V");
	java_float_value = (*env)->GetMethodID(env, java_float_class, "floatValue", "()F");

	java_double_class = (*env)->FindClass(env, "java/lang/Double");
	java_new_double = (*env)->GetMethodID(env, java_double_class, "<init>", "(D)V");
	java_double_value = (*env)->GetMethodID(env, java_double_class, "doubleValue", "()D");

	java_boolean_class = (*env)->FindClass(env, "java/lang/Boolean");
	java_new_boolean = (*env)->GetMethodID(env, java_boolean_class, "<init>", "(Z)V");
	java_boolean_value = (*env)->GetMethodID(env, java_boolean_class, "booleanValue", "()Z");

	java_string_class = (*env)->FindClass(env, "java/lang/String");

	return 1;
}

//release the utility java bindings
void unbindJavaBaseLinks(JNIEnv* env)
{
	(*env)->DeleteLocalRef(env, luajitjava_binding_class);
	(*env)->DeleteLocalRef(env, throwable_class);
	(*env)->DeleteLocalRef(env, java_lang_class);

	(*env)->DeleteLocalRef(env, java_lang_object);

	(*env)->DeleteLocalRef(env, java_byte_class);
	(*env)->DeleteLocalRef(env, java_short_class);
	(*env)->DeleteLocalRef(env, java_int_class);
	(*env)->DeleteLocalRef(env, java_long_class);
	(*env)->DeleteLocalRef(env, java_float_class);
	(*env)->DeleteLocalRef(env, java_double_class);
	(*env)->DeleteLocalRef(env, java_boolean_class);
	(*env)->DeleteLocalRef(env, java_string_class);
}

// init the bindings and get the java environment
void* internal_javaStart(const char* classPath)
{
	JNIEnv *env;
	JavaVM *jvm;
	ljJavaEnvironment_t* returnStruct;
	jclass tempClass = NULL;

	env = create_vm(&jvm, classPath);
	if (env == NULL)
	{
		fprintf(stderr, "\n Unable to create java VM");
		return NULL;
	}

	//store class loader
	jclass threadClass = (*env)->FindClass(env, "java/lang/Thread");
	jmethodID currentThreadMethod = (*env)->GetStaticMethodID(env, threadClass, "currentThread",
		"()Ljava/lang/Thread;");
	jobject currentThread = (*env)->CallStaticObjectMethod(env, threadClass, currentThreadMethod);
	jmethodID getContextClassLoaderMethod = (*env)->GetMethodID(env, threadClass, "getContextClassLoader",
		"()Ljava/lang/ClassLoader;");
	java_class_loader = (*env)->CallObjectMethod(env, currentThread, getContextClassLoaderMethod);
	//cleanup
	(*env)->DeleteLocalRef(env, threadClass);
	(*env)->DeleteLocalRef(env, currentThread);

	if (bindJavaBaseLinks(env) == 0) {
		fprintf(stderr, "\n Unable to bind with java\n");
		return NULL;
	}

	returnStruct = malloc(sizeof(ljJavaEnvironment_t));
	returnStruct->javaEnv = env;
	returnStruct->jvm = jvm;
	return (void*)returnStruct;
}

// release the bindings and destroy the JVM
void internal_javaEnd(void* ljEnv)
{
	ljJavaEnvironment_t* infoStruct = (ljJavaEnvironment_t*)ljEnv;
	JNIEnv * javaEnv = (JNIEnv *) infoStruct->javaEnv;
	JavaVM * jvm = (JavaVM *) infoStruct->jvm;

	free(infoStruct);

	(*javaEnv)->DeleteGlobalRef(javaEnv, java_class_loader);

	unbindJavaBaseLinks(javaEnv);

	(*jvm)->DestroyJavaVM(jvm);

	fprintf(stdout, "destroyed jvm\n");
}

// get a java class handle
int internal_javaBindClass(ljJavaClass_t* classInterface, const char* className)
{
	jstring javaClassName;
	jobject classInstance;
	JNIEnv * javaEnv;

	javaEnv = ((ljJavaEnvironment_t*)classInterface->ljEnv)->javaEnv;
	(*javaEnv)->ExceptionClear(javaEnv);


	javaClassName = (*javaEnv)->NewStringUTF(javaEnv, className);

	classInstance = (*javaEnv)->CallStaticObjectMethod(javaEnv, java_lang_class,
		java_lang_class_forname, javaClassName, JNI_TRUE, java_class_loader);

	(*javaEnv)->DeleteLocalRef(javaEnv, javaClassName);

	jobject jstr = checkException(javaEnv);
	if (jstr) {
		const char * cStr;
		cStr = (*javaEnv)->GetStringUTFChars(javaEnv, jstr, NULL);
		fprintf(stderr, "Error. Couldn't bind java class %s : %s\n", className, cStr);
		(*javaEnv)->ReleaseStringUTFChars(javaEnv, jstr, cStr);
		(*javaEnv)->DeleteLocalRef(javaEnv, jstr);
		return 0;
	}

	if (classInstance == NULL) {
		fprintf(stderr, "Error. Couldn't bind java class %s : unknown error\n", className);
	}

	classInterface->classObject = (*javaEnv)->NewGlobalRef(javaEnv, classInstance);
	(*javaEnv)->DeleteLocalRef(javaEnv, classInstance);

	return 1;
}

// release a java class handle
void internal_javaReleaseClass(ljJavaClass_t* classInterface) {
	JNIEnv * javaEnv = ((ljJavaEnvironment_t*)classInterface->ljEnv)->javaEnv;
	(*javaEnv)->DeleteGlobalRef(javaEnv, classInterface->classObject);
}

// utility function to transform vararg list into an array of java objects
//  that can be fed to a java method or constructor
jobjectArray getjavaArgs(JNIEnv * javaEnv, int nArgs, va_list valist) {
	char param_char;
	short param_short;
	int param_int;
	long param_long;
	double param_double;
	const char* param_string;
	ljJavaObject_t* param_object;

	jobject paramJObject;

	jobjectArray javaArgArray;
	javaArgType_t curType = JTYPE_NONE;

	javaArgArray = (*javaEnv)->NewObjectArray(javaEnv, nArgs/2, java_lang_object, NULL);

	for (int i = 0; i < nArgs; i++) {
		if (curType == JTYPE_NONE) {
			curType = va_arg(valist, javaArgType_t);
		}
		else {
			switch (curType) {
			case JTYPE_BYTE:
				param_char = va_arg(valist, char);
				paramJObject = (*javaEnv)->NewObject(javaEnv, java_byte_class, java_new_byte, param_char);
				break;
			case JTYPE_SHORT:
				param_short = va_arg(valist, short);
				paramJObject = (*javaEnv)->NewObject(javaEnv, java_short_class, java_new_short, param_short);
				break;
			case JTYPE_INT:
				param_int = va_arg(valist, int);
				paramJObject = (*javaEnv)->NewObject(javaEnv, java_int_class, java_new_int, param_int);
				break;
			case JTYPE_LONG:
				param_long = va_arg(valist, long);
				paramJObject = (*javaEnv)->NewObject(javaEnv, java_long_class, java_new_long, param_long);
				break;
			case JTYPE_FLOAT:
				param_double = va_arg(valist, double);
				paramJObject = (*javaEnv)->NewObject(javaEnv, java_float_class, java_new_float, (float) param_double);
				break;
			case JTYPE_DOUBLE:
				param_double = va_arg(valist, double);
				paramJObject = (*javaEnv)->NewObject(javaEnv, java_double_class, java_new_double, param_double);
				break;
			case JTYPE_BOOLEAN:
				param_char = va_arg(valist, char);
				paramJObject = (*javaEnv)->NewObject(javaEnv, java_boolean_class, java_new_boolean, param_char);
				break;
			case JTYPE_CHAR:
				param_char = va_arg(valist, char);
				paramJObject = (*javaEnv)->NewObject(javaEnv, java_byte_class, java_new_byte, param_char);
				break;
			case JTYPE_STRING:
				param_string = va_arg(valist, const char*);
				paramJObject = (jobject)(*javaEnv)->NewStringUTF(javaEnv, param_string);
				break;
			case JTYPE_OBJECT:
				param_object = va_arg(valist, ljJavaObject_t*);
				paramJObject = param_object->object;
				break;
			default:
				fprintf(stderr, "java new => unrecognized parameter type\n");
				va_end(valist);
				return NULL;
			}
			(*javaEnv)->SetObjectArrayElement(javaEnv, javaArgArray,
				i / 2, paramJObject);
			//TODO: remove ref depending if local or global ref cf GetObjectRefType
			//(*javaEnv)->DeleteLocalRef(javaEnv, paramJObject);
			curType = JTYPE_NONE;
		}
	}
	va_end(valist);

	return javaArgArray;
}

// utility function to release the array of java arguments created in the previous method
void releasejavaArgs(JNIEnv * javaEnv, jobjectArray javaArgArray) {
	jobject curJObject;

	for (int i = 0; i < (*javaEnv)->GetArrayLength(javaEnv, javaArgArray); i++) {
		curJObject = (*javaEnv)->GetObjectArrayElement(javaEnv, javaArgArray, i);
		(*javaEnv)->DeleteLocalRef(javaEnv, curJObject);
	}
	(*javaEnv)->DeleteLocalRef(javaEnv, javaArgArray);
}

// lua called method to instantiate an object from a specified class,
//  using provided constructor arguments
int internal_javaNew(ljJavaObject_t* objectInterface, ljJavaClass_t* classInterface, int nArgs, va_list valist)
{
	jobject newObject;
	jobject classInstance;
	JNIEnv * javaEnv;

	javaEnv = ((ljJavaEnvironment_t*)classInterface->ljEnv)->javaEnv;
	(*javaEnv)->ExceptionClear(javaEnv);

	//get java params from args
	jobjectArray javaArgArray = getjavaArgs(javaEnv, nArgs, valist);

	//check given class interface
	classInstance = (jobject) classInterface->classObject;
	if ((*javaEnv)->IsInstanceOf(javaEnv, classInstance, java_lang_class) == JNI_FALSE)
	{
		fprintf(stderr, "Class interface does not seem to contain a class instance\n");
		return 0;
	}

	//create new object through our binding java class
	newObject = (*javaEnv)->CallStaticObjectMethod(javaEnv, luajitjava_binding_class, luajitjava_java_new, classInstance, javaArgArray);
	releasejavaArgs(javaEnv, javaArgArray);

	/* Handles exception */
	jobject jstr = checkException(javaEnv);
	if (jstr) {
		const char * cStr;
		cStr = (*javaEnv)->GetStringUTFChars(javaEnv, jstr, NULL);
		fprintf(stderr, "Error. Couldn't create object : %s\n", cStr);
		(*javaEnv)->ReleaseStringUTFChars(javaEnv, jstr, cStr);
		(*javaEnv)->DeleteLocalRef(javaEnv, jstr);
		return 0;
	}

	if (newObject == NULL)
	{
		fprintf(stderr, "Error. Couldn't create object : unknown reason\n");
	}

	objectInterface->object = (*javaEnv)->NewGlobalRef(javaEnv, newObject);
	(*javaEnv)->DeleteLocalRef(javaEnv, newObject);
	return 1;
}

// lua called method to release a java object handle
void internal_javaReleaseObject(ljJavaObject_t* objectInterface) {
	JNIEnv * javaEnv = ((ljJavaEnvironment_t*)objectInterface->ljEnv)->javaEnv;
	(*javaEnv)->DeleteGlobalRef(javaEnv, objectInterface->object);
}

// method to look for a static field in a class
ljJavaObject_t* internal_javaCheckClassField(ljJavaClass_t* classInterface, const char * key)
{
	JNIEnv * javaEnv;
	jclass containerClass;
	jstring str;
	jobject eventualField;
	ljJavaObject_t* returnObject;

	javaEnv = ((ljJavaEnvironment_t*)classInterface->ljEnv)->javaEnv;
	(*javaEnv)->ExceptionClear(javaEnv);
	containerClass = (jobject)classInterface->classObject;

	str = (*javaEnv)->NewStringUTF(javaEnv, key);
	eventualField = (*javaEnv)->CallStaticObjectMethod(javaEnv, luajitjava_binding_class, luajitjava_check_field, containerClass, str);
	(*javaEnv)->DeleteLocalRef(javaEnv, str);
	/* Handles exception */
	jobject jstr = checkException(javaEnv);
	if (jstr) {
		const char * cStr;
		cStr = (*javaEnv)->GetStringUTFChars(javaEnv, jstr, NULL);
		fprintf(stderr, "Error. excpetion while getting index of object : %s\n", cStr);
		(*javaEnv)->ReleaseStringUTFChars(javaEnv, jstr, cStr);
		(*javaEnv)->DeleteLocalRef(javaEnv, jstr);
		return 0;
	}

	if (eventualField != NULL) {
		returnObject = malloc(sizeof(ljJavaObject_t));
		returnObject->ljEnv = classInterface->ljEnv;
		returnObject->object = (*javaEnv)->NewGlobalRef(javaEnv, eventualField);
		(*javaEnv)->DeleteLocalRef(javaEnv, eventualField);
		return returnObject;
	}
	return NULL;
}

// lua called method to look for a static method in a class corresponding to provided args
//  then run it and return the object result
ljJavaObject_t* internal_javaRunClassMethod(ljJavaClass_t* classInterface, const char * methodName, int nArgs, ...)
{
	JNIEnv * javaEnv;
	jclass containerClass;
	jstring str;
	jobject resultObj;
	ljJavaObject_t* returnObject;

	javaEnv = ((ljJavaEnvironment_t*)classInterface->ljEnv)->javaEnv;
	(*javaEnv)->ExceptionClear(javaEnv);
	containerClass = (jobject)classInterface->classObject;

	//get java params from args
	va_list valist;
	va_start(valist, nArgs);
	jobjectArray javaArgArray = getjavaArgs(javaEnv, nArgs, valist);

	/* Run method through our java proxy */
	str = (*javaEnv)->NewStringUTF(javaEnv, methodName);
	resultObj = (*javaEnv)->CallStaticObjectMethod(javaEnv, luajitjava_binding_class, luajitjava_run_method, containerClass, str, javaArgArray);
	(*javaEnv)->DeleteLocalRef(javaEnv, str);
	releasejavaArgs(javaEnv, javaArgArray);

	/* Handles exception */
	jobject jstr = checkException(javaEnv);
	if (jstr) {
		const char * cStr;
		cStr = (*javaEnv)->GetStringUTFChars(javaEnv, jstr, NULL);
		fprintf(stderr, "Error. exception while getting method of object : %s\n", cStr);
		(*javaEnv)->ReleaseStringUTFChars(javaEnv, jstr, cStr);
		(*javaEnv)->DeleteLocalRef(javaEnv, jstr);
		return NULL;
	}

	if (resultObj != NULL) {
		returnObject = malloc(sizeof(ljJavaObject_t));
		returnObject->ljEnv = classInterface->ljEnv;
		returnObject->object = (*javaEnv)->NewGlobalRef(javaEnv, resultObj);
		(*javaEnv)->DeleteLocalRef(javaEnv, resultObj);
		return returnObject;
	}
	return NULL;
}


// lua called method to look for a field in an object instance
ljJavaObject_t* internal_javaCheckObjectField(ljJavaObject_t* objectInterface, const char * key)
{
	JNIEnv * javaEnv;
	jobject containerObj;
	jstring str;
	jobject eventualField;
	ljJavaObject_t* returnObject;

	javaEnv = ((ljJavaEnvironment_t*)objectInterface->ljEnv)->javaEnv;
	(*javaEnv)->ExceptionClear(javaEnv);
	containerObj = (jobject)objectInterface->object;

	str = (*javaEnv)->NewStringUTF(javaEnv, key);
	eventualField = (*javaEnv)->CallStaticObjectMethod(javaEnv, luajitjava_binding_class, luajitjava_check_field, containerObj, str);
	(*javaEnv)->DeleteLocalRef(javaEnv, str);
	/* Handles exception */
	jobject jstr = checkException(javaEnv);
	if (jstr) {
		const char * cStr;
		cStr = (*javaEnv)->GetStringUTFChars(javaEnv, jstr, NULL);
		fprintf(stderr, "Error. excpetion while getting index of object : %s\n", cStr);
		(*javaEnv)->ReleaseStringUTFChars(javaEnv, jstr, cStr);
		(*javaEnv)->DeleteLocalRef(javaEnv, jstr);
		return 0;
	}

	if (eventualField != NULL) {
		returnObject = malloc(sizeof(ljJavaObject_t));
		returnObject->ljEnv = objectInterface->ljEnv;
		returnObject->object = (*javaEnv)->NewGlobalRef(javaEnv, eventualField);
		(*javaEnv)->DeleteLocalRef(javaEnv, eventualField);
		return returnObject;
	}
	return NULL;
}

// lua called method to look for a method in an object instance corresponding to provided args
//  then run it and return the object result
ljJavaObject_t* internal_javaRunObjectMethod(ljJavaObject_t* objectInterface, const char * methodName, int nArgs, va_list valist)
{
	JNIEnv * javaEnv;
	jobject containerObj;
	jstring str;
	jobject resultObj;
	ljJavaObject_t* returnObject;

	javaEnv = ((ljJavaEnvironment_t*)objectInterface->ljEnv)->javaEnv;
	(*javaEnv)->ExceptionClear(javaEnv);
	containerObj = (jobject)objectInterface->object;

	//get java params from args
	jobjectArray javaArgArray = getjavaArgs(javaEnv, nArgs, valist);

	/* Run method through our java proxy */
	str = (*javaEnv)->NewStringUTF(javaEnv, methodName);
	resultObj = (*javaEnv)->CallStaticObjectMethod(javaEnv, luajitjava_binding_class, luajitjava_run_method, containerObj, str, javaArgArray);
	(*javaEnv)->DeleteLocalRef(javaEnv, str);
	releasejavaArgs(javaEnv, javaArgArray);

	/* Handles exception */
	jobject jstr = checkException(javaEnv);
	if (jstr) {
		const char * cStr;
		cStr = (*javaEnv)->GetStringUTFChars(javaEnv, jstr, NULL);
		fprintf(stderr, "Error. exception while getting method of object : %s\n", cStr);
		(*javaEnv)->ReleaseStringUTFChars(javaEnv, jstr, cStr);
		(*javaEnv)->DeleteLocalRef(javaEnv, jstr);
		return NULL;
	}

	if (resultObj != NULL && !(*javaEnv)->IsSameObject(javaEnv, resultObj, NULL)) {
		returnObject = malloc(sizeof(ljJavaObject_t));
		returnObject->ljEnv = objectInterface->ljEnv;
		returnObject->object = (*javaEnv)->NewGlobalRef(javaEnv, resultObj);
		(*javaEnv)->DeleteLocalRef(javaEnv, resultObj);
		return returnObject;
	}
	return NULL;
}

int internal_javaGetObjectType(ljJavaObject_t* objectInterface) {
	JNIEnv * javaEnv;
	jobject object;
	jclass objectClass;
	int returnValue = JTYPE_NONE;

	javaEnv = ((ljJavaEnvironment_t*)objectInterface->ljEnv)->javaEnv;
	(*javaEnv)->ExceptionClear(javaEnv);
	object = (jobject)objectInterface->object;

	objectClass = (*javaEnv)->GetObjectClass(javaEnv, object);
	/* Handles exception */
	jobject jstr = checkException(javaEnv);
	if (jstr) {
		const char * cStr;
		cStr = (*javaEnv)->GetStringUTFChars(javaEnv, jstr, NULL);
		fprintf(stderr, "Error. exception while getting method of object : %s\n", cStr);
		(*javaEnv)->ReleaseStringUTFChars(javaEnv, jstr, cStr);
		(*javaEnv)->DeleteLocalRef(javaEnv, jstr);
		return returnValue;
	}
	if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_byte_class)) {
		returnValue = JTYPE_BYTE;
	} else if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_short_class)) {
		returnValue = JTYPE_SHORT;
	}
	if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_int_class)) {
		returnValue = JTYPE_INT;
	} else if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_long_class)) {
		returnValue = JTYPE_LONG;
	} else if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_float_class)) {
		returnValue = JTYPE_FLOAT;
	} else if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_double_class)) {
		returnValue = JTYPE_DOUBLE;
	} else if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_boolean_class)) {
		returnValue = JTYPE_BOOLEAN;
	} else if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_string_class)) {
		returnValue = JTYPE_STRING;
	} else {
		returnValue = JTYPE_OBJECT;
	}
	(*javaEnv)->DeleteLocalRef(javaEnv, objectClass);
	return returnValue;
}

int internal_javaGetObjectIntValue(ljJavaObject_t* objectInterface) {
	JNIEnv * javaEnv = ((ljJavaEnvironment_t*)objectInterface->ljEnv)->javaEnv;
	switch (internal_javaGetObjectType(objectInterface)) {
	case JTYPE_BYTE:
		return (*javaEnv)->CallIntMethod(javaEnv, objectInterface->object, java_byte_value);
		break;
	case JTYPE_SHORT:
		return (*javaEnv)->CallIntMethod(javaEnv, objectInterface->object, java_short_value);
		break;
	case JTYPE_INT:
		return (*javaEnv)->CallIntMethod(javaEnv, objectInterface->object, java_int_value);
		break;
	case JTYPE_BOOLEAN:
		return (*javaEnv)->CallBooleanMethod(javaEnv, objectInterface->object, java_boolean_value);
	}
	fprintf(stderr, "Trying to access int value of a non int type\n");
	return 0;
}
long internal_javaGetObjectLongValue(ljJavaObject_t* objectInterface) {
	JNIEnv * javaEnv = ((ljJavaEnvironment_t*)objectInterface->ljEnv)->javaEnv;
	if (internal_javaGetObjectType(objectInterface) == JTYPE_LONG) {
		return (long) (*javaEnv)->CallLongMethod(javaEnv, objectInterface->object, java_long_value);
	}
	fprintf(stderr, "Trying to access int value of a non int type\n");
	return 0;
}
float internal_javaGetObjectFloatValue(ljJavaObject_t* objectInterface) {
	JNIEnv * javaEnv = ((ljJavaEnvironment_t*)objectInterface->ljEnv)->javaEnv;
	if (internal_javaGetObjectType(objectInterface) == JTYPE_FLOAT) {
		return (*javaEnv)->CallFloatMethod(javaEnv, objectInterface->object, java_float_value);
	}
	fprintf(stderr, "Trying to access float value of a non float type\n");
	return 0;
}
double internal_javaGetObjectDoubleValue(ljJavaObject_t* objectInterface) {
	JNIEnv * javaEnv = ((ljJavaEnvironment_t*)objectInterface->ljEnv)->javaEnv;
	if (internal_javaGetObjectType(objectInterface) == JTYPE_DOUBLE) {
		return (*javaEnv)->CallDoubleMethod(javaEnv, objectInterface->object, java_double_value);
	}
	fprintf(stderr, "Trying to access double value of a non double type\n");
	return 0;
}
const char* internal_javaGetObjectStringValue(ljJavaObject_t* objectInterface) {
	JNIEnv * javaEnv = ((ljJavaEnvironment_t*)objectInterface->ljEnv)->javaEnv;
	if (internal_javaGetObjectType(objectInterface) == JTYPE_STRING) {
		return (*javaEnv)->GetStringUTFChars(javaEnv, (jstring)objectInterface->object, NULL);
	}
	fprintf(stderr, "Trying to access string value of a non string type\n");
	return NULL;
}
void internal_javaReleaseStringValue(ljJavaObject_t* objectInterface, const char* stringValue) {
	JNIEnv * javaEnv = ((ljJavaEnvironment_t*)objectInterface->ljEnv)->javaEnv;
	(*javaEnv)->ReleaseStringUTFChars(javaEnv, (jstring)objectInterface->object, stringValue);
}

/***************************************************************
      JAVA THREAD MANAGEMENT
****************************************************************/

static HANDLE javaThreadHandle;
static HANDLE javaCallEvent;
static HANDLE javaCallTreatedEvent;
static long javaThreadTimeout = 5000L;

typedef enum javaCallMethod {
	JAVACALL_METHOD_NONE,
	JAVACALL_METHOD_ENDJAVA,
	JAVACALL_METHOD_BINDCLASS,
	JAVACALL_METHOD_RELEASECLASS,
	JAVACALL_METHOD_NEW,
	JAVACALL_METHOD_RELEASEOBJECT,
	JAVACALL_METHOD_CHECKCLASSFIELD,
	JAVACALL_METHOD_RUNCLASSMETHOD,
	JAVACALL_METHOD_CHECKOBJECTFIELD,
	JAVACALL_METHOD_RUNOBJECTMETHOD,
	JAVACALL_METHOD_GETOBJECTTYPE,
	JAVACALL_METHOD_GETOBJECTINTVALUE,
	JAVACALL_METHOD_GETOBJECTLONGVALUE,
	JAVACALL_METHOD_GETOBJECTFLOATVALUE,
	JAVACALL_METHOD_GETOBJECTDOUBLEVALUE,
	JAVACALL_METHOD_GETOBJECTSTRINGVALUE,
	JAVACALL_METHOD_RELEASESTRINGVALUE
} javaCallMethod_t;

static void* currentCallData;
static void* currentCallReturnPointer;
static int currentCallReturnInt;
static long currentCallReturnLong;
static float currentCallReturnFloat;
static double currentCallReturnDouble;
static javaCallMethod_t currentCallMethod = JAVACALL_METHOD_NONE;

//static params for each call
typedef struct javaStart_params {
	const char* classPath;
} javaStart_params_t;
typedef struct javaBindClass_params {
	ljJavaClass_t* classInterface;
	const char* className;
} javaBindClass_params_t;
typedef struct javaNew_params {
	ljJavaObject_t* objectInterface;
	ljJavaClass_t* classInterface;
	int nArgs;
	va_list valist;
} javaNew_params_t;
typedef struct javaCheckClassField_params {
	ljJavaClass_t* classInterface;
	const char* key;
} javaCheckClassField_params_t;
typedef struct javaRunClassMethod_params {
	ljJavaClass_t* classInterface;
	const char* methodName;
	int nArgs;
	va_list valist;
} javaRunClassMethod_params_t;
typedef struct javaCheckObjectField_params {
	ljJavaObject_t* objectInterface;
	const char* key;
} javaCheckObjectField_params_t;
typedef struct javaRunObjectMethod_params {
	ljJavaObject_t* objectInterface;
	const char* methodName;
	int nArgs;
	va_list valist;
} javaRunObjectMethod_params_t;
typedef struct javaReleaseStringValue_params {
	ljJavaObject_t* objectInterface;
	const char* stringValue;
} javaReleaseStringValue_params_t;


//thread launching the JVM, treating calls and ending the jvm
DWORD WINAPI javaThread(LPVOID args)
{
	void* ljJavaEnv;

	//start java environment
	ljJavaEnv = internal_javaStart( ((javaStart_params_t*)currentCallData)->classPath);
	currentCallReturnPointer = ljJavaEnv;
	SetEvent(javaCallTreatedEvent);//will

	BOOL loop = TRUE;
	while (loop) {
		WaitForSingleObject(javaCallEvent, javaThreadTimeout);

		switch (currentCallMethod) {
		case JAVACALL_METHOD_BINDCLASS:
			currentCallReturnInt = internal_javaBindClass(
				((javaBindClass_params_t*)currentCallData)->classInterface,
				((javaBindClass_params_t*)currentCallData)->className );
			break;
		case JAVACALL_METHOD_RELEASECLASS:
			internal_javaReleaseClass((ljJavaClass_t*)currentCallData);
			break;
		case JAVACALL_METHOD_NEW:
			currentCallReturnInt = internal_javaNew(
				((javaNew_params_t*)currentCallData)->objectInterface,
				((javaNew_params_t*)currentCallData)->classInterface,
				((javaNew_params_t*)currentCallData)->nArgs,
				((javaNew_params_t*)currentCallData)->valist);
			break;
		case JAVACALL_METHOD_RELEASEOBJECT:
			internal_javaReleaseObject((ljJavaObject_t*)currentCallData);
			break;
		case JAVACALL_METHOD_CHECKCLASSFIELD:
			currentCallReturnPointer = internal_javaCheckClassField(
				((javaCheckClassField_params_t*)currentCallData)->classInterface,
				((javaCheckClassField_params_t*)currentCallData)->key);
			break;
		case JAVACALL_METHOD_RUNCLASSMETHOD:
			currentCallReturnPointer = internal_javaRunClassMethod(
				((javaRunClassMethod_params_t*)currentCallData)->classInterface,
				((javaRunClassMethod_params_t*)currentCallData)->methodName,
				((javaRunClassMethod_params_t*)currentCallData)->nArgs,
				((javaRunClassMethod_params_t*)currentCallData)->valist);
			break;
		case JAVACALL_METHOD_CHECKOBJECTFIELD:
			currentCallReturnPointer = internal_javaCheckObjectField(
				((javaCheckObjectField_params_t*)currentCallData)->objectInterface,
				((javaCheckObjectField_params_t*)currentCallData)->key);
			break;
		case JAVACALL_METHOD_RUNOBJECTMETHOD:
			currentCallReturnPointer = internal_javaRunObjectMethod(
				((javaRunObjectMethod_params_t*)currentCallData)->objectInterface,
				((javaRunObjectMethod_params_t*)currentCallData)->methodName,
				((javaRunObjectMethod_params_t*)currentCallData)->nArgs,
				((javaRunObjectMethod_params_t*)currentCallData)->valist);
			break;
			case JAVACALL_METHOD_GETOBJECTTYPE:
				currentCallReturnInt = internal_javaGetObjectType((ljJavaObject_t*)currentCallData);
				break;
			case JAVACALL_METHOD_GETOBJECTINTVALUE:
				currentCallReturnInt = internal_javaGetObjectIntValue((ljJavaObject_t*)currentCallData);
				break;
			case JAVACALL_METHOD_GETOBJECTLONGVALUE:
				currentCallReturnLong = internal_javaGetObjectLongValue((ljJavaObject_t*)currentCallData);
				break;
			case JAVACALL_METHOD_GETOBJECTFLOATVALUE:
				currentCallReturnFloat = internal_javaGetObjectFloatValue((ljJavaObject_t*)currentCallData);
				break;
			case JAVACALL_METHOD_GETOBJECTDOUBLEVALUE:
				currentCallReturnDouble = internal_javaGetObjectDoubleValue((ljJavaObject_t*)currentCallData);
				break;
			case JAVACALL_METHOD_GETOBJECTSTRINGVALUE:
				currentCallReturnPointer = (void*) internal_javaGetObjectStringValue((ljJavaObject_t*)currentCallData);
				break;
			case JAVACALL_METHOD_RELEASESTRINGVALUE:
				internal_javaReleaseStringValue(
					((javaReleaseStringValue_params_t*)currentCallData)->objectInterface,
					((javaReleaseStringValue_params_t*)currentCallData)->stringValue);
				break;

		}
		if (currentCallMethod == JAVACALL_METHOD_ENDJAVA) {
			loop = FALSE;
		}
		else {
			SetEvent(javaCallTreatedEvent);
		}
	}
	fprintf(stdout, "thread exiting\n");

	//stop java
	internal_javaEnd(ljJavaEnv);

	//cleanup
	CloseHandle(javaCallEvent);
	CloseHandle(javaCallTreatedEvent);

	fprintf(stdout, "thread over\n");

	return 1;
}


/***************************************************************
		LUA CALLED METHODS
****************************************************************/

/*
// start the java thread and init bindings
void* javaStart(const char* classPath) {
	// create an event for java calls in the jvm thread
	javaCallEvent = CreateEvent(
		NULL,               // default security attributes
		FALSE,               // event will be reset after it unblocks the treatment
		FALSE,              // initial state is nonsignaled
		TEXT("JavaCallEvent")  // name
		);
	// create an event for java treament over coming from jvm thread
	javaCallTreatedEvent = CreateEvent(
		NULL,               // default security attributes
		FALSE,               // event will be reset after the lua call thread has been unblocked
		FALSE,              // initial state is nonsignaled
		TEXT("JavaCallTreatedEvent")  // name
		);

	javaStart_params_t* java_params = malloc(sizeof(javaStart_params_t));
	java_params->classPath = classPath;
	currentCallData = java_params;
	javaThreadHandle = CreateThread(0, 0,
		javaThread, 0, 0, 0);

	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);
	//clean params
	free(java_params);
	currentCallData = NULL;

	return currentCallReturnPointer;
}

void javaEnd(void* ljEnv)
{
	currentCallMethod = JAVACALL_METHOD_ENDJAVA;
	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);
}

int javaBindClass(ljJavaClass_t* classInterface, const char* className) {
	currentCallMethod = JAVACALL_METHOD_BINDCLASS;
	javaBindClass_params_t* java_params = malloc(sizeof(javaBindClass_params_t));
	java_params->classInterface = classInterface;
	java_params->className = className;
	currentCallData = java_params;

	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);

	//clean params
	free(java_params);
	currentCallData = NULL;

	return currentCallReturnInt;
}

void javaReleaseClass(ljJavaClass_t* classInterface) {
	currentCallMethod = JAVACALL_METHOD_RELEASECLASS;
	currentCallData = classInterface;

	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);

	//clean params
	currentCallData = NULL;
}

int javaNew(ljJavaObject_t* objectInterface, ljJavaClass_t* classInterface, int nArgs, ...) {
	va_list valist;
	va_start(valist, nArgs);

	currentCallMethod = JAVACALL_METHOD_NEW;
	javaNew_params_t* java_params = malloc(sizeof(javaNew_params_t));
	java_params->objectInterface = objectInterface;
	java_params->classInterface = classInterface;
	java_params->nArgs = nArgs;
	java_params->valist = valist;
	currentCallData = java_params;

	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);

	//clean params
	free(java_params);
	currentCallData = NULL;

	return currentCallReturnInt;
}

void javaReleaseObject(ljJavaObject_t* objectInterface) {
	currentCallMethod = JAVACALL_METHOD_RELEASEOBJECT;
	currentCallData = objectInterface;

	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);

	//clean params
	currentCallData = NULL;
}

ljJavaObject_t* javaCheckClassField(ljJavaClass_t* classInterface, const char * key) {
	currentCallMethod = JAVACALL_METHOD_CHECKCLASSFIELD;
	javaCheckClassField_params_t* java_params = malloc(sizeof(javaCheckClassField_params_t));
	java_params->classInterface = classInterface;
	java_params->key = key;
	currentCallData = java_params;

	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);

	//clean params
	free(java_params);
	currentCallData = NULL;

	return currentCallReturnPointer;
}
ljJavaObject_t* javaRunClassMethod(ljJavaClass_t* classInterface, const char * methodName, int nArgs, ...) {
	va_list valist;
	va_start(valist, nArgs);

	currentCallMethod = JAVACALL_METHOD_RUNCLASSMETHOD;
	javaRunClassMethod_params_t* java_params = malloc(sizeof(javaRunClassMethod_params_t));
	java_params->classInterface = classInterface;
	java_params->methodName = methodName;
	java_params->nArgs = nArgs;
	java_params->valist = valist;
	currentCallData = java_params;

	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);

	//clean params
	free(java_params);
	currentCallData = NULL;

	return currentCallReturnPointer;
}

ljJavaObject_t* javaCheckObjectField(ljJavaObject_t* objectInterface, const char * key) {
	currentCallMethod = JAVACALL_METHOD_CHECKOBJECTFIELD;
	javaCheckObjectField_params_t* java_params = malloc(sizeof(javaCheckObjectField_params_t));
	java_params->objectInterface = objectInterface;
	java_params->key = key;
	currentCallData = java_params;

	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);

	//clean params
	free(java_params);
	currentCallData = NULL;

	return currentCallReturnPointer;
}
ljJavaObject_t* javaRunObjectMethod(ljJavaObject_t* objectInterface, const char * methodName, int nArgs, ...) {
	va_list valist;
	va_start(valist, nArgs);

	currentCallMethod = JAVACALL_METHOD_RUNOBJECTMETHOD;
	javaRunObjectMethod_params_t* java_params = malloc(sizeof(javaRunObjectMethod_params_t));
	java_params->objectInterface = objectInterface;
	java_params->methodName = methodName;
	java_params->nArgs = nArgs;
	java_params->valist = valist;
	currentCallData = java_params;

	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);

	//clean params
	free(java_params);
	currentCallData = NULL;

	return currentCallReturnPointer;
}

int javaGetObjectType(ljJavaObject_t* objectInterface) {
	currentCallMethod = JAVACALL_METHOD_GETOBJECTTYPE;
	currentCallData = objectInterface;

	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);

	//clean params
	currentCallData = NULL;

	return currentCallReturnInt;
}
int javaGetObjectIntValue(ljJavaObject_t* objectInterface) {
	currentCallMethod = JAVACALL_METHOD_GETOBJECTINTVALUE;
	currentCallData = objectInterface;

	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);

	//clean params
	currentCallData = NULL;

	return currentCallReturnInt;
}
long javaGetObjectLongValue(ljJavaObject_t* objectInterface) {
	currentCallMethod = JAVACALL_METHOD_GETOBJECTLONGVALUE;
	currentCallData = objectInterface;

	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);

	//clean params
	currentCallData = NULL;

	return currentCallReturnLong;
}
float javaGetObjectFloatValue(ljJavaObject_t* objectInterface) {
	currentCallMethod = JAVACALL_METHOD_GETOBJECTFLOATVALUE;
	currentCallData = objectInterface;

	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);

	//clean params
	currentCallData = NULL;

	return currentCallReturnFloat;
}
double javaGetObjectDoubleValue(ljJavaObject_t* objectInterface) {
	currentCallMethod = JAVACALL_METHOD_GETOBJECTDOUBLEVALUE;
	currentCallData = objectInterface;

	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);

	//clean params
	currentCallData = NULL;

	return currentCallReturnDouble;
}
const char* javaGetObjectStringValue(ljJavaObject_t* objectInterface) {
	currentCallMethod = JAVACALL_METHOD_GETOBJECTSTRINGVALUE;
	currentCallData = objectInterface;

	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);

	//clean params
	currentCallData = NULL;

	return currentCallReturnPointer;
}
void javaReleaseStringValue(ljJavaObject_t* objectInterface, const char* stringValue) {
	currentCallMethod = JAVACALL_METHOD_RELEASESTRINGVALUE;
	javaReleaseStringValue_params_t* java_params = malloc(sizeof(javaReleaseStringValue_params_t));
	java_params->objectInterface = objectInterface;
	java_params->stringValue = stringValue;
	currentCallData = java_params;

	SetEvent(javaCallEvent);
	WaitForSingleObject(javaCallTreatedEvent, javaThreadTimeout);

	//clean params
	currentCallData = NULL;
}
*/

void* javaStart(const char* classPath) {
	return internal_javaStart(classPath);
}

void javaEnd(void* ljEnv)
{
	internal_javaEnd(ljEnv);
}

int javaBindClass(ljJavaClass_t* classInterface, const char* className) {
	return internal_javaBindClass(classInterface, className);
}

void javaReleaseClass(ljJavaClass_t* classInterface) {
	internal_javaReleaseClass(classInterface);
}

int javaNew(ljJavaObject_t* objectInterface, ljJavaClass_t* classInterface, int nArgs, ...) {
	va_list valist;
	va_start(valist, nArgs);

	return internal_javaNew(objectInterface, classInterface, nArgs, valist);
}

void javaReleaseObject(ljJavaObject_t* objectInterface) {
	internal_javaReleaseObject(objectInterface);
}

ljJavaObject_t* javaCheckClassField(ljJavaClass_t* classInterface, const char * key) {
	return internal_javaCheckClassField(classInterface, key);
}
ljJavaObject_t* javaRunClassMethod(ljJavaClass_t* classInterface, const char * methodName, int nArgs, ...) {
	va_list valist;
	va_start(valist, nArgs);

	return internal_javaRunClassMethod(classInterface, methodName, nArgs, valist);
}

ljJavaObject_t* javaCheckObjectField(ljJavaObject_t* objectInterface, const char * key) {
	return internal_javaCheckObjectField(objectInterface, key);
}
ljJavaObject_t* javaRunObjectMethod(ljJavaObject_t* objectInterface, const char * methodName, int nArgs, ...) {
	va_list valist;
	va_start(valist, nArgs);

	return internal_javaRunObjectMethod(objectInterface, methodName, nArgs, valist);
}

int javaGetObjectType(ljJavaObject_t* objectInterface) {
	return internal_javaGetObjectType(objectInterface);
}
int javaGetObjectIntValue(ljJavaObject_t* objectInterface) {
	return internal_javaGetObjectIntValue(objectInterface);
}
long javaGetObjectLongValue(ljJavaObject_t* objectInterface) {
	return internal_javaGetObjectLongValue(objectInterface);
}
float javaGetObjectFloatValue(ljJavaObject_t* objectInterface) {
	return internal_javaGetObjectFloatValue(objectInterface);
}
double javaGetObjectDoubleValue(ljJavaObject_t* objectInterface) {
	return internal_javaGetObjectDoubleValue(objectInterface);
}
const char* javaGetObjectStringValue(ljJavaObject_t* objectInterface) {
	return internal_javaGetObjectStringValue(objectInterface);
}
void javaReleaseStringValue(ljJavaObject_t* objectInterface, const char* stringValue) {
	internal_javaReleaseStringValue(objectInterface, stringValue);
}

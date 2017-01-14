
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

#include "luajitjava.h"

//set of useful java classes we bind with at init for convenience
// including the luajitjava binding class
static jclass    throwable_class = NULL;
static jmethodID get_message_method = NULL;
static jclass    java_function_class = NULL;
static jmethodID java_function_method = NULL;
static jclass    luajitjava_binding_class = NULL;
static jclass    java_lang_class = NULL;
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
		jstr = (*javaEnv)->CallObjectMethod(javaEnv, exp, get_message_method);

		if (jstr == NULL)
		{
			jmethodID methodId;

			methodId = (*javaEnv)->GetMethodID(javaEnv, throwable_class, "toString", "()Ljava/lang/String;");
			jstr = (*javaEnv)->CallObjectMethod(javaEnv, exp, methodId);
		}

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
	jclass tempClass;

	tempClass = (*env)->FindClass(env, "developpeur2000/luajitjava/LuaJitJavaAPI");
	if (tempClass == NULL)
	{
		fprintf(stderr, "Could not find LuaJitJavaAPI class\n");
		return 0;
	}
	if ((luajitjava_binding_class = (*env)->NewGlobalRef(env, tempClass)) == NULL)
	{
		fprintf(stderr, "Could not bind to LuaJitJavaAPI class\n");
		return 0;
	}

	tempClass = (*env)->FindClass(env, "java/lang/Throwable");
	if (tempClass == NULL)
	{
		fprintf(stderr, "Error. Couldn't bind java class java.lang.Throwable\n");
		return 0;
	}
	throwable_class = (*env)->NewGlobalRef(env, tempClass);
	if (throwable_class == NULL)
	{
		fprintf(stderr, "Error. Couldn't bind java class java.lang.Throwable\n");
		return 0;
	}
	get_message_method = (*env)->GetMethodID(env, throwable_class, "getMessage",
		"()Ljava/lang/String;");
	if (get_message_method == NULL)
	{
		fprintf(stderr, "Could not find <getMessage> method in java.lang.String\n");
		return 0;
	}

	tempClass = (*env)->FindClass(env, "java/lang/Class");
	if (tempClass == NULL)
	{
		fprintf(stderr, "Error. Coundn't bind java class java.lang.Class\n");
		return 0;
	}
	java_lang_class = (*env)->NewGlobalRef(env, tempClass);
	if (java_lang_class == NULL)
	{
		fprintf(stderr, "Error. Couldn't bind java class java.lang.Class\n");
		return 0;
	}

	tempClass = (*env)->FindClass(env, "java/lang/Object");
	if (tempClass == NULL)
	{
		fprintf(stderr, "Error. Coundn't bind java class java.lang.Object\n");
		return 0;
	}
	java_lang_object = (*env)->NewGlobalRef(env, tempClass);
	if (java_lang_object == NULL)
	{
		fprintf(stderr, "Error. Couldn't bind java class java.lang.Object\n");
		return 0;
	}

	java_byte_class = (*env)->FindClass(env, "java/lang/Byte");
	if (java_byte_class == NULL)
	{
		fprintf(stderr, "Error. Coundn't bind java class java.lang.Byte\n");
		return 0;
	}
	java_new_byte = (*env)->GetMethodID(env, java_byte_class, "<init>", "(B)V");
	if (!java_new_byte)
	{
		fprintf(stderr, "Could not find constructor method of java.lang.Byte\n");
		return 0;
	}
	java_byte_value = (*env)->GetMethodID(env, java_byte_class, "intValue", "()I");
	if (!java_byte_value)
	{
		fprintf(stderr, "Could not find value method of java.lang.Byte\n");
		return 0;
	}
	java_short_class = (*env)->FindClass(env, "java/lang/Short");
	if (java_short_class == NULL)
	{
		fprintf(stderr, "Error. Coundn't bind java class java.lang.Short\n");
		return 0;
	}
	java_new_short = (*env)->GetMethodID(env, java_short_class, "<init>", "(S)V");
	if (!java_new_short)
	{
		fprintf(stderr, "Could not find constructor method of java.lang.Short\n");
		return 0;
	}
	java_short_value = (*env)->GetMethodID(env, java_short_class, "intValue", "()I");
	if (!java_short_value)
	{
		fprintf(stderr, "Could not find value method of java.lang.Short\n");
		return 0;
	}
	java_int_class = (*env)->FindClass(env, "java/lang/Integer");
	if (java_int_class == NULL)
	{
		fprintf(stderr, "Error. Coundn't bind java class java.lang.Integer\n");
		return 0;
	}
	java_new_int = (*env)->GetMethodID(env, java_int_class, "<init>", "(I)V");
	if (!java_new_int)
	{
		fprintf(stderr, "Could not find constructor method of java.lang.Integer\n");
		return 0;
	}
	java_int_value = (*env)->GetMethodID(env, java_int_class, "intValue", "()I");
	if (!java_int_value)
	{
		fprintf(stderr, "Could not find value method of java.lang.Integer\n");
		return 0;
	}
	java_long_class = (*env)->FindClass(env, "java/lang/Long");
	if (java_long_class == NULL)
	{
		fprintf(stderr, "Error. Coundn't bind java class java.lang.Long\n");
		return 0;
	}
	java_new_long = (*env)->GetMethodID(env, java_long_class, "<init>", "(J)V");
	if (!java_new_long)
	{
		fprintf(stderr, "Could not find constructor method of java.lang.Long\n");
		return 0;
	}
	java_long_value = (*env)->GetMethodID(env, java_long_class, "longValue", "()J");
	if (!java_long_value)
	{
		fprintf(stderr, "Could not find value method of java.lang.Long\n");
		return 0;
	}
	java_float_class = (*env)->FindClass(env, "java/lang/Float");
	if (java_float_class == NULL)
	{
		fprintf(stderr, "Error. Coundn't bind java class java.lang.Float\n");
		return 0;
	}
	java_new_float = (*env)->GetMethodID(env, java_float_class, "<init>", "(F)V");
	if (!java_new_float)
	{
		fprintf(stderr, "Could not find constructor method of java.lang.Float\n");
		return 0;
	}
	java_float_value = (*env)->GetMethodID(env, java_float_class, "floatValue", "()F");
	if (!java_float_value)
	{
		fprintf(stderr, "Could not find value method of java.lang.Float\n");
		return 0;
	}
	java_double_class = (*env)->FindClass(env, "java/lang/Double");
	if (java_double_class == NULL)
	{
		fprintf(stderr, "Error. Coundn't bind java class java.lang.Double\n");
		return 0;
	}
	java_new_double = (*env)->GetMethodID(env, java_double_class, "<init>", "(D)V");
	if (!java_new_double)
	{
		fprintf(stderr, "Could not find constructor method of java.lang.Double\n");
		return 0;
	}
	java_double_value = (*env)->GetMethodID(env, java_double_class, "doubleValue", "()D");
	if (!java_double_value)
	{
		fprintf(stderr, "Could not find value method of java.lang.Double\n");
		return 0;
	}
	java_boolean_class = (*env)->FindClass(env, "java/lang/Boolean");
	if (java_boolean_class == NULL)
	{
		fprintf(stderr, "Error. Coundn't bind java class java.lang.Boolean\n");
		return 0;
	}
	java_new_boolean = (*env)->GetMethodID(env, java_boolean_class, "<init>", "(Z)V");
	if (!java_new_boolean)
	{
		fprintf(stderr, "Could not find constructor method of java.lang.Boolean\n");
		return 0;
	}
	java_boolean_value = (*env)->GetMethodID(env, java_boolean_class, "booleanValue", "()Z");
	if (!java_boolean_value)
	{
		fprintf(stderr, "Could not find value method of java.lang.Boolean\n");
		return 0;
	}
	java_boolean_class = (*env)->FindClass(env, "java/lang/Boolean");
	java_string_class = (*env)->FindClass(env, "java/lang/String");
	if (java_string_class == NULL)
	{
		fprintf(stderr, "Error. Coundn't bind java class java.lang.String\n");
		return 0;
	}

	return 1;
}

// lua called method to init the bindings and get the java environment
void* javaStart(const char* classPath)
{
	JNIEnv *env;
	JavaVM *jvm;
	jclass tempClass = NULL;

	env = create_vm(&jvm, classPath);
	if (env == NULL)
	{
		fprintf(stderr, "\n Unable to create java VM");
		return NULL;
	}

	//store class loader
	jclass threadClass = (*env)->FindClass(env, "java/lang/Thread");
	if (!threadClass) {
		fprintf(stderr, "Could not find Thread class\n");
		return NULL;
	}
	jmethodID currentThreadMethod = (*env)->GetStaticMethodID(env, threadClass, "currentThread",
		"()Ljava/lang/Thread;");
	if (!threadClass) {
		fprintf(stderr, "Could not find Thread currentThread method\n");
		return NULL;
	}
	jobject currentThread = (*env)->CallStaticObjectMethod(env, threadClass, currentThreadMethod);
	if (!threadClass) {
		fprintf(stderr, "Could not get current Thread object\n");
		return NULL;
	}
	jmethodID getContextClassLoaderMethod = (*env)->GetMethodID(env, threadClass, "getContextClassLoader",
		"()Ljava/lang/ClassLoader;");
	if (!threadClass) {
		fprintf(stderr, "Could not find getContextLoader method\n");
		return NULL;
	}
	jobject javaContextClassLoader = (*env)->CallObjectMethod(env, currentThread, getContextClassLoaderMethod);
	if (javaContextClassLoader == NULL)
	{
		fprintf(stderr, "Could not get classLoader\n");
		return NULL;
	}
	java_class_loader = (*env)->NewGlobalRef(env, javaContextClassLoader);


	if (bindJavaBaseLinks(env) == 0) {
		fprintf(stderr, "\n Unable to bind with java\n");
		return NULL;
	}

	return (void*) env;
}

// lua called method to get a java class handle
int javaBindClass(ljJavaClass_t* classInterface, const char* className)
{
	jmethodID method;
	jstring javaClassName;
	jobject classInstance;
	JNIEnv * javaEnv;

	javaEnv = classInterface->javaEnv;
	(*javaEnv)->ExceptionClear(javaEnv);

	method = (*javaEnv)->GetStaticMethodID(javaEnv, java_lang_class, "forName",
		"(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;");
	if (method == NULL) {
		fprintf(stderr, "Unable to find the Class.forName method\n");
	}

	javaClassName = (*javaEnv)->NewStringUTF(javaEnv, className);

	classInstance = (*javaEnv)->CallStaticObjectMethod(javaEnv, java_lang_class,
		method, javaClassName, JNI_TRUE, java_class_loader);

	(*javaEnv)->DeleteLocalRef(javaEnv, javaClassName);

	jobject jstr = checkException(javaEnv);
	if (jstr) {
		const char * cStr;
		cStr = (*javaEnv)->GetStringUTFChars(javaEnv, jstr, NULL);
		fprintf(stderr, "Error. Couldn't bind java class %s : %s\n", className, cStr);
		(*javaEnv)->ReleaseStringUTFChars(javaEnv, jstr, cStr);
		return 0;
	}

	if (classInstance == NULL) {
		fprintf(stderr, "Error. Couldn't bind java class %s : unknown error\n", className);
	}

	classInterface->classObject = (*javaEnv)->NewGlobalRef(javaEnv, classInstance);

	return 1;
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

// lua called method to instantiate an object from a specified class,
//  using provided constructor arguments
int javaNew(ljJavaObject_t* objectInterface, ljJavaClass_t* classInterface, int nArgs, ...)
{
	jobject newObject;
	jclass clazz;
	jmethodID method;
	jobject classInstance;
	JNIEnv * javaEnv;

	javaEnv = classInterface->javaEnv;
	(*javaEnv)->ExceptionClear(javaEnv);

	//get java params from args
	va_list valist;
	va_start(valist, nArgs);
	jobjectArray javaArgArray = getjavaArgs(javaEnv, nArgs, valist);

	//check given class interface
	clazz = (*javaEnv)->FindClass(javaEnv, "java/lang/Class");
	if (clazz == NULL)
	{
		fprintf(stderr, "Could not find class java.lang.Class\n");
		return 0;
	}
	classInstance = (jobject) classInterface->classObject;
	if (classInstance == NULL)
	{
		fprintf(stderr, "given class instance is null !\n");
		return 0;
	}
	if ((*javaEnv)->IsInstanceOf(javaEnv, classInstance, clazz) == JNI_FALSE)
	{
		fprintf(stderr, "Class interface does not seem to contain a class instance\n");
		return 0;
	}

	//create new object through our binding java class
	method = (*javaEnv)->GetStaticMethodID(javaEnv, luajitjava_binding_class, "javaNew",
		"(Ljava/lang/Class;[Ljava/lang/Object;)Ljava/lang/Object;");
	if (method == NULL)
	{
		fprintf(stderr, "Could not find binding method LuaJitJavaAPI.javaNew\n");
		return 0;
	}

	newObject = (*javaEnv)->CallStaticObjectMethod(javaEnv, luajitjava_binding_class, method, classInstance, javaArgArray);

	/* Handles exception */
	jobject jstr = checkException(javaEnv);
	if (jstr) {
		const char * cStr;
		cStr = (*javaEnv)->GetStringUTFChars(javaEnv, jstr, NULL);
		fprintf(stderr, "Error. Couldn't create object : %s\n", cStr);
		(*javaEnv)->ReleaseStringUTFChars(javaEnv, jstr, cStr);
		return 0;
	}

	if (newObject == NULL)
	{
		fprintf(stderr, "Error. Couldn't create object : unknown reason\n");
	}

	objectInterface->object = (*javaEnv)->NewGlobalRef(javaEnv, newObject);
	return 1;
}

// lua called method to look for a static field in a class
ljJavaObject_t* checkClassField(ljJavaClass_t* classInterface, const char * key)
{
	JNIEnv * javaEnv;
	jmethodID method;
	jclass containerClass;
	jstring str;
	jobject eventualField;
	ljJavaObject_t* returnObject;

	javaEnv = (JNIEnv *)classInterface->javaEnv;
	(*javaEnv)->ExceptionClear(javaEnv);
	containerClass = (jobject)classInterface->classObject;

	method = (*javaEnv)->GetStaticMethodID(javaEnv, luajitjava_binding_class, "checkField",
		"(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;");
	str = (*javaEnv)->NewStringUTF(javaEnv, key);
	eventualField = (*javaEnv)->CallStaticObjectMethod(javaEnv, luajitjava_binding_class, method, containerClass, str);
	(*javaEnv)->DeleteLocalRef(javaEnv, str);
	/* Handles exception */
	jobject jstr = checkException(javaEnv);
	if (jstr) {
		const char * cStr;
		cStr = (*javaEnv)->GetStringUTFChars(javaEnv, jstr, NULL);
		fprintf(stderr, "Error. excpetion while getting index of object : %s\n", cStr);
		(*javaEnv)->ReleaseStringUTFChars(javaEnv, jstr, cStr);
		return 0;
	}

	if (eventualField != NULL) {
		returnObject = malloc(sizeof(ljJavaObject_t));
		returnObject->javaEnv = (void*)javaEnv;
		returnObject->object = (*javaEnv)->NewGlobalRef(javaEnv, eventualField);
		return returnObject;
	}
	return NULL;
}

// lua called method to look for a static method in a class corresponding to provided args
//  then run it and return the object result
ljJavaObject_t* runClassMethod(ljJavaClass_t* classInterface, const char * methodName, int nArgs, ...)
{
	JNIEnv * javaEnv;
	jclass containerClass;
	jmethodID method;
	jstring str;
	jobject resultObj;
	ljJavaObject_t* returnObject;

	javaEnv = (JNIEnv *)classInterface->javaEnv;
	(*javaEnv)->ExceptionClear(javaEnv);
	containerClass = (jobject)classInterface->classObject;

	//get java params from args
	va_list valist;
	va_start(valist, nArgs);
	jobjectArray javaArgArray = getjavaArgs(javaEnv, nArgs, valist);

	/* Run method through our java proxy */
	method = (*javaEnv)->GetStaticMethodID(javaEnv, luajitjava_binding_class, "runMethod",
		"(Ljava/lang/Object;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
	if (method == NULL)
	{
		fprintf(stderr, "Could not find binding method LuaJavaAPI.runMethod\n");
		return NULL;
	}
	str = (*javaEnv)->NewStringUTF(javaEnv, methodName);
	resultObj = (*javaEnv)->CallStaticObjectMethod(javaEnv, luajitjava_binding_class, method, containerClass, str, javaArgArray);

	/* Handles exception */
	jobject jstr = checkException(javaEnv);
	if (jstr) {
		const char * cStr;
		cStr = (*javaEnv)->GetStringUTFChars(javaEnv, jstr, NULL);
		fprintf(stderr, "Error. exception while getting method of object : %s\n", cStr);
		(*javaEnv)->ReleaseStringUTFChars(javaEnv, jstr, cStr);
		return NULL;
	}

	if (resultObj != NULL) {
		returnObject = malloc(sizeof(ljJavaObject_t));
		returnObject->javaEnv = (void*)javaEnv;
		returnObject->object = (*javaEnv)->NewGlobalRef(javaEnv, resultObj);
		return returnObject;
	}
	return NULL;
}


// lua called method to look for a field in an object instance
ljJavaObject_t* checkObjectField(ljJavaObject_t* objectInterface, const char * key)
{
	JNIEnv * javaEnv;
	jmethodID method;
	jobject containerObj;
	jstring str;
	jobject eventualField;
	ljJavaObject_t* returnObject;

	javaEnv = (JNIEnv *)objectInterface->javaEnv;
	(*javaEnv)->ExceptionClear(javaEnv);
	containerObj = (jobject)objectInterface->object;

	method = (*javaEnv)->GetStaticMethodID(javaEnv, luajitjava_binding_class, "checkField",
		"(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;");
	str = (*javaEnv)->NewStringUTF(javaEnv, key);
	eventualField = (*javaEnv)->CallStaticObjectMethod(javaEnv, luajitjava_binding_class, method, containerObj, str);
	(*javaEnv)->DeleteLocalRef(javaEnv, str);
	/* Handles exception */
	jobject jstr = checkException(javaEnv);
	if (jstr) {
		const char * cStr;
		cStr = (*javaEnv)->GetStringUTFChars(javaEnv, jstr, NULL);
		fprintf(stderr, "Error. excpetion while getting index of object : %s\n", cStr);
		(*javaEnv)->ReleaseStringUTFChars(javaEnv, jstr, cStr);
		return 0;
	}

	if (eventualField != NULL) {
		returnObject = malloc(sizeof(ljJavaObject_t));
		returnObject->javaEnv = (void*) javaEnv;
		returnObject->object = (*javaEnv)->NewGlobalRef(javaEnv, eventualField);
		return returnObject;
	}
	return NULL;
}

// lua called method to look for a method in an object instance corresponding to provided args
//  then run it and return the object result
ljJavaObject_t* runObjectMethod(ljJavaObject_t* objectInterface, const char * methodName, int nArgs, ...)
{
	JNIEnv * javaEnv;
	jobject containerObj;
	jmethodID method;
	jstring str;
	jobject resultObj;
	ljJavaObject_t* returnObject;

	javaEnv = (JNIEnv *)objectInterface->javaEnv;
	(*javaEnv)->ExceptionClear(javaEnv);
	containerObj = (jobject)objectInterface->object;

	//get java params from args
	va_list valist;
	va_start(valist, nArgs);
	jobjectArray javaArgArray = getjavaArgs(javaEnv, nArgs, valist);

	/* Run method through our java proxy */
	method = (*javaEnv)->GetStaticMethodID(javaEnv, luajitjava_binding_class, "runMethod",
		"(Ljava/lang/Object;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
	if (method == NULL)
	{
		fprintf(stderr, "Could not find binding method LuaJavaAPI.runMethod\n");
		return NULL;
	}
	str = (*javaEnv)->NewStringUTF(javaEnv, methodName);
	resultObj = (*javaEnv)->CallStaticObjectMethod(javaEnv, luajitjava_binding_class, method, containerObj, str, javaArgArray);

	/* Handles exception */
	jobject jstr = checkException(javaEnv);
	if (jstr) {
		const char * cStr;
		cStr = (*javaEnv)->GetStringUTFChars(javaEnv, jstr, NULL);
		fprintf(stderr, "Error. exception while getting method of object : %s\n", cStr);
		(*javaEnv)->ReleaseStringUTFChars(javaEnv, jstr, cStr);
		return NULL;
	}

	if (resultObj != NULL && !(*javaEnv)->IsSameObject(javaEnv, resultObj, NULL)) {
		returnObject = malloc(sizeof(ljJavaObject_t));
		returnObject->javaEnv = (void*)javaEnv;
		returnObject->object = (*javaEnv)->NewGlobalRef(javaEnv, resultObj);
		return returnObject;
	}
	return NULL;
}

int getObjectType(ljJavaObject_t* objectInterface) {
	JNIEnv * javaEnv;
	jobject object;
	jclass objectClass;

	javaEnv = (JNIEnv *)objectInterface->javaEnv;
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
		return JTYPE_NONE;
	}
	if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_byte_class)) {
		return JTYPE_BYTE;
	}
	if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_short_class)) {
		return JTYPE_SHORT;
	}
	if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_int_class)) {
		return JTYPE_INT;
	}
	if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_long_class)) {
		return JTYPE_LONG;
	}
	if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_float_class)) {
		return JTYPE_FLOAT;
	}
	if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_double_class)) {
		return JTYPE_DOUBLE;
	}
	if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_boolean_class)) {
		return JTYPE_BOOLEAN;
	}
	if ((*javaEnv)->IsAssignableFrom(javaEnv, objectClass, java_string_class)) {
		return JTYPE_STRING;
	}
	return JTYPE_OBJECT;
}

int getObjectIntValue(ljJavaObject_t* objectInterface) {
	JNIEnv * javaEnv = (JNIEnv *)objectInterface->javaEnv;
	switch (getObjectType(objectInterface)) {
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
long getObjectLongValue(ljJavaObject_t* objectInterface) {
	JNIEnv * javaEnv = (JNIEnv *)objectInterface->javaEnv;
	if (getObjectType(objectInterface) == JTYPE_LONG) {
		return (long) (*javaEnv)->CallLongMethod(javaEnv, objectInterface->object, java_long_value);
	}
	fprintf(stderr, "Trying to access int value of a non int type\n");
	return 0;
}
float getObjectFloatValue(ljJavaObject_t* objectInterface) {
	JNIEnv * javaEnv = (JNIEnv *)objectInterface->javaEnv;
	if (getObjectType(objectInterface) == JTYPE_FLOAT) {
		return (*javaEnv)->CallFloatMethod(javaEnv, objectInterface->object, java_float_value);
	}
	fprintf(stderr, "Trying to access float value of a non float type\n");
	return 0;
}
double getObjectDoubleValue(ljJavaObject_t* objectInterface) {
	JNIEnv * javaEnv = (JNIEnv *)objectInterface->javaEnv;
	if (getObjectType(objectInterface) == JTYPE_DOUBLE) {
		return (*javaEnv)->CallDoubleMethod(javaEnv, objectInterface->object, java_double_value);
	}
	fprintf(stderr, "Trying to access double value of a non double type\n");
	return 0;
}
const char* getObjectStringValue(ljJavaObject_t* objectInterface) {
	JNIEnv * javaEnv = (JNIEnv *)objectInterface->javaEnv;
	if (getObjectType(objectInterface) == JTYPE_STRING) {
		return (*javaEnv)->GetStringUTFChars(javaEnv, (jstring)objectInterface->object, NULL);
	}
	fprintf(stderr, "Trying to access string value of a non string type\n");
	return NULL;
}

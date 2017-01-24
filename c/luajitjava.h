#define DllExport   __declspec( dllexport )

#ifndef _Included_luajitjava
#define _Included_luajitjava
#ifdef __cplusplus
extern "C" {
#endif

typedef struct ljJavaClass {
	void* ljEnv;
	void* classObject;
} ljJavaClass_t;
typedef struct ljJavaObject {
	void* ljEnv;
	void* object;
} ljJavaObject_t;

typedef enum javaArgType {
	JTYPE_NONE,
	JTYPE_BYTE,
	JTYPE_SHORT,
	JTYPE_INT,
	JTYPE_LONG,
	JTYPE_FLOAT,
	JTYPE_DOUBLE,
	JTYPE_BOOLEAN,
	JTYPE_CHAR,
	JTYPE_STRING,
	JTYPE_OBJECT
} javaArgType_t;

DllExport int isNull(void* ptr) {
	if (ptr == NULL) {
		return 1;
	}
	else {
		return 0;
	}
}

DllExport void* javaStart(const char* classPath);
DllExport void javaEnd(void* ljEnv);
DllExport int javaBindClass(ljJavaClass_t* classInterface, const char* className);
DllExport void javaReleaseClass(ljJavaClass_t* classInterface);
DllExport ljJavaObject_t* javaCheckClassField(ljJavaClass_t* classInterface, const char * key);
DllExport ljJavaObject_t* javaRunClassMethod(ljJavaClass_t* classInterface, const char * methodName, int nArgs, ...);
DllExport int javaNew(ljJavaObject_t* objectInterface, ljJavaClass_t* classInterface, int nArgs, ...);
DllExport void javaReleaseObject(ljJavaObject_t* objectInterface);
DllExport ljJavaObject_t* javaCheckObjectField(ljJavaObject_t* objectInterface, const char * key);
DllExport ljJavaObject_t* javaRunObjectMethod(ljJavaObject_t* objectInterface, const char * methodName, int nArgs, ...);
DllExport int javaGetObjectType(ljJavaObject_t* objectInterface);
DllExport int javaGetObjectIntValue(ljJavaObject_t* objectInterface);
DllExport long javaGetObjectLongValue(ljJavaObject_t* objectInterface);
DllExport float javaGetObjectFloatValue(ljJavaObject_t* objectInterface);
DllExport double javaGetObjectDoubleValue(ljJavaObject_t* objectInterface);
DllExport const char* javaGetObjectStringValue(ljJavaObject_t* objectInterface);
DllExport void javaReleaseStringValue(ljJavaObject_t* objectInterface, const char* stringValue);

#ifdef __cplusplus
}
#endif
#endif

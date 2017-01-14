#define DllExport   __declspec( dllexport )

#ifndef _Included_luajitjava
#define _Included_luajitjava
#ifdef __cplusplus
extern "C" {
#endif

typedef struct ljJavaClass {
	void* javaEnv;
	void* classObject;
} ljJavaClass_t;
typedef struct ljJavaObject {
	void* javaEnv;
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
DllExport int javaBindClass(ljJavaClass_t* classInterface, const char* className);
DllExport ljJavaObject_t* checkClassField(ljJavaClass_t* classInterface, const char * key);
DllExport ljJavaObject_t* runClassMethod(ljJavaClass_t* classInterface, const char * methodName, int nArgs, ...);
DllExport int javaNew(ljJavaObject_t* objectInterface, ljJavaClass_t* classInterface, int nArgs, ...);
DllExport ljJavaObject_t* checkObjectField(ljJavaObject_t* objectInterface, const char * key);
DllExport ljJavaObject_t* runObjectMethod(ljJavaObject_t* objectInterface, const char * methodName, int nArgs, ...);
DllExport int getObjectType(ljJavaObject_t* objectInterface);
DllExport int getObjectIntValue(ljJavaObject_t* objectInterface);
DllExport long getObjectLongValue(ljJavaObject_t* objectInterface);
DllExport float getObjectFloatValue(ljJavaObject_t* objectInterface);
DllExport double getObjectDoubleValue(ljJavaObject_t* objectInterface);
DllExport const char* getObjectStringValue(ljJavaObject_t* objectInterface);

#ifdef __cplusplus
}
#endif
#endif

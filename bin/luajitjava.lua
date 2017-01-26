local luajitjava = {}

local LUAJITJAVA_DLL = "luajitjava.dll"
local LUAJITJAVA_JAR = "luajitjava.jar"

--get current dir
local this_file = debug.getinfo(1,'S').source;
local current_dir = string.format("%s%s", this_file:match("^@(.-)([\\])luajitjava%.lua$"))

--ffi is the luajit library loader package
local ffi = require("ffi")

--first we have to define all methods we are going to use
ffi.cdef[[
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
typedef struct javaArgTypes { javaArgType_t types; } javaArgTypes;

void* javaStart(const char* classPath);
void javaEnd(void* ljEnv);
int javaBindClass(ljJavaClass_t* classInterface, const char* className);
void javaReleaseClass(ljJavaClass_t* classInterface);
ljJavaObject_t* javaCheckClassField(ljJavaClass_t* classInterface, const char * key);
ljJavaObject_t* javaRunClassMethod(ljJavaClass_t* classInterface, const char * methodName, int nArgs, ...);
int javaNew(ljJavaObject_t* objectInterface, ljJavaClass_t* classInterface, int nArgs, ...);
void javaReleaseObject(ljJavaObject_t* objectInterface);
ljJavaObject_t* javaCheckObjectField(ljJavaObject_t* objectInterface, const char * key);
ljJavaObject_t* javaRunObjectMethod(ljJavaObject_t* objectInterface, const char * methodName, int nArgs, ...);
int javaGetObjectType(ljJavaObject_t* objectInterface);
int javaGetObjectIntValue(ljJavaObject_t* objectInterface);
long javaGetObjectLongValue(ljJavaObject_t* objectInterface);
float javaGetObjectFloatValue(ljJavaObject_t* objectInterface);
double javaGetObjectDoubleValue(ljJavaObject_t* objectInterface);
const char* javaGetObjectStringValue(ljJavaObject_t* objectInterface);
void javaReleaseStringValue(ljJavaObject_t* objectInterface, const char* stringValue);

int isNull(void* ptr);
]]

--load the luajitjava bindings C library
local luajitjava_bindings = ffi.load(current_dir .. LUAJITJAVA_DLL)

--give access to arg types
luajitjava.JTYPE_NONE = luajitjava_bindings.JTYPE_NONE
luajitjava.JTYPE_BYTE = luajitjava_bindings.JTYPE_BYTE
luajitjava.JTYPE_SHORT = luajitjava_bindings.JTYPE_SHORT
luajitjava.JTYPE_INT = luajitjava_bindings.JTYPE_INT
luajitjava.JTYPE_LONG = luajitjava_bindings.JTYPE_LONG
luajitjava.JTYPE_FLOAT = luajitjava_bindings.JTYPE_FLOAT
luajitjava.JTYPE_DOUBLE = luajitjava_bindings.JTYPE_DOUBLE
luajitjava.JTYPE_BOOLEAN = luajitjava_bindings.JTYPE_BOOLEAN
luajitjava.JTYPE_CHAR = luajitjava_bindings.JTYPE_CHAR
luajitjava.JTYPE_STRING = luajitjava_bindings.JTYPE_STRING
luajitjava.JTYPE_OBJECT = luajitjava_bindings.JTYPE_OBJECT


--this method start the java virtual machine and load the luajitjava bindings java proxy library
-- and store the java environment in a global variable
local lj_env = nil
function luajitjava.java_init(class_path)
  if lj_env then
    return
  end
  lj_env = luajitjava_bindings.javaStart(string.format("%s;%s%s", class_path, current_dir, LUAJITJAVA_JAR))
end

function luajitjava.java_end()
  if not lj_env then
    return
  end
  luajitjava_bindings.javaEnd(lj_env)
  lj_env = nil
end

--utility func to prepare params from varargs
local function prepare_params(arg_table)
  if #arg_table % 2 ~= 0 then
    print("java params error : should be pairs [type, value], got an odd number of params")
    return
  end
  local result = {}
  local cur_type = luajitjava_bindings.JTYPE_NONE
  for i,v in ipairs(arg_table) do
    if cur_type == luajitjava_bindings.JTYPE_NONE then
      cur_type = v
      table.insert(result, ffi.new("int", cur_type))
    else
      local value
      if cur_type == luajitjava_bindings.JTYPE_BYTE then
        value = ffi.new("char", v)
      elseif cur_type == luajitjava_bindings.JTYPE_SHORT then
        value = ffi.new("short", v)
      elseif cur_type == luajitjava_bindings.JTYPE_INT then
        value = ffi.new("int", v)
      elseif cur_type == luajitjava_bindings.JTYPE_LONG then
        value = ffi.new("long", v)
      elseif cur_type == luajitjava_bindings.JTYPE_FLOAT then
        value = ffi.new("float", v)
      elseif cur_type == luajitjava_bindings.JTYPE_DOUBLE then
        value = ffi.new("double", v)
      elseif cur_type == luajitjava_bindings.JTYPE_BOOLEAN then
        value = ffi.new("char", v)
      elseif cur_type == luajitjava_bindings.JTYPE_CHAR then
        value = ffi.new("char", v)
      elseif cur_type == luajitjava_bindings.JTYPE_STRING then
        value = v
      elseif cur_type == luajitjava_bindings.JTYPE_OBJECT then
        value = v
      else
        print("java params: use of an unknown java type")
      end
      table.insert(result, value)
      cur_type = luajitjava_bindings.JTYPE_NONE
    end
  end
  return result
end

--utility func to pack params from varargs, using previous func as first step
local function pack_lib_args(other_args ,lua_arg_table)
  local params = prepare_params(lua_arg_table)
  if not params then
    return nil
  end
  local lib_args = {}
  for i, param in ipairs(other_args) do
    table.insert(lib_args, param)
  end
  table.insert(lib_args, #params)
  for i, param in ipairs(params) do
    table.insert(lib_args, param)
  end
  return lib_args
end

--predeclarations of functions
local javaIndex
local javaValue

--garbage collector function, to release java object or class
local function javaRelease(self)
--  print("releasing", self)
  if ffi.istype(JavaObjectType, self) then
    luajitjava_bindings.javaReleaseObject(self)
  elseif ffi.istype(JavaClassType, self) then
    luajitjava_bindings.javaReleaseClass(self)
  end
end

--get lua types to instanciate c objects and add metatable for direct calls to parameters or methods
local metatable = {
  __index = function(self, key)
    return javaIndex(self, key)
  end,
}
JavaClassType = ffi.metatype("ljJavaClass_t", metatable)
JavaObjectType = ffi.metatype("ljJavaObject_t", metatable)

--callback common to classes and objects to get fields or methods
javaIndex = function(self, key)
  if not lj_env then
    return
  end
  
  --redirect call to __value
  if key == "__value" then
    return javaValue(self)
  end
  --redirect call to __release
  if key == "__release" then
    return function() javaRelease(self) end
  end
  
  local field
  if ffi.istype(JavaObjectType, self) then
    field = luajitjava_bindings.javaCheckObjectField(self, key)
  elseif ffi.istype(JavaClassType, self) then
    field = luajitjava_bindings.javaCheckClassField(self, key)
  end
  if field and luajitjava_bindings.isNull(field) == 0 then
--    print("created class/object field", field)
    return field
  else
    --not a field, consider it is a method
    local proxy_func = function(foo, ...)
      local lib_args = pack_lib_args({self, key}, {...})
      if not lib_args then
        print("java_object : invalid method params")
        return
      end
      local result_object
      if ffi.istype(JavaObjectType, self) then
        result_object = luajitjava_bindings.javaRunObjectMethod(unpack(lib_args))
      elseif ffi.istype(JavaClassType, self) then
        result_object = luajitjava_bindings.javaRunClassMethod(unpack(lib_args))
      end
      if luajitjava_bindings.isNull(result_object) == 0 then
--        print("created class method result", result_object)
        return result_object
      end
    end
    --end of method
    return proxy_func
  end
end


--get value of a java object as a lua value, only works on type objects
javaValue = function(self)
  local return_value = nil
  if not lj_env then
    return
  end
  if not ffi.istype(JavaObjectType, self) then
    print("java value: java element doesn't seem to be an object")
    return
  end
  local type = luajitjava_bindings.javaGetObjectType(self)
  if type == luajitjava_bindings.JTYPE_BYTE then
    return_value = luajitjava_bindings.javaGetObjectIntValue(self)
  elseif type == luajitjava_bindings.JTYPE_SHORT then
    return_value = luajitjava_bindings.javaGetObjectIntValue(self)
  elseif type == luajitjava_bindings.JTYPE_INT then
    return_value = luajitjava_bindings.javaGetObjectIntValue(self)
  elseif type == luajitjava_bindings.JTYPE_LONG then
    --lua cannot handle 64bits data, this would return a cdata object
    --return luajitjava_bindings.javaGetObjectLongValue(self)
    return nil
  elseif type == luajitjava_bindings.JTYPE_FLOAT then
    return_value = luajitjava_bindings.javaGetObjectFloatValue(self)
  elseif type == luajitjava_bindings.JTYPE_DOUBLE then
    return_value = luajitjava_bindings.javaGetObjectDoubleValue(self)
  elseif type == luajitjava_bindings.JTYPE_BOOLEAN then
    return_value = luajitjava_bindings.javaGetObjectIntValue(self)
  elseif type == luajitjava_bindings.JTYPE_CHAR then
    return_value = luajitjava_bindings.javaGetObjectIntValue(self)
  elseif type == luajitjava_bindings.JTYPE_STRING then
    local jstring_value = luajitjava_bindings.javaGetObjectStringValue(self)
    return_value = ffi.string(jstring_value)
    luajitjava_bindings.javaReleaseStringValue(self, jstring_value)
  elseif type == luajitjava_bindings.JTYPE_OBJECT then
    --the object is not a type object
    print("java value: trying to get the value of an object that is not a type value")
    return nil
  else
    print("java value: use of an unknown java type")
    return nil
  end
--  print("created object value", return_value)
  return return_value
end


function luajitjava.get_java_class(class_name)
  if not lj_env then
    return
  end
  local new_class = JavaClassType(lj_env)
  if (luajitjava_bindings.javaBindClass(new_class, class_name) ~= 0) then
    return new_class
  else
    return nil
  end
end


function luajitjava.new_java_object(java_class, ...)
  if not lj_env then
    return
  end
  local release_class = false
  if type(java_class) == "string" then
    java_class = luajitjava.get_java_class(java_class)
    release_class = true
  elseif type(java_class) ~= "cdata" then
    -- not a class binding struct
    return
  end
  local new_object = JavaObjectType(lj_env)
  local lib_args = pack_lib_args({new_object, java_class}, {...})
  if not lib_args then
    print("new_java_object : invalid constructor params")
    if release_class then
      javaRelease(java_class)
    end
    return
  end
  if (luajitjava_bindings.javaNew(unpack(lib_args)) ~= 0) then
    if release_class then
      javaRelease(java_class)
    end
    return new_object
  end
end

return luajitjava
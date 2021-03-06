local luajitjava = {}

--get current dir
local this_file = debug.getinfo(1,'S').source;
local current_dir = string.format("%s%s", this_file:match("^@(.-)([\\])luajitjava%.lua$"))

--ffi is the luajit library loader package
local ffi = require("ffi")

--first we have to define all methods we are going to use
ffi.cdef[[
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
typedef struct javaArgTypes { javaArgType_t types; } javaArgTypes;

void* javaStart(const char* classPath);
int javaBindClass(ljJavaClass_t* classInterface, const char* className);
ljJavaObject_t* checkClassField(ljJavaClass_t* classInterface, const char * key);
ljJavaObject_t* runClassMethod(ljJavaClass_t* classInterface, const char * methodName, int nArgs, ...);
int javaNew(ljJavaObject_t* objectInterface, ljJavaClass_t* classInterface, int nArgs, ...);
ljJavaObject_t* checkObjectField(ljJavaObject_t* objectInterface, const char * key);
ljJavaObject_t* runObjectMethod(ljJavaObject_t* objectInterface, const char * methodName, int nArgs, ...);

int isNull(void* ptr);
]]

--load the luajitjava bindings C library
local luajitjava_bindings = ffi.load(current_dir .. "luajitjava.dll")

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
java_env = nil
function luajitjava.java_init(class_path)
  if java_env then
    return
  end
  java_env = luajitjava_bindings.javaStart(string.format("%s;%sluajitjava.jar", class_path, current_dir))
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

--common callback to get fields or methods of java classes and objects
local function javaIndex(self, key)
  if not java_env then
    return
  end
  print("looking for",self,key)
  local field
  if ffi.istype(JavaObjectType, self) then
    field = luajitjava_bindings.checkObjectField(self, key)
  elseif ffi.istype(JavaClassType, self) then
    field = luajitjava_bindings.checkClassField(self, key)
  end
  if luajitjava_bindings.isNull(field) == 0 then
    print("found field")
    return field
  else
    --not a field, consider it is a method
    local proxy_func = function(self, ...)
      local lib_args = pack_lib_args({self, key}, {...})
      if not lib_args then
        print("java_object : invalid method params")
        return
      end
      print("run method")
      local result_object
      if ffi.istype(JavaObjectType, self) then
        result_object = luajitjava_bindings.runObjectMethod(unpack(lib_args))
      elseif ffi.istype(JavaClassType, self) then
        result_object = luajitjava_bindings.runClassMethod(unpack(lib_args))
      end
      if luajitjava_bindings.isNull(result_object) == 0 then
        return result_object
      end
    end
    --end of method
    return proxy_func
  end
end

--get lua types to instanciate c objects and add metatable for direct calls to parameters or methods
-- java class type
local class_mt = {
  __index = javaIndex,
}
JavaClassType = ffi.metatype("ljJavaClass_t", class_mt)
-- java object type
local object_mt = {
  __index = javaIndex,
}
JavaObjectType = ffi.metatype("ljJavaObject_t", object_mt)

function luajitjava.get_java_class(class_name)
  if not java_env then
    return
  end
  local new_class = JavaClassType(java_env)
  if (luajitjava_bindings.javaBindClass(new_class, class_name) ~= 0) then
    return new_class
  else
    return nil
  end
end


function luajitjava.new_java_object(java_class, ...)
  if not java_env then
    return
  end
  if type(java_class) == "string" then
    java_class = luajitjava.get_java_class(java_class)
  elseif type(java_class) ~= "cdata" then
    -- not a class binding struct
    return
  end
  local new_object = JavaObjectType(java_env)
  local lib_args = pack_lib_args({new_object, java_class}, {...})
  if not lib_args then
    print("new_java_object : invalid constructor params")
    return
  end
  for i,v in ipairs(lib_args) do
    print(i,v)
  end
  if (luajitjava_bindings.javaNew(unpack(lib_args)) ~= 0) then
    return new_object
  end
end

return luajitjava
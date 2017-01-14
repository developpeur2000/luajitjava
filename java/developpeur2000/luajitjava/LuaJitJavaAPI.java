/*
 * Copyright (C) 2003-2007 Kepler Project - 2017 David Frémont
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
 */

package developpeur2000.luajitjava;

import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

/**
 * Class that contains functions accessed by lua.
 * 
 * @author Thiago Ponte
 */
public final class LuaJitJavaAPI
{

  private LuaJitJavaAPI()
  {
  }
  
  /**
   * Utility method to compare expected args of a function with provided ones
   * 
   * @param methodParams table of expected classes / types
   * @param providedArgs array of Object parameters, can be modified to fit primitive types
   * @return true if method can be called with provided args (eventually modified here)
   */
	private static boolean areCompatibleArgs(Class[] methodParams, Object[] providedArgs) {
		Class argClass;
		if (methodParams.length != providedArgs.length)
			return false;
		for (int j = 0; j < methodParams.length; j++) {
			//System.out.println("class method type vs args type");
			//System.out.println(methodParams[j].toString());
			argClass = providedArgs[j].getClass();
			//System.out.println(argClass.toString());
			if (!methodParams[j].isAssignableFrom(argClass)) {
				//check primitive data types correspondance
				if (methodParams[j] == byte.class && argClass == Byte.class) {
					providedArgs[j] = ((Byte)providedArgs[j]).byteValue();
				} else if (methodParams[j] == short.class && argClass == Short.class) {
					providedArgs[j] = ((Short)providedArgs[j]).shortValue();
				} else if (methodParams[j] == int.class && argClass == Integer.class) {
					providedArgs[j] = ((Integer)providedArgs[j]).intValue();
				} else if (methodParams[j] == long.class && argClass == Long.class) {
					providedArgs[j] = ((Long)providedArgs[j]).longValue();
				} else if (methodParams[j] == float.class && argClass == Float.class) {
					providedArgs[j] = ((Float)providedArgs[j]).floatValue();
				} else if (methodParams[j] == double.class && argClass == Double.class) {
					providedArgs[j] = ((Double)providedArgs[j]).doubleValue();
				} else if (methodParams[j] == boolean.class && argClass == Boolean.class) {
					providedArgs[j] = ((Boolean)providedArgs[j]).booleanValue();
				} else if (methodParams[j] == char.class && argClass == Byte.class) {
					providedArgs[j] = ((Byte)providedArgs[j]).byteValue();
				} else {
					return false;
				}
			}
		}
		return true;
	}


  /**
   * Create a new instance of a java Object of the type className
   * 
   * @param className name of the class
   * @param args array of Object parameters for constructor
   * @return newly created object
   * @throws LuaException
   */
  public static Object javaNewInstance(String className, Object[] args)
      throws LuaException
  {
      Class clazz;
      try
      {
        clazz = Class.forName(className);
      }
      catch (ClassNotFoundException e)
      {
        throw new LuaException(e);
      }
      return javaNew(clazz, args);
  }

  /**
   * javaNew returns a new instance of a given clazz
   * 
   * @param clazz class to be instanciated
   * @param args array of Object parameters for constructor
   * @return newly created object
   * @throws LuaException
   */
	public static Object javaNew(Class clazz, Object[] args) throws LuaException {
		//System.out.println("look for class constructor");
		//System.out.println(clazz.toString());
		Constructor[] constructors = clazz.getConstructors();
		Constructor constructor = null;
		// gets method and arguments
		for (int i = 0; i < constructors.length; i++) {
			if (areCompatibleArgs(constructors[i].getParameterTypes(), args)) {
				constructor = constructors[i];
				break;
			}
		}

		// If method is null means there isn't one receiving the given arguments
		if (constructor == null) {
			throw new LuaException("Invalid method call. No such method.");
		}

		Object ret;
		try {
			ret = constructor.newInstance(args);
		} catch (Exception e) {
			throw new LuaException(e);
		}

		if (ret == null) {
			throw new LuaException("Couldn't instantiate java Object");
		}

		return ret;
	}
	
	/**
	 * Checks if there is a field on the obj with the given name
	 * 
	 * @param obj object to be inspected
	 * @param fieldName name of the field to be inpected
	 * @return the object represented by the field, or null if it does not exist
	 */
	public static Object checkField(Object obj, String fieldName) throws LuaException {
		Field field = null;
		Class objClass;

		if (obj instanceof Class) {
			objClass = (Class) obj;
		} else {
			objClass = obj.getClass();
		}

		try {
			field = objClass.getField(fieldName);
		} catch (Exception e) {
			return null;
		}

		if (field == null) {
			return null;
		}

		Object ret = null;
		try {
			ret = field.get(obj);
		} catch (Exception e1) {
			return null;
		}

		return ret;
	}
  
  	 
  	 
  /**
   * Java implementation of the metamethod __index for running methods
   * 
   * @param obj Object that should provide the method
   * @param methodName the name of the method
   * @return a method or null
   */
	public static Object runMethod(Object obj, String methodName, Object[] objs) throws LuaException {
		Method method = null;
		Class clazz;

		if (obj instanceof Class) {
			clazz = (Class) obj;
			// First try. Static methods of the object
			method = getMethod(clazz, methodName, objs);

			if (method == null)
				clazz = Class.class;
			// Second try. Methods of the java.lang.Class class
			method = getMethod(clazz, methodName, objs);
		} else {
			clazz = obj.getClass();
			method = getMethod(clazz, methodName, objs);
		}

		// If method is null means there isn't one receiving the given arguments
		if (method == null) {
			throw new LuaException("Invalid method call. No such method.");
		}

		Object ret;
		try {
			if (Modifier.isPublic(method.getModifiers())) {
				method.setAccessible(true);
			}

			// if (obj instanceof Class)
			if (Modifier.isStatic(method.getModifiers())) {
				ret = method.invoke(null, objs);
			} else {
				ret = method.invoke(obj, objs);
			}
		} catch (Exception e) {
			throw new LuaException(e);
		}

		return ret;
	}
	
	private static Method getMethod(Class clazz, String methodName, Object[] args) {
		//System.out.println("look for class method");
		//System.out.println(clazz.toString());
		Method[] methods = clazz.getMethods();
		Method method = null;

		// gets method and arguments
		for (int i = 0; i < methods.length; i++) {
			if (!methods[i].getName().equals(methodName))
				continue;

			if (areCompatibleArgs(methods[i].getParameterTypes(), args)) {
				method = methods[i];
				break;
			}

		}

		return method;
	}

  
  /**
   * Java function that implements the __index for Java arrays
   * @param luaState int that indicates the state used
   * @param obj Object to be indexed
   * @param index index number of array. Since Lua index starts from 1,
   * the number used will be (index - 1)
   * @return number of returned objects
   */
/*  public static int arrayIndex(int luaState, Object obj, int index) throws LuaException
  {
    LuaState L = LuaStateFactory.getExistingState(luaState);

    synchronized (L)
    {

      if (!obj.getClass().isArray())
        throw new LuaException("Object indexed is not an array.");
	  
      if (Array.getLength(obj) < index)
        throw new LuaException("Index out of bounds.");
	  
      L.pushObjectValue(Array.get(obj, index - 1));
	  
      return 1;
    }
  }*/

  /**
   * Java function to be called when a java Class metamethod __index is called.
   * This function returns 1 if there is a field with searchName and 2 if there
   * is a method if the searchName
   * 
   * @param luaState int that represents the state to be used
   * @param clazz class to be indexed
   * @param searchName name of the field or method to be accessed
   * @return number of returned objects
   * @throws LuaException
   */
/*  public static int classIndex(int luaState, Class clazz, String searchName)
      throws LuaException
  {
    synchronized (LuaStateFactory.getExistingState(luaState))
    {
      int res;

      res = checkField(luaState, clazz, searchName);

      if (res != 0)
      {
        return 1;
      }

      res = checkMethod(luaState, clazz, searchName);

      if (res != 0)
      {
        return 2;
      }

      return 0;
    }
  }*/

  
  /**
   * Java function to be called when a java object metamethod __newindex is called.
   * 
   * @param luaState int that represents the state to be used
   * @param object to be used
   * @param fieldName name of the field to be set
   * @return number of returned objects
   * @throws LuaException
   */
/*  public static int objectNewIndex(int luaState, Object obj, String fieldName)
    throws LuaException
  {
    LuaState L = LuaStateFactory.getExistingState(luaState);

    synchronized (L)
    {
      Field field = null;
      Class objClass;

      if (obj instanceof Class)
      {
        objClass = (Class) obj;
      }
      else
      {
        objClass = obj.getClass();
      }

      try
      {
        field = objClass.getField(fieldName);
      }
      catch (Exception e)
      {
        throw new LuaException("Error accessing field.", e);
      }
      
      Class type = field.getType();
      Object setObj = compareTypes(L, type, 3);
      
      if (field.isAccessible())
        field.setAccessible(true);
      
      try
      {
        field.set(obj, setObj);
      }
      catch (IllegalArgumentException e)
      {
        throw new LuaException("Ilegal argument to set field.", e);
      }
      catch (IllegalAccessException e)
      {
        throw new LuaException("Field not accessible.", e);
      }
    }
    
    return 0;
  }*/
  
  
  /**
   * Java function to be called when a java array metamethod __newindex is called.
   * 
   * @param luaState int that represents the state to be used
   * @param object to be used
   * @param index index number of array. Since Lua index starts from 1,
   * the number used will be (index - 1)
   * @return number of returned objects
   * @throws LuaException
   */
/*  public static int arrayNewIndex(int luaState, Object obj, int index)
    throws LuaException
  {
    LuaState L = LuaStateFactory.getExistingState(luaState);

    synchronized (L)
    {
      if (!obj.getClass().isArray())
        throw new LuaException("Object indexed is not an array.");
  	  
      if (Array.getLength(obj) < index)
        throw new LuaException("Index out of bounds.");

      Class type = obj.getClass().getComponentType();
      Object setObj = compareTypes(L, type, 3);
      
      Array.set(obj, index - 1, setObj);
    }
    
    return 0;
  }*/
  
  

  /**
   * Calls the static method <code>methodName</code> in class <code>className</code>
   * that receives a LuaState as first parameter.
   * @param luaState int that represents the state to be used
   * @param className name of the class that has the open library method
   * @param methodName method to open library
   * @return number of returned objects
   * @throws LuaException
   */
/*  public static int javaLoadLib(int luaState, String className, String methodName)
  	throws LuaException
  {
    LuaState L = LuaStateFactory.getExistingState(luaState);
    
    synchronized (L)
    {
      Class clazz;
      try
      {
        clazz = Class.forName(className);
      }
      catch (ClassNotFoundException e)
      {
        throw new LuaException(e);
      }

      try
      {
        Method mt = clazz.getMethod(methodName, new Class[] {LuaState.class});
        Object obj = mt.invoke(null, new Object[] {L});
        
        if (obj != null && obj instanceof Integer)
        {
          return ((Integer) obj).intValue();
        }
        else
          return 0;
      }
      catch (Exception e)
      {
        throw new LuaException("Error on calling method. Library could not be loaded. " + e.getMessage());
      }
    }
  }*/



  /**
   * Checks to see if there is a method with the given name.
   * 
   * @param luaState int that represents the state to be used
   * @param obj object to be inspected
   * @param methodName name of the field to be inpected
   * @return number of returned objects
   */
/*  private static int checkMethod(int luaState, Object obj, String methodName)
  {
    LuaState L = LuaStateFactory.getExistingState(luaState);

    synchronized (L)
    {
      Class clazz;

      if (obj instanceof Class)
      {
        clazz = (Class) obj;
      }
      else
      {
        clazz = obj.getClass();
      }

      Method[] methods = clazz.getMethods();

      for (int i = 0; i < methods.length; i++)
      {
        if (methods[i].getName().equals(methodName))
          return 1;
      }

      return 0;
    }
  }*/

  /**
   * Function that creates an object proxy and pushes it into the stack
   * 
   * @param luaState int that represents the state to be used
   * @param implem interfaces implemented separated by comma (<code>,</code>)
   * @return number of returned objects
   * @throws LuaException
   */
/*  public static int createProxyObject(int luaState, String implem)
    throws LuaException
  {
    LuaState L = LuaStateFactory.getExistingState(luaState);

    synchronized (L)
    {
      try
      {
        if (!(L.isTable(2)))
          throw new LuaException(
              "Parameter is not a table. Can't create proxy.");

        LuaObject luaObj = L.getLuaObject(2);

        Object proxy = luaObj.createProxy(implem);
        L.pushJavaObject(proxy);
      }
      catch (Exception e)
      {
        throw new LuaException(e);
      }

      return 1;
    }
  }*/

/*  private static Object compareTypes(LuaState L, Class parameter, int idx)
    throws LuaException
  {
    boolean okType = true;
    Object obj = null;

    if (L.isBoolean(idx))
    {
      if (parameter.isPrimitive())
      {
        if (parameter != Boolean.TYPE)
        {
          okType = false;
        }
      }
      else if (!parameter.isAssignableFrom(Boolean.class))
      {
        okType = false;
      }
      obj = new Boolean(L.toBoolean(idx));
    }
    else if (L.type(idx) == LuaState.LUA_TSTRING.intValue())
    {
      if (!parameter.isAssignableFrom(String.class))
      {
        okType = false;
      }
      else
      {
        obj = L.toString(idx);
      }
    }
    else if (L.isFunction(idx))
    {
      if (!parameter.isAssignableFrom(LuaObject.class))
      {
        okType = false;
      }
      else
      {
        obj = L.getLuaObject(idx);
      }
    }
    else if (L.isTable(idx))
    {
      if (!parameter.isAssignableFrom(LuaObject.class))
      {
        okType = false;
      }
      else
      {
        obj = L.getLuaObject(idx);
      }
    }
    else if (L.type(idx) == LuaState.LUA_TNUMBER.intValue())
    {
      Double db = new Double(L.toNumber(idx));
      
      obj = LuaState.convertLuaNumber(db, parameter);
      if (obj == null)
      {
        okType = false;
      }
    }
    else if (L.isUserdata(idx))
    {
      if (L.isObject(idx))
      {
        Object userObj = L.getObjectFromUserdata(idx);
        if (!parameter.isAssignableFrom(userObj.getClass()))
        {
          okType = false;
        }
        else
        {
          obj = userObj;
        }
      }
      else
      {
        if (!parameter.isAssignableFrom(LuaObject.class))
        {
          okType = false;
        }
        else
        {
          obj = L.getLuaObject(idx);
        }
      }
    }
    else if (L.isNil(idx))
    {
      obj = null;
    }
    else
    {
      throw new LuaException("Invalid Parameters.");
    }

    if (!okType)
    {
      throw new LuaException("Invalid Parameter.");
    }

    return obj;
  }*/

}
 